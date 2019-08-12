//
//  DispatchQueue.cpp
//  AsyncCpp
//
//  Created by Luis Finke on 8/11/19.
//  Copyright © 2019 Luis Finke. All rights reserved.
//

#include "DispatchQueue.hpp"

namespace fgl {
	#ifdef FGL_DISPATCH_USES_MAIN
	DispatchQueue* DispatchQueue::mainQueue = new DispatchQueue(SystemType::MAIN);
	#endif
	
	DispatchQueue::DispatchQueue(String label)
	: label(label), thread([=]() { this->run(); }), alive(true) {
		//
	}
	
	DispatchQueue::DispatchQueue(SystemType systemType)
	: label(labelForSystemType(systemType)), thread(), alive(true) {
		//
	}
	
	DispatchQueue::~DispatchQueue() {
		alive = false;
		queueWaitCondition.notify_one();
		thread.join();
	}
	
	void DispatchQueue::run() {
		while(alive) {
			step();
		}
	}
	
	void DispatchQueue::step() {
		DispatchWorkItem* workItem = nullptr;
		Function<void()> onFinishItem = nullptr;
		std::unique_lock<std::mutex> lock(mutex);
		if(scheduledItemQueue.size() > 0) {
			auto item = scheduledItemQueue.front();
			if(item->timeUntil().count() <= 0) {
				workItem = item->workItem;
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
			lock.unlock();
			workItem->perform();
			if(onFinishItem) {
				onFinishItem();
			}
		}
		else {
			if(scheduledItemQueue.size() > 0) {
				auto nextItem = scheduledItemQueue.front();
				lock.unlock();
				nextItem->wait(queueWaitCondition, [&]() {
					return (itemQueue.size() > 0 || scheduledItemQueue.size() > 0 || !alive);
				});
			}
			else {
				std::mutex waitMutex;
				std::unique_lock<std::mutex> waitLock(waitMutex);
				lock.unlock();
				queueWaitCondition.wait(waitLock, [&]() {
					return (itemQueue.size() > 0 || scheduledItemQueue.size() > 0 || !alive);
				});
			}
		}
	}
	
	void DispatchQueue::async(Function<void()> work) {
		async(new DispatchWorkItem({ .deleteAfterRunning=true }, work));
	}
	
	void DispatchQueue::async(DispatchWorkItem* workItem) {
		std::unique_lock<std::mutex> lock(mutex);
		itemQueue.push_back({ .workItem=workItem });
		lock.unlock();
		queueWaitCondition.notify_one();
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
		lock.unlock();
		
		queueWaitCondition.notify_one();
		cv.wait(waitLock, [&]() {
			return finished;
		});
	}
	
	
	
	#ifdef FGL_DISPATCH_USES_MAIN
	
	void DispatchQueue::dispatchMain() {
		FGL_ASSERT(!mainQueueRunning, "main DispatchQueue has already been dispatched");
		mainQueueRunning = true;
		mainQueue->run();
	}
	
	DispatchQueue* DispatchQueue::getMainQueue() {
		return mainQueue;
	}
	
	#endif
	
	
}
