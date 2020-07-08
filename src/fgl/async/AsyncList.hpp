//
//  AsyncList.hpp
//  AsyncCpp
//
//  Created by Luis Finke on 11/17/19.
//  Copyright © 2019 Luis Finke. All rights reserved.
//

#pragma once

#include <fgl/async/Common.hpp>
#include <fgl/async/Promise.hpp>
#include <fgl/async/ContinuousGenerator.hpp>
#include <fgl/async/AsyncQueue.hpp>
#include <dtl/dtl.hpp>
#include <cmath>
#include <list>
#include <map>
#include <memory>
#include <variant>

namespace fgl {
	struct AsyncListGetLoadedItemsOptions {
		size_t startIndex = 0;
		size_t limit = (size_t)-1;
		bool ignoreValidity = false;
	};

	struct AsyncListGetItemOptions {
		bool trackIndexChanges = false;
		bool forceReload = false;
		std::map<String,Any> loadOptions;
	};

	enum AsyncListIndexMarkerState: uint8_t {
		IN_LIST,
		REMOVED
	};

	struct AsyncListIndexMarkerData {
		size_t index;
		AsyncListIndexMarkerState state;
	};
	typedef std::shared_ptr<AsyncListIndexMarkerData> AsyncListIndexMarker;

	

	template<typename T>
	class AsyncList: public std::enable_shared_from_this<AsyncList<T>> {
	public:
		using ItemGenerator = ContinuousGenerator<LinkedList<T>,void>;
		
		class Mutator {
			friend class AsyncList<T>;
		public:
			Mutator(const Mutator&) = delete;
			
			AsyncList* getList();
			const AsyncList* getList() const;
			
			template<typename Work>
			void lock(Work work);
			void apply(size_t index, LinkedList<T> items);
			void applyAndResize(size_t index, size_t listSize, LinkedList<T> items);
			void set(size_t index, LinkedList<T> items);
			void insert(size_t index, LinkedList<T> items);
			void remove(size_t index, size_t count);
			//void move(size_t index, size_t count, size_t newIndex);
			void resize(size_t count);
			void invalidate(size_t index, size_t count);
			void invalidateAll();
			
		private:
			void applyMerge(size_t index, Optional<size_t> listSize, LinkedList<T> items);
			
			Mutator(AsyncList<T>& list);
			
			AsyncList<T>& list;
		};
		friend class AsyncList<T>::Mutator;
		
		class Delegate {
		public:
			virtual ~Delegate() {}
			
			virtual size_t getAsyncListChunkSize(const AsyncList<T>* list) const = 0;
			virtual Promise<void> loadAsyncListItems(Mutator* mutator, size_t index, size_t count, std::map<String,Any> options) = 0;
			
			virtual bool areAsyncListItemsEqual(const AsyncList<T>* list, const T& item1, const T& item2) const = 0;
			virtual void mergeAsyncListItem(const AsyncList<T>* list, T& overwritingItem, T& existingItem) = 0;
			
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
			DispatchQueue* dispatchQueue = getDefaultPromiseQueue();
			std::map<size_t,T> initialItemsMap;
			ArrayList<T> initialItems;
			size_t initialItemsOffset = 0;
			Optional<size_t> initialSize;
		};
		
		static std::shared_ptr<AsyncList<T>> new$(Options options);
		
		AsyncList(const AsyncList&) = delete;
		AsyncList(Options options);
		
		inline const std::map<size_t,ItemNode>& getMap() const;
		inline bool sizeIsKnown() const;
		inline size_t size() const;
		inline size_t getChunkSize() const;
		
		AsyncListIndexMarker watchIndex(size_t index);
		AsyncListIndexMarker watchIndex(AsyncListIndexMarker index);
		void unwatchIndex(AsyncListIndexMarker index);
		
		bool isItemLoaded(size_t index, bool ignoreValidity = false) const;
		bool areItemsLoaded(size_t index, size_t count, bool ignoreValidity = false) const;
		LinkedList<T> getLoadedItems(AsyncListGetLoadedItemsOptions options = AsyncListGetLoadedItemsOptions()) const;
		LinkedList<Optional<T>> maybeGetLoadedItems(AsyncListGetLoadedItemsOptions options = AsyncListGetLoadedItemsOptions()) const;
		
		Optional<T> itemAt(size_t index, bool ignoreValidity = false) const;
		Promise<Optional<T>> getItem(size_t index, AsyncListGetItemOptions options = AsyncListGetItemOptions());
		Promise<LinkedList<T>> getItems(size_t index, size_t count, AsyncListGetItemOptions options = AsyncListGetItemOptions());
		ItemGenerator generateItems(size_t startIndex=0, AsyncListGetItemOptions options = AsyncListGetItemOptions{.trackIndexChanges=true});
		
		template<typename Callable>
		Optional<size_t> indexWhere(Callable predicate, bool ignoreValidity = false) const;
		
		void forEach(Function<void(T&,size_t)> executor, bool onlyValidItems = true);
		void forEach(Function<void(const T&,size_t)> executor, bool onlyValidItems = true) const;
		void forEachInRange(size_t startIndex, size_t endIndex, Function<void(T&,size_t)> executor, bool onlyValidItems = true);
		void forEachInRange(size_t startIndex, size_t endIndex, Function<void(const T&,size_t)> executor, bool onlyValidItems = true) const;
		
		Promise<void> mutate(Function<Promise<void>(Mutator*)> executor);
		Promise<void> mutate(Function<void(Mutator*)> executor);
		
		void invalidateItems(size_t startIndex, size_t endIndex, bool runInQueue = false);
		void invalidateAllItems(bool runInQueue = false);
		
	private:
		static size_t chunkStartIndexForIndex(size_t index, size_t chunkSize);
		
		mutable std::recursive_mutex mutex;
		std::map<size_t,ItemNode> items;
		Optional<size_t> itemsSize;
		
		std::list<AsyncListIndexMarker> indexMarkers;
		
		AsyncQueue mutationQueue;
		Mutator mutator;
		Delegate* delegate;
	};

	template<typename T>
	class AsyncListOptionalDTLCompare;




#pragma mark AsyncList implementation

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
	mutator(*this), delegate(options.delegate) {
		if(options.delegate == nullptr) {
			throw std::invalid_argument("delegate cannot be null");
		}
		
		// set initial items
		for(auto& pair : options.initialItemsMap) {
			items.insert_or_assign(pair.first, ItemNode{
				.item=pair.second,
				.valid=true
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
	const std::map<size_t,typename AsyncList<T>::ItemNode>& AsyncList<T>::getMap() const {
		return items;
	}

	template<typename T>
	bool AsyncList<T>::sizeIsKnown() const {
		return itemsSize.has_value();
	}

	template<typename T>
	size_t AsyncList<T>::size() const {
		std::unique_lock<std::recursive_mutex> lock(mutex);
		if(itemsSize.has_value()) {
			return itemsSize.value();
		} else if(items.size() > 0) {
			auto it = items.end();
			it--;
			return it->first + 1;
		}
		return 0;
	}

	template<typename T>
	size_t AsyncList<T>::getChunkSize() const {
		std::unique_lock<std::recursive_mutex> lock(mutex);
		size_t chunkSize = delegate->getAsyncListChunkSize(this);
		FGL_ASSERT(chunkSize != 0, "AsyncList chunkSize cannot be 0");
		return chunkSize;
	}

	template<typename T>
	AsyncListIndexMarker AsyncList<T>::watchIndex(size_t index) {
		std::unique_lock<std::recursive_mutex> lock(mutex);
		auto indexMarker = std::make_shared<AsyncListIndexMarkerData>(AsyncListIndexMarkerData{
			.index=index,
			.state=AsyncListIndexMarkerState::IN_LIST
		});
		indexMarkers.push_back(indexMarker);
		return indexMarker;
	}

	template<typename T>
	AsyncListIndexMarker AsyncList<T>::watchIndex(AsyncListIndexMarker indexMarker) {
		std::unique_lock<std::recursive_mutex> lock(mutex);
		auto it = std::find(indexMarkers.begin(), indexMarkers.end(), indexMarker);
		if(it == indexMarkers.end()) {
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
	LinkedList<T> AsyncList<T>::getLoadedItems(AsyncListGetLoadedItemsOptions options) const {
		std::unique_lock<std::recursive_mutex> lock(mutex);
		LinkedList<T> loadedItems;
		auto it = items.find(options.startIndex);
		size_t nextIndex = options.startIndex;
		while(it != items.end() && loadedItems.size() < options.limit) {
			if(!options.ignoreValidity && !it->second.valid) {
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
	LinkedList<Optional<T>> AsyncList<T>::maybeGetLoadedItems(AsyncListGetLoadedItemsOptions options) const {
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
			if(!options.ignoreValidity && !it->second.valid) {
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
	Optional<T> AsyncList<T>::itemAt(size_t index, bool ignoreValidity) const {
		std::unique_lock<std::recursive_mutex> lock(mutex);
		auto it = items.find(index);
		if(it == items.end()) {
			return std::nullopt;
		}
		if(!ignoreValidity && !it->second.valid) {
			return std::nullopt;
		}
		return it->second.item;
	}

	template<typename T>
	Promise<Optional<T>> AsyncList<T>::getItem(size_t index, AsyncListGetItemOptions options) {
		std::unique_lock<std::recursive_mutex> lock(mutex);
		if(!options.forceReload) {
			auto it = items.find(index);
			if(it != items.end() && it->second.valid) {
				return Promise<Optional<T>>::resolve(it->second.item);
			}
		}
		auto indexMarker = std::make_shared<AsyncListIndexMarkerData>(AsyncListIndexMarkerData{
			.index=index,
			.state=AsyncListIndexMarkerState::IN_LIST
		});
		if(options.trackIndexChanges) {
			watchIndex(indexMarker);
		}
		auto self = this->shared_from_this();
		return Promise<Optional<T>>([=](auto resolve, auto reject) {
			self->mutationQueue.run([=](auto task) -> Promise<void> {
				std::unique_lock<std::recursive_mutex> lock(mutex);
				size_t index = indexMarker->index;
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
		if(!options.forceReload) {
			auto startIt = items.find(index);
			if(startIt != items.end() && startIt->second.valid) {
				LinkedList<T> loadedItems;
				size_t nextIndex = index;
				size_t endIndex = index + count;
				if(itemsSize.has_value() && endIndex > itemsSize.value()) {
					endIndex = itemsSize.value();
				}
				bool foundAllItems = false;
				for(auto it=startIt, end=items.end(); it!=end; it++) {
					if(it->first != nextIndex || !it->second.valid) {
						break;
					}
					loadedItems.pushBack(it->second.item);
					nextIndex++;
					if(nextIndex >= endIndex) {
						foundAllItems = true;
						break;
					}
				}
				if(foundAllItems) {
					return Promise<LinkedList<T>>::resolve(loadedItems);
				}
			}
		}
		auto indexMarker = std::make_shared<AsyncListIndexMarkerData>(AsyncListIndexMarkerData{
			.index=index,
			.state=AsyncListIndexMarkerState::IN_LIST
		});
		if(options.trackIndexChanges) {
			watchIndex(indexMarker);
		}
		auto self = this->shared_from_this();
		return Promise<LinkedList<T>>([=](auto resolve, auto reject) {
			self->mutationQueue.run([=](auto task) -> Promise<void> {
				std::unique_lock<std::recursive_mutex> lock(mutex);
				size_t index = indexMarker->index;
				size_t chunkSize = self->getChunkSize();
				size_t chunkStartIndex = chunkStartIndexForIndex(index, chunkSize);
				size_t chunkEndIndex = chunkStartIndexForIndex(index+count, chunkSize);
				if(chunkEndIndex < (index+count)) {
					chunkEndIndex += chunkSize;
				}
				auto promise = Promise<void>::resolve();
				for(size_t loadStartIndex = chunkStartIndex; loadStartIndex < chunkEndIndex; loadStartIndex += chunkSize) {
					promise = promise.then(self->mutationQueue.dispatchQueue(), [=]() {
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
					LinkedList<T> loadedItems;
					if(itemsSize.has_value() && index >= itemsSize.value()) {
						lock.unlock();
						resolve(loadedItems);
						return;
					}
					auto it = items.find(index);
					size_t nextIndex = index;
					size_t endIndex = index + count;
					if(itemsSize.has_value() && endIndex > itemsSize.value()) {
						endIndex = itemsSize.value();
					}
					size_t loadedItemCount = endIndex - index;
					while(it != items.end() && nextIndex < endIndex) {
						if(it->first != nextIndex) {
							lock.unlock();
							reject(std::logic_error("Failed to load all items"));
							return;
						}
						loadedItems.push_back(it->second.item);
						it++;
						nextIndex++;
					}
					if(itemsSize.has_value() && loadedItems.size() < loadedItemCount) {
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
		auto indexMarker = std::make_shared<AsyncListIndexMarkerData>(AsyncListIndexMarkerData{
			.index=startIndex,
			.state=AsyncListIndexMarkerState::IN_LIST
		});
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
	Optional<size_t> AsyncList<T>::indexWhere(Callable predicate, bool ignoreValidity) const {
		std::unique_lock<std::recursive_mutex> lock(mutex);
		for(auto& pair : items) {
			if((ignoreValidity || pair.second.valid) && predicate(pair.second.item)) {
				return pair.first;
			}
		}
		return std::nullopt;
	}

	template<typename T>
	size_t AsyncList<T>::chunkStartIndexForIndex(size_t index, size_t chunkSize) {
		return std::floor(index / chunkSize) * chunkSize;
	}



	template<typename T>
	void AsyncList<T>::forEach(Function<void(T&,size_t)> executor, bool onlyValidItems) {
		if(onlyValidItems) {
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
	void AsyncList<T>::forEach(Function<void(const T&,size_t)> executor, bool onlyValidItems) const {
		std::unique_lock<std::recursive_mutex> lock(mutex);
		if(onlyValidItems) {
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
	void AsyncList<T>::forEachInRange(size_t startIndex, size_t endIndex, Function<void(T&,size_t)> executor, bool onlyValidItems) {
		std::unique_lock<std::recursive_mutex> lock(mutex);
		auto startIt = items.lower_bound(startIndex);
		if(startIt == items.end() || startIt->first >= endIndex) {
			return;
		}
		if(onlyValidItems) {
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
	void AsyncList<T>::forEachInRange(size_t startIndex, size_t endIndex, Function<void(const T&,size_t)> executor, bool onlyValidItems) const {
		std::unique_lock<std::recursive_mutex> lock(mutex);
		auto startIt = items.lower_bound(startIndex);
		if(startIt == items.end() || startIt->first >= endIndex) {
			return;
		}
		if(onlyValidItems) {
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
			return executor(&mutator).then(nullptr, [self]() {});
		}).promise;
	}

	template<typename T>
	Promise<void> AsyncList<T>::mutate(Function<void(Mutator*)> executor) {
		return mutationQueue.run([=](auto task) -> void {
			return executor(&mutator);
		}).promise;
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
	void AsyncList<T>::Mutator::applyMerge(size_t index, Optional<size_t> listSize, LinkedList<T> items) {
		std::unique_lock<std::recursive_mutex> lock(list.mutex);
		
		using DiffType = dtl::Diff<Optional<T>, LinkedList<Optional<T>>, AsyncListOptionalDTLCompare<T>>;
		
		{
			size_t existingItemsLimit = items.size();
			if(listSize.has_value() && (index+items.size()) >= listSize.value()) {
				existingItemsLimit = -1;
			}
			auto existingItems = list.maybeGetLoadedItems({
				.startIndex=index,
				.limit=existingItemsLimit,
				.ignoreValidity=true
			});
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
			while(existingItems.size() < items.size() && (!list.itemsSize.has_value() || (index+existingItems.size()) < list.itemsSize.value())) {
				existingItems.push_back(std::nullopt);
			}
			
			DiffType diff; {
				auto overwritingItems = items.template map<Optional<T>>([](auto& item) {
					return Optional<T>(item);
				});
				
				diff = DiffType(existingItems, overwritingItems, AsyncListOptionalDTLCompare<T>(list));
			}
			diff.compose();
			
			LinkedList<T> settingItems;
			
			auto existingItemIt = existingItems.begin();
			size_t existingItemIndex = 0;
			
			bool displacing = false;
			bool displacementRemovesEmpty = false;
			size_t displacingStartIndex = 0;
			LinkedList<T> addingItems;
			
			size_t maxLookAhead = list.delegate->getAsyncListChunkSize(list);
			if(maxLookAhead < items.size()) {
				maxLookAhead = items.size();
			}
			
			auto itemsIt = items.begin();
			
			auto sesSeq = diff.getSes().getSequence();
			for(auto it=sesSeq.begin(); it != sesSeq.end(); it++) {
				switch(sesSeq->second.type) {
					case dtl::SES_DELETE: {
						if(!displacing) {
							displacing = true;
							displacementRemovesEmpty = false;
							displacingStartIndex = existingItemIndex;
						}
						if(!it->first.has_value()) {
							displacementRemovesEmpty = true;
						}
						// start add/delete chain
						existingItemIt++;
						existingItemIndex++;
					} break;
					case dtl::SES_ADD: {
						if(!displacing) {
							displacing = true;
							displacementRemovesEmpty = false;
							displacingStartIndex = existingItemIndex;
						}
						addingItems.pushBack(sesSeq->first.value());
						itemsIt++;
					} break;
					case dtl::SES_COMMON: {
						if(displacing) {
							displacing = false;
							if(displacingStartIndex == 0 && list.items.size() > 0 && !displacementRemovesEmpty) {
								// go down and find the items in common with the bottom added items and move them
								auto dispIt = std::make_reverse_iterator(list.items.lower_bound(index));
								auto addingItemsIt = addingItems.rbegin();
								size_t addingItemsIndex = addingItems.size() - 1;
								while(list.items.size() > 0 && dispIt != list.items.rend() && addingItemsIt != addingItems.rend()) {
									bool foundMatch = false;
									auto checkDispIt = dispIt;
									size_t lookAheadCount = 0;
									while(checkDispIt != list.items.rend() && lookAheadCount < maxLookAhead) {
										if(list.delegate->areAsyncListItemsEqual(list, checkDispIt->second.value, *addingItemsIt)) {
											foundMatch = true;
											size_t prevIndex = checkDispIt->first;
											size_t newIndex = index + settingItems.size() + addingItemsIndex;
											list.delegate->mergeAsyncListItem(list, *addingItemsIt, checkDispIt->second.value);
											auto fcheckDispIt = std::prev(checkDispIt.base(), 1);
											fcheckDispIt = list.items.erase(fcheckDispIt);
											checkDispIt = std::make_reverse_iterator(fcheckDispIt);
											if(list.items.size() > 0) {
												checkDispIt++;
											}
											dispIt = checkDispIt;
											// update index markers
											for(auto& indexMarker : list.indexMarkers) {
												if(indexMarker->index == prevIndex && indexMarker->state == AsyncListIndexMarkerState::IN_LIST) {
													indexMarker->index = newIndex;
												}
											}
											break;
										}
										lookAheadCount++;
									}
									addingItemsIt++;
									addingItemsIndex--;
								}
							}
							if(addingItems.size() > 0) {
								settingItems.splice(settingItems.end(), addingItems);
								addingItems.clear();
							}
						}
						list.delegate->mergeAsyncListItem(list, *itemsIt, existingItemIt->value());
						settingItems.pushBack(std::move(*itemsIt));
						itemsIt++;
					} break;
				}
			}
			if(displacing) {
				displacing = false;
				// go up and find the items in common with the top added items and move them
				if(list.items.size() > 0 && !displacementRemovesEmpty) {
					auto dispIt = list.items.lower_bound(index+items.size());
					auto addingItemsIt = addingItems.begin();
					size_t addingItemsIndex = 0;
					while(list.items.size() > 0 && dispIt != list.items.end() && addingItemsIt != addingItems.end()) {
						bool foundMatch = false;
						auto checkDispIt = dispIt;
						size_t lookAheadCount = 0;
						while(checkDispIt != list.items.end() && lookAheadCount < maxLookAhead) {
							if(list.delegate->areAsyncListItemsEqual(list, checkDispIt->second.value, *addingItemsIt)) {
								foundMatch = true;
								size_t prevIndex = checkDispIt->first;
								size_t newIndex = index + settingItems.size() + addingItemsIndex;
								list.delegate->mergeAsyncListItem(list, *addingItemsIt, checkDispIt->second.value);
								checkDispIt = list.items.erase(checkDispIt);
								if(list.items.size() > 0) {
									checkDispIt++;
								}
								dispIt = checkDispIt;
								// update index markers
								for(auto& indexMarker : list.indexMarkers) {
									if(indexMarker->index == prevIndex && indexMarker->state == AsyncListIndexMarkerState::IN_LIST) {
										indexMarker->index = newIndex;
									}
								}
								break;
							}
							lookAheadCount++;
						}
						addingItemsIt++;
						addingItemsIndex++;
					}
				}
				if(addingItems.size() > 0) {
					settingItems.splice(settingItems.end(), addingItems);
					addingItems.clear();
				}
			}
			
			FGL_ASSERT(settingItems.size() == items.size(), "settingItems should be the same size as items");
			items = std::move(settingItems.size());
		}
		
		lock([&]() {
			set(index, std::move(items));
			if(listSize.has_value()) {
				resize(listSize.value());
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
		std::unique_lock<std::recursive_mutex> lock(list.mutex);
		size_t i=index;
		for(auto& item : items) {
			list.items.insert_or_assign(i, AsyncList<T>::ItemNode{
				.item=std::move(item),
				.valid=true
			});
			i++;
		}
	}

	template<typename T>
	void AsyncList<T>::Mutator::insert(size_t index, LinkedList<T> items) {
		std::unique_lock<std::recursive_mutex> lock(list.mutex);
		size_t insertCount = items.size();
		// update list size
		if(list.itemsSize.has_value()) {
			list.itemsSize.value() += insertCount;
		}
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
			switch(indexMarker->state) {
				case AsyncListIndexMarkerState::IN_LIST:
					if(indexMarker->index >= index) {
						indexMarker->index += insertCount;
					}
					break;
				case AsyncListIndexMarkerState::REMOVED:
					if(indexMarker->index > index) {
						indexMarker->index += insertCount;
					}
					break;
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
		if(list.itemSize.has_value() && index >= list.itemsSize.value()) {
			return;
		}
		size_t endIndex = index + count;
		if(list.itemSize.has_value() && endIndex > list.itemsSize.value()) {
			endIndex = list.itemsSize.value();
		}
		size_t removeCount = endIndex - index;
		if(removeCount == 0) {
			return;
		}
		// update list items
		if(list.items.size() > 0) {
			using node_type = typename decltype(list->items)::node_type;
			std::list<node_type> reinsertNodes;
			auto it = std::prev(list.items.end(), 1);
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
						removeEndIt = std::next(it, 1);
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
			if(indexMarker->index >= endIndex) {
				indexMarker->index -= removeCount;
			} else if(indexMarker->index >= index) {
				switch(indexMarker->state) {
					case AsyncListIndexMarkerState::IN_LIST: {
						indexMarker->state = AsyncListIndexMarkerState::REMOVED;
						indexMarker->index = index;
					} break;
					case AsyncListIndexMarkerState::REMOVED: {
						indexMarker->index = index;
					} break;
				}
			}
		}
		// update list size
		if(list.itemsSize.has_value()) {
			list.itemsSize.value() -= removeCount;
		}
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
		if(list.items.size() > 0 && std::prev(list.items.end(),1)->first >= count) {
			auto it = std::prev(list.items.end(),1);
			bool removing = false;
			auto removeStartIt = list.items.end();
			auto removeEndIt = list.items.end();
			do {
				if(it->first >= count) {
					if(removing) {
						removeStartIt = it;
					} else {
						removeStartIt = it;
						removeEndIt = std::next(it, 1);
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
			switch(indexMarker->state) {
				case AsyncListIndexMarkerState::IN_LIST: {
					indexMarker->state = AsyncListIndexMarkerState::REMOVED;
					if(list.itemsSize) {
						indexMarker->index = list.itemsSize.value();
					}
				} break;
				case AsyncListIndexMarkerState::REMOVED: {
					if(list.itemsSize) {
						indexMarker->index = list.itemsSize.value();
					}
				} break;
			}
		}
		// update list size
		list.itemsSize = count;
	}

	template<typename T>
	void AsyncList<T>::Mutator::invalidate(size_t index, size_t count) {
		std::unique_lock<std::recursive_mutex> lock(list.mutex);
		size_t endIndex = index + count;
		if(list.itemsSize.has_value() && endIndex >= list.itemsSize) {
			endIndex = list.itemsSize.value();
		}
		for(auto it=list.items.lower_bound(index), end=list.items.end(); it!=end; it++) {
			if(it->first >= endIndex) {
				break;
			} else {
				it->second.valid = false;
			}
		}
	}

	template<typename T>
	void AsyncList<T>::Mutator::invalidateAll() {
		std::unique_lock<std::recursive_mutex> lock(list.mutex);
		for(auto & pair : list.items) {
			pair.second.valid = false;
		}
	}




	template<typename T>
	class AsyncListOptionalDTLCompare: public dtl::Compare<Optional<T>> {
	public:
		AsyncListOptionalDTLCompare(AsyncList<T>& list): list(list) {}
		
		virtual inline bool impl(const Optional<T>& e1, const Optional<T>& e2) const {
			return
				(!e1.has_value() && !e2.has_value())
				|| (e1.has_value() && e2.has_value() && list.delegate->areAsyncListItemsEqual(list, e1.value(), e2.value()));
		}
		
	private:
		AsyncList<T>& list;
	};
}
