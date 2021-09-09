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

namespace fgl {
	struct ObjCCallStack;
	SharedPtr<ObjCCallStack> getObjCCallStack();
	void printObjCCallStack(SharedPtr<ObjCCallStack>);
}

#endif
