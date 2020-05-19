//
//  ContinuousGenerator.hpp
//  AsyncCpp
//
//  Created by Luis Finke on 5/9/20.
//  Copyright © 2020 Luis Finke. All rights reserved.
//

#pragma once

#include <exception>
#include <fgl/async/Common.hpp>
#include <fgl/async/Generator.hpp>

namespace fgl {
	template<typename Yield, typename Next>
	class ContinuousGenerator;

	template<typename Yield, typename Next>
	struct is_generator<ContinuousGenerator<Yield,Next>> {
		static constexpr bool value = true;
		typedef Yield yield_type;
		typedef Next next_type;
		typedef ContinuousGenerator<Yield,Next> generator_type;
		typedef std::nullptr_t null_type;
	};



	template<typename GeneratorType>
	struct is_continuous_generator {
		static constexpr bool value = false;
	};
	
	template<typename Yield, typename Next>
	struct is_continuous_generator<ContinuousGenerator<Yield,Next>> {
		static constexpr bool value = true;
		typedef Yield yield_type;
		typedef Next next_type;
		typedef ContinuousGenerator<Yield,Next> generator_type;
		typedef std::nullptr_t null_type;
	};

	template<typename GeneratorType>
	using IsContinuousGenerator = typename is_continuous_generator<GeneratorType>::generator_type;



	template<typename Yield>
	struct ContinuousGeneratorResult {
		Optionalized<Yield> result;
		std::exception_ptr error;
	};

	template<>
	struct ContinuousGeneratorResult<void> {
		std::exception_ptr error;
	};



	template<typename Yield, typename Next>
	class ContinuousGenerator: Generator<ContinuousGeneratorResult<Yield>,Next> {
	public:
		using BaseGenerator = Generator<ContinuousGeneratorResult<Yield>,Next>;
		using BaseYieldResult = typename BaseGenerator::YieldResult;
		
		using YieldType = Yield;
		using NextType = Next;
		
		using YieldResult = GeneratorYieldResult<Yield>;
		
		using YieldReturner = typename lambda_block<Next,Promise<YieldResult>>::type;
		template<typename T>
		using Mapper = typename lambda_block<Yield,T>::type;
		
		ContinuousGenerator();
		ContinuousGenerator(const BaseGenerator&);
		ContinuousGenerator(BaseGenerator&&);
		explicit ContinuousGenerator(YieldReturner yieldReturner, Function<void()> destructor=nullptr);
		
		template<typename _Next=Next,
			typename std::enable_if<(std::is_same<_Next,Next>::value && !std::is_same<_Next,void>::value), std::nullptr_t>::type = nullptr>
		inline Promise<YieldResult> next(_Next nextValue);
		
		template<typename _Next=Next,
			typename std::enable_if<(std::is_same<_Next,Next>::value && std::is_same<_Next,void>::value), std::nullptr_t>::type = nullptr>
		inline Promise<YieldResult> next();
		
		template<typename NewYield>
		ContinuousGenerator<NewYield,Next> map(DispatchQueue* queue, Mapper<NewYield> transform);
		template<typename NewYield>
		ContinuousGenerator<NewYield,Next> map(Mapper<NewYield> transform);
		template<typename NewYield>
		ContinuousGenerator<NewYield,Next> mapAsync(DispatchQueue* queue, Mapper<Promise<NewYield>> transform);
		template<typename NewYield>
		ContinuousGenerator<NewYield,Next> mapAsync(Mapper<Promise<NewYield>> transform);
	};



	template<typename Yield, typename Next>
	ContinuousGenerator<Yield,Next>::ContinuousGenerator()
	: Generator<ContinuousGeneratorResult<Yield>,Next>() {
		//
	}

	template<typename Yield, typename Next>
	ContinuousGenerator<Yield,Next>::ContinuousGenerator(const BaseGenerator& gen)
	: BaseGenerator(gen) {
		//
	}

	template<typename Yield, typename Next>
	ContinuousGenerator<Yield,Next>::ContinuousGenerator(BaseGenerator&& gen)
	: BaseGenerator(gen) {
		//
	}

	template<typename Yield, typename Next>
	ContinuousGenerator<Yield,Next>::ContinuousGenerator(YieldReturner yieldReturner, Function<void()> destructor)
	: BaseGenerator(([=]() {
		if constexpr(std::is_same<Next,void>::value) {
			return [=]() {
				return yieldReturner().template map<BaseYieldResult>([=](YieldResult yieldResult) {
					if constexpr(std::is_same<Yield,void>::value) {
						return BaseYieldResult{
							.value=ContinuousGeneratorResult<Yield>{},
							.done=yieldResult.done
						};
					} else {
						return BaseYieldResult{
							.value=ContinuousGeneratorResult<Yield>{
								.result=std::move(yieldResult.value)
							},
							.done=yieldResult.done
						};
					}
				}).except([=](std::exception_ptr error) {
					return BaseYieldResult{
						.value=ContinuousGeneratorResult<Yield>{
							.error=error
						},
						.done=false
					};
				});
			};
		} else {
			return [=](Next next) {
				return yieldReturner(std::move(next)).template map<BaseYieldResult>([=](YieldResult yieldResult) {
					if constexpr(std::is_same<Yield,void>::value) {
						return BaseYieldResult{
							.value=ContinuousGeneratorResult<Yield>{},
							.done=yieldResult.done
						};
					} else {
						return BaseYieldResult{
							.value=ContinuousGeneratorResult<Yield>{
								.result=std::move(yieldResult.value)
							},
							.done=yieldResult.done
						};
					}
				}).except([=](std::exception_ptr error) {
					return BaseYieldResult{
						.value=ContinuousGeneratorResult<Yield>{
							.error=error
						},
						.done=false
					};
				});
			};
		}
	})(), destructor) {
		//
	}

	template<typename Yield, typename Next>
	template<typename _Next,
		typename std::enable_if<(std::is_same<_Next,Next>::value && !std::is_same<_Next,void>::value), std::nullptr_t>::type>
	Promise<typename ContinuousGenerator<Yield,Next>::YieldResult> ContinuousGenerator<Yield,Next>::next(_Next nextValue) {
		return BaseGenerator::next(std::move(nextValue)).template map<YieldResult>([=](BaseYieldResult yieldResult) {
			if(yieldResult.value) {
				if(yieldResult.value->error) {
					std::rethrow_exception(yieldResult.value->error);
				} else {
					return YieldResult{
						.value=std::move(yieldResult.value->result),
						.done=yieldResult.done
					};
				}
			} else {
				return YieldResult{
					.value=std::nullopt,
					.done=yieldResult.done
				};
			}
		});
	}

	template<typename Yield, typename Next>
	template<typename _Next,
		typename std::enable_if<(std::is_same<_Next,Next>::value && std::is_same<_Next,void>::value), std::nullptr_t>::type>
	Promise<typename ContinuousGenerator<Yield,Next>::YieldResult> ContinuousGenerator<Yield,Next>::next() {
		return BaseGenerator::next().template map<YieldResult>([=](BaseYieldResult yieldResult) {
			if(yieldResult.value) {
				if(yieldResult.value->error) {
					std::rethrow_exception(yieldResult.value->error);
				} else {
					if constexpr(std::is_same<Yield,void>::value) {
						return YieldResult{
							.done=yieldResult.done
						};
					} else {
						return YieldResult{
							.value=std::move(yieldResult.value->result),
							.done=yieldResult.done
						};
					}
				}
			} else {
				return YieldResult{
					.value=std::nullopt,
					.done=yieldResult.done
				};
			}
		});
	}



	template<typename Yield, typename Next>
	template<typename NewYield>
	ContinuousGenerator<NewYield,Next> ContinuousGenerator<Yield,Next>::map(DispatchQueue* queue, Mapper<NewYield> transform) {
		return BaseGenerator::template map<NewYield>(queue, [=](ContinuousGeneratorResult<Yield> genResult) {
			if(genResult.error) {
				return ContinuousGeneratorResult<NewYield>{
					.error=genResult.error
				};
			} else {
				if constexpr(std::is_same<Optionalized<Yield>,Yield>::value) {
					return ContinuousGeneratorResult<NewYield>{
						.result=transform(std::move(genResult.result))
					};
				} else {
					if(genResult.result.has_value()) {
						return ContinuousGeneratorResult<NewYield>{
							.result=transform(std::move(genResult.result.value()))
						};
					} else {
						return ContinuousGeneratorResult<NewYield>{
							.result=std::nullopt
						};
					}
				}
			}
		});
	}

	template<typename Yield, typename Next>
	template<typename NewYield>
	ContinuousGenerator<NewYield,Next> ContinuousGenerator<Yield,Next>::map(Mapper<NewYield> transform) {
		return map<NewYield>(nullptr, transform);
	}

	template<typename Yield, typename Next>
	template<typename NewYield>
	ContinuousGenerator<NewYield,Next> ContinuousGenerator<Yield,Next>::mapAsync(DispatchQueue* queue, Mapper<Promise<NewYield>> transform) {
		return BaseGenerator::template mapAsync<NewYield>(queue, [=](ContinuousGeneratorResult<Yield> genResult) {
			if(genResult.error) {
				return ContinuousGeneratorResult<NewYield>{
					.error=genResult.error
				};
			} else {
				if constexpr(std::is_same<Optionalized<Yield>,Yield>::value) {
					return transform(std::move(genResult.result)).map<ContinuousGeneratorResult<NewYield>([=](auto result) {
						return ContinuousGeneratorResult<NewYield>{
							.result=result
						};
					});
				} else {
					if(genResult.result.has_value()) {
						return transform(std::move(genResult.result.value())).map<ContinuousGeneratorResult<NewYield>([=](auto result) {
							return ContinuousGeneratorResult<NewYield>{
								.result=result
							};
						});
					} else {
						return ContinuousGeneratorResult<NewYield>{
							.result=std::nullopt
						};
					}
				}
			}
		});
	}

	template<typename Yield, typename Next>
	template<typename NewYield>
	ContinuousGenerator<NewYield,Next> ContinuousGenerator<Yield,Next>::mapAsync(Mapper<Promise<NewYield>> transform) {
		return mapAsync(nullptr, transform);
	}
}
