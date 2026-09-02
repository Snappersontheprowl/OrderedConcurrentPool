# OrderedConcurrentPool

[![CI](https://github.com/Snappersontheprowl/OrderedConcurrentPool/actions/workflows/ci.yml/badge.svg)](https://github.com/Snappersontheprowl/OrderedConcurrentPool/actions/workflows/ci.yml)

OrderedConcurrentPool 是一个 C++17 header-only 有序并发 worker pool。

当前版本：`v0.1.0`

许可证：MIT

它解决的问题很窄：

- 固定数量 worker；
- batch job 并发执行；
- 输出结果严格保持输入顺序；
- 单个 job 异常由调用方转换成失败结果；
- worker startup 失败时清理已启动 worker；
- `shutdown_all()` 可重复调用。

它不是通用线程池框架，不提供动态扩缩容、优先级队列、取消、重试、分布式调度或 coroutine backend。

## 项目结构

```text
include/ocp/ordered_concurrent_pool.hpp  公开 header-only API
tests/ordered_concurrent_pool_test.cpp   GoogleTest 契约测试
examples/basic_batch.cpp                 最小使用示例
benchmarks/ordered_pool_benchmark.cpp    最小 benchmark
cmake/                                   CMake package 配置模板
doc/study_notes/                         项目开发规范与 CI/CD 学习笔记
```

## 最小示例

```cpp
#include "ocp/ordered_concurrent_pool.hpp"

#include <exception>
#include <memory>
#include <string>
#include <vector>

struct Job {
  int value = 0;
};

struct Result {
  int value = 0;
  bool ok = false;
  std::string message;
};

class Worker final : public ocp::Worker<Job, Result> {
 public:
  void start() override {}
  Result run(const Job& job) override {
    return Result{job.value * 2, true, "ok"};
  }
  void stop() noexcept override {}
};

int main() {
  ocp::PoolOptions options;
  options.worker_count = 2;

  ocp::OrderedConcurrentPool<Job, Result> pool(
      options,
      [](std::size_t) {
        return std::unique_ptr<ocp::Worker<Job, Result>>(new Worker());
      },
      [](std::size_t, const Job& job, std::exception_ptr) {
        return Result{job.value, false, "failed"};
      });

  pool.start_all();
  auto results = pool.run_batch(std::vector<Job>{{1}, {2}, {3}});
  pool.shutdown_all();
}
```

完整可编译示例见：

- `examples/basic_batch.cpp`

## 构建

```bash
cmake -S . -B .build/debug -DCMAKE_BUILD_TYPE=Debug -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
cmake --build .build/debug --parallel
ctest --test-dir .build/debug --output-on-failure
```

## CI 验证

GitHub Actions 会在 `main`、`v*` tag 与 pull request 上运行：

- Release build + GoogleTest 契约测试；
- CMake install package 验证；
- 外部 CMake consumer smoke test；
- AddressSanitizer；
- ThreadSanitizer。

其中 consumer smoke test 会先安装 `OrderedConcurrentPool`，再用临时外部项目通过
`find_package(OrderedConcurrentPool CONFIG REQUIRED)` 和
`ocp::ordered_concurrent_pool` 消费安装产物。

## Benchmark

benchmark 默认不构建，需要显式启用：

```bash
cmake -S . -B .build/benchmark \
  -DCMAKE_BUILD_TYPE=Release \
  -DORDERED_CONCURRENT_POOL_BUILD_TESTS=OFF \
  -DORDERED_CONCURRENT_POOL_BUILD_EXAMPLES=OFF \
  -DORDERED_CONCURRENT_POOL_BUILD_BENCHMARKS=ON
cmake --build .build/benchmark --parallel
./.build/benchmark/ordered_concurrent_pool_benchmark
```

输出为 CSV：

```text
worker_count,job_count,latency_ms,throw_every,elapsed_ms
```

该 benchmark 只用于本机回归观察，不代表跨机器性能承诺。

## CMake 使用方式

开发期可直接使用源码目录：

```cmake
add_subdirectory(/path/to/OrderedConcurrentPool OrderedConcurrentPool-build)

target_link_libraries(your_target
  PRIVATE
    ocp::ordered_concurrent_pool
)
```

安装后可使用：

```cmake
find_package(OrderedConcurrentPool CONFIG REQUIRED)

target_link_libraries(your_target
  PRIVATE
    ocp::ordered_concurrent_pool
)
```

## API 边界

`OrderedConcurrentPool` 不依赖任何业务域类型。

它不包含：

- SPICEUnion 类型；
- simulator session；
- work directory；
- Spectre / Ngspice；
- PSF / raw 结果读取；
- 电路指标或 optimizer。

调用方需要提供：

- `Job` 类型；
- `Result` 类型；
- `Worker<Job, Result>` 实现；
- worker factory；
- failure handler。

## 当前验证

当前测试覆盖：

- zero worker；
- null factory；
- null failure handler；
- null worker；
- start all；
- startup failure cleanup；
- run before start；
- empty batch；
- ordered result；
- out-of-order completion；
- per-job exception；
- mixed success failure；
- repeated shutdown；
- stress batch。

## 发布状态

`v0.1.0` 是首个本地发布版本，包含：

- header-only API；
- CMake target；
- CMake install/export package；
- 示例；
- 契约测试；
- 最小 benchmark；
- MIT License。

## 许可证

本项目使用 MIT License，详见 `LICENSE`。
