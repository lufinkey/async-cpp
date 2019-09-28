//
//  Timer.hpp
//  AsyncCpp
//
//  Created by Luis Finke on 9/27/19.
//  Copyright © 2019 Luis Finke. All rights reserved.
//

#pragma once

#include <chrono>
#include <memory>
#include <thread>
#include <fgl/async/Common.hpp>
#include <fgl/async/DispatchQueue.hpp>

namespace fgl {
	class Timer;
	using SharedTimer = std::shared_ptr<Timer>;
	using WeakTimer = std::weak_ptr<Timer>;
	
	class Timer {
	public:
		Timer(const Timer&) = delete;
		Timer& operator=(const Timer&) = delete;
		~Timer();
		
		template<typename Clock, typename Duration>
		static SharedTimer withTimePoint(std::chrono::time_point<Clock,Duration> timePoint, Function<void(SharedTimer)> work);
		template<typename Clock, typename Duration>
		static SharedTimer withTimePoint(std::chrono::time_point<Clock,Duration> timePoint, DispatchQueue* queue, Function<void(SharedTimer)> work);
		template<typename Rep, typename Period>
		static SharedTimer withTimeout(std::chrono::duration<Rep,Period> timeout, Function<void(SharedTimer)> work);
		template<typename Rep, typename Period>
		static SharedTimer withTimeout(std::chrono::duration<Rep,Period> timeout, DispatchQueue* queue, Function<void(SharedTimer)> work);
		template<typename Rep, typename Period>
		static SharedTimer withInterval(std::chrono::duration<Rep,Period> interval, Function<void(SharedTimer)> work);
		template<typename Rep, typename Period>
		static SharedTimer withInterval(std::chrono::duration<Rep,Period> interval, DispatchQueue* queue, Function<void(SharedTimer)> work);
		
		void invoke();
		
		void cancel();
		bool isValid() const;
		
		template<typename Clock = std::chrono::system_clock>
		std::chrono::time_point<Clock> getInvokeTime() const;
		
	private:
		template<typename Clock>
		Timer(std::shared_ptr<Timer>& ptr, std::chrono::time_point<Clock> timePoint, Function<void(SharedTimer)> work);
		template<typename Rep>
		Timer(std::shared_ptr<Timer>& ptr, std::chrono::duration<Rep> timeInterval, Function<void(SharedTimer)> work);
		
		void run();
		
		class Waiter {
		public:
			virtual ~Waiter() {}
			virtual void wait() = 0;
			virtual void cancel() = 0;
			virtual bool isCancelled() const = 0;
			virtual std::chrono::steady_clock::time_point getSteadyTimePoint() const = 0;
			virtual std::chrono::system_clock::time_point getSystemTimePoint() const = 0;
		};
		
		template<typename Clock>
		class SpecialWaiter: public Waiter {
		public:
			SpecialWaiter(std::chrono::time_point<Clock> timePoint)
			: timePoint(timePoint), cancelled(false) {}
			
			virtual void wait() override;
			virtual void cancel() override;
			virtual bool isCancelled() const override;
			virtual std::chrono::steady_clock::time_point getSteadyTimePoint() const override;
			virtual std::chrono::system_clock::time_point getSystemTimePoint() const override;
			
		private:
			std::chrono::time_point<Clock> timePoint;
			std::condition_variable cv;
			bool cancelled;
		};
		
		WeakTimer self;
		std::thread thread;
		Waiter* waiter;
		Function<void()> rescheduleWaiter;
		Function<void(SharedTimer)> work;
		bool valid;
	};
	
	
	
	
#pragma mark Timer implementation
	
	template<typename Clock, typename Duration>
	SharedTimer Timer::withTimePoint(std::chrono::time_point<Clock,Duration> timePoint, Function<void(SharedTimer)> work) {
		std::shared_ptr<Timer> ptr;
		new Timer(ptr, std::chrono::time_point_cast<typename Clock::duration>(timePoint), work);
		return ptr;
	}
	
	template<typename Clock, typename Duration>
	SharedTimer Timer::withTimePoint(std::chrono::time_point<Clock,Duration> timePoint, DispatchQueue* queue, Function<void(SharedTimer)> work) {
		return withTimePoint(timePoint, [=](SharedTimer timer) {
			queue->async([=]() {
				work(timer);
			});
		});
	}
	
	template<typename Rep, typename Period>
	SharedTimer Timer::withTimeout(std::chrono::duration<Rep,Period> timeout, Function<void(SharedTimer)> work) {
		std::shared_ptr<Timer> ptr;
		using Clock = std::chrono::steady_clock;
		new Timer(ptr, std::chrono::time_point_cast<typename Clock::duration>(Clock::now() + timeout), work);
		return ptr;
	}
	
	template<typename Rep, typename Period>
	static SharedTimer withTimeout(std::chrono::duration<Rep,Period> timeout, DispatchQueue* queue, Function<void(SharedTimer)> work) {
		return withTimeout(timeout, [=](SharedTimer timer) {
			queue->async([=]() {
				work(timer);
			});
		});
	}
	
	template<typename Rep, typename Period>
	static SharedTimer withInterval(std::chrono::duration<Rep,Period> interval, Function<void(SharedTimer)> work) {
		std::shared_ptr<Timer> ptr;
		new Timer(ptr, std::chrono::duration_cast<std::chrono::duration<Rep>>(interval), work);
		return ptr;
	}
	
	template<typename Rep, typename Period>
	static SharedTimer withInterval(std::chrono::duration<Rep,Period> interval, DispatchQueue* queue, Function<void(SharedTimer)> work) {
		return withInterval(interval, [=](SharedTimer timer) {
			queue->async([=]() {
				work(timer);
			});
		});
	}
	
	
	
	template<typename Clock>
	Timer::Timer(std::shared_ptr<Timer>& ptr, std::chrono::time_point<Clock> timePoint, Function<void(SharedTimer)> work)
	: waiter(nullptr), work(work), valid(true) {
		ptr = SharedTimer(this);
		self = ptr;
		waiter = new SpecialWaiter<Clock>(timePoint);
		run();
	}
	
	template<typename Rep>
	Timer::Timer(std::shared_ptr<Timer>& ptr, std::chrono::duration<Rep> interval, Function<void(SharedTimer)> work)
	: waiter(nullptr), work(work), valid(true) {
		ptr = SharedTimer(this);
		self = ptr;
		waiter = new SpecialWaiter<std::chrono::steady_clock>(std::chrono::steady_clock::now() + interval);
		rescheduleWaiter = [=]() {
			auto oldWaiter = waiter;
			waiter = new SpecialWaiter<std::chrono::steady_clock>(std::chrono::steady_clock::now() + interval);
			if(oldWaiter != nullptr) {
				delete oldWaiter;
			}
		};
		run();
	}
	
	Timer::~Timer() {
		if(waiter != nullptr) {
			delete waiter;
		}
	}
	
	
	
	template<typename Clock>
	std::chrono::time_point<Clock> Timer::getInvokeTime() const {
		if constexpr(std::is_same<Clock,std::chrono::steady_clock>::value) {
			return waiter->getSteadyTimePoint();
		} else if constexpr(std::is_same<Clock,std::chrono::system_clock>::value) {
			return waiter->getSystemTimePoint();
		} else {
			return Clock::now() + (std::chrono::steady_clock::now() - waiter->getSteadyTimePoint());
		}
	}
	
	
	
	template<typename Clock>
	void Timer::SpecialWaiter<Clock>::wait() {
		std::mutex waitMutex;
		std::unique_lock<std::mutex> lock(waitMutex);
		cv.wait_until(lock, timePoint, [=]() -> bool {
			return (cancelled || timePoint <= Clock::now());
		});
	}
	
	template<typename Clock>
	void Timer::SpecialWaiter<Clock>::cancel() {
		cancelled = true;
		cv.notify_one();
	}
	
	template<typename Clock>
	bool Timer::SpecialWaiter<Clock>::isCancelled() const {
		return cancelled;
	}
	
	template<typename Clock>
	std::chrono::steady_clock::time_point Timer::SpecialWaiter<Clock>::getSteadyTimePoint() const {
		if constexpr(std::is_same<Clock,std::chrono::steady_clock>::value) {
			return timePoint;
		} else {
			return std::chrono::steady_clock::now() + (Clock::now() - timePoint);
		}
	}
	
	template<typename Clock>
	std::chrono::system_clock::time_point Timer::SpecialWaiter<Clock>::getSystemTimePoint() const {
		if constexpr(std::is_same<Clock,std::chrono::system_clock>::value) {
			return timePoint;
		} else {
			return std::chrono::system_clock::now() + (Clock::now() - timePoint);
		}
	}
}
