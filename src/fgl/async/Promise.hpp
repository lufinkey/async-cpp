//
//  Promise.hpp
//  AsyncCpp
//
//  Created by Luis Finke on 8/11/19.
//  Copyright © 2019 Luis Finke. All rights reserved.
//

#pragma once

#include <future>
#include <list>
#include <mutex>
#include <utility>
#include "Common.hpp"
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
		
		Promise<Any> toAny(DispatchQueue* queue) {
			if constexpr(std::is_same<Result,void>::value) {
				return map<Any>(queue, [=]() {
					return Any();
				});
			} else {
				return map<Any>(queue, [=](auto result) {
					return Any(result);
				});
			}
		}
		
		Promise<Any> toAny() {
			return toAny(getDefaultPromiseQueue());
		}
		
		Promise<void> toVoid(DispatchQueue* queue) {
			return map<void>(queue, [=]() {});
		}
		
		Promise<void> toVoid() {
			return toVoid(getDefaultPromiseQueue());
		}
		
		
		
		Result await() {
			return this->continuer->await();
		}
		
		
		
		template<typename _Result=Result,
			typename std::enable_if<!std::is_same<_Result,void>::value, std::nullptr_t>::type = nullptr>
		static Promise<_Result> resolve(_Result result) {
			return Promise<_Result>([&](auto resolve, auto reject) {
				resolve(result);
			});
		}
		
		template<typename _Result=Result,
			typename std::enable_if<std::is_same<_Result,void>::value, std::nullptr_t>::type = nullptr>
		static Promise<_Result> resolve() {
			return Promise<_Result>([&](auto resolve, auto reject) {
				resolve();
			});
		}
		
		
		static Promise<Result> reject(PromiseErrorPtr error) {
			return Promise<Result>([&](auto resolve, auto reject) {
				reject(error);
			});
		}
		
		
		static Promise<Result> nothing() {
			return Promise<Result>([](auto resolve, auto reject) {
				// do nothing
			});
		}
		
		template<typename _Result=Result,
			typename std::enable_if<!std::is_same<_Result,void>::value, std::nullptr_t>::type = nullptr>
		static Promise<ArrayList<_Result>> all(ArrayList<Promise<_Result>> promises) {
			return Promise<ArrayList<_Result>>([&](auto resolve, auto reject) {
				size_t promiseCount = promises.size();
				if(promiseCount == 0) {
					resolve({});
					return;
				}
				
				struct SharedInfo {
					std::mutex mutex;
					bool rejected = false;
					std::list<std::pair<size_t,_Result>> results;
				};
				auto sharedInfoPtr = std::make_shared<SharedInfo>();
				sharedInfoPtr->results.reserve(promiseCount);
				
				auto resolveIndex = [=](size_t index, _Result result) {
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
						ArrayList<_Result> results;
						results.reserve(promiseCount);
						for(auto& pair : sharedInfo.results) {
							results.push_back(std::move(pair.second));
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
		
		template<typename _Result=Result,
			typename std::enable_if<std::is_same<_Result,void>::value, std::nullptr_t>::type = nullptr>
		static Promise<void> all(ArrayList<Promise<_Result>> promises) {
			return Promise<ArrayList<_Result>>([&](auto resolve, auto reject) {
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
		
		template<typename _Result=Result,
			typename std::enable_if<!std::is_same<_Result,void>::value, std::nullptr_t>::type = nullptr>
		static Promise<_Result> race(ArrayList<Promise<_Result>> promises) {
			return Promise<_Result>([&](auto resolve, auto reject) {
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
		
		template<typename _Result=Result,
			typename std::enable_if<std::is_same<_Result,void>::value, std::nullptr_t>::type = nullptr>
		static Promise<void> race(ArrayList<Promise<_Result>> promises) {
			return Promise<_Result>([&](auto resolve, auto reject) {
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
							this->handle(nullptr, [&](auto result) {
								resolved = true;
								result_ptr = std::make_unique(result);
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
	
	
	
	
	template<typename Result>
	Promise<Result> async(DispatchQueue* queue, Function<Result()> executor) {
		return Promise<Result>([&](auto resolve, auto reject) {
			queue->async([=]() {
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
						result = std::make_unique(executor());
					} catch(...) {
						reject(std::current_exception());
						return;
					}
					resolve(std::move(*result));
				}
			});
		});
	}
	
	template<typename Result>
	Result await(Promise<Result> promise) {
		return promise.await();
	}
}
