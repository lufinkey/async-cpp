#include <jni.h>
#include <string>
#include <fgl/data.hpp>
#include <fgl/async.hpp>
#include <android/log.h>
#include <test/AsyncCppTests.hpp>

extern "C" JNIEXPORT jstring JNICALL
Java_com_lufinkey_asynccpp_MainActivity_stringFromJNI(
		JNIEnv *env,
		jobject /* this */) {
	__android_log_print(ANDROID_LOG_DEBUG, "native-lib", "Calling MainActivity.stringFromJNI");
	auto stringTest = fgl::String("Hello from C++");
	auto mainQueue = fgl::DispatchQueue::main();
	mainQueue->async([]() {
		printf("running tests from the main thread\n");
		fgl_async_cpp_tests::runTests();
	});
	return env->NewStringUTF(stringTest.c_str());
}

JNIEXPORT jint JNI_OnLoad(JavaVM* vm, void* reserved) {
	__android_log_print(ANDROID_LOG_DEBUG, "native-lib", "JNI module loaded");
	return JNI_VERSION_1_6;
}
