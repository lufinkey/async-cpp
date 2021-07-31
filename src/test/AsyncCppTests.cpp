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
		return promiseThread([=]() {
			std::this_thread::sleep_for(std::chrono::milliseconds(milliseconds));
		});
	}

	template<typename Result>
	Promise<Result> waitFor(unsigned int milliseconds, Result returnValue) {
		return promiseThread([=]() -> Result {
			std::this_thread::sleep_for(std::chrono::milliseconds(milliseconds));
			return returnValue;
		});
	}


	Promise<int> coroutineTest() {
		println("starting coroutine");
		co_await waitFor(1000);
		println("continuing coroutine after delay");
		co_yield {};
		println("returning final value");
		co_return 13;
	}

	Generator<void> coroutineGeneratorTest() {
		println("starting coroutine generator");
		co_yield initialGenNext();
		println("running next step of coroutine generator");
		co_yield {};
		println("finishing coroutine generator");
	}


	Promise<void> runTests() {
		return promiseThread([=]() {
			println("Starting AsyncCpp tests");
			fgl::DispatchQueue::main()->async([]() {
				println("We queued something to the main thread");
			});

			auto promise = waitFor(2000, 23)
			.then([](int result) {
				println((String)"result 1: " + result);
				return waitFor(5000, 16);
			}).then([](int result) {
				println((String)"result 2: " + result);
				throw std::logic_error("eyy some shit happened");
			}).except([](std::exception& error) {
				println((String)"we caught an error: " + error.what());
			}).then([]() -> Promise<String> {
				return Promise<String>::resolve("ayy");
			}).timeout(std::chrono::seconds(2), []() -> Promise<String> {
				// do nothing
				println((String)"timed out");
				return Promise<String>::resolve("nayyy");
			});
			
			println(fgl::stringify_type<decltype(promise)>());
			
			auto result = promise.get();
			println((String)"got result: " + result);

			auto gen = generatorThread<int>([](auto yield) {
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
			
			auto coPromise = coroutineTest();
			
			waitFor(6000).get();
			
			coPromise.then([=](auto result) {
				println("finished coroutine tests with result "+std::to_string(result));
			}).get();
			
			println("creating coroutine generator");
			auto coGenerator = coroutineGeneratorTest();
			println("calling generator.next");
			coGenerator.next().get();
			println("calling generator.next");
			auto coGenResult = coGenerator.next().get();
			println((String)"coroutine done = "+coGenResult.done);
			
			println("Done running AsyncCpp tests");
		});
	}
}
