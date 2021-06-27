
#ifdef __ANDROID__

#include <jni.h>
#include <functional>
#include <vector>

extern "C" JNIEXPORT void JNICALL
Java_com_lufinkey_libasynccpp_NativeRunnable_callNativeFunction(JNIEnv* env, jclass, jlong func) {
	auto& funcObj = *((std::function<void(JNIEnv*,std::vector<jobject>)>*)func);
	funcObj(env, std::vector<jobject>());
}

extern "C" JNIEXPORT void JNICALL
Java_com_lufinkey_libasynccpp_NativeRunnable_destroyNativeFunction(JNIEnv* env, jclass, jlong func) {
	auto funcObj = (std::function<void(JNIEnv*,std::vector<jobject>)>*)func;
	delete funcObj;
}

#endif
