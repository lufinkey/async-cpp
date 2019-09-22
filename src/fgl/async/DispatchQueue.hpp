//
//  DispatchQueue.hpp
//  AsyncCpp
//
//  Created by Luis Finke on 8/11/19.
//  Copyright © 2019 Luis Finke. All rights reserved.
//

#pragma once

#include <chrono>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <thread>
#include <fgl/async/Common.hpp>
#include <fgl/async/DispatchWorkItem.hpp>

namespace fgl {
	class DispatchQueue {
	public:
		struct Options {
			bool keepThreadAlive = false;
		};
		
		DispatchQueue(const DispatchQueue&) = delete;
		DispatchQueue& operator=(const DispatchQueue&) = delete;
		
		DispatchQueue(String label);
		DispatchQueue(String label, Options options);
		~DispatchQueue();
		
		void async(Function<void()> work);
		void async(DispatchWorkItem* workItem);
		template<typename Clock, typename Duration>
		void asyncAfter(std::chrono::time_point<Clock,Duration> deadline, Function<void()> work);
		template<typename Clock, typename Duration>
		void asyncAfter(std::chrono::time_point<Clock,Duration> deadline, DispatchWorkItem* workItem);
		
		void sync(Function<void()> work);
		void sync(DispatchWorkItem* workItem);
		template<typename T>
		T sync(Function<T()> work);
		
		[[noreturn]]
		static void dispatchMain();
		static DispatchQueue* getMain();
		static bool usesMainQueue() {
			#ifdef FGL_DISPATCH_USES_MAIN
				return true;
			#else
				return false;
			#endif
		}
		
		static DispatchQueue* getLocal();
		
	private:
		enum class Type {
			MAIN,
			BACKGROUND
		};
		enum class SystemType {
			MAIN
		};
		static String labelForSystemType(SystemType);
		static Options optionsForSystemType(SystemType);
		static Type typeForSysemType(SystemType);
		
		DispatchQueue(SystemType);
		
		void notify();
		void run();
		bool shouldWake() const;
		
		struct QueueItem {
			DispatchWorkItem* workItem;
			std::function<void()> onFinish = nullptr;
		};
		
		class ScheduledQueueItem {
		public:
			DispatchWorkItem* workItem;
			
			ScheduledQueueItem(DispatchWorkItem* workItem)
			: workItem(workItem) {}
			virtual ~ScheduledQueueItem() {}
			
			virtual std::chrono::nanoseconds timeUntil() const = 0;
			virtual void wait(std::condition_variable& cv, Function<bool()> pred) const = 0;
		};
		
		template<typename Clock, typename Duration>
		class SpecificScheduledQueueItem: ScheduledQueueItem {
		public:
			std::chrono::time_point<Clock,Duration> time;
			
			SpecificScheduledQueueItem(DispatchWorkItem* workItem, std::chrono::time_point<Clock,Duration> time)
			: ScheduledQueueItem(workItem), time(time) {}
			
			virtual std::chrono::nanoseconds timeUntil() const override;
			virtual void wait(std::condition_variable& cv, Function<bool()> pred) const override;
		};
		
		String label;
		Options options;
		std::thread thread;
		std::mutex mutex;
		
		std::list<QueueItem> itemQueue;
		std::list<ScheduledQueueItem*> scheduledItemQueue;
		
		std::condition_variable queueWaitCondition;
		
		DispatchQueue::Type type;
		bool alive;
		bool stopped;
		
		static DispatchQueue* mainQueue;
		static bool mainQueueRunning;
	};
	
	
	
	
#pragma mark DispatchQueue implementation
	
	template<typename Clock, typename Duration>
	void DispatchQueue::asyncAfter(std::chrono::time_point<Clock,Duration> deadline, Function<void()> work) {
		asyncAfter(deadline, new DispatchWorkItem({ .deleteAfterRunning=true }, work));
	}
	
	template<typename Clock, typename Duration>
	void DispatchQueue::asyncAfter(std::chrono::time_point<Clock,Duration> deadline, DispatchWorkItem* workItem) {
		std::unique_lock<std::mutex> lock(mutex);
		auto scheduledItem = new SpecificScheduledQueueItem<Clock,Duration>(workItem, deadline);
		bool inserted = false;
		for(auto it=scheduledItemQueue.begin(); it!=scheduledItemQueue.end(); it++) {
			auto item = *it;
			if(scheduledItem->timeUntil() <= item->timeUntil()) {
				scheduledItemQueue.insert(it, scheduledItem);
				inserted = true;
				break;
			}
		}
		if(!inserted) {
			scheduledItemQueue.push_back(scheduledItem);
		}
		lock.unlock();
		notify();
	}
	
	template<typename T>
	T DispatchQueue::sync(Function<T()> work) {
		bool rejected = false;
		std::unique_ptr<T> result_ptr = nullptr;
		std::exception_ptr error_ptr = nullptr;
		sync([&]() {
			try {
				result_ptr = std::make_unique<T>(work());
			} catch(...) {
				error_ptr = std::current_exception();
				rejected = true;
			}
		});
		if(rejected) {
			std::rethrow_exception(error_ptr);
		}
		return std::move(*result_ptr.get());
	}
	
	template<typename Clock, typename Duration>
	std::chrono::nanoseconds DispatchQueue::SpecificScheduledQueueItem<Clock,Duration>::timeUntil() const {
		return std::chrono::duration_cast<std::chrono::microseconds>(time - Clock::now());
	}
	
	template<typename Clock, typename Duration>
	void DispatchQueue::SpecificScheduledQueueItem<Clock,Duration>::wait(std::condition_variable& cv, Function<bool()> pred) const {
		std::mutex waitMutex;
		std::unique_lock<std::mutex> waitLock(waitMutex);
		if(pred()) {
			return;
		}
		cv.wait_until(waitLock, time, pred);
	}
}
