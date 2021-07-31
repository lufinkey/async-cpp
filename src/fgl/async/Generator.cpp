//
//  Generator.cpp
//  AsyncCpp
//
//  Created by Luis Finke on 11/17/19.
//  Copyright © 2019 Luis Finke. All rights reserved.
//

#include <fgl/async/Generator.hpp>

namespace fgl {
	initialGenNext::initialGenNext() {}

	setGenResumeQueue::setGenResumeQueue(DispatchQueue* queue, bool enterQueue)
		: queue(queue), enterQueue(enterQueue) {}
}
