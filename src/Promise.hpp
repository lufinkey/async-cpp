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
					continuer->reject(error.ptr());
				});
			}
			else {
				executor([=](Result result) {
					continuer->resolve(result);
				}, [=](PromiseErrorPtr error) {
					continuer->reject(error.ptr());
				});
			}
		}
		
		
		
		Promise<void> then(DispatchQueue* queue, Then<void> onresolve, Catch<std::exception_ptr,void> onreject) {
			FGL_ASSERT(queue != nullptr, "queue cannot be null");
			return Promise<void>([=](auto resolve, auto reject) {
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
					auto rejectHandler = onreject ? [=](auto error) {
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
					auto resolveHandler = onresolve ? [=](auto result) {
						try {
							onresolve(result);
						} catch(...) {
							reject(std::current_exception());
							return;
						}
						resolve();
					} : resolve;
					auto thenQueue = onresolve ? queue : nullptr;
					auto rejectHandler = onreject ? [=](auto error) {
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
		
		Promise<void> then(Then<void> onresolve, Catch<std::exception_ptr,void> onreject) {
			return then(getDefaultPromiseQueue(), onresolve, onreject);
		}
		
		Promise<void> then(DispatchQueue* queue, Then<void> onresolve) {
			return then(queue, onresolve, nullptr);
		}
		
		Promise<void> then(Then<void> onresolve) {
			return then(getDefaultPromiseQueue(), onresolve, nullptr);
		}
		
		template<typename NextResult>
		Promise<NextResult> then(DispatchQueue* queue, Then<Promise<NextResult>> onresolve) {
			FGL_ASSERT(queue != nullptr, "queue cannot be null");
			FGL_ASSERT(onresolve != nullptr, "onresolve cannot be null");
			return Promise<NextResult>([=](auto resolve, auto reject) {
				if constexpr(std::is_same<Result,void>::value) {
					this->continuer->handle(queue, [=]() {
						std::unique_ptr<Promise<NextResult>> nextPromise;
						try {
							nextPromise = std::make_unique(onresolve());
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
							nextPromise = std::make_unique(onresolve(result));
						} catch(...) {
							reject(std::current_exception());
						}
						nextPromise->continuer->handle(nullptr, resolve, nullptr, reject);
					}, nullptr, reject);
				}
			});
		}
		
		
		
		template<typename ErrorType>
		Promise<Result> except(DispatchQueue* queue, Catch<ErrorType,Result> onreject) {
			FGL_ASSERT(queue != nullptr, "queue cannot be null");
			FGL_ASSERT(onreject != nullptr, "onreject cannot be null");
			return Promise<Result>([=](auto resolve, auto reject) {
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
								result = std::make_unique(onreject(error));
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
									result = std::make_unique(onreject(error));
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
		
		template<typename ErrorType>
		Promise<Result> except(Catch<ErrorType,Result> onreject) {
			return except(getDefaultPromiseQueue(), onreject);
		}
		
		template<typename ErrorType>
		Promise<Result> except(DispatchQueue* queue, Catch<ErrorType,Promise<Result>> onreject) {
			FGL_ASSERT(queue != nullptr, "queue cannot be null");
			FGL_ASSERT(onreject != nullptr, "onreject cannot be null");
			return Promise<Result>([=](auto resolve, auto reject) {
				this->continuer->handle(nullptr, resolve, queue, [=](auto error) {
					if constexpr(std::is_same<ErrorType, std::exception_ptr>::value) {
						std::unique_ptr<Promise<Result>> resultPromise;
						try {
							resultPromise = std::make_unique(onreject(error));
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
								resultPromise = std::make_unique(onreject(error));
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
		
		template<typename ErrorType>
		Promise<Result> except(Catch<ErrorType,Promise<Result>> onreject) {
			return except(getDefaultPromiseQueue(), onreject);
		}
		
		
		
		Promise<Result> finally(DispatchQueue* queue, Function<void()> onfinally) {
			FGL_ASSERT(queue != nullptr, "queue cannot be null");
			FGL_ASSERT(onfinally != nullptr, "onfinally cannot be null");
			return Promise<Result>([=](auto resolve, auto reject) {
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
		
		Promise<Result> finally(Function<void()> onfinally) {
			return finally(getDefaultPromiseQueue(), onfinally);
		}
		
		
		
		template<typename NextResult>
		Promise<NextResult> map(DispatchQueue* queue, Then<NextResult> transform) {
			return Promise<NextResult>([=](auto resolve, auto reject) {
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
								newResult = std::make_unique(transform());
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
								newResult = std::make_unique(transform(result));
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
			void reject(std::exception_ptr error) {
				std::unique_lock<std::mutex> lock(mutex);
				FGL_ASSERT(state == State::EXECUTING, "Cannot resolve a promise multiple times");
				// set new state
				promise.set_exception(error);
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
			
			void handle(DispatchQueue* thenQueue, Then<void> onresolve, DispatchQueue* catchQueue, Catch<std::exception_ptr,void> onreject) {
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
								thenQueue->async([=]() {
									if constexpr(std::is_same<Result,void>::value) {
										future.get();
										onresolve();
									}
									else {
										onresolve(future.get());
									}
								});
							} else {
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
								catchQueue->async([=]() {
									try {
										future.get();
									} catch(...) {
										onreject(std::current_exception());
									}
								});
							} else {
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
			
			Result await() {
				std::mutex waitMutex;
				std::unique_lock<std::mutex> waitLock(waitMutex);
				std::condition_variable waitCondition;
				std::unique_lock<std::mutex> lock(mutex);
				switch(state) {
					case State::EXECUTING:
						if constexpr(std::is_same<Result,void>::value) {
							std::exception_ptr error_ptr;
							bool rejected = false;
							bool resolved = false;
							resolvers.push_back([&]() {
								resolved = true;
								waitCondition.notify_one();
							});
							rejecters.push_back([&](auto error) {
								rejected = true;
								error_ptr = error;
								waitCondition.notify_one();
							});
							lock.unlock();
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
							resolvers.push_back([&](auto result) {
								resolved = true;
								result_ptr = std::make_unique(result);
								waitCondition.notify_one();
							});
							rejecters.push_back([&](auto error) {
								rejected = true;
								error_ptr = error;
								waitCondition.notify_one();
							});
							lock.unlock();
							waitCondition.wait(waitLock, [&]() {
								return (resolved || rejected);
							});
							if(rejected) {
								std::rethrow_exception(error_ptr);
							}
							return std::move(*result_ptr);
						}
					case State::RESOLVED:
					case State::REJECTED:
						return future.get();
				}
			}
			
		private:
			std::promise<Result> promise;
			std::shared_future<Result> future;
			std::list<Then<void>> resolvers;
			std::list<Catch<std::exception_ptr,void>> rejecters;
			std::mutex mutex;
			State state;
		};
		
		std::shared_ptr<Continuer> continuer;
	};
}
