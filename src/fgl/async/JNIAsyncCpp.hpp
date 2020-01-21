//
//  JNIAsyncCpp.hpp
//  AsyncCpp
//
//  Created by Luis Finke on 1/20/20.
//  Copyright © 2020 Luis Finke. All rights reserved.
//

#ifdef __ANDROID__
#include <jni.h>

namespace fgl {
	JavaVM* getAsyncCppJavaVM();
	void setAsyncCppJavaVM(JavaVM*);
}

#endif
