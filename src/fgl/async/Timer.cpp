//
//  Timer.cpp
//  AsyncCpp
//
//  Created by Luis Finke on 9/27/19.
//  Copyright © 2019 Luis Finke. All rights reserved.
//

#include <fgl/async/Timer.hpp>

namespace fgl {
	Timer::~Timer() {
		if(waiter != nullptr) {
			delete waiter;
		}
	}
	
	void Timer::invoke() {
		auto self = shared_from_this();
		if(!rescheduleWaiter) {
			std::unique_lock<std::recursive_mutex> lock(mutex);
			valid = false;
			waiter->cancel();
			lock.unlock();
		}
		work(self);
	}
	
	void Timer::cancel() {
		std::unique_lock<std::recursive_mutex> lock(mutex);
		valid = false;
		waiter->cancel();
	}
	
	bool Timer::isValid() const {
		return valid;
	}
	
	void Timer::run() {
		auto self = shared_from_this();
		std::thread([=]() {
			std::unique_lock<std::recursive_mutex> lock(mutex);
			bool shouldInvalidate = false;
			while(valid && !shouldInvalidate) {
				lock.unlock();
				waiter->wait();
				lock.lock();
				if(valid) {
					if(!rescheduleWaiter) {
						shouldInvalidate = true;
						waiter->cancel();
					}
					if(work) {
						if(queue != nullptr) {
							queue->async([=]() {
								std::unique_lock<std::recursive_mutex> lock(mutex);
								if(!valid) {
									return;
								} else if(shouldInvalidate) {
									valid = false;
								}
								work(self);
							});
						} else {
							if(shouldInvalidate) {
								valid = false;
							}
							work(self);
						}
					} else if(shouldInvalidate) {
						valid = false;
					}
				}
				if(valid && !shouldInvalidate && rescheduleWaiter) {
					rescheduleWaiter();
				}
			}
		}).detach();
	}
}
