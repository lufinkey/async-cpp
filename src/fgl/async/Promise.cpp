//
//  Promise.cpp
//  AsyncCpp
//
//  Created by Luis Finke on 8/11/19.
//  Copyright © 2019 Luis Finke. All rights reserved.
//

#include <fgl/async/Promise.hpp>

namespace fgl {
	DispatchQueue* defaultPromiseQueue() {
		if(DispatchQueue::mainQueueEnabled()) {
			return DispatchQueue::main();
		} else {
			return backgroundPromiseQueue();
		}
	}

	static DispatchQueue* _backgroundPromiseQueue = nullptr;
	DispatchQueue* backgroundPromiseQueue() {
		if(_backgroundPromiseQueue == nullptr) {
			_backgroundPromiseQueue = new DispatchQueue("Promise Main");
		}
		return _backgroundPromiseQueue;
	}


	resumeOnQueue::resumeOnQueue(DispatchQueue* queue) {
		FGL_ASSERT(queue != nullptr, "queue must not be null");
		this->queue = queue;
	}
	bool resumeOnQueue::await_ready() { return false; }
	void resumeOnQueue::await_suspend(coroutine_handle<> handle) {
		queue->async([=]() {
			auto h = handle;
			h.resume();
		});
	}
	void resumeOnQueue::await_resume() {}

	bool resumeOnNewThread::await_ready() { return false; }
	void resumeOnNewThread::await_suspend(coroutine_handle<> handle) {
		std::thread([=]() {
			auto h = handle;
			h.resume();
		}).detach();
	}
	void resumeOnNewThread::await_resume() {}
}
