//
//  Common.hpp
//  AsyncCpp
//
//  Created by Luis Finke on 8/11/19.
//  Copyright © 2019 Luis Finke. All rights reserved.
//

#pragma once

#include <exception>
#include <iostream>
#ifdef ASYNC_CPP_STANDALONE
	#include <any>
	#include <functional>
	#include <list>
	#include <string>
	#include <vector>
#else
	#include <fgl/data.hpp>
#endif

namespace fgl {
	#ifdef ASYNC_CPP_STANDALONE
		template<typename T>
		using Function = std::function<T>;
		template<typename T>
		using ArrayList = std::vector<T>;
		template<typename T>
		using LinkedList = std::list<T>;
		using String = std::string;
		using Any = std::any;

		template<typename T>
		using Optional = std::optional<T>;
		template<typename T, bool isOptional = std::is_same<T, Optional<T>>::value>
		struct optionalize_t {
			using type = Optional<T>;
		};
		template<typename T>
		struct optionalize_t<T,true> {
			using type = T;
		};
		template<typename T>
		using Optionalized = typename optionalize_t<T>::type;
	#endif
	
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
	
	#ifdef ASYNC_CPP_STANDALONE
		#ifndef ASYNC_CPP_LIST_PUSH
			#define ASYNC_CPP_LIST_PUSH(list, element) list.push_back(element)
		#endif
	#else
		#ifndef ASYNC_CPP_LIST_PUSH
			#define ASYNC_CPP_LIST_PUSH(list, element) list.pushBack(element)
		#endif
	#endif
}

#ifdef _HAS_FGL_DATA
	#undef _HAS_FGL_DATA
#endif
