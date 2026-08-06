#include "ocp/ordered_concurrent_pool.hpp"

#include <chrono>
#include <exception>
#include <iomanip>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

namespace {

struct Job {
  int id = 0;
  int latency_ms = 0;
  bool should_throw = false;
};

struct Result {
  int id = -1;
  bool ok = false;
};

class BenchmarkWorker final : public ocp::Worker<Job, Result> {
 public:
  void start() override {}

  Result run(const Job& job) override {
    if (job.latency_ms > 0) {
      std::this_thread::sleep_for(std::chrono::milliseconds(job.latency_ms));
    }
    if (job.should_throw) {
      throw std::runtime_error("benchmark requested failure");
    }
    return Result{job.id, true};
  }

  void stop() noexcept override {}
};

struct Scenario {
  std::size_t worker_count = 1;
  int job_count = 0;
  int latency_ms = 0;
  int throw_every = 0;
};

std::vector<Job> make_jobs(const Scenario& scenario) {
  std::vector<Job> jobs;
  jobs.reserve(static_cast<std::size_t>(scenario.job_count));
  for (int id = 0; id < scenario.job_count; ++id) {
    const bool should_throw = scenario.throw_every > 0 && id > 0 && id % scenario.throw_every == 0;
    jobs.push_back(Job{id, scenario.latency_ms, should_throw});
  }
  return jobs;
}

double run_scenario(const Scenario& scenario) {
  ocp::PoolOptions options;
  options.worker_count = scenario.worker_count;

  ocp::OrderedConcurrentPool<Job, Result> pool(
      options,
      [](std::size_t) { return std::unique_ptr<ocp::Worker<Job, Result>>(new BenchmarkWorker()); },
      [](std::size_t, const Job& job, std::exception_ptr) { return Result{job.id, false}; });

  const auto jobs = make_jobs(scenario);
  const auto start = std::chrono::steady_clock::now();
  pool.start_all();
  const auto results = pool.run_batch(jobs);
  pool.shutdown_all();
  const auto stop = std::chrono::steady_clock::now();

  for (std::size_t index = 0; index < results.size(); ++index) {
    if (results[index].id != jobs[index].id) {
      throw std::runtime_error("benchmark detected result ordering violation");
    }
  }

  const std::chrono::duration<double, std::milli> elapsed = stop - start;
  return elapsed.count();
}

}  // namespace

int main() {
  const std::vector<Scenario> scenarios = {
      {1, 100, 0, 0}, {2, 100, 0, 0}, {4, 100, 0, 0},  {8, 100, 0, 0}, {1, 100, 1, 0},
      {2, 100, 1, 0}, {4, 100, 1, 0}, {8, 100, 1, 0},  {1, 100, 5, 0}, {2, 100, 5, 0},
      {4, 100, 5, 0}, {8, 100, 5, 0}, {4, 100, 1, 10},
  };

  std::cout << "worker_count,job_count,latency_ms,throw_every,elapsed_ms\n";
  for (const auto& scenario : scenarios) {
    const auto elapsed_ms = run_scenario(scenario);
    std::cout << scenario.worker_count << ',' << scenario.job_count << ',' << scenario.latency_ms
              << ',' << scenario.throw_every << ',' << std::fixed << std::setprecision(3)
              << elapsed_ms << '\n';
  }

  return 0;
}
