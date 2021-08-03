//
//  Coroutine.hpp
//  AsyncCpp
//
//  Created by Luis Finke on 7/31/21.
//  Copyright © 2021 Luis Finke. All rights reserved.
//

#pragma once

#include <fgl/async/DispatchQueue.hpp>
#include <chrono>
#if __has_include(<coroutine>)
	#include <coroutine>
#else
	#include <experimental/coroutine>
#endif


namespace fgl {
	#if __has_include(<coroutine>)
		template<typename T = void>
		using coroutine_handle = std::coroutine_handle<T>;
		template<typename T, typename... Args>
		using coroutine_traits = std::coroutine_traits<T,Args...>;
		using suspend_never = std::suspend_never;
		using suspend_always = std::suspend_always;
	#else
		template<typename T = void>
		using coroutine_handle = std::experimental::coroutine_handle<T>;
		template<typename T, typename... Args>
		using coroutine_traits = std::experimental::coroutine_traits<T,Args...>;
		using suspend_never = std::experimental::suspend_never;
		using suspend_always = std::experimental::suspend_always;
	#endif


	template<typename Rep, typename Period>
	auto resumeAfter(DispatchQueue* queue, std::chrono::duration<Rep,Period> duration);
	template<typename Rep, typename Period>
	auto resumeAfter(std::chrono::duration<Rep,Period> duration);


	struct resumeOnQueue {
		resumeOnQueue(DispatchQueue* queue, bool alwaysDispatch = false);
		bool await_ready();
		void await_suspend(coroutine_handle<> handle);
		void await_resume();
		DispatchQueue* queue;
		bool alwaysDispatch = false;
	};

	struct resumeOnNewThread {
		explicit resumeOnNewThread();
		bool await_ready();
		void await_suspend(coroutine_handle<> handle);
		void await_resume();
	};


	auto coLambda(auto&& executor);



#pragma mark Coroutine method implementations

	template<typename Rep, typename Period>
	auto resumeAfter(DispatchQueue* queue, std::chrono::duration<Rep,Period> duration) {
		struct awaiter {
			DispatchQueue* queue;
			std::chrono::duration<Rep,Period> duration;
			bool await_ready() { return false; }
			void await_suspend(coroutine_handle<> handle) {
				if(queue != nullptr) {
					queue->asyncAfter(std::chrono::steady_clock::now() + duration, [=]() {
						auto h = handle;
						h.resume();
					});
				} else {
					std::thread([=]() {
						std::this_thread::sleep_for(duration);
						auto h = handle;
						h.resume();
					}).detach();
				}
			}
			void await_resume() {}
		};
		return awaiter{ queue, duration };
	}

	template<typename Rep, typename Period>
	auto resumeAfter(std::chrono::duration<Rep,Period> duration) {
		return resumeAfter(DispatchQueue::local(), duration);
	}

	auto coLambda(auto&& executor) {
		return [executor=std::move(executor)]<typename ...Args>(Args&&... args) {
			using ReturnType = decltype(executor(args...));
			auto exec = new Function<ReturnType(Args...)>(executor);
			auto result = (*exec)(args...);
			result.co_capture_var(exec);
			return result;
		};
	}
}
