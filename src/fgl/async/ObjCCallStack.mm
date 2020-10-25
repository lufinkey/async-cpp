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

namespace sh {
	struct ObjCCallStack {
		NSArray<NSString*>* callStack;
	};
	
	std::shared_ptr<ObjCCallStack> getObjCCallStack() {
		return std::make_shared<ObjCCallStack>(ObjCCallStack{
			.callStack = [NSThread callStackSymbols]
		});
	}
	
	void printObjCCallStack(std::shared_ptr<ObjCCallStack> ptr) {
		NSLog(@"%@", ptr->callStack);
	}
}

#endif
