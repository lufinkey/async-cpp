//
//  AsyncQueue.mm
//  AsyncCpp
//
//  Created by Luis Finke on 6/26/20.
//  Copyright © 2020 Luis Finke. All rights reserved.
//

#include "AsyncQueue.hpp"

#ifdef __OBJC__

namespace fgl {
	class AsyncQueueTaskObjcEventListener: public AsyncQueue::Task::EventListener {
	public:
		AsyncQueueTaskObjcEventListener(id<FGLAsyncQueueTaskEventListener> listener);
		
		virtual void onAsyncQueueTaskBegin(std::shared_ptr<AsyncQueue::Task> task) override;
		virtual void onAsyncQueueTaskCancel(std::shared_ptr<AsyncQueue::Task> task) override;
		virtual void onAsyncQueueTaskStatusChange(std::shared_ptr<AsyncQueue::Task> task) override;
		virtual void onAsyncQueueTaskError(std::shared_ptr<AsyncQueue::Task> task, std::exception_ptr error) override;
		virtual void onAsyncQueueTaskEnd(std::shared_ptr<AsyncQueue::Task> task) override;
		
		__weak id<FGLAsyncQueueTaskEventListener> weakListener;
	};
	
	AsyncQueueTaskObjcEventListener::AsyncQueueTaskObjcEventListener(id<FGLAsyncQueueTaskEventListener> listener)
	: weakListener(listener) {
		//
	}
	
	void AsyncQueueTaskObjcEventListener::onAsyncQueueTaskBegin(std::shared_ptr<AsyncQueue::Task> task) {
		__strong id<FGLAsyncQueueTaskEventListener> listener = weakListener;
		if(listener == nil) {
			task->removeEventListener(this);
			delete this;
			return;
		}
		[listener asyncQueueTaskWillBegin:task];
	}
	
	void AsyncQueueTaskObjcEventListener::onAsyncQueueTaskCancel(std::shared_ptr<AsyncQueue::Task> task) {
		__strong id<FGLAsyncQueueTaskEventListener> listener = weakListener;
		if(listener == nil) {
			task->removeEventListener(this);
			delete this;
			return;
		}
		[listener asyncQueueTaskDidCancel:task];
	}
	
	void AsyncQueueTaskObjcEventListener::onAsyncQueueTaskStatusChange(std::shared_ptr<AsyncQueue::Task> task) {
		__strong id<FGLAsyncQueueTaskEventListener> listener = weakListener;
		if(listener == nil) {
			task->removeEventListener(this);
			delete this;
			return;
		}
		[listener asyncQueueTaskDidChangeStatus:task];
	}
	
	void AsyncQueueTaskObjcEventListener::onAsyncQueueTaskError(std::shared_ptr<AsyncQueue::Task> task, std::exception_ptr error) {
		__strong id<FGLAsyncQueueTaskEventListener> listener = weakListener;
		if(listener == nil) {
			task->removeEventListener(this);
			delete this;
			return;
		}
		[listener asyncQueueTask:task didThrowError:error];
	}
	
	void AsyncQueueTaskObjcEventListener::onAsyncQueueTaskEnd(std::shared_ptr<AsyncQueue::Task> task) {
		__strong id<FGLAsyncQueueTaskEventListener> listener = weakListener;
		if(listener == nil) {
			task->removeEventListener(this);
			delete this;
			return;
		}
		[listener asyncQueueTaskDidEnd:task];
	}
	
	void AsyncQueue::Task::addEventListener(id<FGLAsyncQueueTaskEventListener> listener) {
		FGL_ASSERT(listener != nullptr, "listener cannot be null");
		std::unique_lock<std::recursive_mutex> lock(mutex);
		eventListeners.pushBack(new AsyncQueueTaskObjcEventListener(listener));
	}
	
	void AsyncQueue::Task::removeEventListener(id<FGLAsyncQueueTaskEventListener> listener) {
		FGL_ASSERT(listener != nullptr, "listener cannot be null");
		std::unique_lock<std::recursive_mutex> lock(mutex);
		auto it = eventListeners.findLastWhere([=](auto& cppListener) {
			__strong id<FGLAsyncQueueTaskEventListener> cmpListener = cppListener->weakListener;
			return (listener == cmpListener);
		});
		if(it != eventListeners.end()) {
			auto cppListener = *it;
			eventListeners.erase(it);
			delete cppListener;
		}
	}
}

#endif
