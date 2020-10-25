//
//  ObjCCallStack.h
//  AsyncCpp
//
//  Created by Luis Finke on 10/25/20.
//  Copyright © 2020 Luis Finke. All rights reserved.
//

#include <fgl/async/Common.hpp>
#include <memory>

#ifdef __APPLE__

namespace sh {
	struct ObjCCallStack;
	std::shared_ptr<ObjCCallStack> getObjCCallStack();
	void printObjCCallStack(std::shared_ptr<ObjCCallStack>);
}

#endif
