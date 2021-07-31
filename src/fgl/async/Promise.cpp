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
}
