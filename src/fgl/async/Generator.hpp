//
//  Generator.hpp
//  AsyncCpp
//
//  Created by Luis Finke on 11/17/19.
//  Copyright © 2019 Luis Finke. All rights reserved.
//

#pragma once

#include <fgl/async/Common.hpp>
#include <fgl/async/LambdaTraits.hpp>
#include <fgl/async/Promise.hpp>
#include <fgl/async/Coroutine.hpp>
#include <condition_variable>
#include <thread>

namespace fgl {
	template<typename Yield, typename Next>
	class Generator;
	template<typename Yield, typename Next>
	struct _coroutine_generator_type_base;
	
	template<typename GeneratorType>
	struct is_generator {
		static constexpr bool value = false;
	};
	
	template<typename Yield, typename Next>
	struct is_generator<Generator<Yield,Next>> {
		static constexpr bool value = true;
		typedef Yield yield_type;
		typedef Next next_type;
		typedef Generator<Yield,Next> generator_type;
		typedef std::nullptr_t null_type;
	};

	template<typename GeneratorType>
	using IsGenerator = typename is_generator<GeneratorType>::generator_type;



	template<typename Yield>
	struct GeneratorYieldResult {
		using YieldType = Yield;
		using OptionalYieldType = Optionalized<Yield>;
		OptionalYieldType value;
		bool done = false;
	};
	
	template<>
	struct GeneratorYieldResult<void> {
		using YieldType = void;
		using OptionalYieldType = void;
		bool done = false;
	};



	template<typename Yield, typename Next=void>
	class Generator {
		friend struct _coroutine_generator_type_base<Yield,Next>;
	public:
		using YieldType = Yield;
		using NextType = Next;
		
		using YieldResult = GeneratorYieldResult<Yield>;
		using YieldReturner = typename lambda_block<Next,Promise<YieldResult>>::type;
		
		Generator();
		explicit Generator(YieldReturner yieldReturner, Function<void()> destructor=nullptr);
		
		template<typename _Yield=Yield,
			typename std::enable_if<(std::is_same<_Yield,Yield>::value &&
				!std::is_same<_Yield,void>::value), std::nullptr_t>::type = nullptr>
		static Generator<Yield,Next> resolve(_Yield);
		static Generator<Yield,Next> resolve();
		template<typename ErrorType>
		static Generator<Yield,Next> reject(ErrorType error);
		
		template<typename _Next=Next,
			typename = std::enable_if_t<(std::is_same_v<_Next,Next> && !std::is_void_v<_Next>)>>
		inline Promise<YieldResult> next(_Next nextValue);
		
		template<typename _Next=Next,
			typename = std::enable_if_t<(std::is_same_v<_Next,Next> && (std::is_void_v<_Next> || is_optional_v<_Next>))>>
		inline Promise<YieldResult> next();
		
		template<typename Transform>
		auto map(DispatchQueue* queue, Transform transform);
		template<typename Transform>
		inline auto map(Transform transform);
		template<typename Transform>
		auto mapAsync(DispatchQueue* queue, Transform transform);
		template<typename Transform>
		inline auto mapAsync(Transform transform);
		
	private:
		enum class State {
			WAITING,
			EXECUTING,
			FINISHED
		};
		
		class Continuer: public std::enable_shared_from_this<Continuer> {
		public:
			Continuer(YieldReturner yieldReturner, Function<void()> destructor);
			~Continuer();
			
			template<typename _Next=Next,
				typename = std::enable_if_t<(std::is_same_v<_Next,Next> && !std::is_void_v<_Next>)>>
			Promise<YieldResult> next(_Next nextValue);
			
			template<typename _Next=Next,
				typename = std::enable_if_t<(std::is_same_v<_Next,Next> && (std::is_void_v<_Next> || is_optional_v<_Next>))>>
			Promise<YieldResult> next();
			
		private:
			YieldReturner yieldReturner;
			Function<void()> destructor;
			Optional<Promise<void>> nextPromise;
			std::recursive_mutex mutex;
			State state;
		};
		
		std::shared_ptr<Continuer> continuer;
	};


	struct GenerateDestroyedNotifier;

	template<typename Yield, typename Executor>
	Generator<Yield,void> generatorThread(Executor executor);

	template<typename Yield, typename Next>
	using GenerateItemExecutor = typename lambda_block<Next,Promise<Yield>>::type;
	template<typename Yield, typename Next=void>
	Generator<Yield,Next> generate_items(LinkedList<GenerateItemExecutor<Yield,Next>> items);



#pragma mark Generator implementation

	template<typename Yield, typename Next>
	Generator<Yield,Next>::Generator()
	: continuer(std::make_shared<Continuer>(nullptr, nullptr)) {
		//
	}

	template<typename Yield, typename Next>
	Generator<Yield,Next>::Generator(YieldReturner yieldReturner, Function<void()> destructor)
	: continuer(std::make_shared<Continuer>(yieldReturner, destructor)) {
		//
	}

	template<typename Yield, typename Next>
	template<typename _Yield,
		typename std::enable_if<(std::is_same<_Yield,Yield>::value &&
			!std::is_same<_Yield,void>::value), std::nullptr_t>::type>
	Generator<Yield,Next> Generator<Yield,Next>::resolve(_Yield yieldVal) {
		if constexpr(std::is_same<Next,void>::value) {
			return Generator<Yield,Next>([=]() {
				return Promise<YieldResult>::resolve({
					.value=yieldVal,
					.done=true
				});
			});
		} else {
			return Generator<Yield,Next>([=](Next nextVal) {
				return Promise<YieldResult>::resolve(YieldResult{
					.value=yieldVal,
					.done=true
				});
			});
		}
	}

	template<typename Yield, typename Next>
	Generator<Yield,Next> Generator<Yield,Next>::resolve() {
		if constexpr(std::is_same<Next,void>::value) {
			return Generator<Yield,Next>([=]() {
				return Promise<YieldResult>::resolve(YieldResult{
					.done=true
				});
			});
		} else {
			return Generator<Yield,Next>([=](Next nextVal) {
				return Promise<YieldResult>::resolve(YieldResult{
					.done=true
				});
			});
		}
	}

	template<typename Yield,typename Next>
	template<typename ErrorType>
	Generator<Yield,Next> Generator<Yield,Next>::reject(ErrorType error) {
		if constexpr(std::is_same<Next,void>::value) {
			return Generator<Yield,Next>([=]() {
				return Promise<YieldResult>::reject(error);
			});
		} else {
			return Generator<Yield,Next>([=](Next nextVal) {
				return Promise<YieldResult>::reject(error);
			});
		}
	}

	template<typename Yield, typename Next>
	template<typename _Next, typename _>
	Promise<typename Generator<Yield,Next>::YieldResult> Generator<Yield,Next>::next(_Next nextValue) {
		return continuer->next(nextValue);
	}

	template<typename Yield, typename Next>
	template<typename _Next, typename _>
	Promise<typename Generator<Yield,Next>::YieldResult> Generator<Yield,Next>::next() {
		return continuer->next();
	}



	template<typename Yield, typename Next>
	template<typename Transform>
	auto Generator<Yield,Next>::map(DispatchQueue* queue, Transform transform) {
		if constexpr(std::is_same<Yield,void>::value) {
			using NewYield = decltype(transform());
			using NewYieldResult = typename Generator<NewYield,Next>::YieldResult;
			auto resultTransform = [=](auto yieldResult) {
				if constexpr(std::is_same<NewYield,void>::value) {
					transform();
					return NewYieldResult {
						.done=yieldResult.done
					};
				} else {
					return NewYieldResult{
						.value=transform(),
						.done=yieldResult.done
					};
				}
			};
			auto _continuer = this->continuer;
			if constexpr(std::is_same<Next,void>::value) {
				return Generator<NewYield,Next>([=]() {
					return _continuer->next().map(queue, resultTransform);
				});
			} else {
				return Generator<NewYield,Next>([=](Next nextValue) {
					return _continuer->next(nextValue).map(queue, resultTransform);
				});
			}
		}
		else {
			using NewYield = decltype(transform(std::declval<Yield>()));
			using NewYieldResult = typename Generator<NewYield,Next>::YieldResult;
			auto resultTransform = [=](auto yieldResult) {
				if constexpr(std::is_same<NewYield,void>::value) {
					if constexpr(std::is_same<Optionalized<Yield>,Yield>::value) {
						transform(std::move(yieldResult.value));
						return NewYieldResult{
							.done=yieldResult.done
						};
					} else {
						if(yieldResult.value.has_value()) {
							transform(std::move(yieldResult.value.value()));
							return NewYieldResult{
								.done=yieldResult.done
							};
						} else {
							return NewYieldResult{
								.done=yieldResult.done
							};
						}
					}
				} else {
					if constexpr(std::is_same<Optionalized<Yield>,Yield>::value) {
						return NewYieldResult{
							.value=transform(std::move(yieldResult.value)),
							.done=yieldResult.done
						};
					} else {
						if(yieldResult.value.has_value()) {
							return NewYieldResult{
								.value=transform(std::move(yieldResult.value.value())),
								.done=yieldResult.done
							};
						} else {
							return NewYieldResult{
								.value=std::nullopt,
								.done=yieldResult.done
							};
						}
					}
				}
			};
			auto _continuer = this->continuer;
			if constexpr(std::is_same<Next,void>::value) {
				return Generator<NewYield,Next>([=]() {
					return _continuer->next().map(queue, resultTransform);
				});
			} else {
				return Generator<NewYield,Next>([=](Next nextValue) {
					return _continuer->next(nextValue).map(queue, resultTransform);
				});
			}
		}
	}

	template<typename Yield, typename Next>
	template<typename Transform>
	auto Generator<Yield,Next>::map(Transform transform) {
		return map<Transform>(nullptr, transform);
	}

	template<typename Yield, typename Next>
	template<typename Transform>
	auto Generator<Yield,Next>::mapAsync(DispatchQueue* queue, Transform transform) {
		if constexpr(std::is_same<Yield,void>::value) {
			using NewYieldPromise = decltype(transform());
			using NewYield = typename IsPromise<NewYieldPromise>::ResultType;
			using NewYieldResult = typename Generator<NewYield,Next>::YieldResult;
			auto resultTransform = [=](auto yieldResult) {
				if constexpr(std::is_same<NewYield,void>::value) {
					return transform().map([=]() {
						return NewYieldResult{
							.done=yieldResult.done
						};
					});
				} else {
					return transform().map([=](auto newValue) {
						return NewYieldResult{
							.value=newValue,
							.done=yieldResult.done
						};
					});
				}
			};
			auto _continuer = this->continuer;
			if constexpr(std::is_same<Next,void>::value) {
				return Generator<NewYield,Next>([=]() {
					return _continuer->next().then(queue, resultTransform);
				});
			} else {
				return Generator<NewYield,Next>([=](Next nextValue) {
					return _continuer->next(nextValue).then(queue, resultTransform);
				});
			}
		}
		else {
			using NewYieldPromise = decltype(transform(std::declval<Yield>()));
			using NewYield = typename IsPromise<NewYieldPromise>::ResultType;
			using NewYieldResult = typename Generator<NewYield,Next>::YieldResult;
			auto resultTransform = [=](auto yieldResult) {
				if constexpr(std::is_same<NewYield,void>::value) {
					if constexpr(std::is_same<Optionalized<Yield>,Yield>::value) {
						return transform(std::move(yieldResult.value)).map(nullptr, [=]() {
							return NewYieldResult{
								.done=yieldResult.done
							};
						});
					} else {
						if(yieldResult.value.has_value()) {
							return transform(std::move(yieldResult.value.value())).map(nullptr, [=]() {
								return NewYieldResult{
									.done=yieldResult.done
								};
							});
						} else {
							return NewYieldResult{
								.done=yieldResult.done
							};
						}
					}
				} else {
					if constexpr(std::is_same<Optionalized<Yield>,Yield>::value) {
						return transform(std::move(yieldResult.value)).map(nullptr, [=](auto newValue) {
							return NewYieldResult{
								.value=newValue,
								.done=yieldResult.done
							};
						});
					} else {
						if(yieldResult.value.has_value()) {
							return transform(std::move(yieldResult.value.value())).map(nullptr, [=](auto newValue) {
								return NewYieldResult{
									.value=newValue,
									.done=yieldResult.done
								};
							});
						} else {
							return NewYieldResult{
								.value=std::nullopt,
								.done=yieldResult.done
							};
						}
					}
				}
			};
			auto _continuer = this->continuer;
			if constexpr(std::is_same<Next,void>::value) {
				return Generator<NewYield,Next>([=]() {
					return _continuer->next().then(queue, resultTransform);
				});
			} else {
				return Generator<NewYield,Next>([=](Next nextValue) {
					return _continuer->next(nextValue).then(queue, resultTransform);
				});
			}
		}
	}

	template<typename Yield, typename Next>
	template<typename Transform>
	auto Generator<Yield,Next>::mapAsync(Transform transform) {
		return mapAsync(nullptr, transform);
	}



	template<typename Yield, typename Next>
	Generator<Yield,Next>::Continuer::Continuer(YieldReturner yieldReturner, Function<void()> destructor)
	: yieldReturner(yieldReturner), destructor(destructor), state(yieldReturner ? State::WAITING : State::FINISHED) {
		//
	}

	template<typename Yield, typename Next>
	Generator<Yield,Next>::Continuer::~Continuer() {
		if(destructor) {
			destructor();
		}
	}

	template<typename Yield, typename Next>
	template<typename _Next, typename _>
	Promise<typename Generator<Yield,Next>::YieldResult> Generator<Yield,Next>::Continuer::next(_Next nextValue) {
		std::unique_lock<std::recursive_mutex> lock(mutex);
		if(state == State::FINISHED) {
			return Promise<YieldResult>::resolve(YieldResult{.done=true});
		}
		auto self = this->self.lock();
		auto yieldReturner = this->yieldReturner;
		auto performNext = [=]() -> Promise<YieldResult> {
			state = State::EXECUTING;
			return yieldReturner(nextValue)
			.except(nullptr, [=](std::exception_ptr error) -> YieldResult {
				std::unique_lock<std::recursive_mutex> lock(self->mutex);
				state = State::FINISHED;
				yieldReturner = nullptr;
				self->nextPromise = std::nullopt;
				std::rethrow_exception(error);
			})
			.map(nullptr, [=](YieldResult result) -> YieldResult {
				std::unique_lock<std::recursive_mutex> lock(self->mutex);
				if(result.done) {
					state = State::FINISHED;
					yieldReturner = nullptr;
				} else {
					state = State::WAITING;
				}
				self->nextPromise = std::nullopt;
				return result;
			});
		};
		if(nextPromise.has_value()) {
			auto promise = nextPromise->then(nullptr, [=]() -> Promise<YieldResult> {
				std::unique_lock<std::recursive_mutex> lock(self->mutex);
				if(state == State::FINISHED) {
					return Promise<YieldResult>::resolve(YieldResult{.done=true});
				}
				return performNext();
			});
			if(promise.isComplete()) {
				nextPromise = std::nullopt;
			} else {
				nextPromise = promise.toVoid();
			}
			return promise;
		} else {
			auto promise = performNext();
			if(promise.isComplete()) {
				nextPromise = std::nullopt;
			} else {
				nextPromise = promise.toVoid();
			}
			return promise;
		}
	}

	template<typename Yield, typename Next>
	template<typename _Next, typename _>
	Promise<typename Generator<Yield,Next>::YieldResult> Generator<Yield,Next>::Continuer::next() {
		if constexpr(is_optional_v<Next>) {
			return next(std::nullopt);
		}
		std::unique_lock<std::recursive_mutex> lock(mutex);
		if(state == State::FINISHED) {
			return Promise<YieldResult>::resolve(YieldResult{.done=true});
		}
		auto self = this->shared_from_this();
		auto yieldReturner = this->yieldReturner;
		auto performNext = [=]() -> Promise<YieldResult> {
			state = State::EXECUTING;
			return yieldReturner().map(nullptr, [=](YieldResult result) -> YieldResult {
				std::unique_lock<std::recursive_mutex> lock(self->mutex);
				if(result.done) {
					state = State::FINISHED;
					self->yieldReturner = nullptr;
				} else {
					state = State::WAITING;
				}
				self->nextPromise = std::nullopt;
				return result;
			}).except(nullptr, [=](std::exception_ptr error) -> YieldResult {
				std::unique_lock<std::recursive_mutex> lock(self->mutex);
				state = State::FINISHED;
				self->yieldReturner = nullptr;
				self->nextPromise = std::nullopt;
				std::rethrow_exception(error);
			});
		};
		if(nextPromise.has_value()) {
			auto promise = nextPromise->then(nullptr, [=]() -> Promise<YieldResult> {
				std::unique_lock<std::recursive_mutex> lock(self->mutex);
				if(state == State::FINISHED) {
					return Promise<YieldResult>::resolve(YieldResult{.done=true});
				}
				return performNext();
			});
			if(promise.isComplete()) {
				nextPromise = std::nullopt;
			} else {
				nextPromise = promise.toVoid();
			}
			return promise;
		} else {
			auto promise = performNext();
			if(promise.isComplete()) {
				nextPromise = std::nullopt;
			} else {
				nextPromise = promise.toVoid();
			}
			return promise;
		}
	}


	struct GenerateDestroyedNotifier {
	private:
		bool unused;
	};


	template<typename Yield, typename Executor>
	Generator<Yield,void> generatorThread(Executor executor) {
		using YieldResult = typename Generator<Yield,void>::YieldResult;
		struct ResultDefer {
			typename Promise<YieldResult>::Resolver resolve;
			typename Promise<YieldResult>::Rejecter reject;
		};
		struct SharedData {
			std::mutex mutex;
			std::condition_variable cv;
			// holds the resolve/reject callbacks for the generator.next() promise
			Optional<ResultDefer> yieldDefer;
			bool destroyed = false;
		};
		auto sharedData = std::make_shared<SharedData>();
		// start a new thread for the generator
		std::thread([=]() {
			std::mutex waitMutex;
			std::unique_lock<std::mutex> waitLock(waitMutex);
			// wait for a call to generator.next(), or the destruction of the generator
			sharedData->cv.wait(waitLock, [&]() {
				return sharedData->yieldDefer.has_value() || sharedData->destroyed;
			});
			// if the generator was destroyed and we're not waiting for a callback, end the call
			if(sharedData->destroyed && !sharedData->yieldDefer.has_value()) {
				return;
			}
			auto threadId = std::this_thread::get_id();
			if constexpr(std::is_same<Yield,void>::value) {
				// yield() callback for the generator
				auto yielder = [&]() {
					// yield must be called from the same thread as the executor, so it can block the thread
					if(threadId != std::this_thread::get_id()) {
						throw std::runtime_error("Cannot call yield from a different thread than the executor");
					}
					std::unique_lock<std::mutex> lock(sharedData->mutex);
					// store the generator.next() resolve/reject callbacks and clear them
					auto defer = sharedData->yieldDefer;
					sharedData->yieldDefer = std::nullopt;
					lock.unlock();
					// if we do not have a defer callback, the generator ran without a generator.next() call waiting for it
					if(!defer) {
						throw std::logic_error("Generator continued running after GenerateDestroyedNotifier was thrown");
					}
					// resolve the generator.next() call
					defer->resolve(YieldResult{.done=false});
					// if the generator was destroyed and we're not waiting for a callback, end the call
					if(sharedData->destroyed && !sharedData->yieldDefer.has_value()) {
						throw GenerateDestroyedNotifier();
					}
					// wait for the next generator.next call, or the destruction of the generator
					sharedData->cv.wait(waitLock, [&]() {
						return sharedData->yieldDefer.has_value() || sharedData->destroyed;
					});
					// if the generator was destroyed and we're not waiting for a callback, end the call
					if(sharedData->destroyed && !sharedData->yieldDefer.has_value()) {
						throw GenerateDestroyedNotifier();
					}
				};
				
				try {
					// run the generator
					executor(yielder);
				} catch(GenerateDestroyedNotifier&) {
					return;
				} catch(...) {
					std::unique_lock<std::mutex> lock(sharedData->mutex);
					auto defer = sharedData->yieldDefer;
					sharedData->yieldDefer = std::nullopt;
					lock.unlock();
					// if we do not have a defer callback, the generator ran without a generator.next() call waiting for it
					if(!defer) {
						throw std::logic_error("Generator continued running after GenerateDestroyedNotifier was thrown");
					}
					// fail the generator
					defer->reject(std::current_exception());
					return;
				}
				std::unique_lock<std::mutex> lock(sharedData->mutex);
				auto defer = sharedData->yieldDefer;
				sharedData->yieldDefer = std::nullopt;
				lock.unlock();
				// if we do not have a defer callback, the generator ran without a generator.next() call waiting for it
				if(!defer) {
					throw std::logic_error("Generator continued running after GenerateDestroyedNotifier was thrown");
				}
				// resolve the generator thread
				defer->resolve(YieldResult{.done=true});
			}
			else /*if constexpr(!std::is_same<Yield,void>::value)*/ {
				// yield() callback for the generator
				auto yielder = [&](Yield yieldValue) {
					// yield must be called from the same thread as the executor, so it can block the thread
					if(threadId != std::this_thread::get_id()) {
						throw std::runtime_error("Cannot call yield from a different thread than the executor");
					}
					std::unique_lock<std::mutex> lock(sharedData->mutex);
					// store the generator.next() resolve/reject callbacks and clear them
					auto defer = sharedData->yieldDefer;
					sharedData->yieldDefer = std::nullopt;
					lock.unlock();
					// if we do not have a defer callback, the generator ran without a generator.next() call waiting for it
					if(!defer) {
						throw std::logic_error("Generator continued running after GenerateDestroyedNotifier was thrown");
					}
					// resolve the generator.next() call
					defer->resolve(YieldResult{.value=yieldValue,.done=false});
					// if the generator was destroyed and we're not waiting for a callback, end the call
					if(sharedData->destroyed && !sharedData->yieldDefer.has_value()) {
						throw GenerateDestroyedNotifier();
					}
					// wait for the next generator.next call, or the destruction of the generator
					sharedData->cv.wait(waitLock, [&]() {
						return sharedData->yieldDefer.has_value() || sharedData->destroyed;
					});
					// if the generator was destroyed and we're not waiting for a callback, end the call
					if(sharedData->destroyed && !sharedData->yieldDefer.has_value()) {
						throw GenerateDestroyedNotifier();
					}
				};
				
				std::unique_ptr<Yield> returnVal;
				try {
					// run the generator
					returnVal = std::make_unique<Yield>(executor(yielder));
				} catch(GenerateDestroyedNotifier&) {
					return;
				} catch(...) {
					std::unique_lock<std::mutex> lock(sharedData->mutex);
					auto defer = sharedData->yieldDefer;
					sharedData->yieldDefer = std::nullopt;
					lock.unlock();
					// if we do not have a defer callback, the generator ran without a generator.next() call waiting for it
					if(!defer) {
						throw std::logic_error("Generator continued running after GenerateDestroyedNotifier was thrown");
					}
					// fail the generator
					defer->reject(std::current_exception());
					return;
				}
				std::unique_lock<std::mutex> lock(sharedData->mutex);
				auto defer = sharedData->yieldDefer;
				sharedData->yieldDefer = std::nullopt;
				lock.unlock();
				// if we do not have a defer callback, the generator ran without a generator.next() call waiting for it
				if(!defer) {
					throw std::logic_error("Generator continued running after GenerateDestroyedNotifier was thrown");
				}
				// resolve the generator thread
				defer->resolve(YieldResult{.value=std::move(*returnVal.get()),.done=true});
			}
		}).detach();
		
		return Generator<Yield,void>([=]() -> Promise<YieldResult> {
			return Promise<YieldResult>([&](auto resolve, auto reject) {
				std::unique_lock<std::mutex> lock(sharedData->mutex);
				// save the resolve/reject handlers to call from the generator thread
				sharedData->yieldDefer = ResultDefer{ resolve, reject };
				lock.unlock();
				sharedData->cv.notify_one();
			});
		}, [=]() {
			sharedData->destroyed = true;
			sharedData->cv.notify_one();
		});
	}




	template<typename Yield, typename Next>
	Generator<Yield,Next> generate_items(LinkedList<GenerateItemExecutor<Yield,Next>> items) {
		using Gen = Generator<Yield,Next>;
		using YieldResult = typename Gen::YieldResult;
		auto sharedItems = std::make_shared<LinkedList<GenerateItemExecutor<Yield,Next>>>(items);
		if constexpr(std::is_same<Next,void>::value) {
			return Generator<Yield,Next>([=]() {
				if(sharedItems->size() == 0) {
					if constexpr(std::is_same<Yield,void>::value) {
						return Promise<YieldResult>::resolve(YieldResult{
							.done=true
						});
					} else {
						return Promise<YieldResult>::resolve(YieldResult{
							.value=std::nullopt,
							.done=true
						});
					}
				}
				auto nextFunc = sharedItems->extractFront();
				if constexpr(std::is_same<Yield,void>::value) {
					return nextFunc().map(nullptr, [=]() -> YieldResult {
						return YieldResult{
							.done=(sharedItems->size() == 0)
						};
					});
				} else {
					return nextFunc().map(nullptr, [=](auto yieldVal) -> YieldResult {
						return YieldResult{
							.value=yieldVal,
							.done=(sharedItems->size() == 0)
						};
					});
				}
			});
		} else {
			return Generator<Yield,Next>([=](Next nextVal) {
				if(sharedItems->size() == 0) {
					if constexpr(std::is_same<Yield,void>::value) {
						return Promise<YieldResult>::resolve(YieldResult{
							.done=true
						});
					} else {
						return Promise<YieldResult>::resolve(YieldResult{
							.value=std::nullopt,
							.done=true
						});
					}
				}
				auto nextFunc = sharedItems->extractFront();
				if constexpr(std::is_same<Yield,void>::value) {
					return nextFunc(nextVal).map(nullptr, [=](auto yieldVal) -> YieldResult {
						return YieldResult{
							.done=(sharedItems->size() == 0)
						};
					});
				} else {
					return nextFunc(nextVal).map(nullptr, [=](auto yieldVal) -> YieldResult {
						return YieldResult{
							.value=yieldVal,
							.done=(sharedItems->size() == 0)
						};
					});
				}
			});
		}
	}



	// Return the initial value passed to gen.next()
	struct initialGenNext{
		explicit initialGenNext();
	};

	// Sets the queue that the generator should resume on
	struct setGenResumeQueue{
		setGenResumeQueue(DispatchQueue* queue, bool enterQueue = true);
		
		DispatchQueue* queue;
		bool enterQueue = true;
	};


	template<typename Yield, typename Next>
	struct _coroutine_generator_type_base {
		using YieldResult = GeneratorYieldResult<Yield>;
		coroutine_handle<> handle;
		typename Promise<YieldResult>::Resolver resolve;
		typename Promise<YieldResult>::Rejecter reject;
		member_if<(!std::is_void_v<Next>),
			std::unique_ptr<NullifyVoid<Next>>> nextValue;
		Generator<Yield,Next> generator;
		DispatchQueue* queue = nullptr;
		bool yielded = false;
		bool suspended = false;
		
		_coroutine_generator_type_base(coroutine_handle<> handle)
		: handle(handle), generator(yieldReturner()) {
			//
		}
		
		Generator<Yield,Next> get_return_object() {
			return generator;
		}

		suspend_never initial_suspend() const noexcept {
			return {};
		}
		
		suspend_never final_suspend() const noexcept {
			return {};
		}
		
		inline auto yield_value(setGenResumeQueue setter) {
			queue = setter.queue;
			struct awaiter {
				DispatchQueue* queue;
				bool enterQueue;
				bool await_ready() { return (!enterQueue || queue == nullptr || queue->isLocal()); }
				void await_suspend(coroutine_handle<> handle) {
					queue->async([=]() {
						auto h = handle;
						h.resume();
					});
				}
				void await_resume() {}
			};
			return awaiter{ setter.queue, setter.enterQueue };
		}
		
		inline auto yield_value(initialGenNext) {
			FGL_ASSERT(!yielded, "co_yield initialGenNext() must be called once before any element yields");
			yielded = true;
			return yieldAwaiter();
		}
		
		inline auto yield_value(const NullifyVoid<Yield>& value) {
			FGL_ASSERT(yielded, "co_yield initialGenNext() must be called once before any element yields");
			if constexpr(std::is_void_v<Yield>) {
				resolve({ .done = false });
			} else {
				resolve({
					.value = value,
					.done = false
				});
			}
			return yieldAwaiter();
		}
		
		inline auto yield_value(NullifyVoid<Yield>&& value) {
			FGL_ASSERT(yielded, "co_yield initialGenNext() must be called once before any element yields");
			if constexpr(std::is_void_v<Yield>) {
				resolve({ .done = false });
			} else {
				resolve({
					.value = value,
					.done = false
				});
			}
			return yieldAwaiter();
		}
		
		void unhandled_exception() noexcept {
			reject(std::current_exception());
		}
		
	protected:
		inline auto yieldReturner() {
			auto body = [=]() {
				auto promise = Promise<YieldResult>([=](auto resolve, auto reject) {
					this->resolve = resolve;
					this->reject = reject;
				});
				if(this->suspended) {
					this->suspended = false;
					if(this->queue != nullptr && !this->queue->isLocal()) {
						auto handle = this->handle;
						this->queue->async([=]() {
							auto h = handle;
							h.resume();
						});
					} else {
						this->handle.resume();
					}
				}
				return promise;
			};
			if constexpr(std::is_void_v<Next>) {
				return body;
			} else {
				return [=](auto nextVal) {
					this->nextValue = std::make_unique<Next>(nextVal);
					return body();
				};
			}
		}
		
		inline auto yieldAwaiter() {
			if constexpr(std::is_void_v<Next>) {
				struct awaiter {
					_coroutine_generator_type_base<Yield,Next>* self;
					bool await_ready() { return self->generator.continuer->nextPromise.hasValue(); }
					void await_suspend(coroutine_handle<> handle) {
						self->suspended = true;
					}
					void await_resume() {}
				};
				return awaiter{ this };
			} else {
				struct awaiter {
					_coroutine_generator_type_base<Yield,Next>* self;
					std::unique_ptr<Next> value;
					bool await_ready() { return self->generator.continuer->nextPromise.hasValue(); }
					void await_suspend(coroutine_handle<> handle) {
						self->suspended = true;
					}
					Next await_resume() {
						return std::move(*value.get());
					}
				};
				awaiter a{ this };
				a.value.swap(this->nextValue);
				return a;
			}
		}
	};

	template<typename Yield, typename Next>
	struct coroutine_generator_type: public _coroutine_generator_type_base<Yield,Next> {
		using _coroutine_generator_type_base<Yield,Next>::_coroutine_generator_type_base;
		void return_value(const Yield& value) {
			this->resolve({
				.value = value,
				.done = true
			});
		}
		
		void return_value(Yield&& value) {
			this->resolve({
				.value = value,
				.done = true
			});
		}
	};

	template<typename Next>
	struct coroutine_generator_type<void,Next>: public _coroutine_generator_type_base<void,Next> {
		using _coroutine_generator_type_base<void,Next>::_coroutine_generator_type_base;
		void return_void() {
			this->resolve({ .done = true });
		}
	};
}

#if __has_include(<coroutine>)
template<typename Yield, typename Next, typename... Args>
struct std::coroutine_traits<fgl::Generator<Yield,Next>, Args...> {
	using promise_type = fgl::coroutine_generator_type<Yield,Next>;
};
#else
template<typename Yield, typename Next, typename... Args>
struct std::experimental::coroutine_traits<fgl::Generator<Yield,Next>, Args...> {
	using promise_type = fgl::coroutine_generator_type<Yield,Next>;
};
#endif
