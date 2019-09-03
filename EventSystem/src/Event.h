#pragma once
#include <memory>
#include <vector>

namespace Events
{
	///<summary>
	///Base class for every function wrapper in the system. 
	///</summary>
	template<typename ReturnType, typename ...Args>
	class FunctionWrapperBase
	{
	public:
		/// Execute function with given arguments
		virtual ReturnType operator()(Args&& ...args) = 0;
	};

	template<typename Signature, typename ReturnType, typename ...Args>
	class FunctionWrapper : public FunctionWrapperBase<ReturnType, Args...>
	{
	protected:
		Signature funcPtr;
	public:
		FunctionWrapper(Signature funcPtr) : funcPtr(funcPtr) { }

		bool IsFunction(Signature function)
		{
			return funcPtr == function;
		}
	};

	///<summary>
	///Wrapper for global or static functions.
	///</summary>
	template<typename ReturnType, typename ...Args >
	class GlobalFunctionWrapper : public FunctionWrapper<ReturnType(*)(Args...), ReturnType, Args...>
	{
	public:
		GlobalFunctionWrapper(ReturnType(*funcPtr)(Args...)) : FunctionWrapper<ReturnType(*)(Args...), ReturnType, Args...>(funcPtr) { }

		ReturnType operator() (Args&& ...args) override
		{
			return FunctionWrapper<ReturnType(*)(Args...), ReturnType, Args...>::funcPtr(std::forward<Args>(args)...);
		}

		bool operator ==(ReturnType(*otherFuncPtr)(Args...)) const
		{
			return FunctionWrapper<ReturnType(*)(Args...), ReturnType, Args...>::funcPtr == otherFuncPtr;
		}

		bool operator ==(GlobalFunctionWrapper<ReturnType, Args...>& otherGlobalFunction) const
		{
			return FunctionWrapper<ReturnType(*)(Args...), ReturnType, Args...>::funcPtr == otherGlobalFunction.funcPtr;
		}
	};

	///<summary>
	///Base wrapper class for non-static class functions.
	///</summary>
	template<typename Signature, typename CallerType, typename ReturnType, typename ...Args>
	class MemberFunctionWrapper : public FunctionWrapper<Signature, ReturnType, Args...>
	{
	private:
		CallerType& caller;
	public:
		MemberFunctionWrapper(Signature funcPtr, CallerType& caller) :
			FunctionWrapper<Signature, ReturnType, Args...>(funcPtr), caller(caller) { }

		ReturnType operator()(Args&& ...args) override
		{
			//return caller.funcPtr(std::forward<Args>(args)...);
			//gotta wrap everything in ( ) before the parameters, else the compiler complains about a parameter missing and stuff...
			return (caller.*(FunctionWrapper<Signature, ReturnType, Args...>::funcPtr))(std::forward<Args>(args)...);
		}

		bool IsCaller(CallerType& possibleCaller)
		{
			return &caller == &possibleCaller; //just compare memory addresses
		}
	};

	///<summary>
	///Wrapper for non-static non-const class functions.
	///</summary>
	template<typename CallerType, typename ReturnType, typename ...Args>
	class RegularMemberFunctionWrapper : public MemberFunctionWrapper<ReturnType(CallerType::*)(Args...), CallerType, ReturnType, Args...>
	{
	public:
		RegularMemberFunctionWrapper(ReturnType(CallerType::* funcPtr)(Args...), CallerType& caller) :
			MemberFunctionWrapper<ReturnType(CallerType::*)(Args...), CallerType, ReturnType, Args...>(funcPtr, caller) { }
	};
	
	///<summary>
	///Wrapper for non-static const class functions.
	///</summary>
	template<typename CallerType, typename ReturnType, typename ...Args>
	class ConstMemberFunctionWrapper : public MemberFunctionWrapper<ReturnType(CallerType::*)(Args...) const, CallerType, ReturnType, Args...>
	{
	public:
		ConstMemberFunctionWrapper(ReturnType(CallerType::* funcPtr)(Args...) const, CallerType& caller) :
			MemberFunctionWrapper<ReturnType(CallerType::*)(Args...) const, CallerType, ReturnType, Args...>(funcPtr, caller) {	}
	};	


	/// <summary>
	/// An event stores a vector of functions with no return value.
	/// To bind a function to an event, said function must match the arguments required by the event.
	/// Eg. for an Event<string, int>, any function that wishes to be bound to it must require
	/// exclusively a string and an int parameters, in this exact order, and not return anything.
	/// </summary>
	template<typename... Args>
	class Event
	{
	private:
		std::vector<std::unique_ptr<FunctionWrapperBase<void, Args...>>> boundFunctions;

	private:

		/// <summary>
		/// Unbinds functions that satisfy a given condition. Used internally when public Unbind methods are called to
		/// locate said functions in the vector boundFunctions.
		/// </summary>
		/// <param name="ShouldRemove">Condition that a function must meet to be removed.</param>
		template <typename FuncType, typename ConditionType>
		inline void UnbindFunction(ConditionType&& ShouldRemove)
		{
			for (int i = boundFunctions.size() - 1; i >= 0; --i)
			{
				auto func = dynamic_cast<FuncType*>(boundFunctions[i].get());
				if (func != nullptr && ShouldRemove(func))
				{
					boundFunctions.erase(boundFunctions.begin() + i);
				}
			}
		}

	public:
		~Event()
		{
			UnbindAll();
		}

		void operator() (Args&& ... args)
		{
			for (auto& function : boundFunctions)
			{
				//Note to self: can't call the () operator directly like in the FunctionWrappers because the pointer is not the function itself.
				//Must dereference it first, otherwise the compiler won't be happy and complain about a function receiving X amount of parameters not existing.
				(*function)(std::forward<Args>(args)...);
			}
		}

		//TODO: research how to go about creating a Bind method that works for both reference AND value parameters

		///<summary>
		///Receives a member function from a given object and stores it in the list of functions attached to this event.
		///</summary>
		template <typename CallerType>
		void Bind(void (CallerType::* funcPtr)(Args...), CallerType& caller)
		{
			boundFunctions.emplace_back(std::make_unique<RegularMemberFunctionWrapper<CallerType, void, Args...>>(funcPtr, caller));
		}

		///<summary>
		///Receives a member function from a given object and stores it in the list of functions attached to this event.
		///</summary>
		template <typename CallerType>
		void Bind(void (CallerType::* funcPtr)(Args&&...), CallerType& caller)
		{
			boundFunctions.emplace_back(std::make_unique<RegularMemberFunctionWrapper<CallerType, void, Args...>>(funcPtr, caller));
		}
		
		///<summary>
		///Receives a const member function from a given object and stores it in the list of functions attached to this event.
		///</summary>
		template <typename CallerType>
		void Bind(void(CallerType::* funcPtr) (Args&&...) const, CallerType& caller)
		{
			boundFunctions.emplace_back(std::make_unique<ConstMemberFunctionWrapper<CallerType, void, Args...>>(funcPtr, caller));
		}

		///<summary>
		///Receives a global function and stores it in the list of functions attached to this event.
		///</summary>
		void Bind(void(*funcPtr)(Args...))
		{
			boundFunctions.emplace_back(std::make_unique<GlobalFunctionWrapper<void, Args...>>(funcPtr));
		}

		///<summary>
		///Removes a global function from the list of functions attached to this event.
		///</summary>
		void Unbind(void(*funcPtr)(Args...))
		{
			UnbindFunction<GlobalFunctionWrapper<void, Args...>>
				([funcPtr](GlobalFunctionWrapper<void, Args...>* v)
					{ return (*v) == funcPtr; });
		}

		///<summary>
		///Removes an object's member function from the list of functions attached to this event.
		///</summary>
		template <typename CallerType>
		void Unbind(void (CallerType::* funcPtr)(Args...), CallerType& caller)
		{
			UnbindFunction<MemberFunctionWrapper<CallerType, void, Args...>>
				([funcPtr, &caller](MemberFunctionWrapper<CallerType, void, Args...>* v)
					{ return v->IsFunctionFromCaller(funcPtr, caller); });
		}

		///<summary>
		///Removes an object's const member function from the list of functions attached to this event.
		///</summary>
		template <typename CallerType>
		void Unbind(void(CallerType::* funcPtr) (Args...) const, CallerType& caller)
		{
			UnbindFunction<ConstMemberFunctionWrapper<CallerType, void, Args...>>
				([funcPtr, &caller](ConstMemberFunctionWrapper<CallerType, void, Args...>* v)
					{ return v->IsFunctionFromCaller(funcPtr, caller); });
		}
		
		///<summary>
		///Removes every function from the list of functions attached to this event.
		///</summary>
		void UnbindAll()
		{
			boundFunctions.clear();
		}

	};

}