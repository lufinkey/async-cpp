//
//  AsyncQueue.cpp
//  AsyncCpp
//
//  Created by Luis Finke on 11/17/19.
//  Copyright © 2019 Luis Finke. All rights reserved.
//

#include <fgl/async/AsyncQueue.hpp>
#include <mutex>

namespace fgl {
	size_t nextAsyncQueueTaskBeginListenerId() {
		static std::mutex mtx;
		static size_t nextId = 0;
		mtx.lock();
		size_t listenerId = nextId;
		nextId++;
		mtx.unlock();
		return listenerId;
	}

	size_t nextAsyncQueueTaskStatusChangeListenerId() {
		static std::mutex mtx;
		static size_t nextId = 0;
		mtx.lock();
		size_t listenerId = nextId;
		nextId++;
		mtx.unlock();
		return listenerId;
	}

	size_t nextAsyncQueueTaskCancelListenerId() {
		static std::mutex mtx;
		static size_t nextId = 0;
		mtx.lock();
		size_t listenerId = nextId;
		nextId++;
		mtx.unlock();
		return listenerId;
	}

	size_t nextAsyncQueueTaskErrorListenerId() {
		static std::mutex mtx;
		static size_t nextId = 0;
		mtx.lock();
		size_t listenerId = nextId;
		nextId++;
		mtx.unlock();
		return listenerId;
	}

	size_t nextAsyncQueueTaskEndListenerId() {
		static std::mutex mtx;
		static size_t nextId = 0;
		mtx.lock();
		size_t listenerId = nextId;
		nextId++;
		mtx.unlock();
		return listenerId;
	}



	class AsyncQueueTaskFunctionalEventListener: public AsyncQueue::Task::AutoDeletedEventListener {
	public:
		virtual void onAsyncQueueTaskBegin(std::shared_ptr<AsyncQueue::Task> task) override;
		virtual void onAsyncQueueTaskCancel(std::shared_ptr<AsyncQueue::Task> task) override;
		virtual void onAsyncQueueTaskStatusChange(std::shared_ptr<AsyncQueue::Task> task) override;
		virtual void onAsyncQueueTaskError(std::shared_ptr<AsyncQueue::Task> task, std::exception_ptr error) override;
		virtual void onAsyncQueueTaskEnd(std::shared_ptr<AsyncQueue::Task> task) override;
		
		std::map<size_t,AsyncQueue::Task::BeginListener> beginListeners;
		std::map<size_t,AsyncQueue::Task::CancelListener> cancelListeners;
		std::map<size_t,AsyncQueue::Task::StatusChangeListener> statusChangeListeners;
		std::map<size_t,AsyncQueue::Task::ErrorListener> errorListeners;
		std::map<size_t,AsyncQueue::Task::EndListener> endListeners;
	};

	void AsyncQueueTaskFunctionalEventListener::onAsyncQueueTaskBegin(std::shared_ptr<AsyncQueue::Task> task) {
		std::map<size_t,AsyncQueue::Task::BeginListener> beginListeners;
		beginListeners.swap(this->beginListeners);
		for(auto& pair : beginListeners) {
			pair.second(task);
		}
	}

	void AsyncQueueTaskFunctionalEventListener::onAsyncQueueTaskCancel(std::shared_ptr<AsyncQueue::Task> task) {
		std::map<size_t,AsyncQueue::Task::CancelListener> cancelListeners;
		cancelListeners.swap(this->cancelListeners);
		for(auto& pair : cancelListeners) {
			pair.second(task);
		}
	}

	void AsyncQueueTaskFunctionalEventListener::onAsyncQueueTaskStatusChange(std::shared_ptr<AsyncQueue::Task> task) {
		auto statusChangeListeners = this->statusChangeListeners;
		for(auto& pair : statusChangeListeners) {
			pair.second(task, pair.first);
		}
	}

	void AsyncQueueTaskFunctionalEventListener::onAsyncQueueTaskError(std::shared_ptr<AsyncQueue::Task> task, std::exception_ptr error) {
		this->statusChangeListeners.clear();
		this->cancelListeners.clear();
		std::map<size_t,AsyncQueue::Task::ErrorListener> errorListeners;
		errorListeners.swap(this->errorListeners);
		for(auto& pair : errorListeners) {
			pair.second(task, error);
		}
	}

	void AsyncQueueTaskFunctionalEventListener::onAsyncQueueTaskEnd(std::shared_ptr<AsyncQueue::Task> task) {
		this->statusChangeListeners.clear();
		this->cancelListeners.clear();
		std::map<size_t,AsyncQueue::Task::EndListener> endListeners;
		endListeners.swap(this->endListeners);
		for(auto& pair : endListeners) {
			pair.second(task);
		}
	}
	



	AsyncQueue::AsyncQueue(Options options)
	: options(options), aliveStatus(std::make_shared<AliveStatus>()) {
		//
	}

	AsyncQueue::~AsyncQueue() {
		aliveStatus->alive = false;
	}

	size_t AsyncQueue::taskCount() const {
		return taskQueue.size();
	}

	DispatchQueue* AsyncQueue::dispatchQueue() const {
		return options.dispatchQueue;
	}

	Optional<AsyncQueue::TaskNode> AsyncQueue::getTaskWithName(const String& name) {
		for(auto& taskNode : taskQueue) {
			if(taskNode.task->getName() == name && !(taskNode.task->isCancelled() && !taskNode.task->isPerforming())) {
				return taskNode;
			}
		}
		return std::nullopt;
	}

	Optional<AsyncQueue::TaskNode> AsyncQueue::getTaskWithTag(const String& tag) {
		for(auto& taskNode : taskQueue) {
			if(taskNode.task->getTag() == tag && !(taskNode.task->isCancelled() && !taskNode.task->isPerforming())) {
				return taskNode;
			}
		}
		return std::nullopt;
	}

	LinkedList<AsyncQueue::TaskNode> AsyncQueue::getTasksWithTag(const String& tag) {
		std::unique_lock<std::recursive_mutex> lock(mutex);
		LinkedList<AsyncQueue::TaskNode> tasks;
		for(auto& taskNode : taskQueue) {
			if(taskNode.task->getTag() == tag && !(taskNode.task->isCancelled() && !taskNode.task->isPerforming())) {
				tasks.push_back(taskNode);
			}
		}
		return tasks;
	}

	Optional<size_t> AsyncQueue::indexOfTaskWithTag(const String& tag) const {
		std::unique_lock<std::recursive_mutex> lock(mutex);
		size_t index = 0;
		for(auto& taskNode : taskQueue) {
			if(taskNode.task->getTag() == tag && !(taskNode.task->isCancelled() && !taskNode.task->isPerforming())) {
				return index;
			}
			index++;
		}
		return std::nullopt;
	}

	void AsyncQueue::cancelAllTasks() {
		std::unique_lock<std::recursive_mutex> lock(mutex);
		auto tasks = taskQueue;
		for(auto& taskNode : tasks) {
			taskNode.task->cancel();
		}
	}

	void AsyncQueue::cancelTasksWithTag(const String& tag) {
		std::unique_lock<std::recursive_mutex> lock(mutex);
		auto tasks = taskQueue;
		for(auto& taskNode : tasks) {
			if(taskNode.task->getTag() == tag) {
				taskNode.task->cancel();
			}
		}
	}

	void AsyncQueue::cancelTasksWithTags(const ArrayList<String>& tags) {
		std::unique_lock<std::recursive_mutex> lock(mutex);
		auto tasks = taskQueue;
		for(auto& taskNode : tasks) {
			for(auto& tag : tags) {
				if(taskNode.task->getTag() == tag) {
					taskNode.task->cancel();
					break;
				}
			}
		}
	}

	Promise<void> AsyncQueue::waitForCurrentTasks() const {
		std::unique_lock<std::recursive_mutex> lock(mutex);
		return taskQueuePromise.value_or(Promise<void>::resolve());
	}

	Promise<void> AsyncQueue::waitForTasksWithTag(const String& tag) const {
		std::unique_lock<std::recursive_mutex> lock(mutex);
		LinkedList<Promise<void>> promises;
		for(auto& taskNode : taskQueue) {
			if(taskNode.task->getTag() == tag && !(taskNode.task->isCancelled() && !taskNode.task->isPerforming())) {
				promises.push_back(taskNode.promise);
			}
		}
		return Promise<void>::all(ArrayList<Promise<void>>(std::make_move_iterator(promises.begin()), std::make_move_iterator(promises.end())));
	}

	void AsyncQueue::removeTask(std::shared_ptr<Task> task) {
		std::unique_lock<std::recursive_mutex> lock(mutex);
		for(auto it=taskQueue.begin(), end=taskQueue.end(); it!=end; it++) {
			if(it->task == task) {
				taskQueue.erase(it);
				break;
			}
		}
		if(taskQueue.size() == 0) {
			taskQueuePromise = std::nullopt;
		}
	}



	std::shared_ptr<AsyncQueue::Task> AsyncQueue::Task::new$(Options options, Function<Promise<void>(std::shared_ptr<Task>)> executor) {
		return std::make_shared<Task>(options, executor);
	}

	AsyncQueue::Task::Task(Options options, Function<Promise<void>(std::shared_ptr<Task>)> executor)
	: options(options), executor(executor), cancelled(false), done(false) {
		//
	}

	AsyncQueue::Task::EventListener* AsyncQueue::Task::functionalEventListener() {
		for(auto listener : eventListeners) {
			if(auto castListener = dynamic_cast<AsyncQueueTaskFunctionalEventListener*>(listener)) {
				return castListener;
			}
		}
		auto castListener = new AsyncQueueTaskFunctionalEventListener();
		eventListeners.pushFront(castListener);
		return castListener;
	}

	AsyncQueue::Task::~Task() {
		for(auto listener : eventListeners) {
			if(auto castListener = dynamic_cast<AutoDeletedEventListener*>(listener)) {
				delete castListener;
			}
		}
	}

	Promise<void> AsyncQueue::Task::perform() {
		std::unique_lock<std::recursive_mutex> lock(mutex);
		FGL_ASSERT(executor != nullptr, "Cannot call Task::perform without an executor");
		FGL_ASSERT(!promise.has_value(), "Cannot call Task::perform more than once");
		FGL_ASSERT(!done, "Cannot call Task::perform on a finished task");
		auto self = shared_from_this();
		auto eventListeners = this->eventListeners;
		for(auto listener : eventListeners) {
			listener->onAsyncQueueTaskBegin(self);
		}
		auto promise = executor(self).then(nullptr, [=]() {
			std::unique_lock<std::recursive_mutex> lock(self->mutex);
			self->done = true;
			self->promise = std::nullopt;
			auto eventListeners = self->eventListeners;
			lock.unlock();
			for(auto listener : eventListeners) {
				listener->onAsyncQueueTaskEnd(self);
			}
		}, [=](std::exception_ptr error) {
			std::unique_lock<std::recursive_mutex> lock(self->mutex);
			self->done = true;
			self->promise = std::nullopt;
			auto eventListeners = self->eventListeners;
			lock.unlock();
			for(auto listener : eventListeners) {
				listener->onAsyncQueueTaskError(self, error);
			}
			std::rethrow_exception(error);
		});
		self->promise = promise;
		executor = nullptr;
		if(self->promise && self->promise->isComplete()) {
			self->promise = std::nullopt;
		}
		return promise;
	}

	const String& AsyncQueue::Task::getTag() const {
		return options.tag;
	}

	const String& AsyncQueue::Task::getName() const {
		return options.name;
	}

	void AsyncQueue::Task::addEventListener(EventListener* listener) {
		FGL_ASSERT(listener != nullptr, "listener cannot be null");
		std::unique_lock<std::recursive_mutex> lock(mutex);
		eventListeners.pushBack(listener);
	}

	void AsyncQueue::Task::removeEventListener(EventListener* listener) {
		FGL_ASSERT(listener != nullptr, "listener cannot be null");
		std::unique_lock<std::recursive_mutex> lock(mutex);
		bool removed = eventListeners.removeLastEqual(listener);
		if(removed) {
			if(auto castListener = dynamic_cast<AutoDeletedEventListener*>(listener)) {
				delete castListener;
			}
		}
	}

	size_t AsyncQueue::Task::addBeginListener(BeginListener listener) {
		std::unique_lock<std::recursive_mutex> lock(mutex);
		if(promise || done) {
			return (size_t)-1;
		}
		auto funcListener = static_cast<AsyncQueueTaskFunctionalEventListener*>(functionalEventListener());
		size_t listenerId;
		do {
			listenerId = nextAsyncQueueTaskBeginListenerId();
		} while(funcListener->beginListeners.find(listenerId) != funcListener->beginListeners.end());
		funcListener->beginListeners[listenerId] = listener;
		return listenerId;
	}

	bool AsyncQueue::Task::removeBeginListener(size_t listenerId) {
		std::unique_lock<std::recursive_mutex> lock(mutex);
		auto funcListener = static_cast<AsyncQueueTaskFunctionalEventListener*>(functionalEventListener());
		auto it = funcListener->beginListeners.find(listenerId);
		if(it == funcListener->beginListeners.end()) {
			return false;
		}
		funcListener->beginListeners.erase(it);
		return true;
	}

	size_t AsyncQueue::Task::addErrorListener(ErrorListener listener) {
		std::unique_lock<std::recursive_mutex> lock(mutex);
		if(promise || done) {
			return (size_t)-1;
		}
		auto funcListener = static_cast<AsyncQueueTaskFunctionalEventListener*>(functionalEventListener());
		size_t listenerId;
		do {
			listenerId = nextAsyncQueueTaskErrorListenerId();
		} while(funcListener->errorListeners.find(listenerId) != funcListener->errorListeners.end());
		funcListener->errorListeners[listenerId] = listener;
		return listenerId;
	}

	bool AsyncQueue::Task::removeErrorListener(size_t listenerId) {
		std::unique_lock<std::recursive_mutex> lock(mutex);
		auto funcListener = static_cast<AsyncQueueTaskFunctionalEventListener*>(functionalEventListener());
		auto it = funcListener->errorListeners.find(listenerId);
		if(it == funcListener->errorListeners.end()) {
			return false;
		}
		funcListener->errorListeners.erase(it);
		return true;
	}

	size_t AsyncQueue::Task::addEndListener(EndListener listener) {
		std::unique_lock<std::recursive_mutex> lock(mutex);
		if(promise || done) {
			return (size_t)-1;
		}
		auto funcListener = static_cast<AsyncQueueTaskFunctionalEventListener*>(functionalEventListener());
		size_t listenerId;
		do {
			listenerId = nextAsyncQueueTaskEndListenerId();
		} while(funcListener->endListeners.find(listenerId) != funcListener->endListeners.end());
		funcListener->endListeners[listenerId] = listener;
		return listenerId;
	}

	bool AsyncQueue::Task::removeEndListener(size_t listenerId) {
		std::unique_lock<std::recursive_mutex> lock(mutex);
		auto funcListener = static_cast<AsyncQueueTaskFunctionalEventListener*>(functionalEventListener());
		auto it = funcListener->endListeners.find(listenerId);
		if(it == funcListener->endListeners.end()) {
			return false;
		}
		funcListener->endListeners.erase(it);
		return true;
	}
	
	void AsyncQueue::Task::cancel() {
		std::unique_lock<std::recursive_mutex> lock(mutex);
		if(done) {
			return;
		}
		if(cancelled) {
			return;
		}
		cancelled = true;
		auto self = shared_from_this();
		auto eventListeners = this->eventListeners;
		lock.unlock();
		for(auto listener : eventListeners) {
			listener->onAsyncQueueTaskCancel(self);
		}
	}

	bool AsyncQueue::Task::isCancelled() const {
		return cancelled;
	}

	size_t AsyncQueue::Task::addCancelListener(CancelListener listener) {
		std::unique_lock<std::recursive_mutex> lock(mutex);
		if(cancelled) {
			return (size_t)-1;
		}
		auto funcListener = static_cast<AsyncQueueTaskFunctionalEventListener*>(functionalEventListener());
		size_t listenerId;
		do {
			listenerId = nextAsyncQueueTaskCancelListenerId();
		} while(funcListener->cancelListeners.find(listenerId) != funcListener->cancelListeners.end());
		funcListener->cancelListeners[listenerId] = listener;
		return listenerId;
	}

	bool AsyncQueue::Task::removeCancelListener(size_t listenerId) {
		std::unique_lock<std::recursive_mutex> lock(mutex);
		auto funcListener = static_cast<AsyncQueueTaskFunctionalEventListener*>(functionalEventListener());
		auto it = funcListener->cancelListeners.find(listenerId);
		if(it == funcListener->cancelListeners.end()) {
			return false;
		}
		funcListener->cancelListeners.erase(it);
		return true;
	}

	bool AsyncQueue::Task::isPerforming() const {
		return promise.has_value();
	}

	bool AsyncQueue::Task::isDone() const {
		return done;
	}
	
	AsyncQueue::Task::Status AsyncQueue::Task::getStatus() const {
		std::unique_lock<std::recursive_mutex> lock(mutex);
		auto status = this->status;
		lock.unlock();
		return status;
	}

	void AsyncQueue::Task::setStatus(Status status) {
		std::unique_lock<std::recursive_mutex> lock(mutex);
		this->status = status;
		auto self = shared_from_this();
		auto eventListeners = this->eventListeners;
		lock.unlock();
		for(auto listener : eventListeners) {
			listener->onAsyncQueueTaskStatusChange(self);
		}
	}

	void AsyncQueue::Task::setStatusText(String text) {
		std::unique_lock<std::recursive_mutex> lock(mutex);
		status.text = text;
		auto self = shared_from_this();
		auto eventListeners = this->eventListeners;
		lock.unlock();
		for(auto listener : eventListeners) {
			listener->onAsyncQueueTaskStatusChange(self);
		}
	}

	void AsyncQueue::Task::setStatusProgress(double progress) {
		std::unique_lock<std::recursive_mutex> lock(mutex);
		status.progress = progress;
		auto self = shared_from_this();
		auto eventListeners = this->eventListeners;
		lock.unlock();
		for(auto listener : eventListeners) {
			listener->onAsyncQueueTaskStatusChange(self);
		}
	}

	size_t AsyncQueue::Task::addStatusChangeListener(StatusChangeListener listener) {
		std::unique_lock<std::recursive_mutex> lock(mutex);
		auto funcListener = static_cast<AsyncQueueTaskFunctionalEventListener*>(functionalEventListener());
		size_t listenerId;
		do {
			listenerId = nextAsyncQueueTaskStatusChangeListenerId();
		} while(funcListener->statusChangeListeners.find(listenerId) != funcListener->statusChangeListeners.end());
		funcListener->statusChangeListeners[listenerId] = listener;
		return listenerId;
	}

	bool AsyncQueue::Task::removeStatusChangeListener(size_t listenerId) {
		std::unique_lock<std::recursive_mutex> lock(mutex);
		auto funcListener = static_cast<AsyncQueueTaskFunctionalEventListener*>(functionalEventListener());
		auto it = funcListener->statusChangeListeners.find(listenerId);
		if(it == funcListener->statusChangeListeners.end()) {
			return false;
		}
		funcListener->statusChangeListeners.erase(it);
		return true;
	}
}
