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
			
			template<typename Work>
			void lock(Work work);
			void apply(size_t index, LinkedList<T> items);
			void insert(size_t index, LinkedList<T> items);
			void remove(size_t index, size_t count);
			//void move(size_t index, size_t count, size_t newIndex);
			void resize(size_t count);
			void invalidate(size_t index, size_t count);
			
		private:
			Mutator(AsyncList& list);
			
			AsyncList& list;
		};
		friend class AsyncList<T>::Mutator;
		
		class Delegate {
		public:
			virtual ~Delegate() {}
			
			virtual Promise<void> loadAsyncListItems(Mutator* mutator, size_t index, size_t count) = 0;
			
			virtual bool areAsyncListItemsEqual(const AsyncList* list, const T& item1, const T& item2) const = 0;
			
			//virtual Promise<void> insertAsyncListItems(Mutator* mutator, size_t index, size_t count) = 0;
			//virtual Promise<void> removeAsyncListItems(Mutator* mutator, size_t index, size_t count) = 0;
			//virtual Promise<void> moveAsyncListItems(Mutator* mutator, size_t index, size_t count) = 0;
		};
		
		struct ItemNode {
			T item;
			bool valid = true;
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
		
		inline const std::map<size_t,ItemNode>& getMap() const;
		inline size_t size() const;
		inline size_t getChunkSize() const;
		
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
		
		mutable std::recursive_mutex mutex;
		std::map<size_t,ItemNode> items;
		size_t itemsSize;
		std::map<size_t,Promise<T>> itemPromises;
		
		size_t chunkSize;
		std::list<std::shared_ptr<size_t>> indexMarkers;
		
		AsyncQueue mutationQueue;
		Mutator mutator;
		Delegate* delegate;
	};




#pragma mark AsyncList implementation

	template<typename T>
	AsyncList<T>::AsyncList(Options options)
	: itemsSize(options.initialSize), chunkSize(options.chunkSize), mutator(*this), delegate(options.delegate) {
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
	}

	template<typename T>
	const std::map<size_t,typename AsyncList<T>::ItemNode>& AsyncList<T>::getMap() const {
		return items;
	}

	template<typename T>
	size_t AsyncList<T>::size() const {
		return itemsSize;
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
			watchIndex(indexMarker);
		}
		return Promise<Optional<T>>([&](auto resolve, auto reject) {
			mutationQueue.run([=]() -> Promise<void> {
				if(options.trackIndexChanges) {
					unwatchIndex(indexMarker);
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
			watchIndex(indexMarker);
		}
		return Promise<LinkedList<T>>([=](auto resolve, auto reject) {
			mutationQueue.run([=]() -> Promise<void> {
				if(options.trackIndexChanges) {
					unwatchIndex(indexMarker);
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
		auto indexMarker = watchIndex(startIndex);
		return generate<LinkedList<T>,LinkedList<T>>([=](auto yield) {
			std::unique_lock<std::recursive_mutex> lock(mutex);
			bool done = false;
			try {
				while(!done) {
					size_t index = *indexMarker;
					lock.unlock();
					auto items = getItems(index, chunkSize).get();
					lock.lock();
					*indexMarker += items.size();
					if(*indexMarker >= itemsSize) {
						unwatchIndex(indexMarker);
						return items;
					}
					lock.unlock();
					yield(items);
					lock.lock();
				}
			} catch(...) {
				unwatchIndex(indexMarker);
				std::rethrow_exception(std::current_exception());
			}
		});
	}

	template<typename T>
	size_t AsyncList<T>::chunkStartIndexForIndex(size_t index) const {
		return std::floor(index / chunkSize) * chunkSize;
	}



	template<typename T>
	AsyncList<T>::Mutator::Mutator(AsyncList<T>& list)
	: list(list) {
		//
	}

	template<typename T>
	AsyncList<T>* AsyncList<T>::Mutator::getList() {
		return &list;
	}

	template<typename T>
	const AsyncList<T>* AsyncList<T>::Mutator::getList() const {
		return &list;
	}

	template<typename T>
	template<typename Work>
	void AsyncList<T>::Mutator::lock(Work work) {
		std::unique_lock<std::recursive_mutex> lock(list.mutex);
		work();
	}
	
	template<typename T>
	void AsyncList<T>::Mutator::apply(size_t index, LinkedList<T> items) {
		std::unique_lock<std::recursive_mutex> lock(list.mutex);
		size_t i=index;
		for(auto& item : items) {
			list.items[i] = AsyncList<T>::ItemNode{
				.item=item,
				.valid=true
			};
			i++;
		}
	}

	template<typename T>
	void AsyncList<T>::Mutator::insert(size_t index, LinkedList<T> items) {
		std::unique_lock<std::recursive_mutex> lock(list.mutex);
		size_t insertCount = items.size();
		// update list size
		list.itemsSize += insertCount;
		// update keys for elements above insert range
		for(size_t i=(index+insertCount-1); i>=index && i!=(size_t)-1; i--) {
			auto node = list.items.extract(i);
			if(!node.empty()) {
				node.key() += insertCount;
				list.items.insert(list.items.end(), node);
			}
		}
		// update index markers
		for(auto& indexMarker : list.indexMarkers) {
			if(*indexMarker >= index && *indexMarker != -1) {
				*indexMarker += insertCount;
			}
		}
		// apply new items
		size_t i=index;
		for(auto& item : items) {
			list.items[i] = AsyncList<T>::ItemNode{
				.item=item,
				.valid=true
			};
			i++;
		}
	}

	template<typename T>
	void AsyncList<T>::Mutator::remove(size_t index, size_t count) {
		std::unique_lock<std::recursive_mutex> lock(list.mutex);
		if(index >= list->itemsSize) {
			return;
		}
		size_t endIndex = index + count;
		if(endIndex > list->itemsSize) {
			endIndex = list->itemsSize;
		}
		size_t removeCount = endIndex - index;
		if(removeCount == 0) {
			return;
		}
		// update list items
		if(list.items.size() > 0) {
			using node_type = typename decltype(list->items)::node_type;
			std::list<node_type> reinsertNodes;
			auto it = list.items.end() - 1;
			bool removing = false;
			auto removeStartIt = list.items.end();
			auto removeEndIt = list.items.end();
			do {
				if(it->first >= endIndex) {
					auto nodeIt = it;
					it++;
					auto node = list.items.extract(it);
					node.key() -= removeCount;
					reinsertNodes.emplace_front(std::move(node));
				} else if(it->first < index) {
					break;
				} else {
					if(removing) {
						removeStartIt = it;
					} else {
						removeStartIt = it;
						removeEndIt = it + 1;
						removing = true;
					}
				}
				if(list.items.size() > 0) {
					it--;
				}
			} while(list.items.size() > 0);
			if(removing) {
				list.items.erase(removeStartIt, removeEndIt);
			}
			for(auto& node : reinsertNodes) {
				list.items.insert(list.items.end(), std::move(node));
			}
		}
		// update index markers
		for(auto& indexMarker : list.indexMarkers) {
			if(*indexMarker == (size_t)-1) {
				continue;
			} else if(*indexMarker >= endIndex) {
				*indexMarker -= removeCount;
			} else if(*indexMarker >= index) {
				*indexMarker = (size_t)-1;
			}
		}
		// update list size
		list.itemsSize -= removeCount;
	}

	/*template<typename T>
	void AsyncList<T>::Mutator::move(size_t index, size_t count, size_t newIndex) {
		std::unique_lock<std::recursive_mutex> lock(list.mutex);
		// TODO implement move
	}*/

	template<typename T>
	void AsyncList<T>::Mutator::resize(size_t count) {
		std::unique_lock<std::recursive_mutex> lock(list.mutex);
		// remove list items above count
		if(list.items.size() > 0) {
			auto it = list.items.end() - 1;
			bool removing = false;
			auto removeStartIt = list.items.end();
			auto removeEndIt = list.items.end();
			do {
				if(it->first >= count) {
					if(removing) {
						removeStartIt = it;
					} else {
						removeStartIt = it;
						removeEndIt = it + 1;
						removing = true;
					}
				} else {
					break;
				}
				if(list.items.size() > 0) {
					it--;
				}
			} while(list.items.size() > 0);
			if(removing) {
				list.items.erase(removeStartIt, removeEndIt);
			}
		}
		// eliminate index markers above count
		for(auto& indexMarker : list.indexMarkers) {
			if(*indexMarker == (size_t)-1) {
				continue;
			} else if(*indexMarker >= count) {
				*indexMarker = (size_t)-1;
			}
		}
		// update list size
		list.itemsSize = count;
	}

	template<typename T>
	void AsyncList<T>::Mutator::invalidate(size_t index, size_t count) {
		std::unique_lock<std::recursive_mutex> lock(list.mutex);
		size_t endIndex = index + count;
		if(endIndex >= list.itemsSize) {
			endIndex = list.itemsSize;
		}
		for(auto it=list.items.lower_bound(index), end=list.items.end(); it!=end; it++) {
			if(it->first >= endIndex) {
				break;
			} else {
				it->second.valid = false;
			}
		}
	}
}
