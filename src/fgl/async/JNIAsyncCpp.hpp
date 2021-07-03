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
	jmethodID methodID_##name(JNIEnv* env);

#define FGL_JNI_DECL_STATIC_JMETHOD(name) \
	jmethodID methodID_static_##name(JNIEnv* env);

#define FGL_JNI_DECL_JFIELD(name) \
	jfieldID fieldID_##name(JNIEnv* env);

#define FGL_JNI_DECL_STATIC_JFIELD(name) \
	jfieldID fieldID_static_##name(JNIEnv* env);

#define FGL_JNI_DECL_JCONSTRUCTOR(suffix) \
	FGL_JNI_DECL_JMETHOD(constructor##suffix)

#define FGL_JNI_DEF_JCLASS(enclosure, classpath) \
	jclass _##enclosure##_javaClass = nullptr; \
	jclass enclosure::javaClass(JNIEnv *env) { \
		if (_##enclosure##_javaClass == nullptr) { \
			_##enclosure##_javaClass = env->FindClass(classpath); \
			if (env->ExceptionOccurred() != nullptr) { \
				env->ExceptionDescribe(); \
				env->ExceptionClear(); \
				throw std::runtime_error((std::string)"Unable to find java class " + classpath); \
			} \
			if (_##enclosure##_javaClass != nullptr) { \
				_##enclosure##_javaClass = (jclass) env->NewGlobalRef(_##enclosure##_javaClass); \
			} \
		} \
		return _##enclosure##_javaClass; \
	}

#define FGL_JNI_DEF_JMETHOD(enclosure, name, id, signature) \
	jmethodID _##enclosure##_methodID_##name = nullptr; \
	jmethodID enclosure::methodID_##name(JNIEnv* env) {          \
		if (_##enclosure##_methodID_##name == nullptr) { \
			_##enclosure##_methodID_##name = env->GetMethodID(javaClass(env), id, signature); \
			if (env->ExceptionOccurred() != nullptr) { \
				env->ExceptionDescribe(); \
				env->ExceptionClear(); \
				throw std::runtime_error((std::string)"Unable to find method ID for " + id + " with signature " + signature); \
			} \
		} \
		return _##enclosure##_methodID_##name; \
	}

#define FGL_JNI_DEF_JCONSTRUCTOR(enclosure, suffix, signature) \
	FGL_JNI_DEF_JMETHOD(enclosure, constructor##suffix, "<init>", signature)

#define FGL_JNI_DEF_JFIELD(enclosure, name, id, signature) \
	jfieldID _##enclosure##_fieldID_##name = nullptr; \
	jfieldID enclosure::fieldID_##name(JNIEnv* env) { \
        if (_##enclosure##_fieldID_##name == nullptr) { \
            _##enclosure##_fieldID_##name = env->GetFieldID(javaClass(env), id, signature); \
            if (env->ExceptionOccurred() != nullptr) { \
                env->ExceptionDescribe(); \
                env->ExceptionClear(); \
                throw std::runtime_error((std::string)"Unable to find field ID for " + id + " with signature " + signature); \
            } \
        } \
        return _##enclosure##_fieldID_##name; \
    }

#define FGL_JNI_DEF_STATIC_JMETHOD(enclosure, name, id, signature) \
	jmethodID _##enclosure##_methodID_static_##name = nullptr; \
	jmethodID enclosure::methodID_static_##name(JNIEnv* env) {          \
		if (_##enclosure##_methodID_static_##name == nullptr) { \
			_##enclosure##_methodID_static_##name = env->GetStaticMethodID(javaClass(env), id, signature); \
			if (env->ExceptionOccurred() != nullptr) { \
				env->ExceptionDescribe(); \
				env->ExceptionClear(); \
				throw std::runtime_error((std::string)"Unable to find static method ID for " + id + " with signature " + signature); \
			} \
		} \
		return _##enclosure##_methodID_static_##name; \
	}

#define FGL_JNI_DEF_STATIC_JFIELD(enclosure, name, id, signature) \
	jfieldID _##enclosure##_fieldID_static_##name = nullptr; \
	jfieldID enclosure::fieldID_static_##name(JNIEnv* env) {          \
		if (_##enclosure##_fieldID_static_##name == nullptr) { \
			_##enclosure##_fieldID_static_##name = env->GetStaticFieldID(javaClass(env), id, signature); \
			if (env->ExceptionOccurred() != nullptr) { \
				env->ExceptionDescribe(); \
				env->ExceptionClear(); \
				throw std::runtime_error((std::string)"Unable to find static field ID for " + id + " with signature " + signature); \
			} \
		} \
		return _##enclosure##_fieldID_static_##name; \
	}



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
		FGL_JNI_DECL_STATIC_JMETHOD(getMainLooper)

		jobject getThread(JNIEnv* env, jobject self);
		jobject getMainLooper(JNIEnv* env);
	}
}

#endif
