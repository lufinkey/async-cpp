//
//  AsyncListMutator.impl.h
//  AsyncCpp
//
//  Created by Luis Finke on 7/10/20.
//  Copyright © 2020 Luis Finke. All rights reserved.
//

#pragma once

#include <fgl/async/Common.hpp>
#if defined(ASYNC_CPP_STANDALONE) && !defined(FGL_DONT_USE_DTL)
	#define FGL_DONT_USE_DTL
#endif
#ifndef FGL_DONT_USE_DTL
	#define FGL_ASYNCLIST_USED_DTL
	#include <dtl/dtl.hpp>
#endif

namespace fgl {
	template<typename T>
	class AsyncListOptionalDTLCompare;


	template<typename T>
	AsyncList<T>::Mutator::Mutator(AsyncList<T>* list)
	: list(list), lockCount(0), forwardingMutations(false) {
		//
	}

	template<typename T>
	AsyncList<T>* AsyncList<T>::Mutator::getList() {
		return list;
	}

	template<typename T>
	const AsyncList<T>* AsyncList<T>::Mutator::getList() const {
		return list;
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



	template<typename T>
	template<typename Work>
	void AsyncList<T>::Mutator::lock(Work work) {
		std::unique_lock<std::recursive_mutex> lock(list->mutex);
		FGL_ASSERT(!forwardingMutations, "cannot lock mutator while forwarding mutations");
		size_t listSize = list->itemsSize.value_or(0);
		// increment lock counter
		lockCount++;
		auto f = make_finally([&]() {
			lockCount--;
		});
		// perform work
		work();
		// if we're the last lock
		if(lockCount == 1) {
			// update index markers
			for(auto& mutation : this->mutations) {
				mutation.applyToMarkers(list->indexMarkers, listSize);
			}
			FGL_ASSERT(listSize == list->itemsSize.value_or(0), "listSize should be the same as list->itemsSize");
			
			// swap mutations list
			auto mutations = LinkedList<Mutation>();
			mutations.swap(this->mutations);
			this->mutations.clear();
			
			// prepare to forward mutations
			forwardingMutations = true;
			auto f2 = make_finally([&]() {
				forwardingMutations = false;
			});
			
			// call mutation listeners
			auto listeners = this->listeners;
			for(auto listener : listeners) {
				listener->onAsyncListMutations(list->shared_from_this(), mutations);
			}
		}
	}
	
	template<typename T>
	void AsyncList<T>::Mutator::applyMerge(size_t index, Optional<size_t> listSize, LinkedList<T> items) {
		lock([&]() {
			if(items.size() == 0) {
				return;
			}
			size_t endIndex = index + items.size();
			
			// add insert mutations to get list to correct size
			if(index > list->itemsSize.value_or(0)) {
				this->mutations.push_back(Mutation{
					.type = Mutation::Type::INSERT,
					.index = list->itemsSize.value_or(0),
					.count = (index - list->itemsSize.value_or(0))
				});
			}
			
			// diff items with existing items
			#ifndef FGL_DONT_USE_DTL
			using DiffType = dtl::Diff<Optional<T>, ArrayList<Optional<T>>, AsyncListOptionalDTLCompare<T>>;
			{
				size_t existingItemsLimit = items.size();
				if(listSize.has_value() && endIndex >= listSize.value()) {
					existingItemsLimit = -1;
				}
				auto existingItems = ArrayList<Optional<T>>(list->maybeGetLoadedItems({
					.startIndex=index,
					.limit=existingItemsLimit,
					.onlyValidItems=false
				}));
				bool hasExistingItem = false;
				for(auto& item : existingItems) {
					if(item.has_value()) {
						hasExistingItem = true;
						break;
					}
				}
				if(!hasExistingItem) {
					set(index, items);
					return;
				}
				while(existingItems.size() < items.size() && (index+existingItems.size()) < list->itemsSize.value_or(0)) {
					existingItems.push_back(std::nullopt);
				}
				
				DiffType diff; {
					ArrayList<Optional<T>> overwritingItems;
					overwritingItems.reserve(items.size());
					for(auto& item : items) {
						overwritingItems.pushBack(Optional<T>(item));
					}

					diff = DiffType(existingItems, overwritingItems, AsyncListOptionalDTLCompare<T>(list));
				}
				diff.compose();
				
				LinkedList<T> settingItems;
				
				auto existingItemIt = existingItems.begin();
				size_t existingItemIndex = 0;
				
				bool displacing = false;
				bool displacementRemovesEmpty = false;
				Optional<size_t> nonEmptyRemoveStartIndex;
				size_t displacingStartIndex = 0;
				size_t indexRemovalCount = 0;
				std::list<T> addingItems;
				std::list<Mutation> newRemoveMutations;
				size_t newListIndex = index;
				
				size_t maxLookAhead = list->delegate->getAsyncListChunkSize(list);
				if(maxLookAhead < items.size()) {
					maxLookAhead = items.size();
				}
				
				auto itemsIt = items.begin();
				
				std::list<Mutation> removeMutations;
				std::list<Mutation> addMutations;
				
				// begin add/delete chain function
				auto beginDisplacementChain = [&]() {
					displacing = true;
					displacementRemovesEmpty = false;
					indexRemovalCount = 0;
					nonEmptyRemoveStartIndex = std::nullopt;
					displacingStartIndex = existingItemIndex;
					newRemoveMutations.clear();
				};
				
				// finish add/delete chain function
				auto endDisplacementChain = [&]() {
					if(displacing) {
						displacing = false;
						// if we've been finding non-empty indexes being removed, add those mutations
						if(nonEmptyRemoveStartIndex) {
							// Remove non-empty indexes
							size_t removeCount = (existingItemIndex - nonEmptyRemoveStartIndex.value());
							// add mutation to the front, since higher indexes should be removed before lower indexes
							newRemoveMutations.push_front(Mutation{
								.type = Mutation::Type::REMOVE,
								.index = index + displacingStartIndex,
								.count = removeCount,
								.upperShiftEndIndex = (index + items.size())
							});
							indexRemovalCount += removeCount;
							nonEmptyRemoveStartIndex = std::nullopt;
						}
						
						size_t totalRemoveCount = existingItemIndex - displacingStartIndex;
						size_t emptyCount = totalRemoveCount - indexRemovalCount;
						// if we have more empty indexes than items being added, remove some of the empty indexes
						if(emptyCount > addingItems.size()) {
							size_t emptyRemoveCount = emptyCount - addingItems.size();
							newRemoveMutations.push_back(Mutation{
								.type = Mutation::Type::REMOVE,
								.index = index + displacingStartIndex,
								.count = emptyRemoveCount,
								.upperShiftEndIndex = (index + items.size())
							});
							emptyCount -= emptyRemoveCount;
						}
						
						// if we're not removing any empty indexes and we're either at the top or bottom (or both) of the list
						if(list->items.size() > 0 && !displacementRemovesEmpty && (displacingStartIndex == 0 || (displacingStartIndex + totalRemoveCount) >= (index + items.size()))) {
							ArrayList<Optional<Mutation>> addingItemMutations;
							addingItemMutations.resize(addingItems.size(), std::nullopt);
							// if this displacement chunk is at the bottom of the list
							//  then go down and find the items in common with the bottom added items and move them
							if(displacingStartIndex == 0) {
								auto dispIt = std::make_reverse_iterator(list->items.lower_bound(index));
								auto addingItemsIt = addingItems.rbegin();
								size_t addingItemsIndex = addingItems.size() - 1;
								while(list->items.size() > 0 && dispIt != list->items.rend() && addingItemsIt != addingItems.rend()) {
									bool foundMatch = false;
									auto checkDispIt = dispIt;
									size_t lookAheadCount = 0;
									while(checkDispIt != list->items.rend() && lookAheadCount < maxLookAhead) {
										if(list->delegate->areAsyncListItemsEqual(list, checkDispIt->second.item, *addingItemsIt)) {
											foundMatch = true;
											size_t prevIndex = checkDispIt->first;
											size_t newIndex = index + settingItems.size() + addingItemsIndex;
											list->delegate->mergeAsyncListItem(list, *addingItemsIt, checkDispIt->second.item);
											addingItemMutations[addingItemsIndex] = Mutation{
												.type = Mutation::Type::LIFT_AND_INSERT,
												.index = prevIndex,
												.count = 1,
												.newIndex = newIndex,
												.upperShiftEndIndex = (index + items.size())
											};
											auto fcheckDispIt = std::prev(checkDispIt.base(), 1);
											fcheckDispIt = list->items.erase(fcheckDispIt);
											checkDispIt = std::make_reverse_iterator(fcheckDispIt);
											if(list->items.size() > 0) {
												checkDispIt++;
											}
											dispIt = checkDispIt;
											break;
										}
										lookAheadCount++;
									}
									addingItemsIt++;
									addingItemsIndex--;
								}
							}
							// if this displacement chunk is at the top of the list
							//  then go up and find the items in common with the top added items and move them
							if((displacingStartIndex + totalRemoveCount) >= (index + items.size())) {
								auto dispIt = list->items.lower_bound(index+items.size());
								auto addingItemsIt = addingItems.begin();
								size_t addingItemsIndex = 0;
								while(list->items.size() > 0 && dispIt != list->items.end() && addingItemsIt != addingItems.end()) {
									bool foundMatch = false;
									auto checkDispIt = dispIt;
									size_t lookAheadCount = 0;
									while(checkDispIt != list->items.end() && lookAheadCount < maxLookAhead) {
										if(!addingItemMutations[addingItemsIndex].has_value()
										   && list->delegate->areAsyncListItemsEqual(list, checkDispIt->second.item, *addingItemsIt)) {
											foundMatch = true;
											size_t prevIndex = checkDispIt->first;
											size_t newIndex = index + settingItems.size() + addingItemsIndex;
											list->delegate->mergeAsyncListItem(list, *addingItemsIt, checkDispIt->second.item);
											addingItemMutations[addingItemsIndex] = Mutation{
												.type = Mutation::Type::LIFT_AND_INSERT,
												.index = prevIndex,
												.count = 1,
												.newIndex = newIndex,
												.upperShiftEndIndex = (index + items.size())
											};
											checkDispIt = list->items.erase(checkDispIt);
											if(list->items.size() > 0) {
												checkDispIt++;
											}
											dispIt = checkDispIt;
											break;
										}
										lookAheadCount++;
									}
									addingItemsIt++;
									addingItemsIndex++;
								}
							}
							// loop through mutations and add chained mutations
							Optional<size_t> emptyStartIndex;
							auto completeInsertMutationChain = [&](auto i) {
								if(emptyStartIndex) {
									size_t insertIndex = emptyStartIndex.value();
									emptyStartIndex = std::nullopt;
									size_t insertCount = (i - insertIndex);
									if(emptyCount >= insertCount) {
										emptyCount -= insertCount;
										insertCount = 0;
									}
									else if(emptyCount > 0) {
										insertIndex += emptyCount;
										insertCount -= emptyCount;
										emptyCount = 0;
									}
									if(insertCount > 0) {
										addMutations.push_back(Mutation{
											.type = Mutation::Type::INSERT,
											.index = index + settingItems.size() + insertIndex,
											.count = insertCount,
											.upperShiftEndIndex = (index + items.size())
										});
									}
								}
							};
							for(auto [i, mutation] : enumerate(addingItemMutations)) {
								if(mutation.has_value()) {
									if(emptyStartIndex) {
										completeInsertMutationChain(i);
									}
									addMutations.push_back(std::move(mutation.value()));
								}
								else if(!emptyStartIndex) {
									emptyStartIndex = i;
								}
							}
							if(emptyStartIndex) {
								completeInsertMutationChain(addingItems.size());
							}
						}
						// otherwise if we're not at the top or the bottom of the list
						else {
							if(addingItems.size() > emptyCount) {
								size_t addCount = addingItems.size() - emptyCount;
								addMutations.push_back(Mutation{
									.type = Mutation::Type::INSERT,
									.index = index + settingItems.size() + emptyCount,
									.count = addCount,
									.upperShiftEndIndex = (index + items.size())
								});
							}
						}
						
						// add addingItems to settingItems
						if(addingItems.size() > 0) {
							settingItems.splice(settingItems.end(), addingItems);
							addingItems.clear();
						}
						// add newRemoveMutations to removeMutations and clear
						removeMutations.splice(removeMutations.begin(), newRemoveMutations);
						newRemoveMutations.clear();
					}
				};
				
				Optional<size_t> overflowInsertStart;
				
				// begin overflow insert chain
				auto beginOverflowInsertChain = [&]() {
					if(!overflowInsertStart && newListIndex >= list->itemsSize.value_or(0)) {
						overflowInsertStart = newListIndex;
					}
				};
				
				// end overflow insert chain
				auto endOverflowInsertChain = [&]() {
					if(overflowInsertStart) {
						size_t startIndex = overflowInsertStart.value();
						overflowInsertStart = std::nullopt;
						size_t count = newListIndex - startIndex;
						addMutations.push_back(Mutation{
							.type = Mutation::Type::MOVE,
							.index = startIndex,
							.count = count,
							.newIndex = startIndex,
							.upperShiftEndIndex = (index + items.size())
						});
					}
				};
				
				// loop through diff
				auto sesSeq = diff.getSes().getSequence();
				for(auto it=sesSeq.begin(); it != sesSeq.end(); it++) {
					switch(it->second.type) {
						
						case dtl::SES_DELETE: {
							if(overflowInsertStart) {
								endOverflowInsertChain();
							}
							if(!displacing) {
								beginDisplacementChain();
							}
							bool isEmpty = !it->first.has_value();
							if(isEmpty) {
								displacementRemovesEmpty = true;
							}
							else if(!nonEmptyRemoveStartIndex) {
								nonEmptyRemoveStartIndex = existingItemIndex;
							}
							if(isEmpty && nonEmptyRemoveStartIndex) {
								// Remove non-empty indexes
								size_t removeCount = (existingItemIndex - nonEmptyRemoveStartIndex.value());
								newRemoveMutations.push_front(Mutation{
									.type = Mutation::Type::REMOVE,
									.index = displacingStartIndex,
									.count = removeCount,
									.upperShiftEndIndex = (index + items.size())
								});
								indexRemovalCount += removeCount;
								nonEmptyRemoveStartIndex = std::nullopt;
							}
							existingItemIt++;
							existingItemIndex++;
						} break;
						
						case dtl::SES_ADD: {
							if(overflowInsertStart) {
								endOverflowInsertChain();
							}
							if(!displacing) {
								beginDisplacementChain();
							}
							addingItems.pushBack(it->first.value());
							newListIndex++;
							itemsIt++;
						} break;
						
						case dtl::SES_COMMON: {
							if(displacing) {
								endDisplacementChain();
							}
							if(!overflowInsertStart && newListIndex >= list->itemsSize.value_or(0)) {
								beginOverflowInsertChain();
							}
							list->delegate->mergeAsyncListItem(list, *itemsIt, existingItemIt->value());
							settingItems.pushBack(std::move(*itemsIt));
							existingItemIt++;
							existingItemIndex++;
							newListIndex++;
							itemsIt++;
						} break;
					}
				}
				if(displacing) {
					endDisplacementChain();
				}
				if(overflowInsertStart) {
					endOverflowInsertChain();
				}
				
				FGL_ASSERT(settingItems.size() == items.size(), "settingItems should be the same size as items");
				items = std::move(settingItems);
				
				this->mutations.splice(this->mutations.end(), removeMutations);
				this->mutations.splice(this->mutations.end(), addMutations);
			}
			#else
			{
				size_t existingItemsLimit = items.size();
				auto existingItems = ArrayList<Optional<T>>(list->maybeGetLoadedItems({
					.startIndex=index,
					.limit=existingItemsLimit,
					.onlyValidItems=false
				}));
				auto itemsIt = items.begin();
				for(auto& existingItem : existingItems) {
					// TODO add removal and insert mutations
					if(existingItem.has_value() && list->delegate->areAsyncListItemsEqual(list, existingItem.value(), *itemsIt)) {
						list->delegate->mergeAsyncListItem(list, *itemsIt, existingItem.value());
					}
					itemsIt++;
				}
			}
			#endif
			
			// if we're at the end of the list, remove items above the end of the list
			if(listSize.has_value() && endIndex >= listSize.value() && list->items.size() > 0) {
				auto removeBegin = list->items.lower_bound(endIndex);
				auto removeEnd = list->items.end();
				if(removeBegin != removeEnd) {
					auto removeLastIndex = std::prev(removeEnd, 1)->index;
					list->items.erase(removeBegin, removeEnd);
					// add mutations
					this->mutations.push_back(Mutation{
						.type = Mutation::Type::REMOVE,
						.index = endIndex,
						.count = ((removeLastIndex + 1) - endIndex)
					});
				}
			}
			
			// apply items and size
			set(index, std::move(items));
			if(listSize.has_value()) {
				resize(listSize.value());
			}
			else if(endIndex > list->listSize.value_or(0)) {
				list->itemsSize = endIndex;
			}
		});
	}

	template<typename T>
	void AsyncList<T>::Mutator::apply(size_t index, LinkedList<T> items) {
		applyMerge(index, std::nullopt, items);
	}

	template<typename T>
	void AsyncList<T>::Mutator::applyAndResize(size_t index, size_t listSize, LinkedList<T> items) {
		applyMerge(index, listSize, items);
	}

	template<typename T>
	void AsyncList<T>::Mutator::set(size_t index, LinkedList<T> items) {
		lock([&]() {
			size_t i=index;
			for(auto& item : items) {
				list->items.insert_or_assign(i, AsyncList<T>::ItemNode{
					.item=std::move(item),
					.valid=true
				});
				i++;
			}
		});
	}

	template<typename T>
	void AsyncList<T>::Mutator::insert(size_t index, LinkedList<T> items) {
		lock([&]() {
			size_t insertCount = items.size();
			if(insertCount == 0) {
				return;
			}
			// update list size
			size_t prevItemsSize = list->itemsSize.value_or(0);
			size_t mutationInsertIndex = index;
			size_t mutationInsertCount = items.size();
			if(index > prevItemsSize) {
				mutationInsertIndex = prevItemsSize;
				mutationInsertCount = (index + items.size()) - mutationInsertIndex;
				size_t padInsertCount = (index - prevItemsSize);
				list->itemsSize = prevItemsSize + padInsertCount + insertCount;
			} else {
				list->itemsSize = prevItemsSize + insertCount;
			}
			// update keys for elements above insert range
			auto revIt=list->items.rbegin();
			for(; (revIt != list->items.rend()) && (revIt->first >= index); revIt++) {
				auto insertIt = revIt.base();
				auto extractIt = std::prev(insertIt, 1);
				auto node = list->items.extract(extractIt);
				node.key() += insertCount;
				insertIt = list->items.insert(insertIt, node);
				revIt = std::prev(std::make_reverse_iterator(insertIt), 1);
			}
			// apply new items
			auto listInsertIt = revIt.base();
			size_t i=index;
			for(auto& item : items) {
				using list_item = typename decltype(list->items)::value_type;
				listInsertIt = list->items.insert(listInsertIt, list_item(i, AsyncList<T>::ItemNode{
					.item=std::move(item),
					.valid=true
				}));
				i++;
				listInsertIt++;
			}
			// add mutation
			this->mutations.push_back(Mutation{
				.type = Mutation::Type::INSERT,
				.index = mutationInsertIndex,
				.count = mutationInsertCount
			});
		});
	}

	template<typename T>
	void AsyncList<T>::Mutator::remove(size_t index, size_t count) {
		lock([&]() {
			if(count == 0) {
				return;
			}
			size_t endIndex = index + count;
			// remove items from list
			auto removeStartIt = list->items.lower_bound(index);
			auto removeEndIt = list->items.lower_bound(endIndex);
			auto it = list->items.erase(removeStartIt, removeEndIt);
			// update keys for elements above remove range
			for(; it != list->items.end(); it++) {
				auto insertIt = std::next(it, 1);
				auto node = list->items.extract(it);
				node.key() -= count;
				it = list->items.insert(insertIt, node);
			}
			// update list size
			if(list->itemsSize.has_value() && index < list->itemsSize.value()) {
				size_t removeCount = count;
				if((index + count) > list->itemsSize.value()) {
					removeCount = list->itemsSize.value() - index;
				}
				list->itemsSize.value() -= removeCount;
			}
			// add mutation
			this->mutations.push_back(Mutation{
				.type = Mutation::Type::REMOVE,
				.index = index,
				.count = count
			});
		});
	}

	template<typename T>
	void AsyncList<T>::Mutator::move(size_t index, size_t count, size_t newIndex) {
		lock([&]() {
			if(count == 0) {
				return;
			}
			else if(index == newIndex) {
				return;
			}
			size_t endIndex = (index + count);
			// extract items from list
			using node_type = typename decltype(list->items)::node_type;
			std::list<node_type> extractedNodes;
			auto it = list->items.lower_bound(index);
			while(it != list->items.end() && it->first < endIndex) {
				auto nextIt = std::next(it, 1);
				auto node = list->items.extract(it);
				extractedNodes.emplace_back(node);
				it = nextIt;
			}
			// shift items displaced by move
			if(newIndex < index) {
				for(auto revIt = std::make_reverse_iterator(it);
				   (revIt != list->items.rend()) && (revIt->first >= newIndex);
				   revIt++) {
					auto insertIt = revIt.base();
					auto extractIt = std::prev(insertIt, 1);
					auto node = list->items.extract(extractIt);
					node.key() += count;
					revIt = list->items.insert(insertIt, node);
				}
			}
			else if(newIndex > index) {
				for(auto revIt = std::make_reverse_iterator(list->items.upper_bound(newIndex));
				   (revIt != list->items.rend()) && (revIt->first >= index);
				   revIt++) {
					auto insertIt = revIt.base();
					auto extractIt = std::prev(insertIt, 1);
					auto node = list->items.extract(extractIt);
					node.key() += count;
					revIt = list->items.insert(insertIt, node);
				}
			}
			// reinsert extracted items
			it = list->items.lower_bound(newIndex);
			for(auto& node : extractedNodes) {
				it = list->items.insert(it, std::move(node));
				it++;
			}
			// get insertion count to pad list
			size_t prevListSize = list->itemsSize.value_or(0);
			size_t shrunkListSize = prevListSize;
			if(shrunkListSize > count) {
				shrunkListSize -= count;
			} else {
				shrunkListSize = 0;
			}
			size_t paddedInsertCount = 0;
			size_t moveInsertCount = 0;
			bool indexInside = true;
			bool newIndexInside = true;
			if(index > prevListSize) {
				moveInsertCount += count;
				indexInside = false;
			}
			else if(index == prevListSize) {
				paddedInsertCount += (endIndex - prevListSize);
				indexInside = false;
			}
			else if(endIndex > prevListSize) {
				paddedInsertCount += (endIndex - prevListSize);
			}
			if(newIndex > shrunkListSize) {
				paddedInsertCount += (newIndex - shrunkListSize);
				newIndexInside = false;
			}
			// resize list
			if(indexInside || newIndexInside) {
				list->itemsSize = list->itemsSize.value_or(0) + paddedInsertCount + moveInsertCount;
			}
			// add mutations
			if(indexInside || newIndexInside) {
				if(paddedInsertCount > 0) {
					this->mutations.push_back(Mutation{
						.type = Mutation::Type::INSERT,
						.index = prevListSize,
						.count = paddedInsertCount
					});
				}
			}
			this->mutations.push_back(Mutation{
				.type = Mutation::Type::MOVE,
				.index = index,
				.count = count,
				.newIndex = newIndex
			});
		});
	}

	template<typename T>
	void AsyncList<T>::Mutator::resize(size_t count) {
		lock([&]() {
			// TODO possibly shift some of the overflowing items into open spaces
			// invalidate items above resize
			size_t maxSize = (list->items.size() > 0) ? (std::prev(list->items.end(), 1)->first+1) : count;
			if(maxSize > count) {
				invalidate(count, (maxSize-count));
			}
			// set new items size
			list->itemsSize = count;
			// add mutations
			this->mutations.push_back(Mutation{
				.type = Mutation::Type::RESIZE,
				.count = count
			});
		});
	}

	template<typename T>
	void AsyncList<T>::Mutator::invalidate(size_t index, size_t count) {
		lock([&]() {
			size_t endIndex = index + count;
			for(auto it=list->items.lower_bound(index), end=list->items.end(); it!=end; it++) {
				if(it->first >= endIndex) {
					break;
				} else {
					it->second.valid = false;
				}
			}
		});
	}

	template<typename T>
	void AsyncList<T>::Mutator::invalidateAll() {
		lock([&]() {
			for(auto & pair : list->items) {
				pair.second.valid = false;
			}
		});
	}




#ifndef FGL_DONT_USE_DTL
	template<typename T>
	class AsyncListOptionalDTLCompare: public dtl::Compare<Optional<T>> {
	public:
		AsyncListOptionalDTLCompare(): list(nullptr) {}
		AsyncListOptionalDTLCompare(AsyncList<T>* list): list(list) {}
		
		virtual inline bool impl(const Optional<T>& e1, const Optional<T>& e2) const {
			return
				(!e1.has_value() && !e2.has_value())
				|| (e1.has_value() && e2.has_value() && list->delegate->areAsyncListItemsEqual(list, e1.value(), e2.value()));
		}
		
	private:
		AsyncList<T>* list;
	};
	#endif
}

