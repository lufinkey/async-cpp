//
//  PromiseErrorPtr.hpp
//  AsyncCpp
//
//  Created by Luis Finke on 8/11/19.
//  Copyright © 2019 Luis Finke. All rights reserved.
//

#pragma once

#include <exception>

namespace fgl {
	class PromiseErrorPtr {
	public:
		PromiseErrorPtr(const std::exception_ptr& eptr) : except_ptr(eptr) {}
		PromiseErrorPtr(std::exception_ptr&& eptr) : except_ptr(eptr) {}
		PromiseErrorPtr(const PromiseErrorPtr& eptr) : except_ptr(eptr.except_ptr) {}
		PromiseErrorPtr(PromiseErrorPtr&& eptr) : except_ptr(eptr.except_ptr) {}
		
		template<
			typename E,
			typename E_TYPE = typename std::remove_reference<typename std::remove_cv<E>::type>::type,
			typename std::enable_if<
				!std::is_same<E_TYPE,std::exception_ptr>::value
				&& !std::is_same<E_TYPE,PromiseErrorPtr>::value, std::nullptr_t>::type = nullptr>
		PromiseErrorPtr(const E& e) : except_ptr(std::make_exception_ptr(e)) {}
		
		template<
			typename E,
			typename E_TYPE = typename std::remove_reference<typename std::remove_cv<E>::type>::type,
			typename std::enable_if<
				!std::is_same<E_TYPE,std::exception_ptr>::value
				&& !std::is_same<E_TYPE,PromiseErrorPtr>::value, std::nullptr_t>::type = nullptr>
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
