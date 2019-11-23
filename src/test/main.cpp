//
//  main.cpp
//  AsyncCppTest
//
//  Created by Luis Finke on 8/14/19.
//  Copyright © 2019 Luis Finke. All rights reserved.
//

#include <exception>
#include <iostream>
#include <thread>
#include <type_traits>
#include <cxxabi.h>
#include <fgl/async.hpp>

using namespace fgl;

std::string demangled(std::string const& sym) {
	std::unique_ptr<char, void(*)(void*)>
	name{abi::__cxa_demangle(sym.c_str(), nullptr, nullptr, nullptr), std::free};
	return {name.get()};
}

template<typename T>
void print_type() {
	bool is_lvalue_reference = std::is_lvalue_reference<T>::value;
	bool is_rvalue_reference = std::is_rvalue_reference<T>::value;
	bool is_const = std::is_const<typename std::remove_reference<T>::type>::value;
	
	std::cout << demangled(typeid(T).name());
	if (is_const) {
		std::cout << " const";
	}
	if (is_lvalue_reference) {
		std::cout << " &";
	}
	if (is_rvalue_reference) {
		std::cout << " &&";
	}
	std::cout << std::endl;
};

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


int main(int argc, const char* argv[]) {
	auto promise = waitFor(2000, 23).then([](int result) {
		printf("result 1: %i\n", result);
		return waitFor(5000, 16);
	}).then([](int result) {
		printf("result 2: %i\n", result);
		throw std::logic_error("eyy some shit happened");
	}).except([](std::exception& error) {
		printf("we caught an error: %s\n", error.what());
	}).then([]() -> Promise<String> {
		return Promise<String>::resolve("ayy");
	}).timeout(std::chrono::seconds(2), []() -> Promise<String> {
		// do nothing
		printf("timed out\n");
		return Promise<String>::resolve("nayyy");
	}).delay(std::chrono::seconds(12));
	print_type<decltype(promise)>();
	auto result = await(promise);
	printf("got result: %s\n", result.c_str());
	
	auto gen = generate<int,int>([](auto yield) {
		printf("we're gonna yield 4\n");
		yield(4);
		printf("we're gonna yield 5\n");
		yield(5);
		printf("we're gonna return 6\n");
		return 6;
	});
	printf("running generator\n");
	int genCounter = 0;
	bool genDone = false;
	while(!genDone) {
		auto genResult = gen.next().get();
		genDone = genResult.done;
		printf("we got gen result %i. done? %i, has value? %i, value? %i\n", genCounter, (int)genResult.done, (int)genResult.value.has_value(), genResult.value.value_or(-1));
		genCounter++;
	}
	
	return 0;
}
