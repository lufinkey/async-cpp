//
//  Promise.hpp
//  AsyncCpp
//
//  Created by Luis Finke on 8/11/19.
//  Copyright © 2019 Luis Finke. All rights reserved.
//

#pragma once

#include <chrono>
#include <future>
#include <list>
#include <memory>
#include <mutex>
#include <thread>
#include <type_traits>
#include <utility>
#include <fgl/async/Common.hpp>
#include <fgl/async/DispatchQueue.hpp>
#include <fgl/async/LambdaTraits.hpp>
#include <fgl/async/PromiseErrorPtr.hpp>


namespace fgl {
	class DispatchQueue;
	
	DispatchQueue* getDefaultPromiseQueue();
	
	template<typename Result>
	class Promise;

	template<typename Result, typename Type>
	using PromiseOr = typename std::enable_if<
		(std::is_same<Type,Result>::value || std::is_same<Type,Promise<Result>>::value), Type>::type;
	
	template<typename PromiseType, typename ResultType = typename PromiseType::ResultType,
		bool Test = std::is_same<PromiseType, Promise<ResultType>>::value>
	struct is_promise {
		static constexpr bool value = false;
	};
	
	template<typename PromiseType, typename ResultType>
	struct is_promise<PromiseType, ResultType, true> {
		static constexpr bool value = true;
		typedef ResultType result_type;
		typedef Promise<ResultType> promise_type;
		typedef std::nullptr_t null_type;
	};
	
	template<typename Result>
	class Promise {
		template<typename OtherResult>
		friend class Promise;
	public:
		typedef Result ResultType;
		
		template<typename Arg, typename Return>
		struct _block {
			using type = Function<Return(Arg)>;
		};
		template<typename Return>
		struct _block<void, Return> {
			using type = Function<Return()>;
		};
		
		using Resolver = typename _block<Result,void>::type;
		using Rejecter = Function<void(PromiseErrorPtr)>;
		template<typename Return>
		using Then = typename _block<Result,Return>::type;
		template<typename ErrorType, typename Return>
		using Catch = Function<Return(ErrorType)>;

		explicit Promise(const Function<void(Resolver,Rejecter)>& executor);
		Promise(String name, const Function<void(Resolver,Rejecter)>& executor);


		Promise<void> then(String name, DispatchQueue* queue, Then<void> onresolve, Catch<std::exception_ptr,void> onreject);
		inline Promise<void> then(DispatchQueue* queue, Then<void> onresolve, Catch<std::exception_ptr,void> onreject);

		Promise<void> then(String name, Then<void> onresolve, Catch<std::exception_ptr,void> onreject);
		inline Promise<void> then(Then<void> onresolve, Catch<std::exception_ptr,void> onreject);

		template<typename OnResolve,
			typename Return = typename lambda_traits<OnResolve>::return_type,
			typename std::enable_if<std::is_same<Return,void>::value, std::nullptr_t>::type = nullptr>
		Promise<void> then(String name, DispatchQueue* queue, OnResolve onresolve);
		template<typename OnResolve,
			typename Return = typename lambda_traits<OnResolve>::return_type,
			typename std::enable_if<std::is_same<Return,void>::value, std::nullptr_t>::type = nullptr>
		inline Promise<void> then(DispatchQueue* queue, OnResolve onresolve);

		template<typename OnResolve,
			typename Return = typename lambda_traits<OnResolve>::return_type,
			typename std::enable_if<std::is_same<Return,void>::value, std::nullptr_t>::type = nullptr>
		Promise<void> then(String name, OnResolve onresolve);
		template<typename OnResolve,
			typename Return = typename lambda_traits<OnResolve>::return_type,
			typename std::enable_if<std::is_same<Return,void>::value, std::nullptr_t>::type = nullptr>
		inline Promise<void> then(OnResolve onresolve);

		template<typename OnResolve,
			typename Return = typename lambda_traits<OnResolve>::return_type,
			typename NextResult = typename is_promise<Return>::result_type,
			typename std::enable_if<(is_promise<Return>::value && std::is_same<Return,Promise<NextResult>>::value), std::nullptr_t>::type = nullptr>
		Promise<NextResult> then(String name, DispatchQueue* queue, OnResolve onresolve);
		template<typename OnResolve,
			typename Return = typename lambda_traits<OnResolve>::return_type,
			typename NextResult = typename is_promise<Return>::result_type,
			typename std::enable_if<(is_promise<Return>::value && std::is_same<Return,Promise<NextResult>>::value), std::nullptr_t>::type = nullptr>
		inline Promise<NextResult> then(DispatchQueue* queue, OnResolve onresolve);

		template<typename OnResolve,
				typename Return = typename lambda_traits<OnResolve>::return_type,
				typename NextResult = typename is_promise<Return>::result_type,
				typename std::enable_if<(is_promise<Return>::value && std::is_same<Return,Promise<NextResult>>::value), std::nullptr_t>::type = nullptr>
		Promise<NextResult> then(String name, OnResolve onresolve);
		template<typename OnResolve,
			typename Return = typename lambda_traits<OnResolve>::return_type,
			typename NextResult = typename is_promise<Return>::result_type,
			typename std::enable_if<(is_promise<Return>::value && std::is_same<Return,Promise<NextResult>>::value), std::nullptr_t>::type = nullptr>
		inline Promise<NextResult> then(OnResolve onresolve);


		template<typename OnReject,
			typename ErrorType = typename std::remove_reference<typename std::remove_cv<typename lambda_traits<OnReject>::template arg<0>::type>::type>::type,
			typename Return = typename lambda_traits<OnReject>::return_type,
			typename std::enable_if<std::is_same<Return,Result>::value,std::nullptr_t>::type = nullptr>
		Promise<Result> except(String name, DispatchQueue* queue, OnReject onreject);
		template<typename OnReject,
			typename ErrorType = typename std::remove_reference<typename std::remove_cv<typename lambda_traits<OnReject>::template arg<0>::type>::type>::type,
			typename Return = typename lambda_traits<OnReject>::return_type,
			typename std::enable_if<std::is_same<Return,Result>::value,std::nullptr_t>::type = nullptr>
		inline Promise<Result> except(DispatchQueue* queue, OnReject onreject);

		template<typename OnReject,
			typename ErrorType = typename std::remove_reference<typename std::remove_cv<typename lambda_traits<OnReject>::template arg<0>::type>::type>::type,
			typename Return = typename lambda_traits<OnReject>::return_type,
			typename std::enable_if<std::is_same<Return,Result>::value,std::nullptr_t>::type = nullptr>
		Promise<Result> except(String name, OnReject onreject);
		template<typename OnReject,
			typename ErrorType = typename std::remove_reference<typename std::remove_cv<typename lambda_traits<OnReject>::template arg<0>::type>::type>::type,
			typename Return = typename lambda_traits<OnReject>::return_type,
			typename std::enable_if<std::is_same<Return,Result>::value,std::nullptr_t>::type = nullptr>
		inline Promise<Result> except(OnReject onreject);

		template<typename OnReject,
			typename ErrorType = typename std::remove_reference<typename std::remove_cv<typename lambda_traits<OnReject>::template arg<0>::type>::type>::type,
			typename Return = typename lambda_traits<OnReject>::return_type,
			typename std::enable_if<std::is_same<Return,Promise<Result>>::value,std::nullptr_t>::type = nullptr>
		Promise<Result> except(String name, DispatchQueue* queue, OnReject onreject);
		template<typename OnReject,
			typename ErrorType = typename std::remove_reference<typename std::remove_cv<typename lambda_traits<OnReject>::template arg<0>::type>::type>::type,
			typename Return = typename lambda_traits<OnReject>::return_type,
			typename std::enable_if<std::is_same<Return,Promise<Result>>::value,std::nullptr_t>::type = nullptr>
		inline Promise<Result> except(DispatchQueue* queue, OnReject onreject);

		template<typename OnReject,
			typename ErrorType = typename std::remove_reference<typename std::remove_cv<typename lambda_traits<OnReject>::template arg<0>::type>::type>::type,
			typename Return = typename lambda_traits<OnReject>::return_type,
			typename std::enable_if<std::is_same<Return,Promise<Result>>::value,std::nullptr_t>::type = nullptr>
		Promise<Result> except(String name, OnReject onreject);
		template<typename OnReject,
			typename ErrorType = typename std::remove_reference<typename std::remove_cv<typename lambda_traits<OnReject>::template arg<0>::type>::type>::type,
			typename Return = typename lambda_traits<OnReject>::return_type,
			typename std::enable_if<std::is_same<Return,Promise<Result>>::value,std::nullptr_t>::type = nullptr>
		inline Promise<Result> except(OnReject onreject);


		Promise<Result> finally(String name, DispatchQueue* queue, Function<void()> onfinally);
		inline Promise<Result> finally(DispatchQueue* queue, Function<void()> onfinally);
		Promise<Result> finally(String name, Function<void()> onfinally);
		inline Promise<Result> finally(Function<void()> onfinally);


		template<typename NextResult>
		Promise<NextResult> map(String name, DispatchQueue* queue, Then<NextResult> transform);
		template<typename NextResult>
		inline Promise<NextResult> map(DispatchQueue* queue, Then<NextResult> transform);
		template<typename NextResult>
		Promise<NextResult> map(String name, Then<NextResult> transform);
		template<typename NextResult>
		inline Promise<NextResult> map(Then<NextResult> transform);


		template<typename Rep, typename Period>
		Promise<Result> delay(String name, DispatchQueue* queue, std::chrono::duration<Rep,Period> delay);
		template<typename Rep, typename Period>
		inline Promise<Result> delay(DispatchQueue* queue, std::chrono::duration<Rep,Period> delay);

		template<typename Rep, typename Period>
		inline Promise<Result> delay(String name, std::chrono::duration<Rep,Period> delay);
		template<typename Rep, typename Period>
		inline Promise<Result> delay(std::chrono::duration<Rep,Period> delay);


		inline Promise<Any> toAny(String name);
		inline Promise<Any> toAny();
		inline Promise<void> toVoid(String name);
		inline Promise<void> toVoid();


		inline Result get();

		inline const String& getName() const;
		inline const std::shared_future<Result>& getFuture() const;

		
		template<typename _Result=Result,
			typename std::enable_if<(std::is_same<_Result,Result>::value && !std::is_same<_Result,void>::value), std::nullptr_t>::type = nullptr>
		static Promise<Result> resolve(String name, _Result result);
		template<typename _Result=Result,
			typename std::enable_if<(std::is_same<_Result,Result>::value && !std::is_same<_Result,void>::value), std::nullptr_t>::type = nullptr>
		static Promise<Result> resolve(_Result result);

		template<typename _Result=Result,
			typename std::enable_if<(std::is_same<_Result,Result>::value && std::is_same<_Result,void>::value), std::nullptr_t>::type = nullptr>
		static Promise<Result> resolve(String name);
		template<typename _Result=Result,
			typename std::enable_if<(std::is_same<_Result,Result>::value && std::is_same<_Result,void>::value), std::nullptr_t>::type = nullptr>
		static Promise<Result> resolve();

		static Promise<Result> reject(String name, PromiseErrorPtr error);
		static Promise<Result> reject(PromiseErrorPtr error);

		inline static Promise<Result> never(String name);
		inline static Promise<Result> never();

		
		template<typename _Result=Result,
			typename std::enable_if<(std::is_same<_Result,Result>::value && !std::is_same<_Result,void>::value), std::nullptr_t>::type = nullptr>
		static Promise<ArrayList<Result>> all(String name, ArrayList<Promise<_Result>> promises);
		template<typename _Result=Result,
			typename std::enable_if<(std::is_same<_Result,Result>::value && !std::is_same<_Result,void>::value), std::nullptr_t>::type = nullptr>
		static Promise<ArrayList<Result>> all(ArrayList<Promise<_Result>> promises);

		template<typename _Result=Result,
			typename std::enable_if<(std::is_same<_Result,Result>::value && std::is_same<_Result,void>::value), std::nullptr_t>::type = nullptr>
		static Promise<void> all(String name, ArrayList<Promise<_Result>> promises);
		template<typename _Result=Result,
			typename std::enable_if<(std::is_same<_Result,Result>::value && std::is_same<_Result,void>::value), std::nullptr_t>::type = nullptr>
		static Promise<void> all(ArrayList<Promise<_Result>> promises);

		template<typename _Result=Result,
			typename std::enable_if<(std::is_same<_Result,Result>::value && !std::is_same<_Result,void>::value), std::nullptr_t>::type = nullptr>
		static Promise<Result> race(String name, ArrayList<Promise<_Result>> promises);
		template<typename _Result=Result,
			typename std::enable_if<(std::is_same<_Result,Result>::value && !std::is_same<_Result,void>::value), std::nullptr_t>::type = nullptr>
		static Promise<Result> race(ArrayList<Promise<_Result>> promises);

		template<typename _Result=Result,
			typename std::enable_if<(std::is_same<_Result,Result>::value && std::is_same<_Result,void>::value), std::nullptr_t>::type = nullptr>
		static Promise<void> race(String name, ArrayList<Promise<_Result>> promises);
		template<typename _Result=Result,
			typename std::enable_if<(std::is_same<_Result,Result>::value && std::is_same<_Result,void>::value), std::nullptr_t>::type = nullptr>
		static Promise<void> race(ArrayList<Promise<_Result>> promises);


		template<typename Rep, typename Period, typename AfterDelay,
			typename _Result = Result,
			typename Return = typename lambda_traits<AfterDelay>::return_type,
			typename std::enable_if<(std::is_same<_Result,Result>::value && std::is_same<Return,Result>::value), std::nullptr_t>::type = nullptr>
		static Promise<Result> delayed(String name, std::chrono::duration<Rep,Period> delay, DispatchQueue* queue, AfterDelay afterDelay);
		template<typename Rep, typename Period, typename AfterDelay,
			typename _Result = Result,
			typename Return = typename lambda_traits<AfterDelay>::return_type,
			typename std::enable_if<(std::is_same<_Result,Result>::value && std::is_same<Return,Result>::value), std::nullptr_t>::type = nullptr>
		inline static Promise<Result> delayed(std::chrono::duration<Rep,Period> delay, DispatchQueue* queue, AfterDelay afterDelay);

		template<typename Rep, typename Period, typename AfterDelay,
			typename _Result = Result,
			typename Return = typename lambda_traits<AfterDelay>::return_type,
			typename std::enable_if<(std::is_same<_Result,Result>::value && std::is_same<Return,Result>::value), std::nullptr_t>::type = nullptr>
		inline static Promise<Result> delayed(String name, std::chrono::duration<Rep,Period> delay, AfterDelay afterDelay);
		template<typename Rep, typename Period, typename AfterDelay,
			typename _Result = Result,
			typename Return = typename lambda_traits<AfterDelay>::return_type,
			typename std::enable_if<(std::is_same<_Result,Result>::value && std::is_same<Return,Result>::value), std::nullptr_t>::type = nullptr>
		inline static Promise<Result> delayed(std::chrono::duration<Rep,Period> delay, AfterDelay afterDelay);


		template<typename Rep, typename Period, typename AfterDelay,
			typename _Result = Result,
			typename Return = typename lambda_traits<AfterDelay>::return_type,
			typename std::enable_if<(std::is_same<_Result,Result>::value && std::is_same<Return,Promise<Result>>::value), std::nullptr_t>::type = nullptr>
		static Promise<Result> delayed(String name, std::chrono::duration<Rep,Period> delay, DispatchQueue* queue, AfterDelay afterDelay);
		template<typename Rep, typename Period, typename AfterDelay,
			typename _Result = Result,
			typename Return = typename lambda_traits<AfterDelay>::return_type,
			typename std::enable_if<(std::is_same<_Result,Result>::value && std::is_same<Return,Promise<Result>>::value), std::nullptr_t>::type = nullptr>
		inline static Promise<Result> delayed(std::chrono::duration<Rep,Period> delay, DispatchQueue* queue, AfterDelay afterDelay);

		template<typename Rep, typename Period, typename AfterDelay,
			typename _Result = Result,
			typename Return = typename lambda_traits<AfterDelay>::return_type,
			typename std::enable_if<(std::is_same<_Result,Result>::value && std::is_same<Return,Promise<Result>>::value), std::nullptr_t>::type = nullptr>
		inline static Promise<Result> delayed(String name, std::chrono::duration<Rep,Period> delay, AfterDelay afterDelay);
		template<typename Rep, typename Period, typename AfterDelay,
			typename _Result = Result,
			typename Return = typename lambda_traits<AfterDelay>::return_type,
			typename std::enable_if<(std::is_same<_Result,Result>::value && std::is_same<Return,Promise<Result>>::value), std::nullptr_t>::type = nullptr>
		inline static Promise<Result> delayed(std::chrono::duration<Rep,Period> delay, AfterDelay afterDelay);


	private:
		enum class State {
			EXECUTING,
			RESOLVED,
			REJECTED
		};
		
		class Continuer {
		public:
			Continuer(std::shared_ptr<Continuer>& ptr, String name);

			inline const String& getName() const;
			inline const std::shared_future<Result>& getFuture() const;
			
			// send promise result (non-void)
			template<typename _Result=Result,
				typename std::enable_if<(std::is_same<_Result,Result>::value && !std::is_same<_Result,void>::value), std::nullptr_t>::type = nullptr>
			void resolve(_Result result);
			// send promise result (void)
			template<typename _Result=Result,
				typename std::enable_if<(std::is_same<_Result,Result>::value && std::is_same<_Result,void>::value), std::nullptr_t>::type = nullptr>
			void resolve();
			// send promise error
			void reject(std::exception_ptr error);
			
			void handle(DispatchQueue* thenQueue, Then<void> onresolve, DispatchQueue* catchQueue, Catch<std::exception_ptr,void> onreject);
			
			Result get();
			
		private:
			std::weak_ptr<Continuer> self;
			std::promise<Result> promise;
			std::shared_future<Result> future;
			std::list<Then<void>> resolvers;
			std::list<Catch<std::exception_ptr,void>> rejecters;
			std::mutex mutex;
			String name;
			State state;
		};
		
		std::shared_ptr<Continuer> continuer;
	};
	
	template<typename Result>
	Promise<Result> async(Function<Result()> executor);
	template<typename Result>
	Result await(Promise<Result> promise);
	
	
	
	
#pragma mark Promise implementation

	template<typename Result>
	Promise<Result>::Promise(const Function<void(Resolver,Rejecter)>& executor)
	#ifdef DEBUG_PROMISE_NAMING
	: Promise(String("untitled:") + typeid(Result).name(), executor) {
	#else
	: Promise("", executor) {
	#endif
		//
	}
	
	template<typename Result>
	Promise<Result>::Promise(String name, const Function<void(Resolver,Rejecter)>& executor) {
		FGL_ASSERT(executor != nullptr, "promise executor cannot be null");
		new Continuer(this->continuer, name);
		auto _continuer = this->continuer;
		if constexpr(std::is_same<Result,void>::value) {
			executor([=]() {
				_continuer->resolve();
			}, [=](PromiseErrorPtr error) {
				_continuer->reject(error.ptr());
			});
		}
		else {
			executor([=](Result result) {
				_continuer->resolve(result);
			}, [=](PromiseErrorPtr error) {
				_continuer->reject(error.ptr());
			});
		}
	}
	
	
	
	template<typename Result>
	Promise<void> Promise<Result>::then(String name, DispatchQueue* queue, Then<void> onresolve, Catch<std::exception_ptr,void> onreject) {
		FGL_ASSERT(queue != nullptr, "queue cannot be null");
		return Promise<void>(name, [=](auto resolve, auto reject) {
			if constexpr(std::is_same<Result,void>::value) {
				auto resolveHandler = onresolve ? [=]() {
					try {
						onresolve();
					} catch(...) {
						reject(std::current_exception());
						return;
					}
					resolve();
				} : resolve;
				auto thenQueue = onresolve ? queue : nullptr;
				auto rejectHandler = (onreject != nullptr) ? [=](std::exception_ptr error) {
					try {
						onreject(error);
					} catch(...) {
						reject(std::current_exception());
						return;
					}
					resolve();
				} : reject;
				auto catchQueue = onreject ? queue : nullptr;
				this->continuer->handle(thenQueue, resolveHandler, catchQueue, rejectHandler);
			}
			else {
				auto resolveHandler = onresolve ? Function<void(Result)>([=](Result result) {
					try {
						onresolve(result);
					} catch(...) {
						reject(std::current_exception());
						return;
					}
					resolve();
				}) : [=](Result result) { resolve(); };
				auto thenQueue = onresolve ? queue : nullptr;
				auto rejectHandler = onreject ? [=](std::exception_ptr error) {
					try {
						onreject(error);
					} catch(...) {
						reject(std::current_exception());
						return;
					}
					resolve();
				} : reject;
				auto catchQueue = onreject ? queue : nullptr;
				this->continuer->handle(thenQueue, resolveHandler, catchQueue, rejectHandler);
			}
		});
	}

	template<typename Result>
	Promise<void> Promise<Result>::then(DispatchQueue* queue, Then<void> onresolve, Catch<std::exception_ptr,void> onreject) {
		#ifdef DEBUG_PROMISE_NAMING
		auto thenName = this->continuer->getName() + " -> then(queue,onresolve,onreject)";
		#else
		auto thenName = "";
		#endif
		return then(thenName, queue, onresolve, onreject);
	}
	
	template<typename Result>
	Promise<void> Promise<Result>::then(String name, Then<void> onresolve, Catch<std::exception_ptr,void> onreject) {
		return then(name, getDefaultPromiseQueue(), onresolve, onreject);
	}

	template<typename Result>
	Promise<void> Promise<Result>::then(Then<void> onresolve, Catch<std::exception_ptr,void> onreject) {
		#ifdef DEBUG_PROMISE_NAMING
		auto thenName = this->continuer->getName() + " -> then(onresolve,onreject)";
		#else
		auto thenName = "";
		#endif
		return then(thenName, onresolve, onreject);
	}
	
	
	
	template<typename Result>
	template<typename OnResolve,
		typename Return,
		typename std::enable_if<std::is_same<Return,void>::value, std::nullptr_t>::type>
	Promise<void> Promise<Result>::then(String name, DispatchQueue* queue, OnResolve onresolve) {
		return then(name, queue, onresolve, nullptr);
	}

	template<typename Result>
	template<typename OnResolve,
		typename Return,
		typename std::enable_if<std::is_same<Return,void>::value, std::nullptr_t>::type>
	Promise<void> Promise<Result>::then(DispatchQueue* queue, OnResolve onresolve) {
		#ifdef DEBUG_PROMISE_NAMING
		auto thenName = this->continuer->getName() + " -> then(queue,onresolve)";
		#else
		auto thenName = "";
		#endif
		return then(thenName, queue, onresolve);
	}
	
	template<typename Result>
	template<typename OnResolve,
		typename Return,
		typename std::enable_if<std::is_same<Return,void>::value, std::nullptr_t>::type>
	Promise<void> Promise<Result>::then(String name, OnResolve onresolve) {
		return then(name, getDefaultPromiseQueue(), onresolve, nullptr);
	}

	template<typename Result>
	template<typename OnResolve,
		typename Return,
		typename std::enable_if<std::is_same<Return,void>::value, std::nullptr_t>::type>
	Promise<void> Promise<Result>::then(OnResolve onresolve) {
		#ifdef DEBUG_PROMISE_NAMING
		auto thenName = this->continuer->getName() + " -> then(onresolve)";
		#else
		auto thenName = "";
		#endif
		return then(thenName, onresolve);
	}
	
	template<typename Result>
	template<typename OnResolve,
		typename Return,
		typename NextResult,
		typename std::enable_if<(is_promise<Return>::value && std::is_same<Return,Promise<NextResult>>::value), std::nullptr_t>::type>
	Promise<NextResult> Promise<Result>::then(String name, DispatchQueue* queue, OnResolve onresolve) {
		FGL_ASSERT(queue != nullptr, "queue cannot be null");
		return Promise<NextResult>(name, [=](auto resolve, auto reject) {
			if constexpr(std::is_same<Result,void>::value) {
				this->continuer->handle(queue, [=]() {
					std::unique_ptr<Promise<NextResult>> nextPromise;
					try {
						nextPromise = std::make_unique<Promise<NextResult>>(onresolve());
					} catch(...) {
						reject(std::current_exception());
						return;
					}
					nextPromise->continuer->handle(nullptr, resolve, nullptr, reject);
				}, nullptr, reject);
			}
			else {
				this->continuer->handle(queue, [=](auto result) {
					std::unique_ptr<Promise<NextResult>> nextPromise;
					try {
						nextPromise = std::make_unique<Promise<NextResult>>(onresolve(result));
					} catch(...) {
						reject(std::current_exception());
						return;
					}
					nextPromise->continuer->handle(nullptr, resolve, nullptr, reject);
				}, nullptr, reject);
			}
		});
	}

	template<typename Result>
	template<typename OnResolve,
		typename Return,
		typename NextResult,
		typename std::enable_if<(is_promise<Return>::value && std::is_same<Return,Promise<NextResult>>::value), std::nullptr_t>::type>
	Promise<NextResult> Promise<Result>::then(DispatchQueue* queue, OnResolve onresolve) {
		#ifdef DEBUG_PROMISE_NAMING
		auto thenName = String::join(ArrayList<String>{
			this->continuer->getName(),
			" -> then<", typeid(NextResult).name(), ">(queue,onresolve)"
		});
		#else
		auto thenName = "";
		#endif
		return then(thenName, queue, onresolve);
	}

	template<typename Result>
	template<typename OnResolve,
		typename Return,
		typename NextResult,
		typename std::enable_if<(is_promise<Return>::value && std::is_same<Return,Promise<NextResult>>::value), std::nullptr_t>::type>
	Promise<NextResult> Promise<Result>::then(String name, OnResolve onresolve) {
		return then(name, getDefaultPromiseQueue(), onresolve);
	}
	
	template<typename Result>
	template<typename OnResolve,
		typename Return,
		typename NextResult,
		typename std::enable_if<(is_promise<Return>::value && std::is_same<Return,Promise<NextResult>>::value), std::nullptr_t>::type>
	Promise<NextResult> Promise<Result>::then(OnResolve onresolve) {
		#ifdef DEBUG_PROMISE_NAMING
		auto thenName = String::join(ArrayList<String>{
			this->continuer->getName(),
			" -> then<", typeid(NextResult).name(), ">(onresolve)"
		});
		#else
		auto thenName = "";
		#endif
		return then(thenName, onresolve);
	}
	
	
	
	template<typename Result>
	template<typename OnReject,
		typename ErrorType,
		typename Return,
		typename std::enable_if<std::is_same<Return,Result>::value,std::nullptr_t>::type>
	Promise<Result> Promise<Result>::except(String name, DispatchQueue* queue, OnReject onreject) {
		FGL_ASSERT(queue != nullptr, "queue cannot be null");
		return Promise<Result>(name, [=](auto resolve, auto reject) {
			this->continuer->handle(nullptr, resolve, queue, [=](auto error) {
				if constexpr(std::is_same<Result,void>::value) {
					if constexpr(std::is_same<ErrorType, std::exception_ptr>::value) {
						try {
							onreject(error);
						} catch(...) {
							reject(std::current_exception());
							return;
						}
						resolve();
					}
					else {
						try {
							std::rethrow_exception(error);
						} catch(ErrorType& error) {
							try {
								onreject(error);
							} catch(...) {
								reject(std::current_exception());
								return;
							}
							resolve();
						} catch(...) {
							reject(std::current_exception());
						}
					}
				}
				else {
					if constexpr(std::is_same<ErrorType, std::exception_ptr>::value) {
						std::unique_ptr<Result> result;
						try {
							result = std::make_unique<Result>(onreject(error));
						} catch(...) {
							reject(std::current_exception());
							return;
						}
						resolve(std::move(*result));
					}
					else {
						try {
							std::rethrow_exception(error);
						} catch(ErrorType& error) {
							std::unique_ptr<Result> result;
							try {
								result = std::make_unique<Result>(onreject(error));
							} catch(...) {
								reject(std::current_exception());
								return;
							}
							resolve(std::move(*result));
						} catch(...) {
							reject(std::current_exception());
						}
					}
				}
			});
		});
	}

	template<typename Result>
	template<typename OnReject,
		typename ErrorType,
		typename Return,
		typename std::enable_if<std::is_same<Return,Result>::value,std::nullptr_t>::type>
	Promise<Result> Promise<Result>::except(DispatchQueue* queue, OnReject onreject) {
		#ifdef DEBUG_PROMISE_NAMING
		auto exceptName = String::join(ArrayList<String>{
			this->continuer->getName(),
			" -> except<", typeid(ErrorType).name(), ">(queue,onreject)"
		});
		#else
		auto exceptName = "";
		#endif
		return except(exceptName, queue, onreject);
	}
	
	template<typename Result>
	template<typename OnReject,
		typename ErrorType,
		typename Return,
		typename std::enable_if<std::is_same<Return,Result>::value,std::nullptr_t>::type>
	Promise<Result> Promise<Result>::except(String name, OnReject onreject) {
		return except(name, getDefaultPromiseQueue(), onreject);
	}

	template<typename Result>
	template<typename OnReject,
		typename ErrorType,
		typename Return,
		typename std::enable_if<std::is_same<Return,Result>::value,std::nullptr_t>::type>
	Promise<Result> Promise<Result>::except(OnReject onreject) {
		#ifdef DEBUG_PROMISE_NAMING
		auto exceptName = String::join(ArrayList<String>{
			this->continuer->getName(),
			" -> except<", typeid(ErrorType).name(), ">(onreject)"
		});
		#else
		auto exceptName = "";
		#endif
		return except(exceptName, onreject);
	}
	
	template<typename Result>
	template<typename OnReject,
		typename ErrorType,
		typename Return,
		typename std::enable_if<std::is_same<Return,Promise<Result>>::value,std::nullptr_t>::type>
	Promise<Result> Promise<Result>::except(String name, DispatchQueue* queue, OnReject onreject) {
		FGL_ASSERT(queue != nullptr, "queue cannot be null");
		return Promise<Result>(name, [=](auto resolve, auto reject) {
			this->continuer->handle(nullptr, resolve, queue, [=](auto error) {
				if constexpr(std::is_same<ErrorType, std::exception_ptr>::value) {
					std::unique_ptr<Promise<Result>> resultPromise;
					try {
						resultPromise = std::make_unique<Promise<Result>>(onreject(error));
					} catch(...) {
						reject(std::current_exception());
						return;
					}
					resultPromise->continuer->handle(nullptr, resolve, nullptr, reject);
				}
				else {
					try {
						std::rethrow_exception(error);
					} catch(ErrorType& error) {
						std::unique_ptr<Promise<Result>> resultPromise;
						try {
							resultPromise = std::make_unique<Promise<Result>>(onreject(error));
						} catch(...) {
							reject(std::current_exception());
							return;
						}
						resultPromise->continuer->handle(nullptr, resolve, nullptr, reject);
					} catch(...) {
						reject(std::current_exception());
					}
				}
			});
		});
	}

	template<typename Result>
	template<typename OnReject,
		typename ErrorType,
		typename Return,
		typename std::enable_if<std::is_same<Return,Promise<Result>>::value,std::nullptr_t>::type>
	Promise<Result> Promise<Result>::except(DispatchQueue* queue, OnReject onreject) {
		#ifdef DEBUG_PROMISE_NAMING
		auto exceptName = String::join(ArrayList<String>{
			this->continuer->getName(),
			" -> except<", typeid(ErrorType).name(), ">(queue,onreject)"
		});
		#else
		auto exceptName = "";
		#endif
		return except(exceptName, queue, onreject);
	}

	template<typename Result>
	template<typename OnReject,
		typename ErrorType,
		typename Return,
		typename std::enable_if<std::is_same<Return,Promise<Result>>::value,std::nullptr_t>::type>
	Promise<Result> Promise<Result>::except(String name, OnReject onreject) {
		return except(name, getDefaultPromiseQueue(), onreject);
	}
	
	template<typename Result>
	template<typename OnReject,
		typename ErrorType,
		typename Return,
		typename std::enable_if<std::is_same<Return,Promise<Result>>::value,std::nullptr_t>::type>
	Promise<Result> Promise<Result>::except(OnReject onreject) {
		#ifdef DEBUG_PROMISE_NAMING
		auto exceptName = String::join(ArrayList<String>{
			this->continuer->getName(),
			" -> except<", typeid(ErrorType).name(), ">(onreject)"
		});
		#else
		auto exceptName = "";
		#endif
		return except(exceptName, onreject);
	}
	
	
	
	template<typename Result>
	Promise<Result> Promise<Result>::finally(String name, DispatchQueue* queue, Function<void()> onfinally) {
		FGL_ASSERT(queue != nullptr, "queue cannot be null");
		FGL_ASSERT(onfinally, "onfinally cannot be null");
		return Promise<Result>(name, [=](auto resolve, auto reject) {
			if constexpr(std::is_same<Result,void>::value) {
				this->continuer->handle(queue, [=]() {
					try {
						onfinally();
					} catch(...) {
						reject(std::current_exception());
						return;
					}
					resolve();
				}, queue, [=](auto error) {
					try {
						onfinally();
					} catch(...) {
						reject(std::current_exception());
						return;
					}
					reject(error);
				});
			}
			else {
				this->continuer->handle(queue, [=](auto result) {
					try {
						onfinally();
					} catch(...) {
						reject(std::current_exception());
						return;
					}
					resolve(result);
				}, queue, [=](auto error) {
					try {
						onfinally();
					} catch(...) {
						reject(std::current_exception());
						return;
					}
					reject(error);
				});
			}
		});
	}

	template<typename Result>
	Promise<Result> Promise<Result>::finally(DispatchQueue* queue, Function<void()> onfinally) {
		#ifdef DEBUG_PROMISE_NAMING
		auto finallyName = this->continuer->getName() + " -> finally(queue,onfinally)";
		#else
		auto finallyName = "";
		#endif
		return finally(finallyName, queue, onfinally);
	}
	
	template<typename Result>
	Promise<Result> Promise<Result>::finally(String name, Function<void()> onfinally) {
		return finally(name, getDefaultPromiseQueue(), onfinally);
	}

	template<typename Result>
	Promise<Result> Promise<Result>::finally(Function<void()> onfinally) {
		#ifdef DEBUG_PROMISE_NAMING
		auto finallyName = this->continuer->getName() + " -> finally(onfinally)";
		#else
		auto finallyName = "";
		#endif
		return finally(finallyName, onfinally);
	}
	
	
	
	template<typename Result>
	template<typename NextResult>
	Promise<NextResult> Promise<Result>::map(String name, DispatchQueue* queue, Then<NextResult> transform) {
		return Promise<NextResult>(name, [=](auto resolve, auto reject) {
			if constexpr(std::is_same<Result,void>::value) {
				this->continuer->handle(queue, [=]() {
					if constexpr(std::is_same<NextResult,void>::value) {
						try {
							transform();
						} catch(...) {
							reject(std::current_exception());
							return;
						}
						resolve();
					}
					else {
						std::unique_ptr<NextResult> newResult;
						try {
							newResult = std::make_unique<NextResult>(transform());
						} catch(...) {
							reject(std::current_exception());
							return;
						}
						resolve(std::move(*newResult));
					}
				}, nullptr, reject);
			}
			else {
				this->continuer->handle(queue, [=](auto result) {
					if constexpr(std::is_same<NextResult,void>::value) {
						try {
							transform(result);
						} catch(...) {
							reject(std::current_exception());
							return;
						}
						resolve();
					}
					else {
						std::unique_ptr<NextResult> newResult;
						try {
							newResult = std::make_unique<NextResult>(transform(result));
						} catch(...) {
							reject(std::current_exception());
							return;
						}
						resolve(std::move(*newResult));
					}
				}, nullptr, reject);
			}
		});
	}

	template<typename Result>
	template<typename NextResult>
	Promise<NextResult> Promise<Result>::map(DispatchQueue* queue, Then<NextResult> transform) {
		#ifdef DEBUG_PROMISE_NAMING
		auto mapName = String::join(ArrayList<String>{
			this->continuer->getName(),
			" -> map<", typeid(NextResult).name(), ">(queue,transform)"
		});
		#else
		auto mapName = "";
		#endif
		return map<NextResult>(mapName, queue, transform);
	}
	
	template<typename Result>
	template<typename NextResult>
	Promise<NextResult> Promise<Result>::map(String name, Then<NextResult> transform) {
		return map<NextResult>(name, getDefaultPromiseQueue(), transform);
	}

	template<typename Result>
	template<typename NextResult>
	Promise<NextResult> Promise<Result>::map(Then<NextResult> transform) {
		#ifdef DEBUG_PROMISE_NAMING
		auto mapName = String::join(ArrayList<String>{
			this->continuer->getName(),
			" -> map<", typeid(NextResult).name(), ">(queue,transform)"
		});
		#else
		auto mapName = "";
		#endif
		return map<NextResult>(mapName, transform);
	}


	template<typename Result>
	template<typename Rep, typename Period>
	Promise<Result> Promise<Result>::delay(String name, DispatchQueue* queue, std::chrono::duration<Rep,Period> delay) {
		return Promise<Result>(name, [=](auto resolve, auto reject) {
			if constexpr(std::is_same<Result,void>::value) {
				return this->continuer->handle(nullptr, [=]() {
					if(queue != nullptr) {
						queue->asyncAfter(std::chrono::steady_clock::now() + delay, [=]() {
							resolve();
						});
					} else {
						std::thread([=]() {
							std::this_thread::sleep_for(delay);
							resolve();
						}).detach();
					}
				}, nullptr, reject);
			} else {
				return this->continuer->handle(nullptr, [=](Result result) {
					return Promise<Result>([=](auto resolve, auto reject) {
						if (queue != nullptr) {
							queue->asyncAfter(std::chrono::steady_clock::now() + delay, [=]() {
								resolve(result);
							});
						} else {
							std::thread([=]() {
								std::this_thread::sleep_for(delay);
								resolve(result);
							}).detach();
						}
					});
				}, nullptr, reject);
			}
		});
	}

	template<typename Result>
	template<typename Rep, typename Period>
	Promise<Result> Promise<Result>::delay(DispatchQueue* queue, std::chrono::duration<Rep,Period> delay) {
		#ifdef DEBUG_PROMISE_NAMING
		auto delayName = String::join({
			this->continuer->getName(),
			" -> delay(queue,", String::stream(delay.count()), ")"
		});
		#else
		auto delayName = "";
		#endif
		return delay(delayName, queue, delay);
	}

	template<typename Result>
	template<typename Rep, typename Period>
	Promise<Result> Promise<Result>::delay(String name, std::chrono::duration<Rep,Period> delay) {
		return delay(name, getDefaultPromiseQueue(), delay);
	}

	template<typename Result>
	template<typename Rep, typename Period>
	Promise<Result> Promise<Result>::delay(std::chrono::duration<Rep,Period> delay) {
		#ifdef DEBUG_PROMISE_NAMING
		auto delayName = String::join({
			this->continuer->getName(),
			" -> delay(", String::stream(delay.count()), ")"
		});
		#else
		auto delayName = "";
		#endif
		return delay(delayName, delay);
	}

	
	template<typename Result>
	Promise<Any> Promise<Result>::toAny(String name) {
		if constexpr(std::is_same<Result,void>::value) {
			return map<Any>(name, nullptr, [=]() {
				return Any();
			});
		} else {
			return map<Any>(name, nullptr, [=](auto result) {
				return Any(result);
			});
		}
	}

	template<typename Result>
	Promise<Any> Promise<Result>::toAny() {
		#ifdef DEBUG_PROMISE_NAMING
		auto anyName = this->continuer->getName() + " as Any";
		#else
		auto anyName = "";
		#endif
		return toAny(anyName);
	}
	
	template<typename Result>
	Promise<void> Promise<Result>::toVoid(String name) {
		if constexpr(std::is_same<Result,void>::value) {
			return map<void>(name, nullptr, Then<void>([=]() {}));
		} else {
			return map<void>(name, nullptr, Then<void>([=](auto result) -> void {}));
		}
	}

	template<typename Result>
	Promise<void> Promise<Result>::toVoid() {
		#ifdef DEBUG_PROMISE_NAMING
		auto voidName = this->continuer->getName() + " as void";
		#else
		auto voidName = "";
		#endif
		return toVoid(voidName);
	}
	
	
	
	template<typename Result>
	Result Promise<Result>::get() {
		return this->continuer->get();
	}



	template<typename Result>
	const String& Promise<Result>::getName() const {
		return this->continuer->getName();
	}

	template<typename Result>
	const std::shared_future<Result>& Promise<Result>::getFuture() const {
		return this->continuer->getFuture();
	}
	
	
	
	template<typename Result>
	template<typename _Result,
		typename std::enable_if<(std::is_same<_Result,Result>::value && !std::is_same<_Result,void>::value), std::nullptr_t>::type>
	Promise<Result> Promise<Result>::resolve(String name, _Result result) {
		return Promise<Result>(name, [&](auto resolve, auto reject) {
			resolve(result);
		});
	}

	template<typename Result>
	template<typename _Result,
		typename std::enable_if<(std::is_same<_Result,Result>::value && !std::is_same<_Result,void>::value), std::nullptr_t>::type>
	Promise<Result> Promise<Result>::resolve(_Result result) {
		#ifdef DEBUG_PROMISE_NAMING
		auto resolveName = String::join(ArrayList<String>{
			"resolve<", typeid(Result).name(), ">"
		});
		#else
		auto resolveName = "";
		#endif
		return resolve(resolveName, result);
	}
	
	template<typename Result>
	template<typename _Result,
		typename std::enable_if<(std::is_same<_Result,Result>::value && std::is_same<_Result,void>::value), std::nullptr_t>::type>
	Promise<Result> Promise<Result>::resolve(String name) {
		return Promise<Result>(name, [&](auto resolve, auto reject) {
			resolve();
		});
	}

	template<typename Result>
	template<typename _Result,
			typename std::enable_if<(std::is_same<_Result,Result>::value && std::is_same<_Result,void>::value), std::nullptr_t>::type>
	Promise<Result> Promise<Result>::resolve() {
		#ifdef DEBUG_PROMISE_NAMING
		auto resolveName = "resolve<void>";
		#else
		auto resolveName = "";
		#endif
		return resolve(resolveName);
	}
	
	template<typename Result>
	Promise<Result> Promise<Result>::reject(String name, PromiseErrorPtr error) {
		return Promise<Result>(name, [&](auto resolve, auto reject) {
			reject(error);
		});
	}

	template<typename Result>
	Promise<Result> Promise<Result>::reject(PromiseErrorPtr error) {
		#ifdef DEBUG_PROMISE_NAMING
		auto rejectName = "reject";
		#else
		auto rejectName = "";
		#endif
		return reject(rejectName, error);
	}
	
	template<typename Result>
	Promise<Result> Promise<Result>::never(String name) {
		return Promise<Result>(name, [](auto resolve, auto reject) {
			// never resolve or reject
		});
	}

	template<typename Result>
	Promise<Result> Promise<Result>::never() {
		#ifdef DEBUG_PROMISE_NAMING
		auto neverName = "never";
		#else
		auto neverName = "";
		#endif
		return never(neverName);
	}


	
	template<typename Result>
	template<typename _Result,
		typename std::enable_if<(std::is_same<_Result,Result>::value && !std::is_same<_Result,void>::value), std::nullptr_t>::type>
	Promise<ArrayList<Result>> Promise<Result>::all(String name, ArrayList<Promise<_Result>> promises) {
		return Promise<ArrayList<Result>>(name, [&](auto resolve, auto reject) {
			size_t promiseCount = promises.size();
			if(promiseCount == 0) {
				resolve({});
				return;
			}
			
			struct SharedInfo {
				std::mutex mutex;
				bool rejected = false;
				std::vector<std::pair<size_t,Result>> results;
			};
			auto sharedInfoPtr = std::make_shared<SharedInfo>();
			sharedInfoPtr->results.reserve(promiseCount);
			
			auto resolveIndex = [=](size_t index, Result result) {
				auto& sharedInfo = *sharedInfoPtr;
				std::unique_lock<std::mutex> lock(sharedInfo.mutex);
				if(sharedInfo.rejected) {
					return;
				}
				bool inserted = false;
				for(auto it=sharedInfo.results.begin(); it!=sharedInfo.results.end(); it++) {
					auto& pair = *it;
					if(pair.first > index) {
						sharedInfo.results.insert(it, std::pair(index,result));
						inserted = true;
						break;
					}
				}
				if(!inserted) {
					sharedInfo.results.push_back(std::pair(index,result));
				}
				bool finished = (sharedInfo.results.size() == promiseCount);
				lock.unlock();
				if(finished) {
					ArrayList<Result> results;
					results.reserve(promiseCount);
					for(auto& pair : sharedInfo.results) {
						ASYNC_CPP_LIST_PUSH(results, std::move(pair.second));
					}
					sharedInfo.results.clear();
					sharedInfo.results.shrink_to_fit();
					resolve(results);
				}
			};
			
			auto rejectAll = [=](std::exception_ptr error) {
				auto& sharedInfo = *sharedInfoPtr;
				std::unique_lock<std::mutex> lock(sharedInfo.mutex);
				if(sharedInfo.rejected) {
					return;
				}
				sharedInfo.rejected = true;
				sharedInfo.results.clear();
				sharedInfo.results.shrink_to_fit();
				lock.unlock();
				reject(error);
			};
			
			for(size_t i=0; i<promiseCount; i++) {
				auto& promise = promises[i];
				promise->continuer->handle(nullptr, [=](_Result result) {
					resolveIndex(i, result);
				}, nullptr, [=](auto error) {
					rejectAll(error);
				});
			}
		});
	}

	template<typename Result>
	template<typename _Result,
		typename std::enable_if<(std::is_same<_Result,Result>::value && !std::is_same<_Result,void>::value), std::nullptr_t>::type>
	Promise<ArrayList<Result>> Promise<Result>::all(ArrayList<Promise<_Result>> promises) {
		#ifdef DEBUG_PROMISE_NAMING
		auto allName = String::join(ArrayList<String>{
			"all<", typeid(Result).name(), ">{ ",
			String::join(promises.map([&](auto& promise) {
				return promise.getName();
			}), ", "),
			" }"
		});
		#else
		auto allName = "";
		#endif
		return all(allName, promises);
	}
	
	template<typename Result>
	template<typename _Result,
		typename std::enable_if<(std::is_same<_Result,Result>::value && std::is_same<_Result,void>::value), std::nullptr_t>::type>
	Promise<void> Promise<Result>::all(String name, ArrayList<Promise<_Result>> promises) {
		return Promise<void>(name, [&](auto resolve, auto reject) {
			size_t promiseCount = promises.size();
			if(promiseCount == 0) {
				resolve({});
				return;
			}
			
			struct SharedInfo {
				std::mutex mutex;
				bool rejected = false;
				size_t counter = 0;
			};
			auto sharedInfoPtr = std::make_shared<SharedInfo>();
			
			auto resolveIndex = [=](size_t index) {
				auto& sharedInfo = *sharedInfoPtr;
				std::unique_lock<std::mutex> lock(sharedInfo.mutex);
				if(sharedInfo.rejected) {
					return;
				}
				sharedInfo.counter++;
				bool finished = (sharedInfo.counter == promiseCount);
				lock.unlock();
				if(finished) {
					resolve();
				}
			};
			
			auto rejectAll = [=](std::exception_ptr error) {
				auto& sharedInfo = *sharedInfoPtr;
				std::unique_lock<std::mutex> lock(sharedInfo.mutex);
				if(sharedInfo.rejected) {
					return;
				}
				sharedInfo.rejected = true;
				lock.unlock();
				reject(error);
			};
			
			for(size_t i=0; i<promiseCount; i++) {
				auto& promise = promises[i];
				promise->continuer->handle(nullptr, [=]() {
					resolveIndex(i);
				}, nullptr, [=](auto error) {
					rejectAll(error);
				});
			}
		});
	}

	template<typename Result>
	template<typename _Result,
		typename std::enable_if<(std::is_same<_Result,Result>::value && std::is_same<_Result,void>::value), std::nullptr_t>::type>
	Promise<void> Promise<Result>::all(ArrayList<Promise<_Result>> promises) {
		#ifdef DEBUG_PROMISE_NAMING
		auto allName = String::join(ArrayList<String>{
			"all{ ",
			String::join(promises.map([&](auto& promise) {
				return promise.getName();
			}), ", "),
			" }"
		});
		#else
		auto allName = "";
		#endif
		return all(allName, promises);
	}
	
	template<typename Result>
	template<typename _Result,
		typename std::enable_if<(std::is_same<_Result,Result>::value && !std::is_same<_Result,void>::value), std::nullptr_t>::type>
	Promise<Result> Promise<Result>::race(String name, ArrayList<Promise<_Result>> promises) {
		return Promise<Result>(name, [&](auto resolve, auto reject) {
			size_t promiseCount = promises.size();
			
			struct SharedInfo {
				std::mutex mutex;
				bool finished;
			};
			auto sharedInfoPtr = std::make_shared<SharedInfo>();
			
			auto resolveIndex = [=](size_t index, auto result) {
				auto& sharedInfo = *sharedInfoPtr;
				std::unique_lock<std::mutex> lock(sharedInfo.mutex);
				if(sharedInfo.finished) {
					return;
				}
				sharedInfo.finished = true;
				lock.unlock();
				resolve(result);
			};
			
			auto rejectAll = [=](std::exception_ptr error) {
				auto& sharedInfo = *sharedInfoPtr;
				std::unique_lock<std::mutex> lock(sharedInfo.mutex);
				if(sharedInfo.finished) {
					return;
				}
				sharedInfo.finished = true;
				lock.unlock();
				reject(error);
			};
			
			for(size_t i=0; i<promiseCount; i++) {
				auto& promise = promises[i];
				promise->continuer->handle(nullptr, [=](auto result) {
					resolveIndex(i, result);
				}, nullptr, [=](auto error) {
					rejectAll(error);
				});
			}
		});
	}

	template<typename Result>
	template<typename _Result,
		typename std::enable_if<(std::is_same<_Result,Result>::value && !std::is_same<_Result,void>::value), std::nullptr_t>::type>
	Promise<Result> Promise<Result>::race(ArrayList<Promise<_Result>> promises) {
		#ifdef DEBUG_PROMISE_NAMING
		auto raceName = String::join(ArrayList<String>{
			"race<", typeid(Result).name(), ">{ ",
			String::join(promises.map([&](auto& promise) {
				return promise.getName();
			}), ", "),
			" }"
		});
		#else
		auto raceName = "";
		#endif
		return race(raceName, promises);
	}
	
	template<typename Result>
	template<typename _Result,
		typename std::enable_if<(std::is_same<_Result,Result>::value && std::is_same<_Result,void>::value), std::nullptr_t>::type>
	Promise<void> Promise<Result>::race(String name, ArrayList<Promise<_Result>> promises) {
		return Promise<void>(name, [&](auto resolve, auto reject) {
			size_t promiseCount = promises.size();
			
			struct SharedInfo {
				std::mutex mutex;
				bool finished;
			};
			auto sharedInfoPtr = std::make_shared<SharedInfo>();
			
			auto resolveIndex = [=](size_t index) {
				auto& sharedInfo = *sharedInfoPtr;
				std::unique_lock<std::mutex> lock(sharedInfo.mutex);
				if(sharedInfo.finished) {
					return;
				}
				sharedInfo.finished = true;
				lock.unlock();
				resolve();
			};
			
			auto rejectAll = [=](std::exception_ptr error) {
				auto& sharedInfo = *sharedInfoPtr;
				std::unique_lock<std::mutex> lock(sharedInfo.mutex);
				if(sharedInfo.finished) {
					return;
				}
				sharedInfo.finished = true;
				lock.unlock();
				reject(error);
			};
			
			for(size_t i=0; i<promiseCount; i++) {
				auto& promise = promises[i];
				promise->continuer->handle(nullptr, [=]() {
					resolveIndex(i);
				}, nullptr, [=](auto error) {
					rejectAll(error);
				});
			}
		});
	}

	template<typename Result>
	template<typename _Result,
		typename std::enable_if<(std::is_same<_Result,Result>::value && std::is_same<_Result,void>::value), std::nullptr_t>::type>
	Promise<void> Promise<Result>::race(ArrayList<Promise<_Result>> promises) {
		#ifdef DEBUG_PROMISE_NAMING
		auto raceName = String::join(ArrayList<String>{
			"race{ ",
			String::join(promises.map([&](auto& promise) {
				return promise.getName();
			}), ", "),
			" }"
		});
		#else
		auto raceName = "";
		#endif
		return race(raceName, promises);
	}



	template<typename Result>
	template<typename Rep, typename Period, typename AfterDelay,
		typename _Result,
		typename Return,
		typename std::enable_if<(std::is_same<_Result,Result>::value && std::is_same<Return,Result>::value), std::nullptr_t>::type>
	Promise<Result> Promise<Result>::delayed(String name, std::chrono::duration<Rep,Period> delay, DispatchQueue* queue, AfterDelay afterDelay) {
		return Promise<Result>(name, [=](auto resolve, auto reject) {
			auto finishDelay = [=]() {
				if constexpr(std::is_same<Result,void>::value) {
					try {
						afterDelay();
					} catch(...) {
						reject(std::current_exception());
						return;
					}
					resolve();
				} else {
					std::unique_ptr<Result> result;
					try {
						result = std::make_unique(afterDelay());
					} catch(...) {
						reject(std::current_exception());
						return;
					}
					resolve(std::move(*result.get()));
				}
			};
			if(queue != nullptr) {
				queue->asyncAfter(std::chrono::steady_clock::now() + delay, [=]() {
					finishDelay();
				});
			} else {
				std::thread([=]() {
					std::this_thread::sleep_for(delay);
					finishDelay();
				}).detach();
			}
		});
	}

	template<typename Result>
	template<typename Rep, typename Period, typename AfterDelay,
		typename _Result,
		typename Return,
		typename std::enable_if<(std::is_same<_Result,Result>::value && std::is_same<Return,Result>::value), std::nullptr_t>::type>
	Promise<Result> Promise<Result>::delayed(std::chrono::duration<Rep,Period> delay, DispatchQueue* queue, AfterDelay afterDelay) {
		#ifdef DEBUG_PROMISE_NAMING
		auto delayedName = String::join({
			"delayed(", String::stream(delay.count()), ",queue,afterdelay)"
		});
		#else
		auto delayedName = "";
		#endif
		return delayed(delayedName, delay, queue, afterDelay);
	}

	template<typename Result>
	template<typename Rep, typename Period, typename AfterDelay,
		typename _Result,
		typename Return,
		typename std::enable_if<(std::is_same<_Result,Result>::value && std::is_same<Return,Result>::value), std::nullptr_t>::type>
	Promise<Result> Promise<Result>::delayed(String name, std::chrono::duration<Rep,Period> delay, AfterDelay afterDelay) {
		return delayed(name, delay, getDefaultPromiseQueue(), afterDelay);
	}

	template<typename Result>
	template<typename Rep, typename Period, typename AfterDelay,
		typename _Result,
		typename Return,
		typename std::enable_if<(std::is_same<_Result,Result>::value && std::is_same<Return,Result>::value), std::nullptr_t>::type>
	Promise<Result> Promise<Result>::delayed(std::chrono::duration<Rep,Period> delay, AfterDelay afterDelay) {
		#ifdef DEBUG_PROMISE_NAMING
		auto delayedName = String::join({
			"delayed(", String::stream(delay.count()), ",afterdelay)"
		});
		#else
		auto delayedName = "";
		#endif
		return delayed(delayedName, delay, afterDelay);
	}

	template<typename Result>
	template<typename Rep, typename Period, typename AfterDelay,
		typename _Result,
		typename Return,
		typename std::enable_if<(std::is_same<_Result,Result>::value && std::is_same<Return,Promise<Result>>::value), std::nullptr_t>::type>
	Promise<Result> Promise<Result>::delayed(String name, std::chrono::duration<Rep,Period> delay, DispatchQueue* queue, AfterDelay afterDelay) {
		return Promise<Result>(name, [=](auto resolve, auto reject) {
			auto finishDelay = [=]() {
				std::unique_ptr<Promise<Result>> resultPromise;
				try {
					resultPromise = std::make_unique(afterDelay());
				} catch(...) {
					reject(std::current_exception());
					return;
				}
				resultPromise.then(nullptr, resolve, reject);
			};
			if(queue != nullptr) {
				queue->asyncAfter(std::chrono::steady_clock::now() + delay, [=]() {
					finishDelay();
				});
			} else {
				std::thread([=]() {
					std::this_thread::sleep_for(delay);
					finishDelay();
				}).detach();
			}
		});
	}

	template<typename Result>
	template<typename Rep, typename Period, typename AfterDelay,
		typename _Result,
		typename Return,
		typename std::enable_if<(std::is_same<_Result,Result>::value && std::is_same<Return,Promise<Result>>::value), std::nullptr_t>::type>
	Promise<Result> Promise<Result>::delayed(std::chrono::duration<Rep,Period> delay, DispatchQueue* queue, AfterDelay afterDelay) {
		#ifdef DEBUG_PROMISE_NAMING
		auto delayedName = String::join({
			"delayed(", String::stream(delay.count()), ",queue,asyncAfterdelay)"
		});
		#else
		auto delayedName = "";
		#endif
		return delayed(delayedName, delay, queue, afterDelay);
	}

	template<typename Result>
	template<typename Rep, typename Period, typename AfterDelay,
		typename _Result,
		typename Return,
		typename std::enable_if<(std::is_same<_Result,Result>::value && std::is_same<Return,Promise<Result>>::value), std::nullptr_t>::type>
	Promise<Result> Promise<Result>::delayed(String name, std::chrono::duration<Rep,Period> delay, AfterDelay afterDelay) {
		return delayed(name, delay, getDefaultPromiseQueue(), afterDelay);
	}

	template<typename Result>
	template<typename Rep, typename Period, typename AfterDelay,
		typename _Result,
		typename Return,
		typename std::enable_if<(std::is_same<_Result,Result>::value && std::is_same<Return,Promise<Result>>::value), std::nullptr_t>::type>
	Promise<Result> Promise<Result>::delayed(std::chrono::duration<Rep,Period> delay, AfterDelay afterDelay) {
		#ifdef DEBUG_PROMISE_NAMING
		auto delayedName = String::join({
			"delayed(", String::stream(delay.count()), ",asyncAfterdelay)"
		});
		#else
		auto delayedName = "";
		#endif
		return delayed(delayedName, delay, afterDelay);
	}


	
	
	template<typename Result>
	Promise<Result>::Continuer::Continuer(std::shared_ptr<Continuer>& ptr, String name)
	: future(promise.get_future().share()), name(name), state(State::EXECUTING) {
		ptr = std::shared_ptr<Continuer>(this);
		self = ptr;
	}

	template<typename Result>
	const String& Promise<Result>::Continuer::getName() const {
		return name;
	}

	template<typename Result>
	const std::shared_future<Result>& Promise<Result>::Continuer::getFuture() const {
		return future;
	}

	template<typename Result>
	template<typename _Result,
		typename std::enable_if<(std::is_same<_Result,Result>::value && !std::is_same<_Result,void>::value), std::nullptr_t>::type>
	void Promise<Result>::Continuer::resolve(_Result result) {
		std::unique_lock<std::mutex> lock(mutex);
		FGL_ASSERT(state == State::EXECUTING, "Cannot resolve a promise multiple times");
		// set new state
		promise.set_value(result);
		state = State::RESOLVED;
		// copy callbacks and clear
		std::list<Resolver> callbacks;
		callbacks.swap(resolvers);
		resolvers.clear();
		rejecters.clear();
		lock.unlock();
		// call callbacks
		for(auto& callback : callbacks) {
			callback(result);
		}
	}
	
	template<typename Result>
	template<typename _Result,
		typename std::enable_if<(std::is_same<_Result,Result>::value && std::is_same<_Result,void>::value), std::nullptr_t>::type>
	void Promise<Result>::Continuer::resolve() {
		std::unique_lock<std::mutex> lock(mutex);
		FGL_ASSERT(state == State::EXECUTING, "Cannot resolve a promise multiple times");
		// set new state
		promise.set_value();
		state = State::RESOLVED;
		// copy callbacks and clear
		std::list<Then<void>> callbacks;
		callbacks.swap(resolvers);
		resolvers.clear();
		rejecters.clear();
		lock.unlock();
		// call callbacks
		for(auto& callback : callbacks) {
			callback();
		}
	}
	
	template<typename Result>
	void Promise<Result>::Continuer::reject(std::exception_ptr error) {
		std::unique_lock<std::mutex> lock(mutex);
		FGL_ASSERT(state == State::EXECUTING, "Cannot resolve a promise multiple times");
		// set new state
		promise.set_exception(error);
		state = State::REJECTED;
		// copy callbacks and clear
		std::list<Catch<std::exception_ptr,void>> callbacks;
		callbacks.swap(rejecters);
		resolvers.clear();
		rejecters.clear();
		lock.unlock();
		// call callbacks
		for(auto& callback : callbacks) {
			callback(error);
		}
	}
	
	template<typename Result>
	void Promise<Result>::Continuer::handle(DispatchQueue* thenQueue, Then<void> onresolve, DispatchQueue* catchQueue, Catch<std::exception_ptr,void> onreject) {
		std::unique_lock<std::mutex> lock(mutex);
		switch(state) {
			case State::EXECUTING: {
				if(onresolve) {
					if constexpr(std::is_same<Result,void>::value) {
						resolvers.push_back([=]() {
							if(thenQueue != nullptr) {
								thenQueue->async([=]() {
									onresolve();
								});
							} else {
								onresolve();
							}
						});
					}
					else {
						resolvers.push_back([=](auto result) {
							if(thenQueue != nullptr) {
								thenQueue->async([=]() {
									onresolve(result);
								});
							} else {
								onresolve(result);
							}
						});
					}
				}
				if(onreject) {
					rejecters.push_back([=](auto error) {
						if(catchQueue != nullptr) {
							catchQueue->async([=]() {
								onreject(error);
							});
						} else {
							onreject(error);
						}
					});
				}
			}
			break;
				
			case State::RESOLVED: {
				lock.unlock();
				if(onresolve) {
					if(thenQueue != nullptr) {
						auto self = this->self.lock();
						thenQueue->async([=]() {
							auto future = self->future;
							if constexpr(std::is_same<Result,void>::value) {
								future.get();
								onresolve();
							}
							else {
								onresolve(future.get());
							}
						});
					} else {
						auto future = this->future;
						if constexpr(std::is_same<Result,void>::value) {
							future.get();
							onresolve();
						}
						else {
							onresolve(future.get());
						}
					}
				}
			}
			break;
			
			case State::REJECTED: {
				lock.unlock();
				if(onreject) {
					if(catchQueue != nullptr) {
						auto self = this->self.lock();
						catchQueue->async([=]() {
							auto future = self->future;
							try {
								future.get();
							} catch(...) {
								onreject(std::current_exception());
							}
						});
					} else {
						auto future = this->future;
						try {
							future.get();
						} catch(...) {
							onreject(std::current_exception());
						}
					}
				}
			}
			break;
		}
	}
	
	template<typename Result>
	Result Promise<Result>::Continuer::get() {
		switch(state) {
			case State::EXECUTING: {
				std::mutex waitMutex;
				std::unique_lock<std::mutex> waitLock(waitMutex);
				std::condition_variable waitCondition;
				if constexpr(std::is_same<Result,void>::value) {
					std::exception_ptr error_ptr;
					bool rejected = false;
					bool resolved = false;
					this->handle(nullptr, [&]() {
						resolved = true;
						waitCondition.notify_one();
					}, nullptr, [&](auto error) {
						rejected = true;
						error_ptr = error;
						waitCondition.notify_one();
					});
					waitCondition.wait(waitLock, [&]() {
						return (resolved || rejected);
					});
					if(rejected) {
						std::rethrow_exception(error_ptr);
					}
					return;
				}
				else {
					std::unique_ptr<Result> result_ptr;
					std::exception_ptr error_ptr;
					bool rejected = false;
					bool resolved = false;
					this->handle(nullptr, [&](Result result) {
						resolved = true;
						result_ptr = std::make_unique<Result>(result);
						waitCondition.notify_one();
					}, nullptr, [&](auto error) {
						rejected = true;
						error_ptr = error;
						waitCondition.notify_one();
					});
					waitCondition.wait(waitLock, [&]() {
						return (resolved || rejected);
					});
					if(rejected) {
						std::rethrow_exception(error_ptr);
					}
					return std::move(*result_ptr);
				}
			}
			case State::RESOLVED:
			case State::REJECTED: {
				auto future = this->future;
				return future.get();
			}
		}
	}
	
	
	
	
	template<typename Result>
	Promise<Result> async(Function<Result()> executor) {
		return Promise<Result>([=](auto resolve, auto reject) {
			std::thread([=]() {
				if constexpr(std::is_same<Result,void>::value) {
					try {
						executor();
					} catch(...) {
						reject(std::current_exception());
						return;
					}
					resolve();
				}
				else {
					std::unique_ptr<Result> result;
					try {
						result = std::make_unique<Result>(executor());
					} catch(...) {
						reject(std::current_exception());
						return;
					}
					resolve(std::move(*result));
				}
			}).detach();
		});
	}
	
	template<typename Result>
	Result await(Promise<Result> promise) {
		return promise.get();
	}
}
