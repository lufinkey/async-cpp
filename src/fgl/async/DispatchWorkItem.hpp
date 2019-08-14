//
//  DispatchWorkItem.hpp
//  AsyncCpp
//
//  Created by Luis Finke on 8/11/19.
//  Copyright © 2019 Luis Finke. All rights reserved.
//

#pragma once

#include <condition_variable>
#include <list>
#include <mutex>
#include "Common.hpp"
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
		std::list<Function<void()>> notifyItems;
		std::mutex mutex;
		Options options;
		bool ranOnce;
		bool cancelled;
	};
	
	
	
	
#pragma mark DispatchWorkItem implementation
	
	template<typename Rep, typename Period>
	DispatchTimeoutResult DispatchWorkItem::waitFor(std::chrono::duration<Rep,Period> timeout) {
		std::condition_variable cv;
		std::mutex waitMutex;
		std::unique_lock<std::mutex> waitLock(waitMutex);
		bool finished = false;
		Function<void()> notifyBlock = [&]() {
			finished = true;
			cv.notify_one();
		};
		
		std::unique_lock<std::mutex> lock(mutex);
		notifyItems.push_back(notifyBlock);
		lock.unlock();
		auto result = cv.wait_for(waitLock, timeout, [&]() {
			return finished;
		});
		
		switch(result) {
			case std::cv_status::no_timeout:
				return DispatchTimeoutResult::SUCCESS;
			case std::cv_status::timeout:
				lock.lock();
				for(auto it=notifyItems.begin(); it!=notifyItems.end(); it++) {
					auto& item = *it;
					if(notifyBlock.target<void(*)()>() != item.target<void(*)()>()) {
						notifyItems.erase(it);
						break;
					}
				}
				lock.unlock();
				return DispatchTimeoutResult::TIMED_OUT;
		}
		FGL_ASSERT(false, "invalid std::cv_status value");
	}
	
	template<typename Clock, typename Duration>
	DispatchTimeoutResult DispatchWorkItem::waitUntil(std::chrono::time_point<Clock,Duration> time) {
		std::condition_variable cv;
		std::mutex waitMutex;
		std::unique_lock<std::mutex> waitLock(waitMutex);
		bool finished = false;
		Function<void()> notifyBlock = [&]() {
			finished = true;
			cv.notify_one();
		};
		
		std::unique_lock<std::mutex> lock(mutex);
		notifyItems.push_back(notifyBlock);
		lock.unlock();
		auto result = cv.wait_until(waitLock, time, [&]() {
			return finished;
		});
		
		switch(result) {
			case std::cv_status::no_timeout:
				return DispatchTimeoutResult::SUCCESS;
			case std::cv_status::timeout:
				lock.lock();
				for(auto it=notifyItems.begin(); it!=notifyItems.end(); it++) {
					auto& item = *it;
					if(notifyBlock.target<void(*)()>() != item.target<void(*)()>()) {
						notifyItems.erase(it);
						break;
					}
				}
				lock.unlock();
				return DispatchTimeoutResult::TIMED_OUT;
		}
		FGL_ASSERT(false, "invalid std::cv_status value");
	}
}
