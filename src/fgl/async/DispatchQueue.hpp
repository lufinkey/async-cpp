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
		using Clock = std::chrono::steady_clock;
		
		struct Options {
			bool keepThreadAlive = false;
		};
		
		DispatchQueue(const DispatchQueue&) = delete;
		DispatchQueue& operator=(const DispatchQueue&) = delete;
		
		DispatchQueue(String label);
		DispatchQueue(String label, Options options);
		~DispatchQueue();
		
		void async(Function<void()> work);
		virtual void async(DispatchWorkItem* workItem);
		template<typename Duration>
		void asyncAfter(std::chrono::time_point<Clock,Duration> deadline, Function<void()> work);
		template<typename Duration>
		void asyncAfter(std::chrono::time_point<Clock,Duration> deadline, DispatchWorkItem* workItem);
		virtual void asyncAfter(Clock::time_point deadline, DispatchWorkItem* workItem);
		
		void sync(Function<void()> work);
		virtual void sync(DispatchWorkItem* workItem);
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
			LOCAL,
			BACKGROUND
		};
		
		DispatchQueue(Type type, String label, Options options);
		
		void notify();
		void run();
		bool shouldWake() const;
		
		struct QueueItem {
			DispatchWorkItem* workItem;
			std::function<void()> onFinish = nullptr;
		};
		struct ScheduledQueueItem {
			DispatchWorkItem* workItem;
			Clock::time_point deadline;
			inline Clock::duration timeUntil() const;
			void wait(std::condition_variable& cv, Function<bool()> pred) const;
		};
		
		String label;
		Options options;
		std::thread thread;
		std::mutex mutex;
		
		std::list<QueueItem> itemQueue;
		std::list<ScheduledQueueItem> scheduledItemQueue;
		
		std::condition_variable queueWaitCondition;
		
		DispatchQueue::Type type;
		bool killed;
		bool stopped;
		
		static DispatchQueue* mainQueue;
		static bool mainQueueRunning;
	};
	
	
	
	
#pragma mark DispatchQueue implementation
	
	template<typename Duration>
	void DispatchQueue::asyncAfter(std::chrono::time_point<Clock,Duration> deadline, Function<void()> work) {
		asyncAfter(deadline, new DispatchWorkItem({ .deleteAfterRunning=true }, work));
	}
	
	template<typename Duration>
	void DispatchQueue::asyncAfter(std::chrono::time_point<Clock,Duration> deadline, DispatchWorkItem* workItem) {
		asyncAfter(Clock::time_point(deadline), workItem);
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

	DispatchQueue::Clock::duration DispatchQueue::ScheduledQueueItem::timeUntil() const {
		return deadline - Clock::now();
	}
}
