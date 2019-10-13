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
		auto strongSelf = self.lock();
		if(!rescheduleWaiter) {
			std::unique_lock<std::recursive_mutex> lock(mutex);
			valid = false;
			waiter->cancel();
			lock.unlock();
		}
		work(strongSelf);
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
		auto strongSelf = self.lock();
		std::thread([=]() {
			auto self = strongSelf;
			std::unique_lock<std::recursive_mutex> lock(mutex);
			while(valid) {
				lock.unlock();
				waiter->wait();
				lock.lock();
				if(valid) {
					if(!rescheduleWaiter) {
						valid = false;
						waiter->cancel();
					}
					lock.unlock();
					if(work) {
						work(self);
					}
					lock.lock();
				}
				if(valid && rescheduleWaiter) {
					rescheduleWaiter();
				}
			}
		}).detach();
	}
}
