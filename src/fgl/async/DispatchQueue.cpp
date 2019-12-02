//
//  DispatchQueue.cpp
//  AsyncCpp
//
//  Created by Luis Finke on 8/11/19.
//  Copyright © 2019 Luis Finke. All rights reserved.
//

#include <fgl/async/DispatchQueue.hpp>
#ifdef __APPLE__
#include <dispatch/dispatch.h>
#endif

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
	: data(new Data{
		.label=label,
		.options=options,
		.type=type,
		.killed=false,
		.stopped=true
	}) {
		//
	}

	#ifdef __APPLE__
	DispatchQueue::DispatchQueue(dispatch_queue_t queue)
	: data(new NativeData{
		.queue=queue
	}){
		//
	}
	#endif
	
	DispatchQueue::~DispatchQueue() {
		if(this->data.index() == 0) {
			auto data = std::get<Data*>(this->data);
			FGL_ASSERT((data->itemQueue.size() == 0 || data->scheduledItemQueue.size() == 0), "Trying to destroy DispatchQueue \""+data->label+"\" while unfinished items remain");
			data->killed = true;
			data->queueWaitCondition.notify_one();
			if(data->thread.joinable()) {
				data->thread.join();
			}
			delete data;
		} else {
			auto nativeData = std::get<NativeData*>(this->data);
			delete nativeData;
		}
	}



	String DispatchQueue::getLabel() const {
		if(this->data.index() == 0) {
			auto& data = *std::get<Data*>(this->data);
			return data.label;
		} else {
		#ifdef __APPLE__
			auto& nativeData = *std::get<NativeData*>(this->data);
			return dispatch_queue_get_label(nativeData.queue);
		#else
			throw std::logic_error("not implemented");
		#endif
		}
	}
	
	
	
	void DispatchQueue::notify() {
		if(this->data.index() == 0) {
			auto& data = *std::get<Data*>(this->data);
			if(data.stopped && data.type != Type::LOCAL) {
				if(data.thread.joinable()) {
					data.thread.join();
				}
				data.thread = std::thread([=]() {
					this->run();
				});
			}
			else {
				data.queueWaitCondition.notify_one();
			}
		} else {
			throw std::logic_error("cannot call DispatchQueue::notify on a native queue");
		}
	}
	
	void DispatchQueue::run() {
		if(this->data.index() == 0) {
			localDispatchQueue = this;
			auto& data = *std::get<Data*>(this->data);
			data.stopped = false;
			while(!data.killed) {
				std::unique_lock<std::mutex> lock(data.mutex);
				
				// get next work item
				DispatchWorkItem* workItem = nullptr;
				Function<void()> onFinishItem = nullptr;
				if(data.scheduledItemQueue.size() > 0) {
					auto& item = data.scheduledItemQueue.front();
					if(item.timeUntil().count() <= 0) {
						workItem = item.workItem;
						data.scheduledItemQueue.pop_front();
					}
				}
				if(workItem == nullptr && data.itemQueue.size() > 0) {
					auto& item = data.itemQueue.front();
					workItem = item.workItem;
					onFinishItem = item.onFinish;
					data.itemQueue.pop_front();
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
					if(data.scheduledItemQueue.size() > 0) {
						// wait for next scheduled item
						auto& nextItem = data.scheduledItemQueue.front();
						lock.unlock();
						nextItem.wait(data.queueWaitCondition, [&]() {
							return shouldWake();
						});
					}
					else if(data.options.keepThreadAlive) {
						// wait for any item
						std::mutex waitMutex;
						std::unique_lock<std::mutex> waitLock(waitMutex);
						lock.unlock();
						data.queueWaitCondition.wait(waitLock, [&]() { return shouldWake(); });
					}
					else {
						// no more items, so stop the thread for now
						data.stopped = true;
						lock.unlock();
						break;
					}
				}
			}
			localDispatchQueue = nullptr;
		} else {
			throw std::logic_error("cannot call DispatchQueue::run on a native queue");
		}
	}
	
	bool DispatchQueue::shouldWake() const {
		if(this->data.index() == 0) {
			auto& data = *std::get<Data*>(this->data);
			if(data.killed) {
				return true;
			}
			if(data.itemQueue.size() > 0) {
				return true;
			}
			if(data.scheduledItemQueue.size() > 0) {
				auto& item = data.scheduledItemQueue.front();
				if(item.timeUntil().count() <= 0) {
					return true;
				}
			}
			return false;
		} else {
			throw std::logic_error("cannot call DispatchQueue::shouldWake on a native queue");
		}
	}
	
	
	
	void DispatchQueue::async(Function<void()> work) {
		async(new DispatchWorkItem({ .deleteAfterRunning=true }, work));
	}
	
	void DispatchQueue::async(DispatchWorkItem* workItem) {
		if(this->data.index() == 0) {
			auto& data = *std::get<Data*>(this->data);
			std::unique_lock<std::mutex> lock(data.mutex);
			data.itemQueue.push_back({ .workItem=workItem });
			notify();
			lock.unlock();
		} else {
		#ifdef __APPLE
			auto& nativeData = *std::get<NativeData*>(this->data);
			dispatch_async(nativeData.queue, ^{
				workItem->perform();
			});
		#else
			throw std::logic_error("not implemented");
		#endif
		}
	}

	void DispatchQueue::asyncAfter(Clock::time_point deadline, DispatchWorkItem* workItem) {
		if(this->data.index() == 0) {
			auto& data = *std::get<Data*>(this->data);
			std::unique_lock<std::mutex> lock(data.mutex);
			
			auto scheduledItem = ScheduledQueueItem{
				.workItem=workItem,
				.deadline=deadline
			};
			bool inserted = false;
			for(auto it=data.scheduledItemQueue.begin(), end=data.scheduledItemQueue.end(); it!=end; it++) {
				auto& item = *it;
				if(scheduledItem.timeUntil() <= item.timeUntil()) {
					data.scheduledItemQueue.insert(it, scheduledItem);
					inserted = true;
					break;
				}
			}
			if(!inserted) {
				data.scheduledItemQueue.push_back(scheduledItem);
			}
			notify();
			lock.unlock();
		} else {
		#ifdef __APPLE
			auto& nativeData = *std::get<NativeData*>(this->data);
			auto nanoseconds = std::chrono::nanoseconds(deadline - Clock::now()).count();
			if(nanoseconds < 0) {
				nanoseconds = 0;
			}
			dispatch_after(dispatch_time(DISPATCH_TIME_NOW, nanoseconds), nativeData.queue, ^{
				workItem->perform();
			});
		#else
			throw std::logic_error("not implemented");
		#endif
		}
	}
	
	void DispatchQueue::sync(Function<void()> work) {
		sync(new DispatchWorkItem({ .deleteAfterRunning=true }, work));
	}
	
	void DispatchQueue::sync(DispatchWorkItem* workItem) {
		if(this->data.index() == 0) {
			auto& data = *std::get<Data*>(this->data);
			std::condition_variable cv;
			std::mutex waitMutex;
			std::unique_lock<std::mutex> waitLock(waitMutex);
			bool finished = false;
			
			std::unique_lock<std::mutex> lock(data.mutex);
			data.itemQueue.push_back({ .workItem=workItem, .onFinish=[&]() {
				finished = true;
				cv.notify_one();
			} });
			notify();
			lock.unlock();
			
			cv.wait(waitLock, [&]() {
				return finished;
			});
		} else {
		#ifdef __APPLE
			auto& nativeData = *std::get<NativeData*>(this->data);
			dispatch_sync(nativeData.queue, ^{
				workItem->perform();
			});
		#else
			throw std::logic_error("not implemented");
		#endif
		}
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
		#ifdef __APPLE__
			dispatch_main();
		#else
			mainQueue->run();
			exit(0);
		#endif
	}
	
	DispatchQueue* DispatchQueue::getMain() {
		if(mainQueue == nullptr && usesMainQueue()) {
			#ifdef __APPLE__
				mainQueue = new DispatchQueue(dispatch_get_main_queue());
				mainQueue->async([]() {
					localDispatchQueue = mainQueue;
				});
			#else
				mainQueue = new DispatchQueue(Type::LOCAL, "Main", {
					.keepThreadAlive=true
				});
			#endif
		}
		return mainQueue;
	}
	
	DispatchQueue* DispatchQueue::getLocal() {
		return localDispatchQueue;
	}
}
