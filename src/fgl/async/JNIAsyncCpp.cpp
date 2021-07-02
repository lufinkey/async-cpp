//
//  JNIAsyncCpp.cpp
//  AsyncCpp
//
//  Created by Luis Finke on 1/20/20.
//  Copyright © 2020 Luis Finke. All rights reserved.
//

#ifdef __ANDROID__
#include <fgl/async/JNIAsyncCpp.hpp>
#include <fgl/async/DispatchQueue.hpp>
#include <android/log.h>
#include <dlfcn.h>

namespace fgl {
	JavaVM* sharedJavaVM = nullptr;

	JavaVM* getJavaVM() {
		// get java vm from JNI_OnLoad, if it's been called
		if(sharedJavaVM != nullptr) {
			return sharedJavaVM;
		}
		// attempt to find JNI_GetCreatedJavaVMs in several shared libraries
		using JNI_GetCreatedJavaVMs_t = jint(*)(JavaVM**,jsize,jsize*);
		JNI_GetCreatedJavaVMs_t getJavaVMs = nullptr;
		auto libraries = ArrayList<String>{ "libart.so", "libartd.so", "libjvm.so", "libdvm.so", "/system/lib/libvdm.so" };
		for(auto& library : libraries) {
			void *so_handle = dlopen(library.c_str(), RTLD_NOW);
			if (so_handle != nullptr) {
				getJavaVMs = (JNI_GetCreatedJavaVMs_t) dlsym(so_handle, "JNI_GetCreatedJavaVMs");
				if(getJavaVMs != nullptr) {
					__android_log_print(ANDROID_LOG_DEBUG, "AsyncCpp", "found JNI_GetCreatedJavaVMs in %s", library.c_str());
					break;
				}
			} else {
				__android_log_print(ANDROID_LOG_DEBUG, "AsyncCpp", "Could not load shared library %s", library.c_str());
			}
		}
		if(getJavaVMs == nullptr) {
			getJavaVMs = (JNI_GetCreatedJavaVMs_t) dlsym(RTLD_DEFAULT, "JNI_GetCreatedJavaVMs");
		}
		if(getJavaVMs == nullptr) {
			__android_log_print(ANDROID_LOG_DEBUG, "AsyncCpp", "could not find JNI_GetCreatedJavaVMs in any shared objects");
			return nullptr;
		}
		// get java vm
		JavaVM* vm = nullptr;
		jsize vms_size = 0;
		getJavaVMs(&vm, 1, &vms_size);
		if (vms_size == 0) {
			return nullptr;
		}
		return vm;
	}
}

JNIEXPORT jint JNI_OnLoad(JavaVM* vm, void* reserved) {
	__android_log_print(ANDROID_LOG_DEBUG, "AsyncCpp", "JNI module initialized");
	// save java vm
	fgl::sharedJavaVM = vm;
	// ensure we instantiate the main DispatchQueue
	fgl::DispatchQueue::main();
	return JNI_VERSION_1_6;
}

#endif
