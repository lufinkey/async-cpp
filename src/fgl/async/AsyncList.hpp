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
#include <cmath>
#include <list>
#include <map>
#include <memory>
#include <variant>

namespace fgl {
	struct AsyncListGetLoadedItemsOptions {
		size_t startIndex = 0;
		size_t limit = (size_t)-1;
		bool onlyValidItems = true;
	};

	struct AsyncListLoadItemOptions {
		bool trackIndexChanges = false;
		bool forceReload = false;
		std::map<String,Any> loadOptions;
	};

	struct AsyncListIndexAccessOptions {
		bool onlyValidItems = true;
	};

	enum class AsyncListIndexMarkerState: uint8_t {
		// the index is within the list
		IN_LIST,
		// the index has been pushed outside the list bounds, but has not been verified as removed from the list
		DISPLACED,
		// the index has been removed and now sits between the end of the previous index and the beginning of the marker index
		REMOVED
	};
	String AsyncListIndexMarkerState_toString(AsyncListIndexMarkerState);

	struct AsyncListIndexMarkerData {
		size_t index;
		AsyncListIndexMarkerState state;
		
		AsyncListIndexMarkerData(size_t index, AsyncListIndexMarkerState state);
		static SharedPtr<AsyncListIndexMarkerData> new$(size_t index, AsyncListIndexMarkerState state);
		
		String toString() const;
	};
	typedef SharedPtr<AsyncListIndexMarkerData> AsyncListIndexMarker;

	struct AsyncListMutation {
		enum class Type {
			REMOVE,
			INSERT,
			MOVE,
			LIFT_AND_INSERT,
			RESIZE
		};
		
		Type type = (Type)-1;
		size_t index = -1;
		size_t count = 0;
		Optional<size_t> newIndex;
		Optional<size_t> upperShiftEndIndex;
		
		void applyToMarkers(LinkedList<AsyncListIndexMarker>& markers, size_t& listSize);
	};

	struct AsyncListChange {
		LinkedList<AsyncListMutation> mutations;
		Optional<size_t> prevSize;
		size_t prevCapacity;
	};

	template<typename T, typename InsT>
	class AsyncListOptionalDTLCompare;


	

	

	template<typename T, typename InsT=T>
	class AsyncList: public std::enable_shared_from_this<AsyncList<T,InsT>> {
	public:
		using Mutation = AsyncListMutation;
		using ItemGenerator = ContinuousGenerator<LinkedList<T>,void>;
		friend class AsyncListOptionalDTLCompare<T,InsT>;
		
		class Mutator {
			friend class AsyncList;
		public:
			Mutator(const Mutator&) = delete;
			
			AsyncList* getList();
			const AsyncList* getList() const;
			
			template<typename Work>
			void lock(Work work);
			void apply(size_t index, LinkedList<T> items);
			void apply(std::map<size_t,T> items);
			void applyAndResize(size_t index, size_t listSize, LinkedList<T> items);
			void applyAndResize(size_t listSize, std::map<size_t,T> items);
			void set(size_t index, LinkedList<T> items);
			void insert(size_t index, LinkedList<T> items);
			void remove(size_t index, size_t count);
			void move(size_t index, size_t count, size_t newIndex);
			void resize(size_t count);
			void invalidate(size_t index, size_t count);
			void invalidateAll();
			void resetItems();
			void resetSize();
			void reset();
			
		private:
			void applyMerge(size_t index, Optional<size_t> listSize, LinkedList<T> items);
			
			Mutator(AsyncList* list);
			
			AsyncList* list;
			size_t lockCount;
			std::list<Mutation> mutations;
			bool forwardingMutations;
		};
		friend class AsyncList<T,InsT>::Mutator;
		
		class Delegate {
		public:
			virtual ~Delegate() {}
			
			virtual size_t getAsyncListChunkSize(const AsyncList* list) const = 0;
			virtual Promise<void> loadAsyncListItems(Mutator* mutator, size_t index, size_t count, Map<String,Any> options) = 0;
			
			virtual bool areAsyncListItemsEqual(const AsyncList* list, const T& item1, const T& item2) const = 0;
			virtual void mergeAsyncListItem(const AsyncList* list, T& overwritingItem, T& existingItem) = 0;
			virtual Promise<void> insertAsyncListItems(Mutator* mutator, size_t index, LinkedList<InsT> items, Map<String,Any> options) = 0;
			virtual Promise<void> appendAsyncListItems(Mutator* mutator, LinkedList<InsT> items, Map<String,Any> options) = 0;
			virtual Promise<void> removeAsyncListItems(Mutator* mutator, size_t index, size_t count, Map<String,Any> options) = 0;
			virtual Promise<void> moveAsyncListItems(Mutator* mutator, size_t index, size_t count, size_t newIndex, Map<String,Any> options) = 0;
			
			virtual void onAsyncListMutations(const AsyncList<T,InsT>* list, AsyncListChange change) = 0;
		};
		
		struct ItemNode {
			T item;
			bool valid = true;
		};
		
		struct Options {
			Delegate* delegate = nullptr;
			DispatchQueue* dispatchQueue = defaultPromiseQueue();
			std::map<size_t,T> initialItems;
			Optional<size_t> initialSize;
			//bool overwriteDisabled = false;
		};
		
		static SharedPtr<AsyncList<T,InsT>> new$(Options options);
		
		AsyncList(const AsyncList&) = delete;
		AsyncList& operator=(const AsyncList&) = delete;
		AsyncList(Options options);
		
		void destroy();
		void reset();
		void resetItems();
		void resetSize();
		
		inline const std::map<size_t,ItemNode>& getMap() const;
		inline Optional<size_t> size() const;
		inline size_t length() const;
		inline size_t capacity() const;
		inline size_t getChunkSize() const;
		bool hasAllItems() const;
		
		AsyncListIndexMarker watchIndex(size_t index);
		AsyncListIndexMarker watchRemovedIndex(size_t index);
		AsyncListIndexMarker watchIndex(AsyncListIndexMarker index);
		void unwatchIndex(AsyncListIndexMarker index);
		
		bool isItemLoaded(size_t index, const AsyncListIndexAccessOptions& options = AsyncListIndexAccessOptions()) const;
		bool areItemsLoaded(size_t index, size_t count, const AsyncListIndexAccessOptions& options = AsyncListIndexAccessOptions()) const;
		LinkedList<T> getLoadedItems(const AsyncListGetLoadedItemsOptions& options = AsyncListGetLoadedItemsOptions()) const;
		LinkedList<Optional<T>> maybeGetLoadedItems(const AsyncListGetLoadedItemsOptions& options = AsyncListGetLoadedItemsOptions()) const;
		
		Optional<ItemNode> itemNodeAt(size_t index) const;
		Optional<T> itemAt(size_t index, const AsyncListIndexAccessOptions& options = AsyncListIndexAccessOptions()) const;
		Promise<Optional<T>> getItem(size_t index, AsyncListLoadItemOptions options = AsyncListLoadItemOptions());
		Promise<LinkedList<T>> getItems(size_t index, size_t count, AsyncListLoadItemOptions options = AsyncListLoadItemOptions());
		Promise<void> loadItems(size_t index, size_t count, AsyncListLoadItemOptions options = AsyncListLoadItemOptions());
		ItemGenerator generateItems(size_t startIndex=0, AsyncListLoadItemOptions options = AsyncListLoadItemOptions());
		
		template<typename Callable>
		Optional<size_t> indexWhere(Callable predicate, const AsyncListIndexAccessOptions& options = AsyncListIndexAccessOptions()) const;
		
		void forEachNode(Function<void(ItemNode&,size_t)> executor);
		void forEachNode(Function<void(const ItemNode&,size_t)> executor) const;
		void forEachNodeInRange(size_t startIndex, size_t endIndex, Function<void(ItemNode&,size_t)> executor);
		void forEachNodeInRange(size_t startIndex, size_t endIndex, Function<void(const ItemNode&,size_t)> executor) const;
		
		void forEach(Function<void(T&,size_t)> executor, const AsyncListIndexAccessOptions& options = AsyncListIndexAccessOptions());
		void forEach(Function<void(const T&,size_t)> executor, const AsyncListIndexAccessOptions& options = AsyncListIndexAccessOptions()) const;
		void forEachInRange(size_t startIndex, size_t endIndex, Function<void(T&,size_t)> executor, const AsyncListIndexAccessOptions& options = AsyncListIndexAccessOptions());
		void forEachInRange(size_t startIndex, size_t endIndex, Function<void(const T&,size_t)> executor, const AsyncListIndexAccessOptions& options = AsyncListIndexAccessOptions()) const;
		
		template<typename MutatorFunc>
		Promise<void> mutate(MutatorFunc executor);
		template<typename Work>
		auto lock(Work work) -> decltype(work((Mutator*)nullptr));
		
		void invalidateItems(size_t startIndex, size_t endIndex, bool runInQueue = false);
		void invalidateAllItems(bool runInQueue = false);
		
		Promise<void> insertItems(size_t index, LinkedList<InsT> items, Map<String,Any> options = {});
		Promise<void> appendItems(LinkedList<InsT> items, Map<String,Any> options = {});
		Promise<void> removeItems(size_t index, size_t count, Map<String,Any> options = {});
		Promise<void> moveItems(size_t index, size_t count, size_t newIndex, Map<String,Any> options = {});
		
	private:
		static size_t chunkStartIndexForIndex(size_t index, size_t chunkSize);
		
		mutable std::recursive_mutex mutex;
		std::map<size_t,ItemNode> items;
		Optional<size_t> itemsSize;
		//bool overwriteDisabled;
		
		LinkedList<AsyncListIndexMarker> indexMarkers;
		
		AsyncQueue mutationQueue;
		Mutator mutator;
		Delegate* delegate;
	};
}

#include <fgl/async/AsyncList.impl.hpp>
#include <fgl/async/AsyncListMutator.impl.h>
