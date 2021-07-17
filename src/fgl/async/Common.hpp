//
//  Common.hpp
//  AsyncCpp
//
//  Created by Luis Finke on 8/11/19.
//  Copyright © 2019 Luis Finke. All rights reserved.
//

#pragma once

#ifdef ASYNC_CPP_STANDALONE
	#include <any>
	#include <functional>
	#include <list>
	#include <string>
	#include <vector>
	#include <optional>
	#include <type_traits>
#else
	#include <fgl/data.hpp>
#endif
#include <exception>
#include <iostream>

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
		template<typename ...T>
		using Tuple = std::tuple<T...>;

		template<typename T>
		using Optional = std::optional<T>;
		template<typename T>
		struct optionalize_t {
			using type = Optional<T>;
		};
		template<typename T>
		struct optionalize_t<Optional<T>> {
			using type = T;
		};
		template<typename T>
		struct optionalize_t<void> {
			using type = Optional<std::nullptr_t>;
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
					assert(false); \
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

	#ifndef FGL_FINALLY_DEF
		#define FGL_FINALLY_DEF
		template<typename Callable>
		class Finally {
		public:
			Finally(Callable c): callable(c), enabled(true) {}
			Finally(Finally&& f): callable(f.callable), enabled(f.enabled) {
				f.disable();
			}
			~Finally() {
				if(enabled) {
					callable();
				}
			}
			void disable() {
				enabled = false;
			}
		private:
			Callable callable;
			bool enabled;
		};
		template<typename Callable>
		Finally<Callable> make_finally(Callable c) {
			return Finally<Callable>(c);
		}
	#endif
}
