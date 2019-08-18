//
//  DispatchTimeoutResult.hpp
//  AsyncCpp
//
//  Created by Luis Finke on 8/11/19.
//  Copyright © 2019 Luis Finke. All rights reserved.
//

#pragma once

#include <fgl/async/Common.hpp>

namespace fgl {
	enum class DispatchTimeoutResult {
		SUCCESS,
		TIMED_OUT
	};
}
