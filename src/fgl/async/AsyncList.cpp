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



	void AsyncListMutation::applyToMarkers(LinkedList<AsyncListIndexMarker>& markers, size_t& listSize) {
		switch(type) {
			case Type::REMOVE: {
				size_t endIndex = (index + count);
				for(auto& marker : markers) {
					// if we're below the indexes being removed
					if(marker->index < index) {
						// nothing needs to happen
						continue;
					}
					// if we're within the range being removed
					else if(marker->index < endIndex) {
						// set the removed index to the beginning of the removed block
						switch(marker->state) {
							case AsyncListIndexMarkerState::IN_LIST:
							case AsyncListIndexMarkerState::DISPLACED:
								marker->state = AsyncListIndexMarkerState::REMOVED;
								marker->index = index;
								break;
							case AsyncListIndexMarkerState::REMOVED:
								marker->index = index;
								break;
						}
					}
					// if we're above the range being removed, but below the "upperShiftEndIndex"
					else if(!upperShiftEndIndex.has_value() || marker->index < upperShiftEndIndex.value()) {
						marker->index -= count;
					}
				}
				// if the removal happened within the ranges of the list
				if(index < listSize) {
					// if the entire removal was within the range of the list
					if(endIndex <= listSize) {
						// subtract the whole removal count
						listSize -= count;
					}
					else {
						// only subtract the count of items within the list range
						size_t removeCount = (listSize - index);
						listSize -= removeCount;
					}
				}
			} break;
				
			case Type::INSERT: {
				size_t newListSize = listSize + count;
				for(auto& marker : markers) {
					// if we're below the insertion index
					if(marker->index < index) {
						// nothing needs to happen
						continue;
					}
					// if we're at or above the insertion index, but below the "upperShiftEndIndex"
					else if(!upperShiftEndIndex.has_value() || marker->index < upperShiftEndIndex.value()) {
						switch(marker->state) {
							case AsyncListIndexMarkerState::IN_LIST:
								// shift up the items above the insertion index
								marker->index += count;
								break;
							case AsyncListIndexMarkerState::DISPLACED:
								// shift up the items above the insertion index
								marker->index += count;
								break;
							case AsyncListIndexMarkerState::REMOVED:
								// if we're above the insertion index (removed indexes equal to the insertion index don't move)
								if(marker->index > index) {
									// shift up the items
									marker->index += count;
								}
								break;
						}
						// ensure we aren't moving indexes above the shift end. This would cause anomalies
						FGL_ASSERT(!upperShiftEndIndex.has_value() || marker->index < upperShiftEndIndex.value(),
							"marker moved above shift end index. upperShiftEndIndex is an optimizer and should not be set outside of mutator");
					}
				}
				listSize = newListSize;
			} break;
			
			case Type::MOVE: {
				FGL_ASSERT(newIndex.has_value(), "newIndex must have value for Mutation::Type::MOVE");
				size_t endIndex = (index + count);
				size_t prevListSize = listSize;
				// ensure newIndex is within range
				size_t shrunkListSize = prevListSize;
				if(shrunkListSize > count) {
					shrunkListSize -= count;
				} else {
					shrunkListSize = 0;
				}
				FGL_ASSERT(newIndex.value() <= shrunkListSize, "newIndex must be within the bounds of the list");
				// get amount that the list needs to resize
				size_t insertCount = 0;
				bool indexInside = true;
				if(index >= prevListSize) {
					insertCount += count;
					indexInside = false;
				}
				else if(endIndex > prevListSize) {
					insertCount += (endIndex - prevListSize);
				}
				// calculate new list size
				size_t newListSize = listSize + insertCount;
				// loop through markers and shift indexes
				for(auto& marker : markers) {
					// if we're inside the range being moved
					if(marker->index >= index && marker->index < endIndex) {
						size_t offset = (marker->index - index);
						switch(marker->state) {
							case AsyncListIndexMarkerState::IN_LIST:
								// move the index to its new location
								marker->index = newIndex.value() + offset;
								break;
							case AsyncListIndexMarkerState::DISPLACED:
								// move the index to its new location
								marker->index = newIndex.value() + offset;
								// if the new index is within the new list size,
								//  then update the marker to be "in the list"
								if(marker->index < newListSize) {
									marker->state = AsyncListIndexMarkerState::IN_LIST;
								}
								break;
							case AsyncListIndexMarkerState::REMOVED:
								// "removed" indexes only move at the beginning of the moved chunk
								//  if they're being moved upward due to inserted indexes below
								if(marker->index == index) {
									if(newIndex.value() < index) {
										marker->index += count;
									}
								} else {
									marker->index = newIndex.value() + offset;
								}
								break;
						}
					}
					// if we're outside the range being moved, but below the "upperShiftEndIndex"
					else if(!upperShiftEndIndex.has_value() || marker->index < upperShiftEndIndex.value()) {
						// if the new index is below the original index
						if(newIndex.value() < index) {
							// if we're between the new index and the original index
							if(marker->index >= newIndex.value() && marker->index < index) {
								// shift indexes up
								switch(marker->state) {
									case AsyncListIndexMarkerState::IN_LIST:
									case AsyncListIndexMarkerState::DISPLACED:
										marker->index += count;
										// ensure we aren't moving indexes above the shift end. This would cause anomalies
										FGL_ASSERT(!upperShiftEndIndex.has_value() || marker->index < upperShiftEndIndex.value(),
											"marker moved above shift end index. upperShiftEndIndex is an optimizer and should not be set outside of mutator");
										break;
									case AsyncListIndexMarkerState::REMOVED:
										// the "removed" indexes at the new index (the bottom of the shift)
										//  don't get moved up because they're technically between indexes
										if(marker->index > newIndex.value()) {
											marker->index += count;
										}
										break;
								}
							}
						}
						// if the new index is above the original index
						else if(newIndex.value() > index) {
							// if we're between the the original end index and the new index
							if(marker->index >= endIndex && marker->index < newIndex.value()) {
								// shift indexes up
								switch(marker->state) {
									case AsyncListIndexMarkerState::IN_LIST:
									case AsyncListIndexMarkerState::DISPLACED:
										marker->index += count;
										// ensure we aren't moving indexes above the shift end. This would cause anomalies
										FGL_ASSERT(!upperShiftEndIndex.has_value() || marker->index < upperShiftEndIndex.value(),
											"marker moved above shift end index. upperShiftEndIndex is an optimizer and should not be set outside of mutator");
										break;
									case AsyncListIndexMarkerState::REMOVED:
										// the "removed" indexes at the original end index (the bottom of the shift)
										//  don't get moved up because they're technically between indexes
										if(marker->index > endIndex) {
											marker->index += count;
										}
										break;
								}
							}
						}
					}
				}
				// resize list
				listSize = newListSize;
			} break;
			
			case Type::LIFT_AND_INSERT: {
				FGL_ASSERT(newIndex.has_value(), "newIndex must have value for Mutation::Type::LIFT_AND_INSERT");
				FGL_ASSERT(newIndex.value() <= listSize, "newIndex must be within the bounds of the list");
				size_t endIndex = (index + count);
				size_t newListSize = listSize + count;
				for(auto& marker : markers) {
					// if we're inside the range being moved
					if(marker->index >= index && marker->index < endIndex) {
						size_t offset = (marker->index - index);
						switch(marker->state) {
							case AsyncListIndexMarkerState::IN_LIST:
								// move the index to its new location
								marker->index = newIndex.value() + offset;
								break;
							case AsyncListIndexMarkerState::DISPLACED:
								// move the index to its new location
								marker->index = newIndex.value() + offset;
								// if the new index is within the new list size,
								//  then update the marker to be "in the list"
								if(marker->index < newListSize) {
									marker->state = AsyncListIndexMarkerState::IN_LIST;
								}
								break;
							case AsyncListIndexMarkerState::REMOVED:
								// "removed" indexes only move at the beginning of the moved chunk
								//  if they're being moved upward due to inserted indexes below
								if(marker->index > index) {
									if(newIndex.value() < index) {
										marker->index += count;
									}
								} else {
									marker->index = newIndex.value() + offset;
								}
								break;
						}
					}
					// if we're at or above the insertion index, but below the "upperShiftEndIndex"
					else if(marker->index >= newIndex.value() && (!upperShiftEndIndex.has_value() || marker->index < upperShiftEndIndex.value())) {
						switch(marker->state) {
							case AsyncListIndexMarkerState::IN_LIST:
								// shift up the indexes
								marker->index += count;
								// ensure we aren't moving indexes above the shift end. This would cause anomalies
								FGL_ASSERT(!upperShiftEndIndex.has_value() || marker->index < upperShiftEndIndex.value(),
									"marker moved above shift end index. upperShiftEndIndex is an optimizer and should not be set outside of mutator");
								break;
							case AsyncListIndexMarkerState::DISPLACED:
								// shift up the indexes
								marker->index += count;
								// ensure we aren't moving indexes above the shift end. This would cause anomalies
								FGL_ASSERT(!upperShiftEndIndex.has_value() || marker->index < upperShiftEndIndex.value(),
									"marker moved above shift end index. upperShiftEndIndex is an optimizer and should not be set outside of mutator");
								break;
							case AsyncListIndexMarkerState::REMOVED:
								// if we're above the insertion index (removed indexes equal to the insertion index don't move)
								if(marker->index > newIndex.value()) {
									// shift up the indexes
									marker->index += count;
									// ensure we aren't moving indexes above the shift end. This would cause anomalies
									FGL_ASSERT(!upperShiftEndIndex.has_value() || marker->index < upperShiftEndIndex.value(),
										"marker moved above shift end index. upperShiftEndIndex is an optimizer and should not be set outside of mutator");
								}
								break;
						}
					}
				}
				// resize list
				listSize = newListSize;
			} break;
			
			case Type::RESIZE: {
				size_t newListSize = count;
				for(auto& marker : markers) {
					if(marker->index < newListSize) {
						switch(marker->state) {
							case AsyncListIndexMarkerState::IN_LIST: {
								// do nothing
							} break;
							case AsyncListIndexMarkerState::DISPLACED: {
								marker->state = AsyncListIndexMarkerState::IN_LIST;
							} break;
							case AsyncListIndexMarkerState::REMOVED: {
								// do nothing
							} break;
						}
					} else {
						switch(marker->state) {
							case AsyncListIndexMarkerState::IN_LIST: {
								marker->state = AsyncListIndexMarkerState::DISPLACED;
							} break;
							case AsyncListIndexMarkerState::DISPLACED: {
								// do nothing
							} break;
							case AsyncListIndexMarkerState::REMOVED: {
								// do nothing
							} break;
						}
					}
				}
				// resize list
				listSize = newListSize;
			} break;
		}
	}
}
