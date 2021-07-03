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
}

JNIEXPORT jint JNI_OnLoad(JavaVM* vm, void* reserved) {
	using namespace fgl;
	__android_log_print(ANDROID_LOG_DEBUG, "AsyncCpp", "JNI module initialized");
	sharedJavaVM = vm;
	jniScope(vm, [](JNIEnv* env) {
		// ensure we instantiate the main DispatchQueue
		DispatchQueue::main();
		// instantiate linked methods
		jni::NativeRunnable::method_constructor(env);
		jni::Thread::method_getName(env);
		jni::android::Handler::method_constructor_looper(env);
		jni::android::Handler::method_getLooper(env);
		jni::android::Handler::method_post(env);
		jni::android::Handler::method_postDelayed(env);
		jni::android::Looper::method_getThread(env);
	});
	return JNI_VERSION_1_6;
}


namespace fgl {
	void jniScope(JavaVM* vm, Function<void(JNIEnv*)> work) {
		if(vm == nullptr) {
			throw std::runtime_error("given VM is null");
		}
		JNIEnv* env = nullptr;
		bool attachedToThread = false;
		auto envResult = vm->GetEnv((void**)&env, JNI_VERSION_1_6);
		if (envResult == JNI_EDETACHED) {
			if (vm->AttachCurrentThread(&env, NULL) == JNI_OK) {
				attachedToThread = true;
			} else {
				throw std::runtime_error("Failed to attach to thread");
			}
		} else if (envResult == JNI_EVERSION) {
			throw std::runtime_error("Unsupported JNI version");
		}
		work(env);
		if(attachedToThread) {
			vm->DetachCurrentThread();
		}
	}



	JavaVM* getJavaVM() {
		// get java vm from JNI_OnLoad, if it's been called
		if(sharedJavaVM != nullptr) {
			return sharedJavaVM;
		}
		__android_log_print(ANDROID_LOG_DEBUG, "AsyncCpp", "sharedJavaVM is unavailable. Attempting to load from shared objects");
		// attempt to find JNI_GetCreatedJavaVMs in several shared libraries
		using JNI_GetCreatedJavaVMs_t = jint(*)(JavaVM**,jsize,jsize*);
		JNI_GetCreatedJavaVMs_t getJavaVMs = nullptr;
		auto libraries = ArrayList<String>{ "libart.so", "libartd.so", "libjvm.so", "libdvm.so", "/system/lib/libvdm.so" };
		for(auto& library : libraries) {
			void *so_handle = dlopen(library.c_str(), RTLD_NOW);
			if (so_handle != nullptr) {
				getJavaVMs = (JNI_GetCreatedJavaVMs_t)dlsym(so_handle, "JNI_GetCreatedJavaVMs");
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



#pragma mark NativeRunnable

namespace fgl::jni {
	namespace NativeRunnable {
		FGL_JNI_DEF_JCLASS("com/lufinkey/asynccpp/NativeRunnable")
		FGL_JNI_DEF_JCONSTRUCTOR(,"(J)V")

		jobject newObject(JNIEnv *env, Callback callback) {
			auto callbackPtr = new std::function<void(JNIEnv *, std::vector<jobject>)>(callback);
			return env->NewObject(javaClass(env), method_constructor(env), (jlong) callbackPtr);
		}
	}
}

extern "C" JNIEXPORT void JNICALL
Java_com_lufinkey_asynccpp_NativeRunnable_callNativeFunction(JNIEnv* env, jclass, jlong func) {
	auto& funcObj = *((fgl::jni::NativeRunnable::Callback*)func);
	funcObj(env, std::vector<jobject>());
}

extern "C" JNIEXPORT void JNICALL
Java_com_lufinkey_asynccpp_NativeRunnable_destroyNativeFunction(JNIEnv* env, jclass, jlong func) {
	auto funcObj = (fgl::jni::NativeRunnable::Callback*)func;
	delete funcObj;
}



#pragma mark Thread

namespace fgl::jni {
	namespace Thread {
		FGL_JNI_DEF_JCLASS("java/lang/Thread")
		FGL_JNI_DEF_JMETHOD(getName, "getName", "()Ljava/lang/String;")

		jstring getName(JNIEnv* env, jobject self) {
			return (jstring)env->CallObjectMethod(self, method_getName(env));
		}
	}
}



#pragma mark Android classes

namespace fgl::jni::android {
	namespace Handler {
		FGL_JNI_DEF_JCLASS("android/os/Handler")
		FGL_JNI_DEF_JCONSTRUCTOR(_looper, "(Landroid/os/Looper;)V")
		FGL_JNI_DEF_JMETHOD(getLooper, "getLooper", "()Landroid/os/Looper;")
		FGL_JNI_DEF_JMETHOD(post, "post", "(Ljava/lang/Runnable;)Z")
		FGL_JNI_DEF_JMETHOD(postDelayed, "postDelayed", "(Ljava/lang/Runnable;J)Z")

		jobject newObject(JNIEnv* env, LooperInitParams params) {
			return env->NewObject(javaClass(env), method_constructor_looper(env), params.looper);
		}

		jobject getLooper(JNIEnv* env, jobject self) {
			return env->CallObjectMethod(self, method_getLooper(env));
		}
		jboolean post(JNIEnv* env, jobject self, jobject runnable) {
			return env->CallBooleanMethod(self, method_post(env), runnable);
		}
		jboolean postDelayed(JNIEnv* env, jobject self, jobject runnable, jlong delayMillis) {
			return env->CallBooleanMethod(self, method_postDelayed(env), runnable, delayMillis);
		}
	}


	namespace Looper {
		FGL_JNI_DEF_JCLASS("android/os/Looper")
		FGL_JNI_DEF_JMETHOD(getThread, "getThread", "()Ljava/lang/Thread;")

		jobject getThread(JNIEnv* env, jobject self) {
			return env->CallObjectMethod(self, method_getThread(env));
		}
	}
}

#endif
