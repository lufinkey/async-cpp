//
//  Generator.hpp
//  AsyncCpp
//
//  Created by Luis Finke on 11/17/19.
//  Copyright © 2019 Luis Finke. All rights reserved.
//

#pragma once

#include <condition_variable>
#include <thread>
#include <fgl/async/Common.hpp>
#include <fgl/async/Promise.hpp>

namespace fgl {
	template<typename Yield, typename Next>
	class Generator {
	public:
		typedef Yield YieldType;
		typedef Next NextType;
		
		template<typename _Yield>
		struct _YieldResult {
			Optionalized<_Yield> value;
			bool done = false;
		};
		
		template<>
		struct _YieldResult<void> {
			bool done = false;
		};
		
		using YieldResult = _YieldResult<Yield>;
		
		template<typename Arg, typename Return>
		struct _block {
			using type = Function<Return(Arg)>;
		};
		template<typename Return>
		struct _block<void, Return> {
			using type = Function<Return()>;
		};
		
		using YieldReturner = typename _block<Next,Promise<YieldResult>>::type;
		
		explicit Generator(YieldReturner yieldReturner);
		
		template<typename _Next=Next,
			typename std::enable_if<(std::is_same<_Next,Next>::value && !std::is_same<_Next,void>::value), std::nullptr_t>::type = nullptr>
		inline Promise<YieldResult> next(_Next nextValue);
		
		template<typename _Next=Next,
			typename std::enable_if<(std::is_same<_Next,Next>::value && std::is_same<_Next,void>::value), std::nullptr_t>::type = nullptr>
		inline Promise<YieldResult> next();
		
	private:
		enum class State {
			WAITING,
			EXECUTING,
			FINISHED
		};
		
		class Continuer {
		public:
			Continuer(std::shared_ptr<Continuer>& ptr, YieldReturner yieldReturner);
			
			template<typename _Next=Next,
				typename std::enable_if<(std::is_same<_Next,Next>::value && !std::is_same<_Next,void>::value), std::nullptr_t>::type = nullptr>
			Promise<YieldResult> next(_Next nextValue);
			
			template<typename _Next=Next,
				typename std::enable_if<(std::is_same<_Next,Next>::value && std::is_same<_Next,void>::value), std::nullptr_t>::type = nullptr>
			Promise<YieldResult> next();
			
		private:
			std::weak_ptr<Continuer> self;
			YieldReturner yieldReturner;
			Optional<Promise<void>> nextPromise;
			std::recursive_mutex mutex;
			State state;
		};
		
		std::shared_ptr<Continuer> continuer;
	};



	template<typename Yield, typename Return,
		typename std::enable_if<std::is_same<Return,Yield>::value || std::is_same<Return,void>::value,std::nullptr_t>::type = nullptr>
	Generator<Yield,void> generate(Function<Return(Function<void(Yield)> yield)> executor) {
		using YieldResult = typename Generator<Yield,void>::YieldResult;
		struct ResultDefer {
			typename Promise<YieldResult>::Resolver resolve;
			typename Promise<YieldResult>::Rejecter reject;
		};
		struct SharedData {
			std::mutex mutex;
			std::condition_variable cv;
			Optional<ResultDefer> yieldDefer;
		};
		auto sharedData = std::make_shared<SharedData>();
		std::thread([=]() {
			std::mutex waitMutex;
			std::unique_lock<std::mutex> waitLock(waitMutex);
			sharedData->cv.wait(waitLock, [&]() {
				return sharedData->yieldDefer.has_value();
			});
			auto threadId = std::this_thread::get_id();
			auto yielder = [&](Yield yieldValue) {
				if(threadId != std::this_thread::get_id()) {
					throw std::runtime_error("Cannot call yield from a different thread than the executor");
				}
				std::unique_lock<std::mutex> lock(sharedData->mutex);
				auto defer = sharedData->yieldDefer;
				sharedData->yieldDefer = std::nullopt;
				lock.unlock();
				defer->resolve(YieldResult{.value=yieldValue,.done=false});
				sharedData->cv.wait(waitLock, [&]() {
					return sharedData->yieldDefer.has_value();
				});
			};
			if constexpr(std::is_same<Return,void>::value) {
				try {
					executor(yielder);
				} catch(...) {
					std::unique_lock<std::mutex> lock(sharedData->mutex);
					auto defer = sharedData->yieldDefer;
					sharedData->yieldDefer = std::nullopt;
					lock.unlock();
					defer->reject(std::current_exception());
					return;
				}
				{
					std::unique_lock<std::mutex> lock(sharedData->mutex);
					auto defer = sharedData->yieldDefer;
					sharedData->yieldDefer = std::nullopt;
					lock.unlock();
					defer->resolve(YieldResult{.value=std::nullopt,.done=true});
				}
			} else {
				std::unique_ptr<Return> returnVal;
				try {
					returnVal = std::make_unique<Return>(executor(yielder));
				} catch(...) {
					std::unique_lock<std::mutex> lock(sharedData->mutex);
					auto defer = sharedData->yieldDefer;
					sharedData->yieldDefer = std::nullopt;
					lock.unlock();
					defer->reject(std::current_exception());
					return;
				}
				{
					std::unique_lock<std::mutex> lock(sharedData->mutex);
					auto defer = sharedData->yieldDefer;
					sharedData->yieldDefer = std::nullopt;
					lock.unlock();
					defer->resolve(YieldResult{.value=Optional<Return>(std::move(*returnVal.get())),.done=true});
				}
			}
		}).detach();
		return Generator<Yield,void>([=]() -> Promise<YieldResult> {
			return Promise<YieldResult>([&](auto resolve, auto reject) {
				std::unique_lock<std::mutex> lock(sharedData->mutex);
				sharedData->yieldDefer = ResultDefer{ resolve, reject };
				lock.unlock();
				sharedData->cv.notify_one();
			});
		});
	}



#pragma mark - Generator implementation

	template<typename Yield, typename Next>
	Generator<Yield,Next>::Generator(YieldReturner yieldReturner) {
		new Continuer(this->continuer, yieldReturner);
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
	Generator<Yield,Next>::Continuer::Continuer(std::shared_ptr<Continuer>& ptr, YieldReturner yieldReturner)
	: yieldReturner(yieldReturner), state(State::WAITING) {
		ptr = std::shared_ptr<Continuer>(this);
		self = ptr;
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
			auto yieldPromise = yieldReturner(nextValue);
			yieldPromise.then(nullptr, [=](YieldResult result) {
				std::unique_lock<std::recursive_mutex> lock(self->mutex);
				if(result.done) {
					state = State::FINISHED;
					yieldReturner = nullptr;
				} else {
					state = State::WAITING;
				}
				self->nextPromise = std::nullopt;
			}, [=](std::exception_ptr error) {
				std::unique_lock<std::recursive_mutex> lock(self->mutex);
				state = State::FINISHED;
				yieldReturner = nullptr;
				self->nextPromise = std::nullopt;
			});
			return yieldPromise;
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
			auto yieldPromise = yieldReturner();
			yieldPromise.then(nullptr, [=](YieldResult result) {
				std::unique_lock<std::recursive_mutex> lock(self->mutex);
				if(result.done) {
					state = State::FINISHED;
					self->yieldReturner = nullptr;
				} else {
					state = State::WAITING;
				}
				self->nextPromise = std::nullopt;
			}, [=](std::exception_ptr error) {
				std::unique_lock<std::recursive_mutex> lock(self->mutex);
				state = State::FINISHED;
				self->yieldReturner = nullptr;
				self->nextPromise = std::nullopt;
			});
			return yieldPromise;
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
}
