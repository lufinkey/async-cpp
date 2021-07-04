//
//  ContinuousGenerator.hpp
//  AsyncCpp
//
//  Created by Luis Finke on 5/9/20.
//  Copyright © 2020 Luis Finke. All rights reserved.
//

#pragma once

#include <fgl/async/Common.hpp>
#include <fgl/async/Generator.hpp>
#include <exception>

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
		using YieldType = Yield;
		using OptionalYieldType = Optionalized<Yield>;
		OptionalYieldType result;
		std::exception_ptr error;
	};

	template<>
	struct ContinuousGeneratorResult<void> {
		using YieldType = void;
		using OptionalYieldType = void;
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
		
		template<typename _Yield=Yield,
			typename std::enable_if<(std::is_same<_Yield,Yield>::value &&
				!std::is_same<_Yield,void>::value), std::nullptr_t>::type = nullptr>
		static ContinuousGenerator<Yield,Next> resolve(_Yield);
		static ContinuousGenerator<Yield,Next> resolve();
		template<typename ErrorType>
		static ContinuousGenerator<Yield,Next> reject(ErrorType error);
		
		template<typename _Next=Next,
			typename std::enable_if<(std::is_same<_Next,Next>::value && !std::is_same<_Next,void>::value), std::nullptr_t>::type = nullptr>
		inline Promise<YieldResult> next(_Next nextValue);
		
		template<typename _Next=Next,
			typename std::enable_if<(std::is_same<_Next,Next>::value && std::is_same<_Next,void>::value), std::nullptr_t>::type = nullptr>
		inline Promise<YieldResult> next();
		
		template<typename Transform>
		auto map(DispatchQueue* queue, Transform transform);
		template<typename Transform>
		auto map(Transform transform);
		template<typename Transform>
		auto mapAsync(DispatchQueue* queue, Transform transform);
		template<typename Transform>
		auto mapAsync(Transform transform);
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
				return yieldReturner().map([=](YieldResult yieldResult) -> BaseYieldResult {
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
				return yieldReturner(std::move(next)).map([=](YieldResult yieldResult) -> BaseYieldResult {
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
	template<typename _Yield,
		typename std::enable_if<(std::is_same<_Yield,Yield>::value &&
			!std::is_same<_Yield,void>::value), std::nullptr_t>::type>
	ContinuousGenerator<Yield,Next> ContinuousGenerator<Yield,Next>::resolve(_Yield yieldVal) {
		if constexpr(std::is_same<Next,void>::value) {
			return Generator<Yield,Next>([=]() {
				return Promise<YieldResult>::resolve({
					.value=yieldVal,
					.done=true
				});
			});
		} else {
			return Generator<Yield,Next>([=](Next nextVal) {
				return Promise<YieldResult>::resolve(YieldResult{
					.value=yieldVal,
					.done=true
				});
			});
		}
	}

	template<typename Yield, typename Next>
	ContinuousGenerator<Yield,Next> ContinuousGenerator<Yield,Next>::resolve() {
		if constexpr(std::is_same<Next,void>::value) {
			return Generator<Yield,Next>([=]() {
				return Promise<YieldResult>::resolve(YieldResult{
					.done=true
				});
			});
		} else {
			return Generator<Yield,Next>([=](Next nextVal) {
				return Promise<YieldResult>::resolve(YieldResult{
					.done=true
				});
			});
		}
	}

	template<typename Yield, typename Next>
	template<typename ErrorType>
	ContinuousGenerator<Yield,Next> ContinuousGenerator<Yield,Next>::reject(ErrorType error) {
		if constexpr(std::is_same<Next,void>::value) {
			return Generator<Yield,Next>([=]() {
				return Promise<YieldResult>::reject(error);
			});
		} else {
			return Generator<Yield,Next>([=](Next nextVal) {
				return Promise<YieldResult>::reject(error);
			});
		}
	}



	template<typename Yield, typename Next>
	template<typename _Next,
		typename std::enable_if<(std::is_same<_Next,Next>::value && !std::is_same<_Next,void>::value), std::nullptr_t>::type>
	Promise<typename ContinuousGenerator<Yield,Next>::YieldResult> ContinuousGenerator<Yield,Next>::next(_Next nextValue) {
		return BaseGenerator::next(std::move(nextValue)).map([=](BaseYieldResult yieldResult) -> YieldResult {
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
		return BaseGenerator::next().map([=](BaseYieldResult yieldResult) -> YieldResult {
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
	template<typename Transform>
	auto ContinuousGenerator<Yield,Next>::map(DispatchQueue* queue, Transform transform) {
		if constexpr(std::is_same<Yield,void>::value) {
			using NewYield = decltype(transform());
			using NewGenResult = ContinuousGeneratorResult<NewYield>;
			using NewGen = ContinuousGenerator<NewYield,Next>;
			return NewGen(BaseGenerator::map(queue, [=](ContinuousGeneratorResult<Yield> genResult) -> NewGenResult {
				if(genResult.error) {
					return NewGenResult{
						.error=genResult.error
					};
				} else {
					if constexpr(std::is_same<NewYield,void>::value) {
						try {
							transform();
						} catch(...) {
							return NewGenResult{
								.error=std::current_exception()
							};
						}
						return NewGenResult{};
					} else {
						try {
							return NewGenResult{
								.result=transform()
							};
						} catch(...) {
							return NewGenResult{
								.error=std::current_exception()
							};
						}
					}
				}
			}));
		}
		else {
			using NewYield = decltype(transform(std::declval<Yield>()));
			using NewGenResult = ContinuousGeneratorResult<NewYield>;
			using NewGen = ContinuousGenerator<NewYield,Next>;
			return NewGen(BaseGenerator::map(queue, [=](ContinuousGeneratorResult<Yield> genResult) -> NewGenResult {
				if(genResult.error) {
					return NewGenResult{
						.error=genResult.error
					};
				} else {
					if constexpr(std::is_same<Optionalized<Yield>,Yield>::value) {
						if constexpr(std::is_same<NewYield,void>::value) {
							try {
								transform(std::move(genResult.result));
							} catch(...) {
								return NewGenResult{
									.error=std::current_exception()
								};
							}
							return NewGenResult{};
						} else {
							try {
								return NewGenResult{
									.result=transform(std::move(genResult.result))
								};
							} catch(...) {
								return NewGenResult{
									.error=std::current_exception()
								};
							}
						}
					} else {
						if(genResult.result.has_value()) {
							if constexpr(std::is_same<NewYield,void>::value) {
								try {
									transform(std::move(genResult.result.value()));
								} catch(...) {
									return NewGenResult{
										.error=std::current_exception()
									};
								}
								return NewGenResult{};
							} else {
								try {
									return NewGenResult{
										.result=transform(std::move(genResult.result.value()))
									};
								} catch(...) {
									return NewGenResult{
										.error=std::current_exception()
									};
								}
							}
						} else {
							if constexpr(std::is_same<NewYield,void>::value) {
								return ContinuousGeneratorResult<NewYield>{};
							} else {
								return ContinuousGeneratorResult<NewYield>{
									.result=std::nullopt
								};
							}
						}
					}
				}
			}));
		}
	}

	template<typename Yield, typename Next>
	template<typename Transform>
	auto ContinuousGenerator<Yield,Next>::map(Transform transform) {
		return map<Transform>(nullptr, transform);
	}

	template<typename Yield, typename Next>
	template<typename Transform>
	auto ContinuousGenerator<Yield,Next>::mapAsync(DispatchQueue* queue, Transform transform) {
		if constexpr(std::is_same<Yield,void>::value) {
			using NewYieldPromise = decltype(transform());
			using NewYield = typename IsPromise<NewYieldPromise>::ResultType;
			using NewGenResult = ContinuousGeneratorResult<NewYield>;
			using NewGen = ContinuousGenerator<NewYield,Next>;
			return NewGen(BaseGenerator::mapAsync(queue, [=](auto genResult) -> NewGenResult {
				if(genResult.error) {
					return NewGenResult{
						.error=genResult.error
					};
				} else {
					if constexpr(std::is_same<NewYield,void>::value) {
						return transform().map([=]() {
							return NewGenResult{};
						});
					} else {
						return transform().map([=](auto newValue) {
							return NewGenResult{
								.result=newValue
							};
						});
					}
				}
			}));
		} else {
			using NewYieldPromise = decltype(transform(std::declval<Yield>()));
			using NewYield = typename IsPromise<NewYieldPromise>::ResultType;
			using NewGenResult = ContinuousGeneratorResult<NewYield>;
			using NewGen = ContinuousGenerator<NewYield,Next>;
			return NewGen(BaseGenerator::mapAsync(queue, [=](auto genResult) -> NewGenResult {
				if(genResult.error) {
					return NewGenResult{
						.error=genResult.error
					};
				} else {
					if constexpr(std::is_same<NewYield,void>::value) {
						if constexpr(std::is_same<Optionalized<Yield>,Yield>::value) {
							return transform(std::move(genResult.result)).map([=]() -> NewGenResult {
								return NewGenResult{};
							});
						} else {
							if(genResult.result.has_value()) {
								return transform(std::move(genResult.result.value())).map([=]() -> NewGenResult {
									return NewGenResult{};
								});
							} else {
								return NewGenResult{};
							}
						}
					} else {
						if constexpr(std::is_same<Optionalized<Yield>,Yield>::value) {
							return transform(std::move(genResult.result)).map([=](auto result) -> NewGenResult {
								return NewGenResult{
									.result=result
								};
							});
						} else {
							if(genResult.result.has_value()) {
								return transform(std::move(genResult.result.value())).map([=](auto result) -> NewGenResult {
									return NewGenResult{
										.result=result
									};
								});
							} else {
								return NewGenResult{
									.result=std::nullopt
								};
							}
						}
					}
				}
			}));
		}
	}

	template<typename Yield, typename Next>
	template<typename Transform>
	auto ContinuousGenerator<Yield,Next>::mapAsync(Transform transform) {
		return mapAsync(nullptr, transform);
	}
}
