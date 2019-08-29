//
//  Promise.cpp
//  AsyncCpp
//
//  Created by Luis Finke on 8/11/19.
//  Copyright © 2019 Luis Finke. All rights reserved.
//

#include <fgl/async/Promise.hpp>

namespace fgl {
	static DispatchQueue* defaultPromiseQueue = nullptr;
	
	DispatchQueue* getDefaultPromiseQueue() {
		if(defaultPromiseQueue == nullptr) {
			if(DispatchQueue::usesMainQueue()) {
				defaultPromiseQueue = DispatchQueue::getMain();
			}
			else {
				defaultPromiseQueue = new DispatchQueue("Promise Main");
			}
		}
		return defaultPromiseQueue;
	}
}
