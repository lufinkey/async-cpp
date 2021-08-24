//
//  PromiseErrorPtr.hpp
//  AsyncCpp
//
//  Created by Luis Finke on 8/11/19.
//  Copyright © 2019 Luis Finke. All rights reserved.
//

#pragma once

#include <fgl/async/Common.hpp>
#include <exception>
#include <type_traits>

namespace fgl {
	class PromiseErrorPtr {
	public:
		PromiseErrorPtr(const std::exception_ptr& eptr) : except_ptr(eptr) {}
		PromiseErrorPtr(std::exception_ptr&& eptr) : except_ptr(eptr) {}
		PromiseErrorPtr(const PromiseErrorPtr& eptr) : except_ptr(eptr.except_ptr) {}
		PromiseErrorPtr(PromiseErrorPtr&& eptr) : except_ptr(eptr.except_ptr) {}
		
		template<
			typename E,
			typename E_TYPE = typename std::remove_cvref_t<E>,
			typename std::enable_if_t<
				!std::is_same_v<E_TYPE,std::exception_ptr>
				&& !std::is_same_v<E_TYPE,PromiseErrorPtr>, std::nullptr_t> = nullptr>
		PromiseErrorPtr(const E& e) : except_ptr(std::make_exception_ptr(e)) {}
		
		template<
			typename E,
			typename E_TYPE = typename std::remove_cvref_t<E>,
			typename std::enable_if_t<
				!std::is_same_v<E_TYPE,std::exception_ptr>
				&& !std::is_same_v<E_TYPE,PromiseErrorPtr>, std::nullptr_t> = nullptr>
		PromiseErrorPtr(E&& e) : except_ptr(std::make_exception_ptr(e)) {}
		
		PromiseErrorPtr() : except_ptr() {}
		
		operator std::exception_ptr() const {
			return except_ptr;
		}
		
		std::exception_ptr ptr() const {
			return except_ptr;
		}
		
		void rethrow() const {
			std::rethrow_exception(except_ptr);
		}
		
	private:
		std::exception_ptr except_ptr;
	};
}
