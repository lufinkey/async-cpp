#include <jni.h>
#include <string>
#include <fgl/data.hpp>

extern "C" JNIEXPORT jstring JNICALL
Java_com_lufinkey_asynccpp_MainActivity_stringFromJNI(
		JNIEnv *env,
		jobject /* this */) {
	auto stringTest = fgl::String("Hello from C++");
	auto anyTest = fgl::Any();
	return env->NewStringUTF(stringTest.c_str());
}
