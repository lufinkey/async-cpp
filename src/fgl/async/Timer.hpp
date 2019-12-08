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
#include <fgl/async/Promise.hpp>

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
		
		template<typename Clock, typename Duration>
		static SharedTimer withTimePoint(std::chrono::time_point<Clock,Duration> timePoint, Function<void()> work);
		template<typename Clock, typename Duration>
		static SharedTimer withTimePoint(std::chrono::time_point<Clock,Duration> timePoint, DispatchQueue* queue, Function<void()> work);
		
		
		template<typename Rep, typename Period>
		static SharedTimer withTimeout(std::chrono::duration<Rep,Period> timeout, Function<void(SharedTimer)> work);
		template<typename Rep, typename Period>
		static SharedTimer withTimeout(std::chrono::duration<Rep,Period> timeout, DispatchQueue* queue, Function<void(SharedTimer)> work);
		
		template<typename Rep, typename Period>
		static SharedTimer withTimeout(std::chrono::duration<Rep,Period> timeout, Function<void()> work);
		template<typename Rep, typename Period>
		static SharedTimer withTimeout(std::chrono::duration<Rep,Period> timeout, DispatchQueue* queue, Function<void()> work);
		
		
		template<typename Rep, typename Period>
		static SharedTimer withInterval(std::chrono::duration<Rep,Period> interval, Function<void(SharedTimer)> work);
		template<typename Rep, typename Period>
		static SharedTimer withInterval(std::chrono::duration<Rep,Period> interval, DispatchQueue* queue, Function<void(SharedTimer)> work);
		
		template<typename Rep, typename Period>
		static SharedTimer withInterval(std::chrono::duration<Rep,Period> interval, Function<void()> work);
		template<typename Rep, typename Period>
		static SharedTimer withInterval(std::chrono::duration<Rep,Period> interval, DispatchQueue* queue, Function<void()> work);
		
		
		template<typename Rep, typename Period>
		static Promise<void> delay(std::chrono::duration<Rep,Period> timeout);
		
		
		void invoke();
		
		void cancel();
		bool isValid() const;
		
		template<typename Clock = std::chrono::steady_clock, typename Duration = typename Clock::duration>
		std::chrono::time_point<Clock,Duration> getInvokeTime() const;
		
	private:
		template<typename Clock, typename Duration>
		Timer(std::shared_ptr<Timer>& ptr, std::chrono::time_point<Clock,Duration> timePoint, DispatchQueue* queue, Function<void(SharedTimer)> work);
		template<typename Rep, typename Period>
		Timer(std::shared_ptr<Timer>& ptr, std::chrono::duration<Rep,Period> timeInterval, DispatchQueue* queue, Function<void(SharedTimer)> work);
		
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
		
		template<typename Clock, typename Duration>
		class SpecialWaiter: public Waiter {
		public:
			SpecialWaiter(std::chrono::time_point<Clock,Duration> timePoint)
			: timePoint(timePoint), cancelled(false) {}
			
			virtual void wait() override;
			virtual void cancel() override;
			virtual bool isCancelled() const override;
			virtual std::chrono::steady_clock::time_point getSteadyTimePoint() const override;
			virtual std::chrono::system_clock::time_point getSystemTimePoint() const override;
			
		private:
			std::chrono::time_point<Clock,Duration> timePoint;
			std::condition_variable cv;
			bool cancelled;
		};
		
		WeakTimer self;
		mutable std::recursive_mutex mutex;
		Waiter* waiter;
		Function<void()> rescheduleWaiter;
		DispatchQueue* queue;
		Function<void(SharedTimer)> work;
		bool valid;
	};
	
	
	
	
#pragma mark Timer implementation
	
	template<typename Clock, typename Duration>
	SharedTimer Timer::withTimePoint(std::chrono::time_point<Clock,Duration> timePoint, Function<void(SharedTimer)> work) {
		std::shared_ptr<Timer> ptr;
		new Timer(ptr, timePoint, nullptr, work);
		return ptr;
	}
	
	template<typename Clock, typename Duration>
	SharedTimer Timer::withTimePoint(std::chrono::time_point<Clock,Duration> timePoint, DispatchQueue* queue, Function<void(SharedTimer)> work) {
		std::shared_ptr<Timer> ptr;
		new Timer(ptr, timePoint, queue, work);
		return ptr;
	}
	
	template<typename Clock, typename Duration>
	SharedTimer Timer::withTimePoint(std::chrono::time_point<Clock,Duration> timePoint, Function<void()> work) {
		return withTimePoint(timePoint, (work ? [=](SharedTimer) {
			work();
		} : Function<void(SharedTimer)>()));
	}

	template<typename Clock, typename Duration>
	SharedTimer Timer::withTimePoint(std::chrono::time_point<Clock,Duration> timePoint, DispatchQueue* queue, Function<void()> work) {
		return withTimePoint(timePoint, queue, (work ? [=](SharedTimer) {
			work();
		} : Function<void(SharedTimer)>()));
	}


	
	template<typename Rep, typename Period>
	SharedTimer Timer::withTimeout(std::chrono::duration<Rep,Period> timeout, Function<void(SharedTimer)> work) {
		std::shared_ptr<Timer> ptr;
		using Clock = std::chrono::steady_clock;
		new Timer(ptr, (Clock::now() + timeout), nullptr, work);
		return ptr;
	}
	
	template<typename Rep, typename Period>
	static SharedTimer withTimeout(std::chrono::duration<Rep,Period> timeout, DispatchQueue* queue, Function<void(SharedTimer)> work) {
		std::shared_ptr<Timer> ptr;
		using Clock = std::chrono::steady_clock;
		new Timer(ptr, (Clock::now() + timeout), queue, work);
		return ptr;
	}

	template<typename Rep, typename Period>
	SharedTimer Timer::withTimeout(std::chrono::duration<Rep,Period> timeout, Function<void()> work) {
		return withTimeout(timeout, (work ? [=](SharedTimer) {
			work();
		} : Function<void(SharedTimer)>()));
	}
	
	template<typename Rep, typename Period>
	static SharedTimer withTimeout(std::chrono::duration<Rep,Period> timeout, DispatchQueue* queue, Function<void()> work) {
		return withTimeout(timeout, queue, (work ? [=](SharedTimer) {
			work();
		} : Function<void(SharedTimer)>()));
	}


	
	template<typename Rep, typename Period>
	static SharedTimer withInterval(std::chrono::duration<Rep,Period> interval, Function<void(SharedTimer)> work) {
		std::shared_ptr<Timer> ptr;
		new Timer(ptr, interval, nullptr, work);
		return ptr;
	}
	
	template<typename Rep, typename Period>
	static SharedTimer withInterval(std::chrono::duration<Rep,Period> interval, DispatchQueue* queue, Function<void(SharedTimer)> work) {
		std::shared_ptr<Timer> ptr;
		new Timer(ptr, interval, queue, work);
		return ptr;
	}

	template<typename Rep, typename Period>
	static SharedTimer withInterval(std::chrono::duration<Rep,Period> interval, Function<void()> work) {
		return withInterval(interval, (work ? [=](SharedTimer) {
			work();
		} : Function<void(SharedTimer)>()));
	}
	
	template<typename Rep, typename Period>
	static SharedTimer withInterval(std::chrono::duration<Rep,Period> interval, DispatchQueue* queue, Function<void()> work) {
		return withInterval(interval, queue, (work ? [=](SharedTimer) {
			work();
		} : Function<void(SharedTimer)>()));
	}



	template<typename Rep, typename Period>
	Promise<void> Timer::delay(std::chrono::duration<Rep,Period> timeout) {
		return Promise<void>([=](auto resolve, auto reject) {
			withTimeout(timeout, [=]() {
				resolve();
			});
		});
	}
	
	
	
	template<typename Clock, typename Duration>
	Timer::Timer(std::shared_ptr<Timer>& ptr, std::chrono::time_point<Clock,Duration> timePoint, DispatchQueue* queue, Function<void(SharedTimer)> work)
	: waiter(nullptr), queue(queue), work(work), valid(true) {
		ptr = SharedTimer(this);
		self = ptr;
		waiter = new SpecialWaiter<Clock,Duration>(timePoint);
		run();
	}
	
	template<typename Rep, typename Period>
	Timer::Timer(std::shared_ptr<Timer>& ptr, std::chrono::duration<Rep,Period> interval, DispatchQueue* queue, Function<void(SharedTimer)> work)
	: waiter(nullptr), work(work), valid(true) {
		ptr = SharedTimer(this);
		self = ptr;
		waiter = new SpecialWaiter<std::chrono::steady_clock,std::chrono::steady_clock::duration>(std::chrono::steady_clock::now() + interval);
		rescheduleWaiter = [=]() {
			std::unique_lock<std::recursive_mutex> lock(mutex);
			auto oldWaiter = waiter;
			waiter = new SpecialWaiter<std::chrono::steady_clock,std::chrono::steady_clock::duration>(std::chrono::steady_clock::now() + interval);
			if(oldWaiter != nullptr) {
				delete oldWaiter;
			}
		};
		run();
	}
	
	
	
	template<typename Clock, typename Duration>
	std::chrono::time_point<Clock,Duration> Timer::getInvokeTime() const {
		std::unique_lock<std::recursive_mutex> lock(mutex);
		if constexpr(std::is_same<Clock,std::chrono::steady_clock>::value) {
			return waiter->getSteadyTimePoint();
		} else if constexpr(std::is_same<Clock,std::chrono::system_clock>::value) {
			return waiter->getSystemTimePoint();
		} else {
			return Clock::now() + (std::chrono::steady_clock::now() - waiter->getSteadyTimePoint());
		}
	}
	
	
	
	template<typename Clock, typename Duration>
	void Timer::SpecialWaiter<Clock,Duration>::wait() {
		std::mutex waitMutex;
		std::unique_lock<std::mutex> lock(waitMutex);
		cv.wait_until(lock, timePoint, [=]() -> bool {
			return (cancelled || timePoint <= Clock::now());
		});
	}
	
	template<typename Clock, typename Duration>
	void Timer::SpecialWaiter<Clock,Duration>::cancel() {
		cancelled = true;
		cv.notify_one();
	}
	
	template<typename Clock, typename Duration>
	bool Timer::SpecialWaiter<Clock,Duration>::isCancelled() const {
		return cancelled;
	}
	
	template<typename Clock, typename Duration>
	std::chrono::steady_clock::time_point Timer::SpecialWaiter<Clock,Duration>::getSteadyTimePoint() const {
		if constexpr(std::is_same<Clock,std::chrono::steady_clock>::value) {
			return timePoint;
		} else {
			using SteadyClock = std::chrono::steady_clock;
			return std::chrono::time_point_cast<typename SteadyClock::duration>(SteadyClock::now() + Duration(Clock::now() - timePoint));
		}
	}
	
	template<typename Clock, typename Duration>
	std::chrono::system_clock::time_point Timer::SpecialWaiter<Clock,Duration>::getSystemTimePoint() const {
		if constexpr(std::is_same<Clock,std::chrono::system_clock>::value) {
			return timePoint;
		} else {
			using SystemClock = std::chrono::system_clock;
			return std::chrono::time_point_cast<typename SystemClock::duration>(SystemClock::now() + Duration(Clock::now() - timePoint));
		}
	}
}
