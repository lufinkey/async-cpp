//
//  DispatchWorkItem.hpp
//  AsyncCpp
//
//  Created by Luis Finke on 8/11/19.
//  Copyright © 2019 Luis Finke. All rights reserved.
//

#pragma once

#include "Macros.hpp"
#include "Types.hpp"
#include "DispatchTimeoutResult.hpp"

namespace fgl {
	class DispatchQueue;
	
	class DispatchWorkItem {
	public:
		struct Options {
			bool deleteAfterRunning = false;
		};
		
		DispatchWorkItem(const DispatchQueue&) = delete;
		DispatchWorkItem& operator=(const DispatchQueue&) = delete;
		
		DispatchWorkItem(Function<void()> work);
		DispatchWorkItem(Options options, Function<void()> work);
		
		void perform();
		
		void notify(DispatchQueue* queue, Function<void()> work);
		void notify(DispatchQueue* queue, DispatchWorkItem* workItem);
		
		void wait();
		template<typename Rep, typename Period>
		DispatchTimeoutResult waitFor(std::chrono::duration<Rep,Period> timeout);
		template<typename Clock, typename Duration>
		DispatchTimeoutResult waitUntil(std::chrono::time_point<Clock,Duration> time);
		
		void cancel();
		bool isCancelled() const;
		
	private:
		Function<void()> work;
		LinkedList<Function<void()>> notifyItems;
		Mutex mutex;
		Options options;
		bool ranOnce;
		bool cancelled;
	};
	
	
	
	
	template<typename Rep, typename Period>
	DispatchTimeoutResult DispatchWorkItem::waitFor(std::chrono::duration<Rep,Period> timeout) {
		std::condition_variable cv;
		std::mutex waitMutex;
		std::unique_lock<std::mutex> lock(waitMutex);
		bool finished = false;
		mutex.lock();
		notifyItems.push_back([&]() {
			finished = true;
			cv.notify_one();
		});
		mutex.unlock();
		auto result = cv.wait_for(lock, timeout, [&]() {
			return finished;
		});
		switch(result) {
			case std::cv_status::no_timeout:
				return DispatchTimeoutResult::SUCCESS;
			case std::cv_status::timeout:
				return DispatchTimeoutResult::TIMED_OUT;
		}
		FGL_ASSERT(false, "invalid std::cv_status value");
	}
	
	template<typename Clock, typename Duration>
	DispatchTimeoutResult DispatchWorkItem::waitUntil(std::chrono::time_point<Clock,Duration> time) {
		std::condition_variable cv;
		std::mutex waitMutex;
		std::unique_lock<std::mutex> lock(waitMutex);
		bool finished = false;
		mutex.lock();
		notifyItems.push_back([&]() {
			finished = true;
			cv.notify_one();
		});
		mutex.unlock();
		auto result = cv.wait_until(lock, time, [&]() {
			return finished;
		});
		switch(result) {
			case std::cv_status::no_timeout:
				return DispatchTimeoutResult::SUCCESS;
			case std::cv_status::timeout:
				return DispatchTimeoutResult::TIMED_OUT;
		}
		FGL_ASSERT(false, "invalid std::cv_status value");
	}
}
