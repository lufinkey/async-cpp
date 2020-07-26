//
//  AsyncQueue.mm
//  AsyncCpp
//
//  Created by Luis Finke on 6/26/20.
//  Copyright © 2020 Luis Finke. All rights reserved.
//

#include <fgl/async/AsyncQueue.hpp>

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
		if([listener respondsToSelector:@selector(asyncQueueTaskDidBegin:)]) {
			[listener asyncQueueTaskDidBegin:task];
		}
	}
	
	void AsyncQueueTaskObjcEventListener::onAsyncQueueTaskCancel(std::shared_ptr<AsyncQueue::Task> task) {
		__strong id<FGLAsyncQueueTaskEventListener> listener = weakListener;
		if(listener == nil) {
			task->removeEventListener(this);
			delete this;
			return;
		}
		if([listener respondsToSelector:@selector(asyncQueueTaskDidCancel:)]) {
			[listener asyncQueueTaskDidCancel:task];
		}
	}
	
	void AsyncQueueTaskObjcEventListener::onAsyncQueueTaskStatusChange(std::shared_ptr<AsyncQueue::Task> task) {
		__strong id<FGLAsyncQueueTaskEventListener> listener = weakListener;
		if(listener == nil) {
			task->removeEventListener(this);
			delete this;
			return;
		}
		if([listener respondsToSelector:@selector(asyncQueueTaskDidChangeStatus:)]) {
			[listener asyncQueueTaskDidChangeStatus:task];
		}
	}
	
	void AsyncQueueTaskObjcEventListener::onAsyncQueueTaskError(std::shared_ptr<AsyncQueue::Task> task, std::exception_ptr error) {
		__strong id<FGLAsyncQueueTaskEventListener> listener = weakListener;
		if(listener == nil) {
			task->removeEventListener(this);
			delete this;
			return;
		}
		if([listener respondsToSelector:@selector(asyncQueueTask:didThrowError:)]) {
			[listener asyncQueueTask:task didThrowError:error];
		}
	}
	
	void AsyncQueueTaskObjcEventListener::onAsyncQueueTaskEnd(std::shared_ptr<AsyncQueue::Task> task) {
		__strong id<FGLAsyncQueueTaskEventListener> listener = weakListener;
		if(listener == nil) {
			task->removeEventListener(this);
			delete this;
			return;
		}
		if([listener respondsToSelector:@selector(asyncQueueTaskDidEnd:)]) {
			[listener asyncQueueTaskDidEnd:task];
		}
	}
	
	void AsyncQueue::Task::addEventListener(id<FGLAsyncQueueTaskEventListener> listener) {
		FGL_ASSERT(listener != nil, "listener cannot be nil");
		std::unique_lock<std::recursive_mutex> lock(mutex);
		eventListeners.pushBack(new AsyncQueueTaskObjcEventListener(listener));
	}
	
	void AsyncQueue::Task::removeEventListener(id<FGLAsyncQueueTaskEventListener> listener) {
		FGL_ASSERT(listener != nil, "listener cannot be nil");
		std::unique_lock<std::recursive_mutex> lock(mutex);
		auto it = eventListeners.findLastWhere([=](auto& cmpListener) {
			auto cppListener = dynamic_cast<AsyncQueueTaskObjcEventListener*>(cmpListener);
			if(cppListener == nullptr) {
				return false;
			}
			__strong id<FGLAsyncQueueTaskEventListener> objcListener = cppListener->weakListener;
			if(listener == objcListener) {
				return true;
			}
			return false;
		});
		if(it != eventListeners.end()) {
			auto cppListener = *it;
			eventListeners.erase(it);
			delete cppListener;
		}
	}
}

#endif
