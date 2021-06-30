//
//  main.cpp
//  AsyncCppTest
//
//  Created by Luis Finke on 8/14/19.
//  Copyright © 2019 Luis Finke. All rights reserved.
//

#include "AsyncCppTests.hpp"
#include <fgl/data.hpp>
#include <exception>
#include <iostream>
#include <thread>
#include <type_traits>
#include <cxxabi.h>

#ifdef __ANDROID__
#include <android/log.h>
#endif

namespace fgl_async_cpp_tests {
	using namespace fgl;

	void println(const String& str) {
		#ifdef __ANDROID__
			__android_log_print(ANDROID_LOG_DEBUG, "AsyncCppTests", "%s\n", str.c_str());
		#else
			printf("%s\n", str.c_str());
		#endif
	}

	Promise<void> waitFor(unsigned int milliseconds) {
		return async<void>([=]() {
			std::this_thread::sleep_for(std::chrono::milliseconds(milliseconds));
		});
	}

	template<typename Result>
	Promise<Result> waitFor(unsigned int milliseconds, Result returnValue) {
		return async<Result>([=]() -> Result {
			std::this_thread::sleep_for(std::chrono::milliseconds(milliseconds));
			return returnValue;
		});
	}


	void runTests() {
		println("Starting AsyncCpp tests");
		auto backgroundQueue = fgl::backgroundPromiseQueue();
		auto promise = waitFor(2000, 23)
		.then(backgroundQueue, [](int result) {
			println((String)"result 1: " + result);
			return waitFor(5000, 16);
		}).then(backgroundQueue, [](int result) {
			println((String)"result 2: " + result);
			throw std::logic_error("eyy some shit happened");
		}).except(backgroundQueue, [](std::exception& error) {
			println((String)"we caught an error: " + error.what());
		}).then(backgroundQueue, []() -> Promise<String> {
			return Promise<String>::resolve("ayy");
		}).timeout(backgroundQueue, std::chrono::seconds(2), []() -> Promise<String> {
			// do nothing
			println((String)"timed out");
			return Promise<String>::resolve("nayyy");
		});
		
		println(fgl::stringify_type<decltype(promise)>());
		
		auto result = await(promise);
		println((String)"got result: " + result);

		auto gen = generate<int>([](auto yield) {
			println("we're gonna yield 4");
			yield(4);
			println("we're gonna yield 5");
			yield(5);
			println("we're gonna return 6");
			return 6;
		});
		println("running generator");
		int genCounter = 0;
		bool genDone = false;
		while(!genDone) {
			auto genResult = gen.next().get();
			genDone = genResult.done;
			println((String)"we got gen result " + genCounter + ". done? " + genResult.done + ", has value? " + genResult.value.has_value() + ", value? " + genResult.value.value_or(-1));
			genCounter++;
		}

		std::shared_ptr<AsyncList<String>> asyncList;
		
		await(waitFor(12000));
	}

}