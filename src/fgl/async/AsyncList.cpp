//
//  AsyncList.cpp
//  AsyncCpp
//
//  Created by Luis Finke on 11/17/19.
//  Copyright © 2019 Luis Finke. All rights reserved.
//

#include <fgl/async/AsyncList.hpp>

namespace fgl {
	AsyncListIndexMarkerData::AsyncListIndexMarkerData(size_t index, AsyncListIndexMarkerState state)
	: index(index), state(state) {
		switch(state) {
			case AsyncListIndexMarkerState::IN_LIST:
			case AsyncListIndexMarkerState::DISPLACED:
			case AsyncListIndexMarkerState::REMOVED:
				break;
			default:
				FGL_ASSERT(false, "Invalid AsynListIndexMarkerState");
		}
	}

	AsyncListIndexMarker new$(size_t index, AsyncListIndexMarkerState state) {
		return std::make_shared<AsyncListIndexMarkerData>(index, state);
	}
}
