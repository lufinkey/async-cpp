//
//  main.cpp
//  AsyncCppTest
//
//  Created by Luis Finke on 8/14/19.
//  Copyright © 2019 Luis Finke. All rights reserved.
//

#include "AsyncCppTests.hpp"

int main(int argc, const char* argv[]) {
	fgl_async_cpp_tests::runTests();
	std::this_thread::sleep_for(std::chrono::seconds(4));
	return 0;
}
