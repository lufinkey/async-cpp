//
//  DispatchWorkItem.cpp
//  AsyncCpp
//
//  Created by Luis Finke on 8/11/19.
//  Copyright © 2019 Luis Finke. All rights reserved.
//

#include <fgl/async/DispatchWorkItem.hpp>
#include <fgl/async/DispatchQueue.hpp>

namespace fgl {
	DispatchWorkItem::DispatchWorkItem(Function<void()> work)
	: DispatchWorkItem(Options(), work) {
		//
	}
	
	DispatchWorkItem::DispatchWorkItem(Options options, Function<void()> work)
	: work(work), options(options), ranOnce(false), cancelled(false) {
		//
	}
	
	void DispatchWorkItem::perform() {
		std::unique_lock<std::mutex> lock(mutex);
		if(cancelled) {
			std::list<Function<void()>> notifyItems;
			notifyItems.swap(this->notifyItems);
			lock.unlock();
			for(auto& notifyItem : notifyItems) {
				notifyItem();
			}
			if(options.deleteAfterRunning) {
				delete this;
			}
			return;
		}
		if(options.deleteAfterRunning && ranOnce) {
			FGL_WARN("DispatchWorkItem with option deleteAfterRunning is being run more than once");
		}
		ranOnce = true;
		lock.unlock();
		
		work();
		
		std::list<Function<void()>> notifyItems;
		lock.lock();
		notifyItems.swap(this->notifyItems);
		lock.unlock();
		for(auto& notifyItem : notifyItems) {
			notifyItem();
		}
		
		if(options.deleteAfterRunning) {
			delete this;
		}
	}
	
	void DispatchWorkItem::notify(DispatchQueue* queue, Function<void()> work) {
		notify(queue, new DispatchWorkItem({ .deleteAfterRunning=true }, work));
	}
	
	void DispatchWorkItem::notify(DispatchQueue* queue, DispatchWorkItem* workItem) {
		mutex.lock();
		if(cancelled) {
			mutex.unlock();
			return;
		}
		notifyItems.push_back([=]() {
			queue->async(workItem);
		});
		mutex.unlock();
	}
	
	void DispatchWorkItem::wait() {
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
		cv.wait(lock, [&]() {
			return finished;
		});
	}
	
	void DispatchWorkItem::cancel() {
		mutex.lock();
		cancelled = true;
		mutex.unlock();
	}
	
	bool DispatchWorkItem::isCancelled() const {
		return cancelled;
	}
}
