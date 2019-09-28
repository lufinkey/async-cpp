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
			valid = false;
			waiter->cancel();
		}
		work(strongSelf);
	}
	
	void Timer::cancel() {
		valid = false;
		waiter->cancel();
	}
	
	bool Timer::isValid() const {
		return valid;
	}
	
	void Timer::run() {
		auto strongSelf = self.lock();
		thread = std::thread([=]() {
			auto self = strongSelf;
			while(valid) {
				waiter->wait();
				if(valid) {
					if(!rescheduleWaiter) {
						valid = false;
						waiter->cancel();
					}
					work(self);
				}
				if(valid && rescheduleWaiter) {
					rescheduleWaiter();
				}
			}
		});
	}
}
