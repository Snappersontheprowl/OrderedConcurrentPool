#pragma once

#include <condition_variable>
#include <cstddef>
#include <exception>
#include <functional>
#include <future>
#include <memory>
#include <mutex>
#include <queue>
#include <stdexcept>
#include <utility>
#include <vector>

namespace ocp {

struct PoolOptions {
  std::size_t worker_count = 1;
};

template <class Job, class Result>
class Worker {
 public:
  virtual ~Worker() = default;

  virtual void start() = 0;
  virtual Result run(const Job& job) = 0;
  virtual void stop() noexcept = 0;
};

template <class Job, class Result>
class OrderedConcurrentPool {
 public:
  using WorkerType = Worker<Job, Result>;
  using WorkerPtr = std::unique_ptr<WorkerType>;
  using WorkerFactory = std::function<WorkerPtr(std::size_t worker_id)>;
  using FailureHandler =
      std::function<Result(std::size_t worker_id, const Job& job, std::exception_ptr error)>;

  OrderedConcurrentPool(PoolOptions options, WorkerFactory factory, FailureHandler failure_handler)
      : options_(options), failure_handler_(std::move(failure_handler)) {
    if (options_.worker_count == 0) {
      throw std::invalid_argument("worker_count must be positive");
    }
    if (!factory) {
      throw std::invalid_argument("worker factory is required");
    }
    if (!failure_handler_) {
      throw std::invalid_argument("failure handler is required");
    }

    workers_.reserve(options_.worker_count);
    for (std::size_t worker_id = 0; worker_id < options_.worker_count; ++worker_id) {
      auto worker = factory(worker_id);
      if (!worker) {
        throw std::runtime_error("worker factory returned null");
      }
      workers_.push_back(WorkerSlot{worker_id, std::move(worker)});
    }
  }

  ~OrderedConcurrentPool() {
    shutdown_all();
  }

  OrderedConcurrentPool(const OrderedConcurrentPool&) = delete;
  OrderedConcurrentPool& operator=(const OrderedConcurrentPool&) = delete;

  void start_all() {
    {
      std::lock_guard<std::mutex> lock(mutex_);
      if (started_) {
        return;
      }
    }

    std::vector<std::future<void>> futures;
    futures.reserve(workers_.size());
    for (auto& worker : workers_) {
      auto* worker_ptr = worker.worker.get();
      futures.push_back(std::async(std::launch::async, [worker_ptr]() { worker_ptr->start(); }));
    }

    std::exception_ptr first_error;
    for (auto& future : futures) {
      try {
        future.get();
      } catch (...) {
        if (!first_error) {
          first_error = std::current_exception();
        }
      }
    }

    if (first_error) {
      shutdown_all();
      std::rethrow_exception(first_error);
    }

    {
      std::lock_guard<std::mutex> lock(mutex_);
      clear_available_workers();
      for (std::size_t index = 0; index < workers_.size(); ++index) {
        available_workers_.push(index);
      }
      started_ = true;
    }
    available_.notify_all();
  }

  std::vector<Result> run_batch(const std::vector<Job>& jobs) {
    if (jobs.empty()) {
      return {};
    }
    {
      std::lock_guard<std::mutex> lock(mutex_);
      if (!started_) {
        throw std::runtime_error("ordered concurrent pool has not been started");
      }
    }

    std::vector<Result> results(jobs.size());
    std::vector<std::future<void>> futures;
    futures.reserve(jobs.size());

    for (std::size_t job_index = 0; job_index < jobs.size(); ++job_index) {
      futures.push_back(std::async(std::launch::async, [this, &jobs, &results, job_index]() {
        const auto worker_index = acquire_worker();
        const WorkerLease lease(*this, worker_index);
        const auto worker_id = workers_[worker_index].id;
        try {
          results[job_index] = workers_[worker_index].worker->run(jobs[job_index]);
        } catch (...) {
          results[job_index] =
              failure_handler_(worker_id, jobs[job_index], std::current_exception());
        }
      }));
    }

    for (auto& future : futures) {
      future.get();
    }

    return results;
  }

  void shutdown_all() noexcept {
    for (auto& worker : workers_) {
      worker.worker->stop();
    }

    {
      std::lock_guard<std::mutex> lock(mutex_);
      clear_available_workers();
      started_ = false;
    }
    available_.notify_all();
  }

  bool started() const noexcept {
    std::lock_guard<std::mutex> lock(mutex_);
    return started_;
  }

  std::size_t worker_count() const noexcept {
    return workers_.size();
  }

 private:
  struct WorkerSlot {
    std::size_t id = 0;
    WorkerPtr worker;
  };

  class WorkerLease {
   public:
    WorkerLease(OrderedConcurrentPool& pool, std::size_t worker_index) noexcept
        : pool_(pool), worker_index_(worker_index) {}

    ~WorkerLease() {
      pool_.release_worker(worker_index_);
    }

    WorkerLease(const WorkerLease&) = delete;
    WorkerLease& operator=(const WorkerLease&) = delete;

   private:
    OrderedConcurrentPool& pool_;
    std::size_t worker_index_;
  };

  std::size_t acquire_worker() {
    std::unique_lock<std::mutex> lock(mutex_);
    available_.wait(lock, [this]() { return !available_workers_.empty() || !started_; });
    if (available_workers_.empty()) {
      throw std::runtime_error("ordered concurrent pool has been stopped");
    }
    const auto worker_index = available_workers_.front();
    available_workers_.pop();
    return worker_index;
  }

  void release_worker(std::size_t worker_index) {
    {
      std::lock_guard<std::mutex> lock(mutex_);
      if (started_) {
        available_workers_.push(worker_index);
      }
    }
    available_.notify_one();
  }

  void clear_available_workers() noexcept {
    while (!available_workers_.empty()) {
      available_workers_.pop();
    }
  }

  PoolOptions options_;
  std::vector<WorkerSlot> workers_;
  FailureHandler failure_handler_;
  mutable std::mutex mutex_;
  std::condition_variable available_;
  std::queue<std::size_t> available_workers_;
  bool started_ = false;
};

}  // namespace ocp
