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
		struct empty{};

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
		template<typename Key, typename T>
		using Map = std::map<Key, T>;

		template<typename T>
		using Optional = std::optional<T>;
		template<typename T>
		struct _optionalize {
			using type = Optional<T>;
		};
		template<typename T>
		struct _optionalize<Optional<T>> {
			using type = Optional<T>;
		};
		template<typename T>
		struct _optionalize<void> {
			using type = Optional<std::nullptr_t>;
		};
		template<typename T>
		using Optionalized = typename optionalize_t<T>::type;

		template<typename T>
		struct is_optional: std::false_type {};
		template<typename T>
		struct is_optional<Optional<T>>: std::true_type {};

		template<typename T>
		inline constexpr bool is_optional_v = is_optional<T>::value;


		template<typename T>
		struct optional_or_void {
			using type = Optionalized<T>;
		};
		template<>
		struct optional_or_void<void> {
			using type = void
		};
		template<typename T>
		using OptionalOrVoid = typename optional_or_void<T>::type;


		template<bool Condition, typename T>
		using member_if = typename std::conditional<Condition,T,empty[0]>::type;

		template<typename T>
		using NullifyVoid = typename std::conditional<std::is_void_v<T>,std::nullptr_t,T>::type;
	#endif
	
	#ifndef FGL_ASSERT
		#ifdef NDEBUG
			#define FGL_ASSERT(condition, message) assert(condition)
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
