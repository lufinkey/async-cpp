//
//  AsyncList.impl.hpp
//  AsyncCpp
//
//  Created by Luis Finke on 7/10/20.
//  Copyright © 2020 Luis Finke. All rights reserved.
//

#pragma once

#include <fgl/async/Common.hpp>

namespace fgl {
	template<typename T>
	std::shared_ptr<AsyncList<T>> AsyncList<T>::new$(Options options) {
		return std::make_shared<AsyncList<T>>(options);
	}

	template<typename T>
	AsyncList<T>::AsyncList(Options options)
	: itemsSize(options.initialSize),
	mutationQueue({
		.dispatchQueue=options.dispatchQueue
	}),
	mutator(this), delegate(options.delegate) {
		if(options.delegate == nullptr) {
			throw std::invalid_argument("delegate cannot be null");
		}
		
		// set initial items
		for(auto& pair : options.initialItemsMap) {
			items.insert_or_assign(pair.first, ItemNode{
				.item = pair.second,
				.valid = true
			});
		}
		size_t initialItemsOffset = options.initialItemsOffset;
		for(auto& item : options.initialItems) {
			items.insert_or_assign(initialItemsOffset, ItemNode{
				.item = item,
				.valid = true
			});
			initialItemsOffset++;
		}
	}

	template<typename T>
	void AsyncList<T>::destroy() {
		std::unique_lock<std::recursive_mutex> lock(mutex);
		mutator.reset();
		delegate = nullptr;
	}

	template<typename T>
	void AsyncList<T>::reset() {
		std::unique_lock<std::recursive_mutex> lock(mutex);
		auto self = this->shared_from_this();
		mutator.reset();
		if(mutationQueue.taskCount() > 0) {
			mutate([=](auto mutator) {
				self->mutator.reset();
			});
		}
	}

	template<typename T>
	void AsyncList<T>::resetItems() {
		std::unique_lock<std::recursive_mutex> lock(mutex);
		auto self = this->shared_from_this();
		mutator.resetItems();
		if(mutationQueue.taskCount() > 0) {
			mutate([=](auto mutator) {
				self->mutator.resetItems();
			});
		}
	}

	template<typename T>
	void AsyncList<T>::resetSize() {
		std::unique_lock<std::recursive_mutex> lock(mutex);
		auto self = this->shared_from_this();
		mutator.resetSize();
		if(mutationQueue.taskCount() > 0) {
			mutate([=](auto mutator) {
				self->mutator.resetSize();
			});
		}
	}

	template<typename T>
	const std::map<size_t,typename AsyncList<T>::ItemNode>& AsyncList<T>::getMap() const {
		return items;
	}

	template<typename T>
	Optional<size_t> AsyncList<T>::size() const {
		std::unique_lock<std::recursive_mutex> lock(mutex);
		return itemsSize;
	}

	template<typename T>
	size_t AsyncList<T>::length() const {
		return itemsSize.value_or(0);
	}

	template<typename T>
	size_t AsyncList<T>::capacity() const {
		size_t itemsCapacity = itemsSize.value_or(0);
		if(items.size() > 0) {
			size_t listEnd = std::prev(items.end(), 1)->first + 1;
			if(listEnd > itemsCapacity) {
				itemsCapacity = listEnd;
			}
		}
		return itemsCapacity;
	}

	template<typename T>
	size_t AsyncList<T>::getChunkSize() const {
		std::unique_lock<std::recursive_mutex> lock(mutex);
		if(delegate == nullptr) {
			return 0;
		}
		size_t chunkSize = delegate->getAsyncListChunkSize(this);
		FGL_ASSERT(chunkSize != 0, "AsyncList chunkSize cannot be 0");
		return chunkSize;
	}

	template<typename T>
	AsyncListIndexMarker AsyncList<T>::watchIndex(size_t index) {
		std::unique_lock<std::recursive_mutex> lock(mutex);
		auto state = AsyncListIndexMarkerState::IN_LIST;
		if(index >= itemsSize.value_or(0)) {
			state = AsyncListIndexMarkerState::DISPLACED;
		}
		auto indexMarker = AsyncListIndexMarkerData::new$(index, state);
		indexMarkers.push_back(indexMarker);
		return indexMarker;
	}

	template<typename T>
	AsyncListIndexMarker AsyncList<T>::watchIndex(AsyncListIndexMarker indexMarker) {
		std::unique_lock<std::recursive_mutex> lock(mutex);
		auto it = std::find(indexMarkers.begin(), indexMarkers.end(), indexMarker);
		if(it == indexMarkers.end()) {
			if(indexMarker->index < itemsSize.value_or(0)) {
				if(indexMarker->state == AsyncListIndexMarkerState::DISPLACED) {
					indexMarker->state = AsyncListIndexMarkerState::IN_LIST;
				}
			}
			else {
				if(indexMarker->state == AsyncListIndexMarkerState::IN_LIST) {
					indexMarker->state = AsyncListIndexMarkerState::DISPLACED;
				}
			}
			indexMarkers.push_back(indexMarker);
		}
		return indexMarker;
	}

	template<typename T>
	void AsyncList<T>::unwatchIndex(AsyncListIndexMarker index) {
		std::unique_lock<std::recursive_mutex> lock(mutex);
		auto it = std::find(indexMarkers.begin(), indexMarkers.end(), index);
		if(it != indexMarkers.end()) {
			indexMarkers.erase(it);
		}
	}
	
	template<typename T>
	bool AsyncList<T>::isItemLoaded(size_t index, const AsyncListIndexAccessOptions& options) const {
		std::unique_lock<std::recursive_mutex> lock(mutex);
		auto it = items.find(index);
		if(it == items.end()) {
			return false;
		}
		if(options.onlyValidItems) {
			return it->second.valid;
		}
		return true;
	}

	template<typename T>
	bool AsyncList<T>::areItemsLoaded(size_t index, size_t count, const AsyncListIndexAccessOptions& options) const {
		if(count == 0) {
			#ifndef ASYNC_CPP_STANDALONE
				FGL_WARN(stringify(*this)+"::areItemsLoaded("+stringify(index)+","+stringify(count)+","+stringify(options.onlyValidItems)+") called with count = 0");
			#else
				FGL_WARN("AsyncList::areItemsLoaded called with count = 0");
			#endif
			return false;
		}
		std::unique_lock<std::recursive_mutex> lock(mutex);
		auto it = items.find(index);
		size_t endIndex = index + count;
		size_t nextIndex = index;
		while(it != items.end()) {
			if(options.onlyValidItems && !it->second.valid) {
				return false;
			}
			if(it->first != nextIndex) {
				return false;
			}
			if(it->first == endIndex) {
				return true;
			}
			it++;
			nextIndex++;
		}
		return false;
	}

	template<typename T>
	LinkedList<T> AsyncList<T>::getLoadedItems(const AsyncListGetLoadedItemsOptions& options) const {
		std::unique_lock<std::recursive_mutex> lock(mutex);
		LinkedList<T> loadedItems;
		auto it = items.find(options.startIndex);
		size_t nextIndex = options.startIndex;
		while(it != items.end() && loadedItems.size() < options.limit) {
			if(options.onlyValidItems && !it->second.valid) {
				return loadedItems;
			}
			if(it->first != nextIndex) {
				return loadedItems;
			}
			loadedItems.push_back(it->second.item);
			it++;
			nextIndex++;
		}
		return loadedItems;
	}

	template<typename T>
	LinkedList<Optional<T>> AsyncList<T>::maybeGetLoadedItems(const AsyncListGetLoadedItemsOptions& options) const {
		std::unique_lock<std::recursive_mutex> lock(mutex);
		LinkedList<Optional<T>> loadedItems;
		auto it = items.find(options.startIndex);
		size_t nextIndex = options.startIndex;
		while(it != items.end() && loadedItems.size() < options.limit) {
			if(it->first != nextIndex) {
				for(size_t j=nextIndex; (j < it->first) && (loadedItems.size() < options.limit); j++) {
					loadedItems.push_back(std::nullopt);
				}
				if(loadedItems.size() >= options.limit) {
					break;
				}
				nextIndex = it->first;
			}
			if(options.onlyValidItems && !it->second.valid) {
				loadedItems.push_back(std::nullopt);
			} else {
				loadedItems.push_back(Optional<T>(it->second.item));
			}
			it++;
			nextIndex++;
		}
		return loadedItems;
	}

	template<typename T>
	Optional<typename AsyncList<T>::ItemNode> AsyncList<T>::itemNodeAt(size_t index) const {
		std::unique_lock<std::recursive_mutex> lock(mutex);
		auto it = items.find(index);
		if(it == items.end()) {
			return std::nullopt;
		}
		return it->second;
	}
	
	template<typename T>
	Optional<T> AsyncList<T>::itemAt(size_t index, const AsyncListIndexAccessOptions& options) const {
		std::unique_lock<std::recursive_mutex> lock(mutex);
		auto it = items.find(index);
		if(it == items.end()) {
			return std::nullopt;
		}
		if(options.onlyValidItems && !it->second.valid) {
			return std::nullopt;
		}
		return it->second.item;
	}

	template<typename T>
	Promise<Optional<T>> AsyncList<T>::getItem(size_t index, AsyncListGetItemOptions options) {
		std::unique_lock<std::recursive_mutex> lock(mutex);
		if(!options.forceReload && mutationQueue.taskCount() > 0) {
			auto it = items.find(index);
			if(it != items.end() && it->second.valid) {
				return Promise<Optional<T>>::resolve(it->second.item);
			}
		}
		auto indexMarker = AsyncListIndexMarkerData::new$(index, AsyncListIndexMarkerState::IN_LIST);
		if(options.trackIndexChanges) {
			watchIndex(indexMarker);
		}
		auto self = this->shared_from_this();
		return Promise<Optional<T>>([=](auto resolve, auto reject) {
			self->mutationQueue.run([=](auto task) -> Promise<void> {
				std::unique_lock<std::recursive_mutex> lock(mutex);
				if(self->delegate == nullptr) {
					if(options.trackIndexChanges) {
						self->unwatchIndex(indexMarker);
					}
					resolve(std::nullopt);
					return Promise<void>::resolve();
				}
				size_t index = indexMarker->index;
				if(!options.forceReload) {
					auto it = items.find(index);
					if(it != items.end() && it->second.valid) {
						resolve(it->second.item);
						return Promise<void>::resolve();
					}
				}
				size_t chunkSize = self->getChunkSize();
				size_t chunkStartIndex = chunkStartIndexForIndex(index, chunkSize);
				return self->delegate->loadAsyncListItems(&self->mutator, chunkStartIndex, chunkSize, options.loadOptions)
				.finally(self->mutationQueue.dispatchQueue(), [=]() {
					if(options.trackIndexChanges) {
						self->unwatchIndex(indexMarker);
					}
				})
				.then(self->mutationQueue.dispatchQueue(), [=]() {
					resolve(self->itemAt(indexMarker->index));
				}, reject);
			});
		});
	}

	template<typename T>
	Promise<LinkedList<T>> AsyncList<T>::getItems(size_t index, size_t count, AsyncListGetItemOptions options) {
		std::unique_lock<std::recursive_mutex> lock(mutex);
		if(!options.forceReload && mutationQueue.taskCount() > 0) {
			auto loadedItems = getLoadedItems({
				.startIndex = index,
				.limit = count
			});
			size_t endIndex = index + count;
			if(itemsSize.has_value() && endIndex > itemsSize.value()) {
				endIndex = itemsSize.value();
			}
			if((index + loadedItems.size()) >= endIndex) {
				return Promise<LinkedList<T>>::resolve(loadedItems);
			}
		}
		auto indexMarker = AsyncListIndexMarkerData::new$(index, AsyncListIndexMarkerState::IN_LIST);
		if(options.trackIndexChanges) {
			watchIndex(indexMarker);
		}
		auto self = this->shared_from_this();
		return Promise<LinkedList<T>>([=](auto resolve, auto reject) {
			self->mutationQueue.run([=](auto task) -> Promise<void> {
				std::unique_lock<std::recursive_mutex> lock(mutex);
				if(self->delegate == nullptr) {
					if(options.trackIndexChanges) {
						unwatchIndex(indexMarker);
					}
					resolve({});
					return Promise<void>::resolve();
				}
				size_t index = indexMarker->index;
				if(!options.forceReload) {
					auto loadedItems = getLoadedItems({
						.startIndex = index,
						.limit = count
					});
					size_t endIndex = index + count;
					if(itemsSize.has_value() && endIndex > itemsSize.value()) {
						endIndex = itemsSize.value();
					}
					if((index + loadedItems.size()) >= endIndex) {
						resolve(loadedItems);
						return Promise<void>::resolve();
					}
				}
				size_t chunkSize = self->getChunkSize();
				size_t chunkStartIndex = chunkStartIndexForIndex(index, chunkSize);
				size_t chunkEndIndex = chunkStartIndexForIndex(index+count, chunkSize);
				if(chunkEndIndex < (index+count)) {
					chunkEndIndex += chunkSize;
				}
				auto promise = Promise<void>::resolve();
				for(size_t loadStartIndex = chunkStartIndex; loadStartIndex < chunkEndIndex; loadStartIndex += chunkSize) {
					promise = promise.then(self->mutationQueue.dispatchQueue(), [=]() {
						std::unique_lock<std::recursive_mutex> lock(self->mutex);
						if(self->delegate == nullptr) {
							return Promise<void>::resolve();
						}
						return self->delegate->loadAsyncListItems(&self->mutator, loadStartIndex, chunkSize, options.loadOptions);
					});
				}
				return promise
				.finally(self->mutationQueue.dispatchQueue(), [=]() {
					if(options.trackIndexChanges) {
						self->unwatchIndex(indexMarker);
					}
				})
				.then(self->mutationQueue.dispatchQueue(), [=]() {
					std::unique_lock<std::recursive_mutex> lock(self->mutex);
					size_t index = indexMarker->index;
					auto loadedItems = getLoadedItems({
						.startIndex = index,
						.limit = count
					});
					size_t endIndex = index + count;
					if(itemsSize.has_value() && endIndex > itemsSize.value()) {
						endIndex = itemsSize.value();
					}
					if(itemsSize.has_value() && (index + loadedItems.size()) < endIndex) {
						lock.unlock();
						reject(std::logic_error("Failed to load all items"));
						return;
					}
					lock.unlock();
					resolve(loadedItems);
				}, reject);
			});
		});
	}
	
	template<typename T>
	typename AsyncList<T>::ItemGenerator AsyncList<T>::generateItems(size_t startIndex, AsyncListGetItemOptions options) {
		using YieldResult = typename ItemGenerator::YieldResult;
		std::unique_lock<std::recursive_mutex> lock(mutex);
		auto indexMarker = AsyncListIndexMarkerData::new$(startIndex, AsyncListIndexMarkerState::IN_LIST);
		if(options.trackIndexChanges) {
			watchIndex(indexMarker);
		}
		auto self = this->shared_from_this();
		return ItemGenerator([=]() {
			std::unique_lock<std::recursive_mutex> lock(self->mutex);
			size_t chunkSize = self->getChunkSize();
			return getItems(indexMarker->index, chunkSize, options).template map<YieldResult>([=](auto items) {
				std::unique_lock<std::recursive_mutex> lock(self->mutex);
				indexMarker->index += items.size();
				if(itemsSize.has_value() && indexMarker->index >= itemsSize.value()) {
					if(options.trackIndexChanges) {
						self->unwatchIndex(indexMarker);
					}
					return YieldResult{
						.value=items,
						.done=true
					};
				}
				return YieldResult{
					.value=items,
					.done=false
				};
			});
		}, [=]() {
			if(options.trackIndexChanges) {
				self->unwatchIndex(indexMarker);
			}
		});
	}

	template<typename T>
	template<typename Callable>
	Optional<size_t> AsyncList<T>::indexWhere(Callable predicate, const AsyncListIndexAccessOptions& options) const {
		std::unique_lock<std::recursive_mutex> lock(mutex);
		if(options.onlyValidItems) {
			for(auto& pair : items) {
				if(pair.second.valid && predicate(pair.second.item)) {
					return pair.first;
				}
			}
		} else {
			for(auto& pair : items) {
				if(predicate(pair.second.item)) {
					return pair.first;
				}
			}
		}
		return std::nullopt;
	}

	template<typename T>
	size_t AsyncList<T>::chunkStartIndexForIndex(size_t index, size_t chunkSize) {
		return std::floor(index / chunkSize) * chunkSize;
	}



	template<typename T>
	void AsyncList<T>::forEachNode(Function<void(ItemNode&,size_t)> executor) {
		for(auto& pair : items) {
			executor(pair.second, pair.first);
		}
	}

	template<typename T>
	void AsyncList<T>::forEachNode(Function<void(const ItemNode&,size_t)> executor) const {
		std::unique_lock<std::recursive_mutex> lock(mutex);
		for(auto& pair : items) {
			executor(pair.second, pair.first);
		}
	}

	template<typename T>
	void AsyncList<T>::forEachNodeInRange(size_t startIndex, size_t endIndex, Function<void(ItemNode&,size_t)> executor) {
		std::unique_lock<std::recursive_mutex> lock(mutex);
		auto startIt = items.lower_bound(startIndex);
		if(startIt == items.end() || startIt->first >= endIndex) {
			return;
		}
		for(auto it=startIt; it!=items.end() && it->first < endIndex; it++) {
			executor(it->second, it->first);
		}
	}

	template<typename T>
	void AsyncList<T>::forEachNodeInRange(size_t startIndex, size_t endIndex, Function<void(const ItemNode&,size_t)> executor) const {
		std::unique_lock<std::recursive_mutex> lock(mutex);
		auto startIt = items.lower_bound(startIndex);
		if(startIt == items.end() || startIt->first >= endIndex) {
			return;
		}
		for(auto it=startIt; it!=items.end() && it->first < endIndex; it++) {
			executor(it->second, it->first);
		}
	}



	template<typename T>
	void AsyncList<T>::forEach(Function<void(T&,size_t)> executor, const AsyncListIndexAccessOptions& options) {
		if(options.onlyValidItems) {
			for(auto& pair : items) {
				if(pair.second.valid) {
					executor(pair.second.item, pair.first);
				}
			}
		} else {
			for(auto& pair : items) {
				executor(pair.second.item, pair.first);
			}
		}
	}

	template<typename T>
	void AsyncList<T>::forEach(Function<void(const T&,size_t)> executor, const AsyncListIndexAccessOptions& options) const {
		std::unique_lock<std::recursive_mutex> lock(mutex);
		if(options.onlyValidItems) {
			for(auto& pair : items) {
				if(pair.second.valid) {
					executor(pair.second.item, pair.first);
				}
			}
		} else {
			for(auto& pair : items) {
				executor(pair.second.item, pair.first);
			}
		}
	}

	template<typename T>
	void AsyncList<T>::forEachInRange(size_t startIndex, size_t endIndex, Function<void(T&,size_t)> executor, const AsyncListIndexAccessOptions& options) {
		std::unique_lock<std::recursive_mutex> lock(mutex);
		auto startIt = items.lower_bound(startIndex);
		if(startIt == items.end() || startIt->first >= endIndex) {
			return;
		}
		if(options.onlyValidItems) {
			for(auto it=startIt; it!=items.end() && it->first < endIndex; it++) {
				if(it->second.valid) {
					executor(it->second.item, it->first);
				}
			}
		} else {
			for(auto it=startIt; it!=items.end() && it->first < endIndex; it++) {
				executor(it->second.item, it->first);
			}
		}
	}

	template<typename T>
	void AsyncList<T>::forEachInRange(size_t startIndex, size_t endIndex, Function<void(const T&,size_t)> executor, const AsyncListIndexAccessOptions& options) const {
		std::unique_lock<std::recursive_mutex> lock(mutex);
		auto startIt = items.lower_bound(startIndex);
		if(startIt == items.end() || startIt->first >= endIndex) {
			return;
		}
		if(options.onlyValidItems) {
			for(auto it=startIt; it!=items.end() && it->first < endIndex; it++) {
				if(it->second.valid) {
					executor(it->second.item, it->first);
				}
			}
		} else {
			for(auto it=startIt; it!=items.end() && it->first < endIndex; it++) {
				executor(it->second.item, it->first);
			}
		}
	}



	template<typename T>
	Promise<void> AsyncList<T>::mutate(Function<Promise<void>(Mutator*)> executor) {
		auto self = this->shared_from_this();
		return mutationQueue.run([=](auto task) -> Promise<void> {
			return executor(&self->mutator).then(nullptr, [self]() {});
		}).promise;
	}

	template<typename T>
	Promise<void> AsyncList<T>::mutate(Function<void(Mutator*)> executor) {
		auto self = this->shared_from_this();
		return mutationQueue.run([=](auto task) -> void {
			return executor(&self->mutator);
		}).promise;
	}

	template<typename T>
	template<typename Work>
	auto AsyncList<T>::lock(Work work) -> decltype(work((Mutator*)nullptr)) {
		std::unique_lock<std::recursive_mutex> lock(mutex);
		return work(&mutator);
	}

	template<typename T>
	void AsyncList<T>::invalidateItems(size_t startIndex, size_t endIndex, bool runInQueue) {
		FGL_ASSERT(endIndex < startIndex, "endIndex must be greater than or equal to startIndex");
		mutator.invalidate(startIndex, (endIndex - startIndex));
		if(runInQueue) {
			std::weak_ptr<AsyncList<T>> weakSelf = this->shared_from_this();
			mutationQueue.run([=](auto task) -> void {
				auto self = weakSelf.lock();
				if(!self) {
					return;
				}
				self->mutator.invalidate(startIndex, (endIndex - startIndex));
			});
		}
	}

	template<typename T>
	void AsyncList<T>::invalidateAllItems(bool runInQueue) {
		mutator.invalidateAll();
		if(runInQueue) {
			std::weak_ptr<AsyncList<T>> weakSelf = this->shared_from_this();
			mutationQueue.run([=](auto task) -> void {
				auto self = weakSelf.lock();
				if(!self) {
					return;
				}
				self->mutator.invalidateAll();
			});
		}
	}
}
