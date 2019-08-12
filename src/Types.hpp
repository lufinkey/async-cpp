//
//  Types.hpp
//  AsyncCpp
//
//  Created by Luis Finke on 8/11/19.
//  Copyright © 2019 Luis Finke. All rights reserved.
//

#pragma once

#include <chrono>
#include <functional>
#include <list>
#include <string>

namespace fgl {
	template<typename T>
	using Function = std::function<T>;
	template<typename T>
	using LinkedList = std::list<T>;
	using String = std::string;
}
