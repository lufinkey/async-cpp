//
//  JNIAsyncCpp.cpp
//  AsyncCpp
//
//  Created by Luis Finke on 1/20/20.
//  Copyright © 2020 Luis Finke. All rights reserved.
//

#ifdef __ANDROID__
#include <fgl/async/JNIAsyncCpp.hpp>

namespace fgl {
	JavaVM* mainAsyncCppJavaVM = nullptr;

	JavaVM* getAsyncCppJavaVM() {
		return mainAsyncCppJavaVM;
	}

	void setAsyncCppJavaVM(JavaVM* javaVm) {
		mainAsyncCppJavaVM = javaVm;
	}
}

extern "C"
JNIEXPORT
jint JNICALL JNI_OnLoad_AsyncCpp(JavaVM* vm, void* reserved) {
	fgl::mainAsyncCppJavaVM = vm;
	return JNI_VERSION_1_6;
}

#endif
