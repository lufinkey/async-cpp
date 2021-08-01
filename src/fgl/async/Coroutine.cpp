//
//  Coroutine.cpp
//  AsyncCpp
//
//  Created by Luis Finke on 7/31/21.
//  Copyright © 2021 Luis Finke. All rights reserved.
//

#include <fgl/async/Coroutine.hpp>

namespace fgl {
	resumeOnQueue::resumeOnQueue(DispatchQueue* queue, bool alwaysDispatch): queue(queue), alwaysDispatch(alwaysDispatch) {
		FGL_ASSERT(queue != nullptr, "queue must not be null");
	}
	bool resumeOnQueue::await_ready() { return !alwaysDispatch && queue->isLocal(); }
	void resumeOnQueue::await_suspend(coroutine_handle<> handle) {
		queue->async([=]() {
			auto h = handle;
			h.resume();
		});
	}
	void resumeOnQueue::await_resume() {}


	resumeOnNewThread::resumeOnNewThread() {}
	bool resumeOnNewThread::await_ready() { return false; }
	void resumeOnNewThread::await_suspend(coroutine_handle<> handle) {
		std::thread([=]() {
			auto h = handle;
			h.resume();
		}).detach();
	}
	void resumeOnNewThread::await_resume() {}
}
