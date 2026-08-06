#include "ocp/ordered_concurrent_pool.hpp"

#include <exception>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

namespace {

struct Job {
  int value = 0;
  bool fail = false;
};

struct Result {
  int value = 0;
  bool ok = false;
  std::string message;
};

class DoublingWorker final : public ocp::Worker<Job, Result> {
 public:
  void start() override {}

  Result run(const Job& job) override {
    if (job.fail) {
      throw std::runtime_error("job requested failure");
    }
    return Result{job.value * 2, true, "ok"};
  }

  void stop() noexcept override {}
};

std::string exception_message(std::exception_ptr error) {
  try {
    if (error) {
      std::rethrow_exception(error);
    }
  } catch (const std::exception& exc) {
    return exc.what();
  } catch (...) {
    return "unknown exception";
  }
  return "unknown exception";
}

}  // namespace

int main() {
  ocp::PoolOptions options;
  options.worker_count = 2;

  ocp::OrderedConcurrentPool<Job, Result> pool(
      options,
      [](std::size_t) { return std::unique_ptr<ocp::Worker<Job, Result>>(new DoublingWorker()); },
      [](std::size_t, const Job& job, std::exception_ptr error) {
        return Result{job.value, false, exception_message(error)};
      });

  pool.start_all();
  const auto results = pool.run_batch(std::vector<Job>{{1, false}, {2, true}, {3, false}});
  pool.shutdown_all();

  for (const auto& result : results) {
    std::cout << (result.ok ? "ok" : "failed") << " value=" << result.value
              << " message=" << result.message << '\n';
  }

  return 0;
}
