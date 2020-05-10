//
//  LambdaTraits.hpp
//  AsyncCpp
//
//  Created by Luis Finke on 8/15/19.
//  Copyright © 2019 Luis Finke. All rights reserved.
//

#ifndef LambdaTraits_h
#define LambdaTraits_h

#include <tuple>
#include <type_traits>

namespace fgl {
	template<class Ret, class Cls, class IsMutable, class... Args>
	struct lambda_detail {
		using is_mutable = IsMutable;
		
		enum { arity = sizeof...(Args) };
		
		using return_type = Ret;
		
		template<size_t i>
		struct arg {
			typedef typename std::tuple_element<i, std::tuple<Args...>>::type type;
		};
	};
	
	template<class Ld>
	struct lambda_traits: lambda_traits<decltype(&Ld::operator())>
	{};
	
	template<class Ret, class Cls, class... Args>
	struct lambda_traits<Ret(Cls::*)(Args...)>: lambda_detail<Ret,Cls,std::true_type,Args...>
	{};
	
	template<class Ret, class Cls, class... Args>
	struct lambda_traits<Ret(Cls::*)(Args...) const>: lambda_detail<Ret,Cls,std::false_type,Args...>
	{};


	template<typename Arg, typename Return>
	struct lambda_block {
		using type = Function<Return(Arg)>;
	};
	template<typename Return>
	struct lambda_block<void, Return> {
		using type = Function<Return()>;
	};
}

#endif /* LambdaTraits_h */
