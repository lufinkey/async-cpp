//
//  Generator.cpp
//  AsyncCpp
//
//  Created by Luis Finke on 11/17/19.
//  Copyright © 2019 Luis Finke. All rights reserved.
//

#include <fgl/async/Generator.hpp>

namespace fgl {
	_initialGenNext::_initialGenNext(std::nullptr_t) {}
	_initialGenNext initialGenNext() {
		return _initialGenNext(nullptr);
	}

	_setGenResumeQueue::_setGenResumeQueue(DispatchQueue* queue, bool enterQueue)
		: queue(queue), enterQueue(enterQueue) {}
	_setGenResumeQueue setGenResumeQueue(DispatchQueue* queue, bool enterQueue) {
		return _setGenResumeQueue(queue, enterQueue);
	}
}
