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
		Promise<YieldResult> next(Next nextValue);
		
		template<typename _Next=Next,
			typename std::enable_if<(std::is_same<_Next,Next>::value && std::is_same<_Next,void>::value), std::nullptr_t>::type = nullptr>
		Promise<YieldResult> next();
		
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
			Promise<YieldResult> next(Next nextValue);
			
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



#pragma mark - Generator implementation

	template<typename Yield, typename Next>
	Generator<Yield,Next>::Generator(YieldReturner yieldReturner) {
		new Continuer(this->continuer, yieldReturner);
	}

	template<typename Yield, typename Next>
	template<typename _Next,
		typename std::enable_if<(std::is_same<_Next,Next>::value && !std::is_same<_Next,void>::value), std::nullptr_t>::type>
	Promise<typename Generator<Yield,Next>::YieldResult> Generator<Yield,Next>::next(Next nextValue) {
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
	Promise<typename Generator<Yield,Next>::YieldResult> Generator<Yield,Next>::Continuer::next(Next nextValue) {
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
}
