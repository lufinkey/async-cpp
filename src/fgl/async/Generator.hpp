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
#include <condition_variable>
#include <thread>

namespace fgl {
	template<typename Yield, typename Next>
	class Generator;
	
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
		Optionalized<Yield> value;
		bool done = false;
	};
	
	template<>
	struct GeneratorYieldResult<void> {
		bool done = false;
	};



	template<typename Yield, typename Next>
	class Generator {
	public:
		using YieldType = Yield;
		using NextType = Next;
		
		using YieldResult = GeneratorYieldResult<Yield>;
		
		using YieldReturner = typename lambda_block<Next,Promise<YieldResult>>::type;
		template<typename T>
		using Mapper = typename lambda_block<Yield,T>::type;
		
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
			typename std::enable_if<(std::is_same<_Next,Next>::value && !std::is_same<_Next,void>::value), std::nullptr_t>::type = nullptr>
		inline Promise<YieldResult> next(_Next nextValue);
		
		template<typename _Next=Next,
			typename std::enable_if<(std::is_same<_Next,Next>::value && std::is_same<_Next,void>::value), std::nullptr_t>::type = nullptr>
		inline Promise<YieldResult> next();
		
		template<typename NewYield>
		Generator<NewYield,Next> map(DispatchQueue* queue, Mapper<NewYield> transform);
		template<typename NewYield>
		Generator<NewYield,Next> map(Mapper<NewYield> transform);
		template<typename NewYield>
		Generator<NewYield,Next> mapAsync(DispatchQueue* queue, Mapper<Promise<NewYield>> transform);
		template<typename NewYield>
		Generator<NewYield,Next> mapAsync(Mapper<Promise<NewYield>> transform);
		
	private:
		enum class State {
			WAITING,
			EXECUTING,
			FINISHED
		};
		
		class Continuer {
		public:
			Continuer(std::shared_ptr<Continuer>& ptr, YieldReturner yieldReturner, Function<void()> destructor=nullptr);
			~Continuer();
			
			template<typename _Next=Next,
				typename std::enable_if<(std::is_same<_Next,Next>::value && !std::is_same<_Next,void>::value), std::nullptr_t>::type = nullptr>
			Promise<YieldResult> next(_Next nextValue);
			
			template<typename _Next=Next,
				typename std::enable_if<(std::is_same<_Next,Next>::value && std::is_same<_Next,void>::value), std::nullptr_t>::type = nullptr>
			Promise<YieldResult> next();
			
		private:
			std::weak_ptr<Continuer> self;
			YieldReturner yieldReturner;
			Function<void()> destructor;
			Optional<Promise<void>> nextPromise;
			std::recursive_mutex mutex;
			State state;
		};
		
		std::shared_ptr<Continuer> continuer;
	};


	struct GenerateDestroyedNotifier;
	template<typename Yield>
	using GenerateYielder = typename lambda_block<Yield,void>::type;

	template<typename Yield>
	Generator<Yield,void> generate(Function<Yield(GenerateYielder<Yield> yield)> executor);

	template<typename Yield, typename Next>
	using GenerateItemExecutor = typename lambda_block<Next,Promise<Yield>>::type;
	template<typename Yield, typename Next=void>
	Generator<Yield,Next> generate_items(LinkedList<GenerateItemExecutor<Yield,Next>> items);



#pragma mark Generator implementation

	template<typename Yield, typename Next>
	Generator<Yield,Next>::Generator() {
		new Continuer(this->continuer, nullptr, nullptr);
	}

	template<typename Yield, typename Next>
	Generator<Yield,Next>::Generator(YieldReturner yieldReturner, Function<void()> destructor) {
		new Continuer(this->continuer, yieldReturner, destructor);
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
	template<typename _Next,
		typename std::enable_if<(std::is_same<_Next,Next>::value && !std::is_same<_Next,void>::value), std::nullptr_t>::type>
	Promise<typename Generator<Yield,Next>::YieldResult> Generator<Yield,Next>::next(_Next nextValue) {
		return continuer->next(nextValue);
	}

	template<typename Yield, typename Next>
	template<typename _Next,
		typename std::enable_if<(std::is_same<_Next,Next>::value && std::is_same<_Next,void>::value), std::nullptr_t>::type>
	Promise<typename Generator<Yield,Next>::YieldResult> Generator<Yield,Next>::next() {
		return continuer->next();
	}



	template<typename Yield, typename Next>
	template<typename NewYield>
	Generator<NewYield,Next> Generator<Yield,Next>::map(DispatchQueue* queue, Mapper<NewYield> transform) {
		using NewYieldResult = typename Generator<NewYield,Next>::YieldResult;
		auto resultTransform = [=](auto yieldResult) {
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
		};
		auto _continuer = this->continuer;
		if constexpr(std::is_same<Next,void>::value) {
			return Generator<NewYield,Next>([=]() {
				return _continuer->next().template map<NewYieldResult>(queue, resultTransform);
			});
		} else {
			return Generator<NewYield,Next>([=](Next nextValue) {
				return _continuer->next(nextValue).template map<NewYieldResult>(queue, resultTransform);
			});
		}
	}

	template<typename Yield, typename Next>
	template<typename NewYield>
	Generator<NewYield,Next> Generator<Yield,Next>::map(Mapper<NewYield> transform) {
		return map<NewYield>(nullptr, transform);
	}

	template<typename Yield, typename Next>
	template<typename NewYield>
	Generator<NewYield,Next> Generator<Yield,Next>::mapAsync(DispatchQueue* queue, Mapper<Promise<NewYield>> transform) {
		using NewYieldResult = typename Generator<NewYield,Next>::YieldResult;
		auto resultTransform = [=](auto yieldResult) {
			if constexpr(std::is_same<Optionalized<Yield>,Yield>::value) {
				return transform(std::move(yieldResult.value)).template map<NewYieldResult>(nullptr, [=](NewYield newValue) {
					return NewYieldResult{
						.value=newValue,
						.done=yieldResult.done
					};
				});
			} else {
				if(yieldResult.value.has_value()) {
					return transform(std::move(yieldResult.value.value())).template map<NewYieldResult>(nullptr, [=](NewYield newValue) {
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
		};
		auto _continuer = this->continuer;
		if constexpr(std::is_same<Next,void>::value) {
			return Generator<NewYield,Next>([=]() {
				return _continuer->next().template then<NewYieldResult>(queue, resultTransform);
			});
		} else {
			return Generator<NewYield,Next>([=](Next nextValue) {
				return _continuer->next(nextValue).template then<NewYieldResult>(queue, resultTransform);
			});
		}
	}

	template<typename Yield, typename Next>
	template<typename NewYield>
	Generator<NewYield,Next> Generator<Yield,Next>::mapAsync(Mapper<Promise<NewYield>> transform) {
		return mapAsync(nullptr, transform);
	}



	template<typename Yield, typename Next>
	Generator<Yield,Next>::Continuer::Continuer(std::shared_ptr<Continuer>& ptr, YieldReturner yieldReturner, Function<void()> destructor)
	: yieldReturner(yieldReturner), destructor(destructor), state(yieldReturner ? State::WAITING : State::FINISHED) {
		ptr = std::shared_ptr<Continuer>(this);
		self = ptr;
	}

	template<typename Yield, typename Next>
	Generator<Yield,Next>::Continuer::~Continuer() {
		if(destructor) {
			destructor();
		}
	}

	template<typename Yield, typename Next>
	template<typename _Next,
		typename std::enable_if<(std::is_same<_Next,Next>::value && !std::is_same<_Next,void>::value), std::nullptr_t>::type>
	Promise<typename Generator<Yield,Next>::YieldResult> Generator<Yield,Next>::Continuer::next(_Next nextValue) {
		std::unique_lock<std::recursive_mutex> lock(mutex);
		if(state == State::FINISHED) {
			return Promise<YieldResult>::resolve(YieldResult{.done=true});
		}
		auto self = this->self.lock();
		auto yieldReturner = this->yieldReturner;
		auto performNext = [=]() -> Promise<YieldResult> {
			state = State::EXECUTING;
			return yieldReturner(nextValue).template map<YieldResult>(nullptr, [=](YieldResult result) {
				std::unique_lock<std::recursive_mutex> lock(self->mutex);
				if(result.done) {
					state = State::FINISHED;
					yieldReturner = nullptr;
				} else {
					state = State::WAITING;
				}
				self->nextPromise = std::nullopt;
				return result;
			}).except([=](std::exception_ptr error) -> YieldResult {
				std::unique_lock<std::recursive_mutex> lock(self->mutex);
				state = State::FINISHED;
				yieldReturner = nullptr;
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

	template<typename Yield, typename Next>
	template<typename _Next,
		typename std::enable_if<(std::is_same<_Next,Next>::value && std::is_same<_Next,void>::value), std::nullptr_t>::type>
	Promise<typename Generator<Yield,Next>::YieldResult> Generator<Yield,Next>::Continuer::next() {
		std::unique_lock<std::recursive_mutex> lock(mutex);
		if(state == State::FINISHED) {
			return Promise<YieldResult>::resolve(YieldResult{.done=true});
		}
		auto self = this->self.lock();
		auto yieldReturner = this->yieldReturner;
		auto performNext = [=]() -> Promise<YieldResult> {
			state = State::EXECUTING;
			return yieldReturner().template map<YieldResult>(nullptr, [=](YieldResult result) -> YieldResult {
				std::unique_lock<std::recursive_mutex> lock(self->mutex);
				if(result.done) {
					state = State::FINISHED;
					self->yieldReturner = nullptr;
				} else {
					state = State::WAITING;
				}
				self->nextPromise = std::nullopt;
				return result;
			}).except([=](std::exception_ptr error) -> YieldResult {
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


	template<typename Yield>
	Generator<Yield,void> generate(Function<Yield(GenerateYielder<Yield> yield)> executor) {
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
				
			} else /*if constexpr(!std::is_same<Yield,void>::value)*/ {
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
					return nextFunc().template map<YieldResult>(nullptr, [=]() {
						return YieldResult{
							.done=(sharedItems->size() == 0)
						};
					});
				} else {
					return nextFunc().template map<YieldResult>(nullptr, [=](auto yieldVal) {
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
					return nextFunc(nextVal).template map<YieldResult>(nullptr, [=](auto yieldVal) {
						return YieldResult{
							.done=(sharedItems->size() == 0)
						};
					});
				} else {
					return nextFunc(nextVal).template map<YieldResult>(nullptr, [=](auto yieldVal) {
						return YieldResult{
							.value=yieldVal,
							.done=(sharedItems->size() == 0)
						};
					});
				}
			});
		}
	}
}
