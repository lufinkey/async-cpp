//
//  JNIAsyncCpp.hpp
//  AsyncCpp
//
//  Created by Luis Finke on 1/20/20.
//  Copyright © 2020 Luis Finke. All rights reserved.
//

#pragma once

#include <fgl/async/Common.hpp>

#ifdef __ANDROID__
#include <jni.h>

namespace fgl {
	JavaVM* getJavaVM();

	void jniScope(JavaVM* vm, Function<void(JNIEnv*)> work);
}



#define FGL_JNI_DECL_JCLASS \
	jclass javaClass(JNIEnv* env);

#define FGL_JNI_DECL_JMETHOD(name) \
	jmethodID method_##name(JNIEnv* env);

#define FGL_JNI_DECL_JCONSTRUCTOR(suffix) \
	FGL_JNI_DECL_JMETHOD(constructor##suffix)

#define FGL_JNI_DEF_JCLASS(classpath) \
	jclass _javaClass = nullptr; \
	jclass javaClass(JNIEnv *env) { \
		if (_javaClass == nullptr) { \
			_javaClass = env->FindClass(classpath); \
			if (env->ExceptionOccurred() != nullptr) { \
				env->ExceptionDescribe(); \
				env->ExceptionClear(); \
				throw std::runtime_error((std::string)"Unable to find java class " + classpath); \
			} \
			if (_javaClass != nullptr) { \
				_javaClass = (jclass) env->NewGlobalRef(_javaClass); \
			} \
		} \
		return _javaClass; \
	}

#define FGL_JNI_DEF_JMETHOD(name, id, signature) \
	jmethodID _method_##name = nullptr; \
	jmethodID method_##name(JNIEnv* env) {          \
		if (_method_##name == nullptr) { \
			_method_##name = env->GetMethodID(javaClass(env), id, signature); \
			if (env->ExceptionOccurred() != nullptr) { \
				env->ExceptionDescribe(); \
				env->ExceptionClear(); \
				throw std::runtime_error((std::string)"Unable to find method ID for " + id + " with signature " + signature); \
			} \
		} \
		return _method_##name; \
	}

#define FGL_JNI_DEF_JCONSTRUCTOR(suffix, signature) \
	FGL_JNI_DEF_JMETHOD(constructor##suffix, "<init>", signature)



namespace fgl::jni {
	namespace NativeRunnable {
		using Callback = std::function<void(JNIEnv*,std::vector<jobject>)>;
		FGL_JNI_DECL_JCLASS
		FGL_JNI_DECL_JCONSTRUCTOR()

		jobject newObject(JNIEnv* env, Callback callback);
	}

	namespace Thread {
		FGL_JNI_DECL_JCLASS
		FGL_JNI_DECL_JMETHOD(getName)

		jstring getName(JNIEnv* env, jobject self);
	}
}

namespace fgl::jni::android {
	namespace Handler {
		FGL_JNI_DECL_JCLASS
		FGL_JNI_DECL_JCONSTRUCTOR(_looper)
		FGL_JNI_DECL_JMETHOD(getLooper)
		FGL_JNI_DECL_JMETHOD(post)
		FGL_JNI_DECL_JMETHOD(postDelayed)

		struct LooperInitParams {
			jobject looper;
		};
		jobject newObject(JNIEnv* env, LooperInitParams params);

		jobject getLooper(JNIEnv* env, jobject self);
		jboolean post(JNIEnv* env, jobject self, jobject runnable);
		jboolean postDelayed(JNIEnv* env, jobject self, jobject runnable, jlong delayMillis);
	}

	namespace Looper {
		FGL_JNI_DECL_JCLASS
		FGL_JNI_DECL_JMETHOD(getThread)

		jobject getThread(JNIEnv* env, jobject self);
	}
}

#endif
