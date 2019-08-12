//
//  Promise.hpp
//  AsyncCpp
//
//  Created by Luis Finke on 8/11/19.
//  Copyright © 2019 Luis Finke. All rights reserved.
//

#pragma once

#include <future>
#include <mutex>
#include <list>
#include "Macros.hpp"
#include "Types.hpp"
#include "DispatchQueue.hpp"
#include "PromiseErrorPtr.hpp"

namespace fgl {
	class DispatchQueue;
	
	DispatchQueue* getDefaultPromiseQueue();
	
	template<typename Result>
	class Promise {
	public:
		template<typename Arg, typename Return>
		struct _Block {
			using type = Function<Return(Arg)>;
		};
		template<typename Return>
		struct _Block<void, Return> {
			using type = Function<Return()>;
		};
		
		using Resolver = typename _Block<Result,void>::type;
		using Rejecter = Function<void(PromiseErrorPtr)>;
		template<typename Return>
		using Then = typename _Block<Result,Return>::type;
		template<typename ErrorType, typename Return>
		using Catch = Function<Return(ErrorType)>;
		
		explicit Promise(const Function<void(Resolver,Rejecter)>& executor) {
			FGL_ASSERT(executor != nullptr, "promise executor cannot be null");
			auto continuer = this->continuer;
			if constexpr(std::is_same<Result,void>::value) {
				executor([=]() {
					continuer->resolve();
				}, [=](PromiseErrorPtr error) {
					continuer->reject(error);
				});
			}
			else {
				executor([=](Result result) {
					continuer->resolve(result);
				}, [=](PromiseErrorPtr error) {
					continuer->reject(error);
				});
			}
		}
		
		Promise<void> then(DispatchQueue* queue, Then<void> onresolve, Catch<std::exception_ptr,void> onreject) {
			return Promise<void>([=](auto resolve, auto reject) {
				if constexpr(std::is_same<Result,void>::value) {
					return this->continuer->handle(queue, [=]() {
						try {
							onresolve();
						} catch(...) {
							reject(std::current_exception());
							return;
						}
						resolve();
					}, [=](auto error) {
						try {
							onreject(error);
						} catch(...) {
							reject(std::current_exception());
							return;
						}
						resolve();
					});
				}
				else {
					return this->continuer->handle(queue, [=](auto result) {
						try {
							onresolve(result);
						} catch(...) {
							reject(std::current_exception());
							return;
						}
						resolve();
					}, [=](auto error) {
						try {
							onreject(error);
						} catch(...) {
							reject(std::current_exception());
							return;
						}
						resolve();
					});
				}
			});
		}
		
		
		
	private:
		enum class State {
			EXECUTING,
			RESOLVED,
			REJECTED
		};
		
		class Continuer {
		public:
			// send promise result (non-void)
			template<typename _Result=Result,
				typename std::enable_if<!std::is_same<_Result,void>::value, std::nullptr_t>::type = nullptr>
			void resolve(_Result result) {
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
			
			// send promise result (void)
			template<typename _Result=Result,
				typename std::enable_if<std::is_same<_Result,void>::value, std::nullptr_t>::type = nullptr>
			void resolve() {
				std::unique_lock<std::mutex> lock(mutex);
				FGL_ASSERT(state == State::EXECUTING, "Cannot resolve a promise multiple times");
				// set new state
				promise.set_value();
				state = State::RESOLVED;
				// copy callbacks and clear
				std::list<Resolver> callbacks;
				callbacks.swap(resolvers);
				resolvers.clear();
				rejecters.clear();
				lock.unlock();
				// call callbacks
				for(auto& callback : callbacks) {
					callback();
				}
			}
			
			// send promise error
			void reject(PromiseErrorPtr error) {
				std::unique_lock<std::mutex> lock(mutex);
				FGL_ASSERT(state == State::EXECUTING, "Cannot resolve a promise multiple times");
				// set new state
				promise.set_exception(error.ptr());
				state = State::REJECTED;
				// copy callbacks and clear
				std::list<Rejecter> callbacks;
				callbacks.swap(rejecters);
				resolvers.clear();
				rejecters.clear();
				lock.unlock();
				// call callbacks
				for(auto& callback : callbacks) {
					callback(error);
				}
			}
			
			void handle(DispatchQueue* queue, Then<void> onresolve, Catch<std::exception_ptr,void> onreject) {
				std::unique_lock<std::mutex> lock(mutex);
				switch(state) {
					case State::EXECUTING: {
						if(onresolve) {
							if constexpr(std::is_same<Result,void>::value) {
								resolvers.push_back([=]() {
									queue->async([=]() {
										onresolve();
									});
								});
							}
							else {
								resolvers.push_back([=](auto result) {
									queue->async([=]() {
										onresolve(result);
									});
								});
							}
						}
						if(onreject) {
							rejecters.push_back([=](auto exceptionPtr) {
								queue->async([=]() {
									onreject(exceptionPtr.ptr());
								});
							});
						}
					}
					break;
						
					case State::RESOLVED: {
						lock.unlock();
						if(onresolve) {
							queue->async([=]() {
								if constexpr(std::is_same<Result,void>::value) {
									future.get();
									onresolve();
								}
								else {
									onresolve(future.get());
								}
							});
						}
					}
					break;
						
					case State::REJECTED: {
						lock.unlock();
						if(onreject) {
							queue->async([=]() {
								try {
									future.get();
								} catch(...) {
									onreject(std::current_exception());
								}
							});
						}
					}
					break;
				}
			}
			
		private:
			std::promise<Result> promise;
			std::shared_future<Result> future;
			LinkedList<Resolver> resolvers;
			LinkedList<Rejecter> rejecters;
			std::mutex mutex;
			State state;
		};
		
		std::shared_ptr<Continuer> continuer;
	};
}
