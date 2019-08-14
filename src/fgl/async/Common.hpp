//
//  Common.hpp
//  AsyncCpp
//
//  Created by Luis Finke on 8/11/19.
//  Copyright © 2019 Luis Finke. All rights reserved.
//

#pragma once

#include <any>
#include <chrono>
#include <exception>
#include <functional>
#include <iostream>
#include <list>
#include <string>
#include <vector>

namespace fgl {
	template<typename T>
	using Function = std::function<T>;
	template<typename T>
	using ArrayList = std::vector<T>;
	template<typename T>
	using LinkedList = std::list<T>;
	using String = std::string;
	using Any = std::any;
	
	#ifndef FGL_ASSERT
		#ifdef NDEBUG
			#define FGL_ASSERT(condition, message)
		#else
			#define FGL_ASSERT(condition, message) { \
				if(!(condition)) { \
					std::cerr << "Assertion `" #condition "` failed in " << __FILE__ \
						<< " line " << __LINE__ << ": " << (message) << std::endl; \
					std::terminate(); \
				} \
			}
		#endif
	#endif
	
	#ifndef FGL_WARN
		#ifdef NDEBUG
			#define FGL_WARN(message)
		#else
			#define FGL_WARN(message) { \
				std::cerr << "Warning: " << (message) << std::endl; \
			}
		#endif
	#endif
}
