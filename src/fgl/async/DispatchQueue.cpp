//
//  DispatchQueue.cpp
//  AsyncCpp
//
//  Created by Luis Finke on 8/11/19.
//  Copyright © 2019 Luis Finke. All rights reserved.
//


#ifdef __ANDROID__
#include <jni.h>
#endif
#include <fgl/async/DispatchQueue.hpp>
#include <mutex>

#ifdef __ANDROID__
	#include <fgl/async/JNIAsyncCpp.hpp>
#endif
#ifdef __APPLE__
#include <dispatch/dispatch.h>
#include <fgl/async/ObjCCallStack.h>
#endif

namespace fgl {
	std::mutex DispatchQueue_mainQueueMutex;
	DispatchQueue* DispatchQueue::mainQueue = nullptr;
	bool DispatchQueue_mainQueueRunning = false;
	thread_local DispatchQueue* localDispatchQueue = nullptr;



	#if defined(__APPLE__)
	struct _DispatchQueueNativeData {
		dispatch_queue_t queue;
	};
	#elif defined(__ANDROID__)
	struct _DispatchQueueNativeData {
		JavaVM* vm;
		jobject handler;
	};
	#else
	struct _DispatchQueueNativeData {
		//
	};
	#endif



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
		dispatch_retain(queue);
	}
	#endif

	#ifdef __ANDROID__
	DispatchQueue::DispatchQueue(JNIEnv* env, jobject looper)
	: data((NativeData*)nullptr) {
		JavaVM* vm = nullptr;
		env->GetJavaVM(&vm);
		if(vm == nullptr) {
			throw std::runtime_error("Unable to get JavaVM");
		}
		jobject handler = jni::android::Handler::newObject(env, {.looper = looper});
		handler = env->NewGlobalRef(handler);
		data = new NativeData{
			.vm=vm,
			.handler=handler
		};
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
			#if defined(__APPLE__)
				dispatch_release(nativeData->queue);
			#elif defined(__ANDROID__)
				jniScope(nativeData->vm, [=](auto env) {
					env->DeleteGlobalRef(nativeData->handler);
				});
			#endif
			delete nativeData;
		}
	}



	String DispatchQueue::getLabel() const {
		if(this->data.index() == 0) {
			auto& data = *std::get<Data*>(this->data);
			return data.label;
		} else {
		#if defined(__APPLE__)
			auto& nativeData = *std::get<NativeData*>(this->data);
			return dispatch_queue_get_label(nativeData.queue);
		#elif defined(__ANDROID__)
			auto& nativeData = *std::get<NativeData*>(this->data);
			String label;
			jniScope(nativeData.vm, [&](auto env) {
				jobject looper = jni::android::Handler::getLooper(env, nativeData.handler);
				jobject thread = jni::android::Looper::getThread(env, looper);
				jstring name = jni::Thread::getName(env, thread);
				label = String(env, name);
			});
			return label;
		#else
			throw std::logic_error("not implemented");
		#endif
		}
	}

	bool DispatchQueue::isLocal() const {
		return DispatchQueue::local() == this;
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
			} else {
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
			auto nativeData = std::get<NativeData*>(this->data);
		#if defined(__APPLE__)
			#ifdef DISPATCH_PRESERVE_CALL_STACK
				auto callStack = getObjCCallStack();
			#endif
			dispatch_async(nativeData->queue, ^{
				#ifdef DISPATCH_PRESERVE_CALL_STACK
					if(false) { callStack; } // ensure callStack is preserved in memory
				#endif
				workItem->perform();
			});
		#elif defined(__ANDROID__)
			jniScope(nativeData->vm, [=](auto env) {
				jobject runnable = jni::NativeRunnable::newObject(env, [=](auto env, auto args) {
					workItem->perform();
				});
				jboolean success = jni::android::Handler::post(env, nativeData->handler, runnable);
				if(!success) {
					throw std::runtime_error("unable to add item to DispatchQueue: Handler.post failed");
				}
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
			auto nativeData = std::get<NativeData*>(this->data);
		#if defined(__APPLE__)
			auto nanoseconds = std::chrono::nanoseconds(deadline - Clock::now()).count();
			if(nanoseconds < 0) {
				nanoseconds = 0;
			}
			#ifdef DISPATCH_PRESERVE_CALL_STACK
				auto callStack = getObjCCallStack();
			#endif
			dispatch_after(dispatch_time(DISPATCH_TIME_NOW, nanoseconds), nativeData->queue, ^{
				#ifdef DISPATCH_PRESERVE_CALL_STACK
					if(false) { callStack; } // ensure callStack is preserved in memory
				#endif
				workItem->perform();
			});
		#elif defined(__ANDROID__)
			auto milliseconds = std::chrono::duration_cast<std::chrono::milliseconds>(deadline - Clock::now()).count();
			if(milliseconds < 0) {
				milliseconds = 0;
			}
			jniScope(nativeData->vm, [=](auto env) {
				jobject runnable = jni::NativeRunnable::newObject(env, [=](auto env, auto args) {
					workItem->perform();
				});
				jboolean success = jni::android::Handler::postDelayed(env, nativeData->handler, runnable, (jlong)milliseconds);
				if(!success) {
					throw std::runtime_error("unable to add item to DispatchQueue: Handler.postDelayed failed");
				}
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
			auto nativeData = std::get<NativeData*>(this->data);
		#if defined(__APPLE__)
			dispatch_sync(nativeData->queue, ^{
				workItem->perform();
			});
		#elif defined(__ANDROID__)
			std::condition_variable cv;
			std::mutex waitMutex;
			std::unique_lock<std::mutex> waitLock(waitMutex);
			bool finished = false;

			jniScope(nativeData->vm, [&](auto env) {
				jobject runnable = jni::NativeRunnable::newObject(env, [&](auto env, auto args) {
					workItem->perform();
					finished = true;
					cv.notify_one();
				});
				jboolean success = jni::android::Handler::post(env, nativeData->handler, runnable);
				if(!success) {
					throw std::runtime_error("unable to add item to DispatchQueue: Handler.post failed");
				}
			});

			cv.wait(waitLock, [&]() {
				return finished;
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



	bool DispatchQueue::mainQueueEnabled() {
		#if defined(__APPLE__)
			return true;
		#elif defined(__ANDROID__)
			return true;
		#else
			if(mainQueue != nullptr) {
				return true;
			}
			#ifdef FGL_DISABLE_AUTOMATIC_LOCAL_MAIN_QUEUE
				return false;
			#else
				return true
			#endif
		#endif
	}

	void DispatchQueue::instantiateLocalMainQueue() {
		FGL_ASSERT(mainQueue == nullptr, "Cannot call DispatchQueue::instantiateLocalMainQueue more than once")
		mainQueue = new DispatchQueue(DispatchQueue::Type::LOCAL, "Main", {
			.keepThreadAlive=true
		});
	}

	void DispatchQueue::allowLocalMainQueue() {
		if(mainQueue != nullptr) {
			return;
		}
		#if defined(__APPLE__) || defined(__ANDROID__)
			FGL_ASSERT(DispatchQueue::main() != nullptr, "platform-specific main queue could not be instantiated or found")
		#else
			instantiateLocalMainQueue();
		#endif
	}

	void DispatchQueue::dispatchMain() {
		FGL_ASSERT(!DispatchQueue_mainQueueRunning, "main DispatchQueue has already been dispatched");
		DispatchQueue_mainQueueRunning = true;
		// generate main queue
		DispatchQueue::main();
		#if defined(__APPLE__)
			dispatch_main();
		#elif defined(__ANDROID__)
			throw std::logic_error("cannot call DispatchQueue::dispatchMain on Android");
		#else
			mainQueue->run();
			exit(0);
		#endif
	}
	
	DispatchQueue* DispatchQueue::main() {
		if(mainQueue == nullptr) {
			std::unique_lock<std::mutex> lock(DispatchQueue_mainQueueMutex);
			if(mainQueue != nullptr) {
				return mainQueue;
			}
			#if defined(__APPLE__)
				mainQueue = new DispatchQueue(dispatch_get_main_queue());
				mainQueue->async([]() {
					localDispatchQueue = mainQueue;
				});
			#elif defined(__ANDROID__)
				JavaVM* vm = getJavaVM();
				if(vm == nullptr) {
					throw std::runtime_error("Java VM not found");
				}
				jniScope(vm, [&](auto env) {
					mainQueue = new DispatchQueue(env, jni::android::Looper::getMainLooper(env));
				});
				mainQueue->async([]() {
					localDispatchQueue = mainQueue;
				});
			#else
				#ifndef FGL_DISABLE_AUTOMATIC_LOCAL_MAIN_QUEUE
					instantiateLocalMainQueue();
				#else
					throw std::logic_error("FGL_DISABLE_AUTOMATIC_LOCAL_MAIN_QUEUE is set, so main queue is disabled. Try using defaultPromiseQueue for this logic");
				#endif
			#endif
		}
		return mainQueue;
	}
	
	DispatchQueue* DispatchQueue::local() {
		return localDispatchQueue;
	}
}
