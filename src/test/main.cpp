//
//  main.cpp
//  AsyncCppTest
//
//  Created by Luis Finke on 8/14/19.
//  Copyright © 2019 Luis Finke. All rights reserved.
//

#include "AsyncCppTests.hpp"

int main(int argc, const char* argv[]) {
	auto promise = fgl_async_cpp_tests::runTests();
	promise.then([]() {
		printf("finished tests successfully\n");
		exit(0);
	}, [](std::exception_ptr error) {
		fgl::DispatchQueue::main()->async([=]() {
			std::rethrow_exception(error);
		});
	});
	fgl::DispatchQueue::dispatchMain();
	return 0;
}
