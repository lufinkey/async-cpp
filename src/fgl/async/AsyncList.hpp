//
//  AsyncList.hpp
//  AsyncCpp
//
//  Created by Luis Finke on 11/17/19.
//  Copyright © 2019 Luis Finke. All rights reserved.
//

#pragma once

#include <cmath>
#include <list>
#include <map>
#include <variant>
#include <fgl/async/Common.hpp>
#include <fgl/async/Promise.hpp>
#include <fgl/async/Generator.hpp>
#include <fgl/async/AsyncQueue.hpp>

namespace fgl {
	template<typename T>
	class AsyncList {
	public:
		class Mutator {
			friend class AsyncList<T>;
		public:
			Mutator(const Mutator&) = delete;
			
			AsyncList* getList();
			const AsyncList* getList() const;
			
			void apply(size_t index, LinkedList<T> items);
			void insert(size_t index, LinkedList<T> items);
			void remove(size_t index, LinkedList<T> items);
			void move(size_t index, size_t count, size_t newIndex);
			void resize(size_t count);
			void invalidate(size_t index, size_t count);
			
		private:
			Mutator(AsyncList* list);
			
			AsyncList* list;
		};
		friend class AsyncList<T>::Mutator;
		
		class Delegate {
		public:
			virtual ~Delegate() {}
			
			virtual bool doesAsyncListItemNeedReload(const T& item) const = 0;
			virtual Promise<void> loadAsyncListItems(Mutator* mutator, size_t index, size_t count) = 0;
			
			virtual bool canEvaluateAsyncListItemEquality(const AsyncList* list) const = 0;
			virtual bool areAsyncListItemsEqual(const AsyncList* list, const T& item1, const T& item2) const = 0;
			
			//virtual Promise<void> insertAsyncListItems(Mutator* mutator, size_t index, size_t count) = 0;
			//virtual Promise<void> removeAsyncListItems(Mutator* mutator, size_t index, size_t count) = 0;
			//virtual Promise<void> moveAsyncListItems(Mutator* mutator, size_t index, size_t count) = 0;
		};
		
		struct Options {
			Delegate* delegate = nullptr;
			size_t chunkSize = 0;
			ArrayList<T> initialItems;
			size_t initialItemsOffset = 0;
			size_t initialSize = 0;
		};
		
		AsyncList(const AsyncList&) = delete;
		AsyncList(Options options);
		~AsyncList();
		
		size_t getChunkSize() const;
		
		std::shared_ptr<size_t> watchIndex(size_t index);
		std::shared_ptr<size_t> watchIndex(std::shared_ptr<size_t> index);
		void unwatchIndex(std::shared_ptr<size_t> index);
		
		bool isItemLoaded(size_t index, bool ignoreValidity = false) const;
		bool areItemsLoaded(size_t index, size_t count, bool ignoreValidity = false) const;
		LinkedList<T> getLoadedItems(size_t startIndex = 0, bool ignoreValidity = false) const;
		
		Optional<T> itemAt(size_t index, bool ignoreValidity = false) const;
		struct GetItemOptions {
			bool trackIndexChanges = false;
		};
		Promise<Optional<T>> getItem(size_t index, GetItemOptions options = GetItemOptions());
		Promise<LinkedList<T>> getItems(size_t index, size_t count, GetItemOptions options = GetItemOptions());
		
		Generator<LinkedList<T>,void> generateItems(size_t startIndex=0);
		
	private:
		size_t chunkStartIndexForIndex(size_t index) const;
		
		struct ItemNode {
			T item;
			bool valid = true;
		};
		
		mutable std::recursive_mutex mutex;
		std::map<size_t,ItemNode> items;
		size_t itemsSize;
		std::map<size_t,Promise<T>> itemPromises;
		
		size_t chunkSize;
		std::list<std::shared_ptr<size_t>> indexMarkers;
		
		AsyncQueue mutationQueue;
		Mutator* mutator;
		Delegate* delegate;
	};




#pragma mark AsyncList implementation

	template<typename T>
	AsyncList<T>::AsyncList(Options options)
	: itemsSize(options.initialSize), chunkSize(options.chunkSize), mutator(nullptr), delegate(options.delegate) {
		if(options.delegate == nullptr) {
			throw std::invalid_argument("delegate cannot be null");
		} else if(options.chunkSize == 0) {
			throw std::invalid_argument("chunkSize cannot be 0");
		}
		
		// set initial items
		size_t initialItemsOffset = options.initialItemsOffset;
		for(auto& item : options.initialItems) {
			items[initialItemsOffset] = ItemNode{
				.item = item,
				.valid = true
			};
			initialItemsOffset++;
		}
		
		mutator = new Mutator(this);
	}

	template<typename T>
	AsyncList<T>::~AsyncList() {
		delete mutator;
	}

	template<typename T>
	size_t AsyncList<T>::getChunkSize() const {
		return chunkSize;
	}

	template<typename T>
	std::shared_ptr<size_t> AsyncList<T>::watchIndex(size_t index) {
		auto indexMarker = std::make_shared<size_t>(index);
		indexMarkers.push_back(indexMarker);
		return indexMarker;
	}

	template<typename T>
	std::shared_ptr<size_t> AsyncList<T>::watchIndex(std::shared_ptr<size_t> index) {
		auto it = std::find(indexMarkers.begin(), indexMarkers.end(), index);
		if(it == indexMarkers.end()) {
			indexMarkers.push_back(index);
		}
		return index;
	}

	template<typename T>
	void AsyncList<T>::unwatchIndex(std::shared_ptr<size_t> index) {
		auto it = std::find(indexMarkers.begin(), indexMarkers.end(), index);
		if(it != indexMarkers.end()) {
			indexMarkers.erase(it);
		}
	}
	
	template<typename T>
	bool AsyncList<T>::isItemLoaded(size_t index, bool ignoreValidity) const {
		std::unique_lock<std::recursive_mutex> lock(mutex);
		auto it = items.find(index);
		if(it == items.end()) {
			return false;
		}
		if(ignoreValidity) {
			return true;
		}
		return it->second.valid;
	}

	template<typename T>
	bool AsyncList<T>::areItemsLoaded(size_t index, size_t count, bool ignoreValidity) const {
		std::unique_lock<std::recursive_mutex> lock(mutex);
		auto it = items.find(index);
		size_t endIndex = index + count;
		size_t nextIndex = index;
		while(it != items.end()) {
			if(!ignoreValidity && !it->second.valid) {
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
	LinkedList<T> AsyncList<T>::getLoadedItems(size_t startIndex, bool ignoreValidity) const {
		std::unique_lock<std::recursive_mutex> lock(mutex);
		LinkedList<T> loadedItems;
		auto it = items.find(startIndex);
		size_t nextIndex = startIndex;
		while(it != items.end()) {
			if(!ignoreValidity && !it->second.valid) {
				return loadedItems;
			}
			if(it->first != nextIndex) {
				return loadedItems;
			}
			ASYNC_CPP_LIST_PUSH(loadedItems, it->second);
			it++;
			nextIndex++;
		}
		return loadedItems;
	}
	
	template<typename T>
	Optional<T> AsyncList<T>::itemAt(size_t index, bool ignoreValidity) const {
		std::unique_lock<std::recursive_mutex> lock(mutex);
		auto it = items.find(index);
		if(it == items.end()) {
			return std::nullopt;
		}
		if(!ignoreValidity && !it->second.valid) {
			return std::nullopt;
		}
		return it->second;
	}

	template<typename T>
	Promise<Optional<T>> AsyncList<T>::getItem(size_t index, GetItemOptions options) {
		std::unique_lock<std::recursive_mutex> lock(mutex);
		auto it = items.find(index);
		if(it != items.end() && it->second.valid) {
			return Promise<Optional<T>>::resolve(it->second);
		}
		auto indexMarker = std::make_shared<size_t>(index);
		if(options.trackIndexChanges) {
			indexMarkers.push_back(indexMarker);
		}
		return Promise<Optional<T>>([&](auto resolve, auto reject) {
			mutationQueue.run([=]() -> Promise<void> {
				if(options.trackIndexChanges) {
					auto it = std::find(indexMarkers.begin(), indexMarkers.end(), indexMarker);
					if(it != indexMarkers.end()) {
						indexMarkers.erase(it);
					}
				}
				size_t index = *indexMarker.get();
				size_t chunkStartIndex = chunkStartIndexForIndex(index);
				return delegate->loadAsyncListItems(mutator, chunkStartIndex, chunkSize)
				.then(nullptr, [=]() {
					resolve(itemAt(index));
				}, reject);
			});
		});
	}

	template<typename T>
	Promise<LinkedList<T>> AsyncList<T>::getItems(size_t index, size_t count, GetItemOptions options) {
		std::unique_lock<std::recursive_mutex> lock(mutex);
		auto it = items.find(index);
		if(it != items.end() && it->second.valid) {
			return Promise<Optional<T>>::resolve(it->second);
		}
		auto indexMarker = std::make_shared<size_t>(index);
		if(options.trackIndexChanges) {
			indexMarkers.push_back(indexMarker);
		}
		return Promise<LinkedList<T>>([=](auto resolve, auto reject) {
			mutationQueue.run([=]() -> Promise<void> {
				if(options.trackIndexChanges) {
					auto it = std::find(indexMarkers.begin(), indexMarkers.end(), indexMarker);
					if(it != indexMarkers.end()) {
						indexMarkers.erase(it);
					}
				}
				size_t index = *indexMarker.get();
				size_t chunkStartIndex = chunkStartIndexForIndex(index);
				size_t chunkEndIndex = chunkStartIndexForIndex(index+count);
				return delegate->loadAsyncListItems(mutator, chunkStartIndex, chunkEndIndex-chunkStartIndex)
				.then(nullptr, [=]() {
					LinkedList<T> loadedItems;
					if(index >= itemsSize) {
						resolve(loadedItems);
					}
					auto it = items.find(index);
					size_t nextIndex = index;
					size_t endIndex = index + count;
					if(endIndex > itemsSize) {
						endIndex = itemsSize;
					}
					size_t loadedItemCount = endIndex - index;
					while(it != items.end() && nextIndex < endIndex) {
						if(it->first != nextIndex) {
							reject(std::logic_error("Failed to load all items"));
							return;
						}
						ASYNC_CPP_LIST_PUSH(loadedItems, *it);
						it++;
						nextIndex++;
					}
					if(loadedItems.size() < loadedItemCount) {
						reject(std::logic_error("Failed to load all items"));
						return;
					}
					resolve(loadedItems);
				}, reject);
			});
		});
	}
	
	template<typename T>
	Generator<LinkedList<T>,void> AsyncList<T>::generateItems(size_t startIndex) {
		std::unique_lock<std::recursive_mutex> lock(mutex);
		auto indexMarker = std::make_shared<size_t>(startIndex);
		indexMarkers.push_back(indexMarker);
		return generate<LinkedList<T>,LinkedList<T>>([=](auto yield) {
			std::unique_lock<std::recursive_mutex> lock(mutex);
			bool done = false;
			while(!done) {
				size_t index = *indexMarker;
				lock.unlock();
				auto items = getItems(index, chunkSize).get();
				lock.lock();
				*indexMarker += items.size();
				if(*indexMarker >= itemsSize) {
					return items;
				}
				lock.unlock();
				yield(items);
				lock.lock();
			}
		});
	}

	template<typename T>
	size_t AsyncList<T>::chunkStartIndexForIndex(size_t index) const {
		return std::floor(index / chunkSize) * chunkSize;
	}
}
