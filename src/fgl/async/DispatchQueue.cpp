//
//  DispatchQueue.cpp
//  AsyncCpp
//
//  Created by Luis Finke on 8/11/19.
//  Copyright © 2019 Luis Finke. All rights reserved.
//

#include <fgl/async/DispatchQueue.hpp>

namespace fgl {
	DispatchQueue* DispatchQueue::mainQueue = nullptr;
	bool DispatchQueue::mainQueueRunning = false;
	thread_local DispatchQueue* localDispatchQueue = nullptr;
	
	DispatchQueue::DispatchQueue(String label)
	: DispatchQueue(label, Options()) {
		//
	}

	DispatchQueue::DispatchQueue(String label, Options options)
	: DispatchQueue(Type::BACKGROUND, label, options) {
		//
	}
	
	DispatchQueue::DispatchQueue(Type type, String label, Options options)
	: label(label), options(options), type(type), killed(false), stopped(true) {
		//
	}
	
	DispatchQueue::~DispatchQueue() {
		FGL_ASSERT((itemQueue.size() == 0 || scheduledItemQueue.size() == 0), "Trying to destroy DispatchQueue \""+label+"\" while unfinished items remain");
		killed = true;
		queueWaitCondition.notify_one();
		if(thread.joinable()) {
			thread.join();
		}
	}
	
	
	
	void DispatchQueue::notify() {
		if(stopped && type != Type::LOCAL) {
			if(thread.joinable()) {
				thread.join();
			}
			thread = std::thread([=]() {
				this->run();
			});
		}
		else {
			queueWaitCondition.notify_one();
		}
	}
	
	void DispatchQueue::run() {
		localDispatchQueue = this;
		stopped = false;
		while(!killed) {
			std::unique_lock<std::mutex> lock(mutex);
			
			// get next work item
			DispatchWorkItem* workItem = nullptr;
			Function<void()> onFinishItem = nullptr;
			if(scheduledItemQueue.size() > 0) {
				auto& item = scheduledItemQueue.front();
				if(item.timeUntil().count() <= 0) {
					workItem = item.workItem;
					scheduledItemQueue.pop_front();
				}
			}
			if(workItem == nullptr && itemQueue.size() > 0) {
				auto& item = itemQueue.front();
				workItem = item.workItem;
				onFinishItem = item.onFinish;
				itemQueue.pop_front();
			}
			
			if(workItem != nullptr) {
				// perform next work item
				lock.unlock();
				workItem->perform();
				if(onFinishItem) {
					onFinishItem();
				}
			}
			else {
				if(scheduledItemQueue.size() > 0) {
					// wait for next scheduled item
					auto& nextItem = scheduledItemQueue.front();
					lock.unlock();
					nextItem.wait(queueWaitCondition, [&]() { return shouldWake(); });
				}
				else if(options.keepThreadAlive) {
					// wait for any item
					std::mutex waitMutex;
					std::unique_lock<std::mutex> waitLock(waitMutex);
					lock.unlock();
					queueWaitCondition.wait(waitLock, [&]() { return shouldWake(); });
				}
				else {
					// no more items, so stop the thread for now
					stopped = true;
					lock.unlock();
					break;
				}
			}
		}
		localDispatchQueue = nullptr;
	}
	
	bool DispatchQueue::shouldWake() const {
		if(killed) {
			return true;
		}
		if(itemQueue.size() > 0) {
			return true;
		}
		if(scheduledItemQueue.size() > 0) {
			auto& item = scheduledItemQueue.front();
			if(item.timeUntil().count() <= 0) {
				return true;
			}
		}
		return false;
	}
	
	
	
	void DispatchQueue::async(Function<void()> work) {
		async(new DispatchWorkItem({ .deleteAfterRunning=true }, work));
	}
	
	void DispatchQueue::async(DispatchWorkItem* workItem) {
		std::unique_lock<std::mutex> lock(mutex);
		itemQueue.push_back({ .workItem=workItem });
		notify();
		lock.unlock();
	}

	void DispatchQueue::asyncAfter(Clock::time_point deadline, DispatchWorkItem* workItem) {
		std::unique_lock<std::mutex> lock(mutex);
		
		auto scheduledItem = ScheduledQueueItem{
			.workItem=workItem,
			.deadline=deadline
		};
		bool inserted = false;
		for(auto it=scheduledItemQueue.begin(); it!=scheduledItemQueue.end(); it++) {
			auto& item = *it;
			if(scheduledItem.timeUntil() <= item.timeUntil()) {
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
	
	void DispatchQueue::sync(Function<void()> work) {
		sync(new DispatchWorkItem({ .deleteAfterRunning=true }, work));
	}
	
	void DispatchQueue::sync(DispatchWorkItem* workItem) {
		std::condition_variable cv;
		std::mutex waitMutex;
		std::unique_lock<std::mutex> waitLock(waitMutex);
		bool finished = false;
		
		std::unique_lock<std::mutex> lock(mutex);
		itemQueue.push_back({ .workItem=workItem, .onFinish=[&]() {
			finished = true;
			cv.notify_one();
		} });
		notify();
		lock.unlock();
		
		cv.wait(waitLock, [&]() {
			return finished;
		});
	}



	void DispatchQueue::ScheduledQueueItem::wait(std::condition_variable& cv, Function<bool()> pred) const {
		std::mutex waitMutex;
		std::unique_lock<std::mutex> waitLock(waitMutex);
		if(pred()) {
			return;
		}
		cv.wait_until(waitLock, deadline, pred);
	}
	
	
	
	void DispatchQueue::dispatchMain() {
		FGL_ASSERT(usesMainQueue(), "FGL_DISPATCH_USES_MAIN must be defined in order to use this function");
		FGL_ASSERT(!mainQueueRunning, "main DispatchQueue has already been dispatched");
		mainQueueRunning = true;
		mainQueue->run();
		exit(0);
	}
	
	DispatchQueue* DispatchQueue::getMain() {
		if(mainQueue == nullptr && usesMainQueue()) {
			mainQueue = new DispatchQueue(Type::LOCAL, "Main", {
				.keepThreadAlive=true
			});
		}
		return mainQueue;
	}
	
	DispatchQueue* DispatchQueue::getLocal() {
		return localDispatchQueue;
	}
}
