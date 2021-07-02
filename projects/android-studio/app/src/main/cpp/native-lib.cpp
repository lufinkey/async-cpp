#include <jni.h>
#include <string>
#include <fgl/data.hpp>
#include <fgl/async.hpp>
#include <android/log.h>
#include <test/DataCppTests.hpp>
#include <test/AsyncCppTests.hpp>

extern "C" JNIEXPORT jstring JNICALL
Java_com_lufinkey_asynccpp_MainActivity_stringFromJNI(
		JNIEnv *env,
		jobject /* this */) {
	__android_log_print(ANDROID_LOG_DEBUG, "TestApp", "Calling MainActivity.stringFromJNI");
	fgl_data_cpp_tests::runTests();
	fgl_async_cpp_tests::runTests();
	return env->NewStringUTF("Hello from C++");
}

JNIEXPORT jint JNI_OnLoad(JavaVM* vm, void* reserved) {
	__android_log_print(ANDROID_LOG_DEBUG, "TestApp", "JNI module loaded");
	return JNI_VERSION_1_6;
}
