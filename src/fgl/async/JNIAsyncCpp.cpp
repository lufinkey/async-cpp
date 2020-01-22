//
//  JNIAsyncCpp.cpp
//  AsyncCpp
//
//  Created by Luis Finke on 1/20/20.
//  Copyright © 2020 Luis Finke. All rights reserved.
//

#ifdef __ANDROID__
#include <fgl/async/JNIAsyncCpp.hpp>
#include <android/log.h>

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
jint JNICALL JNI_OnLoad(JavaVM* vm, void* reserved) {
	__android_log_print(ANDROID_LOG_DEBUG, "AsyncCpp", "JNI module loaded");
	fgl::mainAsyncCppJavaVM = vm;
	return JNI_VERSION_1_6;
}

#endif
