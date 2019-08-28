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
	: label(label), options(options), type(Type::BACKGROUND), alive(true), stopped(true) {
		//
	}
	
	DispatchQueue::DispatchQueue(SystemType systemType)
	: label(labelForSystemType(systemType)), type(typeForSysemType(systemType)), alive(true), stopped(true) {
		//
	}
	
	DispatchQueue::~DispatchQueue() {
		FGL_ASSERT((itemQueue.size() == 0 || scheduledItemQueue.size() == 0), "Trying to destroy DispatchQueue \""+label+"\" while unfinished items remain");
		alive = false;
		queueWaitCondition.notify_one();
		if(thread.joinable()) {
			thread.join();
		}
	}
	
	
	
	String DispatchQueue::labelForSystemType(SystemType type) {
		switch(type) {
			case SystemType::MAIN:
				return "Main";
			default:
				FGL_ASSERT(false, "Unknown DispatchQueue::SystemType");
		}
	}
	
	DispatchQueue::Options DispatchQueue::optionsForSystemType(SystemType type) {
		switch(type) {
			case SystemType::MAIN:
				return Options{
					.keepThreadAlive = true
				};
			default:
				FGL_ASSERT(false, "Unknown DispatchQueue::SystemType");
		}
	}
	
	DispatchQueue::Type DispatchQueue::typeForSysemType(SystemType type) {
		switch(type) {
			case SystemType::MAIN:
				return Type::MAIN;
			default:
				FGL_ASSERT(false, "Unknown DispatchQueue::SystemType");
		}
	}
	
	
	
	void DispatchQueue::notify() {
		if(stopped && type != Type::MAIN) {
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
		while(alive) {
			std::unique_lock<std::mutex> lock(mutex);
			
			// get next work item
			DispatchWorkItem* workItem = nullptr;
			Function<void()> onFinishItem = nullptr;
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
					auto nextItem = scheduledItemQueue.front();
					lock.unlock();
					nextItem->wait(queueWaitCondition, [&]() { return shouldWake(); });
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
	}
	
	bool DispatchQueue::shouldWake() const {
		if(!alive) {
			return true;
		}
		if(itemQueue.size() > 0) {
			return true;
		}
		if(scheduledItemQueue.size() > 0) {
			auto item = scheduledItemQueue.front();
			if(item->timeUntil().count() <= 0) {
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
	
	
	
	void DispatchQueue::dispatchMain() {
		FGL_ASSERT(usesMainQueue(), "FGL_DISPATCH_USES_MAIN must be defined in order to use this function");
		FGL_ASSERT(!mainQueueRunning, "main DispatchQueue has already been dispatched");
		mainQueueRunning = true;
		mainQueue->run();
		exit(0);
	}
	
	DispatchQueue* DispatchQueue::getMainQueue() {
		if(mainQueue == nullptr && usesMainQueue()) {
			mainQueue = new DispatchQueue(SystemType::MAIN);
		}
		return mainQueue;
	}
	
	DispatchQueue* DispatchQueue::getLocalQueue() {
		return localDispatchQueue;
	}
}
