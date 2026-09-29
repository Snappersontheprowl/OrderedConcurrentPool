#include "ocp/ordered_concurrent_pool.hpp"

#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <exception>
#include <memory>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

namespace {

struct Job {
  int id = 0;
  int delay_ms = 0;
  bool should_throw = false;
};

struct Result {
  int id = -1;
  std::size_t worker_id = 0;
  bool ok = false;
  std::string message;
};

struct WorkerState {
  std::atomic<int> starts{0};
  std::atomic<int> stops{0};
  std::atomic<int> runs{0};
  bool fail_start = false;
};

class RecordingWorker final : public ocp::Worker<Job, Result> {
 public:
  RecordingWorker(std::size_t worker_id, std::shared_ptr<WorkerState> state)
      : worker_id_(worker_id), state_(std::move(state)) {}

  void start() override {
    state_->starts.fetch_add(1);
    if (state_->fail_start) {
      throw std::runtime_error("start failed");
    }
  }

  Result run(const Job& job) override {
    state_->runs.fetch_add(1);
    if (job.delay_ms > 0) {
      std::this_thread::sleep_for(std::chrono::milliseconds(job.delay_ms));
    }
    if (job.should_throw) {
      throw std::runtime_error("job failed");
    }
    return Result{job.id, worker_id_, true, "ok"};
  }

  void stop() noexcept override {
    state_->stops.fetch_add(1);
  }

 private:
  std::size_t worker_id_;
  std::shared_ptr<WorkerState> state_;
};

ocp::PoolOptions options(std::size_t worker_count = 2) {
  ocp::PoolOptions opts;
  opts.worker_count = worker_count;
  return opts;
}

auto failure_handler() {
  return [](std::size_t worker_id, const Job& job, std::exception_ptr error) {
    std::string message = "unknown";
    try {
      if (error) {
        std::rethrow_exception(error);
      }
    } catch (const std::exception& exc) {
      message = exc.what();
    }
    return Result{job.id, worker_id, false, message};
  };
}

auto factory_with_states(std::vector<std::shared_ptr<WorkerState>>* states) {
  return [states](std::size_t worker_id) -> std::unique_ptr<ocp::Worker<Job, Result>> {
    if (worker_id >= states->size()) {
      states->resize(worker_id + 1);
    }
    if (!(*states)[worker_id]) {
      (*states)[worker_id] = std::make_shared<WorkerState>();
    }
    return std::unique_ptr<ocp::Worker<Job, Result>>(
        new RecordingWorker(worker_id, (*states)[worker_id]));
  };
}

using TestPool = ocp::OrderedConcurrentPool<Job, Result>;

void construct_pool_with_zero_workers() {
  std::vector<std::shared_ptr<WorkerState>> states;
  TestPool pool(options(0), factory_with_states(&states), failure_handler());
}

void construct_pool_with_null_factory() {
  TestPool pool(options(1), TestPool::WorkerFactory{}, failure_handler());
}

void construct_pool_with_null_failure_handler() {
  std::vector<std::shared_ptr<WorkerState>> states;
  TestPool pool(options(1), factory_with_states(&states), TestPool::FailureHandler{});
}

void construct_pool_with_null_worker() {
  TestPool pool(
      options(1), [](std::size_t) -> std::unique_ptr<ocp::Worker<Job, Result>> { return {}; },
      failure_handler());
}

}  // namespace

TEST(OrderedConcurrentPoolTest, RejectsInvalidConstructionArguments) {
  EXPECT_THROW(construct_pool_with_zero_workers(), std::invalid_argument);
  EXPECT_THROW(construct_pool_with_null_factory(), std::invalid_argument);
  EXPECT_THROW(construct_pool_with_null_failure_handler(), std::invalid_argument);
  EXPECT_THROW(construct_pool_with_null_worker(), std::runtime_error);
}

TEST(OrderedConcurrentPoolTest, StartAllStartsEveryWorker) {
  std::vector<std::shared_ptr<WorkerState>> states;
  ocp::OrderedConcurrentPool<Job, Result> pool(options(3), factory_with_states(&states),
                                               failure_handler());

  pool.start_all();

  ASSERT_EQ(states.size(), 3U);
  EXPECT_EQ(pool.worker_count(), 3U);
  EXPECT_TRUE(pool.started());
  EXPECT_EQ(states[0]->starts.load(), 1);
  EXPECT_EQ(states[1]->starts.load(), 1);
  EXPECT_EQ(states[2]->starts.load(), 1);
}

TEST(OrderedConcurrentPoolTest, StartupFailureStopsWorkersAndPropagates) {
  std::vector<std::shared_ptr<WorkerState>> states(2);
  states[0] = std::make_shared<WorkerState>();
  states[1] = std::make_shared<WorkerState>();
  states[0]->fail_start = true;

  ocp::OrderedConcurrentPool<Job, Result> pool(options(2), factory_with_states(&states),
                                               failure_handler());

  EXPECT_THROW(pool.start_all(), std::runtime_error);
  EXPECT_FALSE(pool.started());
  EXPECT_GE(states[0]->stops.load(), 1);
  EXPECT_GE(states[1]->stops.load(), 1);
}

TEST(OrderedConcurrentPoolTest, RunBeforeStartRejectsNonEmptyBatch) {
  std::vector<std::shared_ptr<WorkerState>> states;
  ocp::OrderedConcurrentPool<Job, Result> pool(options(1), factory_with_states(&states),
                                               failure_handler());

  EXPECT_TRUE(pool.run_batch({}).empty());
  EXPECT_THROW(pool.run_batch({Job{1, 0, false}}), std::runtime_error);
}

TEST(OrderedConcurrentPoolTest, ResultsKeepInputOrderWhenJobsFinishOutOfOrder) {
  std::vector<std::shared_ptr<WorkerState>> states;
  ocp::OrderedConcurrentPool<Job, Result> pool(options(2), factory_with_states(&states),
                                               failure_handler());
  pool.start_all();

  auto results = pool.run_batch({Job{1, 80, false}, Job{2, 0, false}, Job{3, 10, false}});

  ASSERT_EQ(results.size(), 3U);
  EXPECT_EQ(results[0].id, 1);
  EXPECT_EQ(results[1].id, 2);
  EXPECT_EQ(results[2].id, 3);
  EXPECT_TRUE(results[0].ok);
  EXPECT_TRUE(results[1].ok);
  EXPECT_TRUE(results[2].ok);
}

TEST(OrderedConcurrentPoolTest, JobExceptionIsConvertedWithoutPoisoningOtherResults) {
  std::vector<std::shared_ptr<WorkerState>> states;
  ocp::OrderedConcurrentPool<Job, Result> pool(options(2), factory_with_states(&states),
                                               failure_handler());
  pool.start_all();

  auto results = pool.run_batch({Job{1, 0, false}, Job{2, 0, true}, Job{3, 0, false}});

  ASSERT_EQ(results.size(), 3U);
  EXPECT_TRUE(results[0].ok);
  EXPECT_FALSE(results[1].ok);
  EXPECT_TRUE(results[2].ok);
  EXPECT_EQ(results[1].id, 2);
  EXPECT_EQ(results[1].message, "job failed");
}

TEST(OrderedConcurrentPoolTest, ShutdownDuringBatchIsConvertedIntoFailureResults) {
  std::vector<std::shared_ptr<WorkerState>> states;
  ocp::OrderedConcurrentPool<Job, Result> pool(options(1), factory_with_states(&states),
                                               failure_handler());
  pool.start_all();

  // Every job is far longer than the shutdown delay, so at most a couple of jobs can lease the
  // single worker before the pool is stopped and the rest have to fail through the handler.
  std::vector<Job> jobs;
  for (int id = 0; id < 20; ++id) {
    jobs.push_back(Job{id, 200, false});
  }

  std::thread stopper([&pool]() {
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    pool.shutdown_all();
  });

  std::vector<Result> results;
  ASSERT_NO_THROW(results = pool.run_batch(jobs));
  stopper.join();

  ASSERT_EQ(results.size(), jobs.size());
  std::size_t failures = 0;
  for (std::size_t index = 0; index < results.size(); ++index) {
    EXPECT_EQ(results[index].id, jobs[index].id);
    if (!results[index].ok) {
      ++failures;
      EXPECT_EQ(results[index].worker_id, pool.worker_count());
    }
  }
  EXPECT_GE(failures, 15U);
  EXPECT_FALSE(pool.started());
}

TEST(OrderedConcurrentPoolTest, ShutdownIsRepeatable) {
  std::vector<std::shared_ptr<WorkerState>> states;
  ocp::OrderedConcurrentPool<Job, Result> pool(options(1), factory_with_states(&states),
                                               failure_handler());
  pool.start_all();

  pool.shutdown_all();
  pool.shutdown_all();

  EXPECT_FALSE(pool.started());
  ASSERT_EQ(states.size(), 1U);
  EXPECT_GE(states[0]->stops.load(), 2);
}

TEST(OrderedConcurrentPoolTest, StressBatchReturnsStableOrderedResults) {
  std::vector<std::shared_ptr<WorkerState>> states;
  ocp::OrderedConcurrentPool<Job, Result> pool(options(4), factory_with_states(&states),
                                               failure_handler());
  pool.start_all();

  std::vector<Job> jobs;
  for (int id = 0; id < 100; ++id) {
    jobs.push_back(Job{id, 5 - (id % 6), false});
  }

  auto results = pool.run_batch(jobs);

  ASSERT_EQ(results.size(), jobs.size());
  for (std::size_t index = 0; index < results.size(); ++index) {
    EXPECT_TRUE(results[index].ok);
    EXPECT_EQ(results[index].id, jobs[index].id);
  }
}
