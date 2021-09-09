//
//  ObjCCallStack.mm
//  AsyncCpp
//
//  Created by Luis Finke on 10/25/20.
//  Copyright © 2020 Luis Finke. All rights reserved.
//

#import "ObjCCallStack.h"
#include <memory>

#ifdef __APPLE__
#import <Foundation/Foundation.h>

namespace fgl {
	struct ObjCCallStack {
		NSArray<NSString*>* callStack;
	};
	
	SharedPtr<ObjCCallStack> getObjCCallStack() {
		return std::make_shared<ObjCCallStack>(ObjCCallStack{
			.callStack = [NSThread callStackSymbols]
		});
	}
	
	void printObjCCallStack(SharedPtr<ObjCCallStack> ptr) {
		NSLog(@"%@", ptr->callStack);
	}
}

#endif
