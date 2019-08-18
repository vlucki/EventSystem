#pragma once
#include <memory>
#include <vector>

namespace Events
{
	///<summary>
	///Base class for every function wrapper in the system. 
	///</summary>
	template<typename ReturnType, typename ...Args>
	class FunctionWrapper
	{
	public:
		/// Execute function with given arguments
		virtual ReturnType operator()(Args&& ...args) = 0;
	};

	///<summary>
	///Wrapper for global or static functions.
	///</summary>
	template<typename ReturnType, typename ...Args>
	class GlobalFunctionWrapper : public FunctionWrapper<ReturnType, Args...>
	{
	private:
		ReturnType(*funcPtr)(Args...);

	public:
		GlobalFunctionWrapper(ReturnType(*funcPtr)(Args...)) : funcPtr(funcPtr) {}

		/// See FunctionWrapper operator()
		ReturnType operator()(Args&& ...args) override
		{
			return funcPtr(std::forward<Args>(args)...);
		}

		bool operator ==(ReturnType(*otherFuncPtr)(Args...)) const
		{
			return funcPtr == otherFuncPtr;
		}


		bool operator ==(GlobalFunctionWrapper<ReturnType, Args...>& otherGlobalFunction) const
		{
			return funcPtr == otherGlobalFunction.funcPtr;
		}
	};

	///<summary>
	///Base wrapper class for non-static class functions.
	///</summary>
	template<typename CallerType, typename ReturnType, typename ...Args>
	class MemberFunctionWrapper : public FunctionWrapper<ReturnType, Args...>
	{
	protected:
		CallerType& caller;

	public:
		MemberFunctionWrapper(CallerType& caller) : caller(caller) {}

		bool IsCaller(CallerType& possibleCaller)
		{
			return &caller == &possibleCaller; //just compare memory addresses
		}

		template<typename FuncType>
		ReturnType Execute(FuncType* function, Args&& ...args)
		{
			return caller.*function(std::forward<Args>(args)...);
		}
	};
	
	///<summary>
	///Wrapper for non-static non-const class functions.
	///</summary>
	template<typename CallerType, typename ReturnType, typename ...Args>
	class RegularMemberFunctionWrapper : public MemberFunctionWrapper<CallerType, ReturnType, Args...>
	{
	private:
		ReturnType(CallerType::* funcPtr)(Args...);

	public:
		RegularMemberFunctionWrapper(ReturnType(CallerType::* funcPtr)(Args...), CallerType& caller) : 
		MemberFunctionWrapper<CallerType, ReturnType, Args...>(caller), funcPtr(funcPtr) {}

		ReturnType operator()(Args&& ...args) override
		{
			return MemberFunctionWrapper<CallerType, ReturnType, Args...>::Execute(funcPtr, std::forward<Args>(args)...);
			//return (this->caller.*funcPtr)(std::forward<Args>(args)...);
		}

		bool IsFunctionFromCaller(ReturnType(CallerType::* otherFuncPtr)(Args...), CallerType& caller)
		{
			return IsFunction(otherFuncPtr) && IsCaller(caller);
		}

		bool IsFunction(ReturnType(CallerType::* otherFuncPtr)(Args...)) 
		{
			return funcPtr == otherFuncPtr;
		}
	};
	
	///<summary>
	///Wrapper for non-static const class functions.
	///</summary>
	template<typename CallerType, typename ReturnType, typename ...Args>
	class ConstMemberFunctionWrapper : public MemberFunctionWrapper<CallerType, ReturnType, Args...>
	{
	private:
		ReturnType(CallerType::*funcPtr) (Args...) const;

	public:
		ConstMemberFunctionWrapper(ReturnType(CallerType::* funcPtr)(Args...) const, CallerType& caller) :
			MemberFunctionWrapper<CallerType, ReturnType, Args...>(caller), funcPtr(funcPtr) { }

		/// See FunctionWrapper operator()
		ReturnType operator()(Args&& ...args) override
		{
			return (this->caller.*funcPtr)(std::forward<Args>(args)...);
		}

		bool IsFunctionFromCaller(ReturnType(CallerType::* otherFuncPtr)(Args...) const, CallerType& caller)
		{
			return IsFunction(otherFuncPtr) && IsCaller(caller);
		}

		bool IsFunction(ReturnType(CallerType::* otherFuncPtr)(Args...) const)
		{
			return funcPtr == otherFuncPtr;
		}
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
		std::vector<std::unique_ptr<FunctionWrapper<void, Args...>>> boundFunctions;

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
				//Must dereference it firts, otherwise the compiler won't be happy and complain about a function receiving X amount of parameters not existing.
				(*function)(std::forward<Args>(args)...);
			}
		}

		///<summary>
		///Receives a member function from a given object and stores it in the list of functions attached to this event.
		///</summary>
		template <typename CallerType>
		void Bind(void (CallerType::* funcPtr)(Args...), CallerType& caller)
		{
			boundFunctions.emplace_back(std::make_unique<RegularMemberFunctionWrapper<CallerType, void, Args...>>(funcPtr, caller));
		}


		///<summary>
		///Receives a const member function from a given object and stores it in the list of functions attached to this event.
		///</summary>
		template <typename CallerType>
		void Bind(void(CallerType::* funcPtr) (Args...) const, CallerType& caller)
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