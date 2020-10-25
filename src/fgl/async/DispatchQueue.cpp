//
//  DispatchQueue.cpp
//  AsyncCpp
//
//  Created by Luis Finke on 8/11/19.
//  Copyright © 2019 Luis Finke. All rights reserved.
//


#ifdef __ANDROID__
#include <jni.h>
#include <fgl/async/JNIAsyncCpp.hpp>
#endif
#include <fgl/async/DispatchQueue.hpp>
#include <mutex>
#ifdef __APPLE__
#include <dispatch/dispatch.h>
#include <fgl/async/ObjCCallStack.h>
#endif

namespace fgl {
	std::mutex DispatchQueue_mainQueueMutex;
	DispatchQueue* DispatchQueue::mainQueue = nullptr;
	bool DispatchQueue_mainQueueRunning = false;
	#ifdef FGL_DISPATCH_USES_MAIN
		bool DispatchQueue_mainQueueEnabled = true;
	#else
		bool DispatchQueue_mainQueueEnabled = false;
	#endif
	thread_local DispatchQueue* localDispatchQueue = nullptr;



	#if defined(__APPLE__)
	struct _DispatchQueueNativeData {
		dispatch_queue_t queue;
	};
	#elif defined(__ANDROID__)
	struct _DispatchQueueNativeData {
		JavaVM* vm;
		jobject handler;
		jclass nativeRunnableClass;
		jmethodID NativeRunnable_init;
		jmethodID Handler_getLooper;
		jmethodID Handler_post;
		jmethodID Handler_postDelayed;
		jmethodID Looper_getThread;
		jmethodID Thread_getName;

		jobject newNativeRunnable(JNIEnv* env, std::function<void(JNIEnv*,std::vector<jobject>)> func) {
			auto nativeFunc = new std::function<void(JNIEnv*,std::vector<jobject>)>(func);
			return env->NewObject(nativeRunnableClass, NativeRunnable_init, (jlong)nativeFunc);
		}
	};
	#else
	struct _DispatchQueueNativeData {
		//
	};
	#endif



	#ifdef JNIEXPORT
	void DispatchQueue::jniScope(JavaVM* vm, Function<void(JNIEnv*)> work) {
		if(vm == nullptr) {
			throw std::runtime_error("given VM is null");
		}
		JNIEnv* env = nullptr;
		bool attachedToThread = false;
		auto envResult = vm->GetEnv((void**)&env, JNI_VERSION_1_6);
		if (envResult == JNI_EDETACHED) {
			if (vm->AttachCurrentThread(&env, NULL) == JNI_OK) {
				attachedToThread = true;
			} else {
				throw std::runtime_error("Failed to attach to thread");
			}
		} else if (envResult == JNI_EVERSION) {
			throw std::runtime_error("Unsupported JNI version");
		}
		work(env);
		if(attachedToThread) {
			vm->DetachCurrentThread();
		}
	}
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
		jclass handlerClass = env->FindClass("android/os/Handler");
		jclass looperClass = env->FindClass("android/os/Looper");
		jclass threadClass = env->FindClass("java/lang/Thread");
		jclass nativeRunnableClass = env->FindClass("com/lufinkey/libasynccpp/NativeRunnable");
		nativeRunnableClass = (jclass)env->NewGlobalRef(nativeRunnableClass);
		jmethodID handlerInit = env->GetMethodID(handlerClass, "<init>", "(Landroid/os/Looper;)V");
		jobject handler = env->NewObject(handlerClass, handlerInit, looper);
		handler = env->NewGlobalRef(handler);
		data = new NativeData{
			.vm=vm,
			.handler=handler,
			.nativeRunnableClass=nativeRunnableClass,
			.NativeRunnable_init=env->GetMethodID(nativeRunnableClass, "<init>", "(J)V"),
			.Handler_getLooper=env->GetMethodID(handlerClass, "getLooper", "()Landroid/os/Looper;"),
			.Handler_post=env->GetMethodID(handlerClass, "post", "(Ljava/lang/Runnable;)Z"),
			.Handler_postDelayed=env->GetMethodID(handlerClass, "postDelayed", "(Ljava/lang/Runnable;J)Z"),
			.Looper_getThread=env->GetMethodID(looperClass, "getThread", "()Ljava/lang/Thread;"),
			.Thread_getName=env->GetMethodID(threadClass, "getName", "()Ljava/lang/String;")
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
					env->DeleteGlobalRef(nativeData->nativeRunnableClass);
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
				jobject looper = env->CallObjectMethod(nativeData.handler, nativeData.Handler_getLooper);
				jobject thread = env->CallObjectMethod(looper, nativeData.Looper_getThread);
				jobject name = env->CallObjectMethod(thread, nativeData.Thread_getName);
				label = String(env, (jstring)name);
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
				jobject runnable = nativeData->newNativeRunnable(env, [=](auto env, auto args) {
					workItem->perform();
				});
				jboolean success = env->CallBooleanMethod(nativeData->handler, nativeData->Handler_post, runnable);
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
				jobject runnable = nativeData->newNativeRunnable(env, [=](auto env, auto args) {
					workItem->perform();
				});
				jboolean success = env->CallBooleanMethod(nativeData->handler, nativeData->Handler_postDelayed, runnable, (jlong)milliseconds);
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
				jobject runnable = nativeData->newNativeRunnable(env, [&](auto env, auto args) {
					workItem->perform();
					finished = true;
					cv.notify_one();
				});
				jboolean success = env->CallBooleanMethod(nativeData->handler, nativeData->Handler_post, runnable);
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
	
	
	
	void DispatchQueue::dispatchMain() {
		FGL_ASSERT(usesMainQueue(), "enableMainQueue() must be called in order to use this function");
		FGL_ASSERT(!DispatchQueue_mainQueueRunning, "main DispatchQueue has already been dispatched");
		DispatchQueue_mainQueueRunning = true;
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
		if(mainQueue == nullptr && usesMainQueue()) {
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
				JavaVM* vm = getAsyncCppJavaVM();
				if(vm == nullptr) {
					throw std::runtime_error("Java VM not found");
				}
				DispatchQueue::jniScope(vm, [&](auto env) {
					jclass looperClass = env->FindClass("android/os/Looper");
					jmethodID Looper_getMainLooper = env->GetStaticMethodID(looperClass, "getMainLooper", "()Landroid/os/Looper;");
					jobject looper = env->CallStaticObjectMethod(looperClass, Looper_getMainLooper);
					mainQueue = new DispatchQueue(env, looper);
				});
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

	bool DispatchQueue::usesMainQueue() {
		return DispatchQueue_mainQueueEnabled;
	}

	bool DispatchQueue::enableMainQueue() {
		DispatchQueue_mainQueueEnabled = true;
		return true;
	}
	
	DispatchQueue* DispatchQueue::local() {
		return localDispatchQueue;
	}
}
