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
- batch 运行中被 `shutdown_all()` 打断、或执行单元创建失败的 job，同样由调用方转换成失败结果；
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
#include <optional>
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
      [](std::optional<std::size_t>, const Job& job, std::exception_ptr) {
        return Result{job.value, false, "failed"};
      });

  pool.start_all();
  auto results = pool.run_batch(std::vector<Job>{{1}, {2}, {3}});
  pool.shutdown_all();
}
```

完整可编译示例见：

- `examples/basic_batch.cpp`

## 下游使用

本库目前被 [SPICEUnion](https://github.com/Snappersontheprowl/SPICEUnion) 用作仿真调度基础组件：
`src/pool/simulator_pool.cpp` 把 `SimulatorSession` 封装成
`ocp::Worker<ParameterState, TaskResult>`，一个 worker 对应一个可复用的仿真 session 与独立工作目录，
复用本库的保序、异常隔离与生命周期管理能力。

本库的边界也来自这里：API 只保留仿真调度真正需要的部分，业务类型（参数点、任务结果、失败分类）
全部留在下游。

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

本机一次参考结果（100 个 job，每个 job 1ms 延迟，Release 构建）：

| worker_count | 1 | 2 | 4 | 8 |
| --- | --- | --- | --- | --- |
| elapsed_ms | 118.6 | 57.4 | 28.8 | 15.4 |

同一批次中延迟为 0 的 100 个 job 约耗时 4.5–5.3 ms，即单 job 调度开销约 45–53 µs。

### 线程模型

本库的 worker 是**可复用的重资源句柄**（例如仿真 session、工作目录、外部进程上下文），不是 OS 线程。
`run_batch()` 为每个 job 创建一个执行单元，由 available worker 队列限制同时进入 `Worker::run()` 的数量，
因此：

- 同时执行的 job 数不超过 `worker_count`；
- 执行单元（线程）数量随 batch 中的 job 数量增长，与 `worker_count` 无关。

这是刻意的取舍：单个 job 的耗时远大于调度开销（仿真单点通常是百毫秒量级）时，收益来自复用重资源，
而不是把线程数固定住。如果一个 batch 里堆进上万个轻量 job，就会出现同等数量的线程，
此时应换成“固定后台线程 + 任务队列”模型。

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

## 契约与边界

调用方需要遵守的契约：

- `Result` 必须可默认构造，并且可移动或拷贝赋值：`run_batch()` 会先按 `jobs.size()` 构造结果向量，
  再用每个 job 的结果覆盖对应槽位。
- 必须先 `start_all()` 再 `run_batch()`。对未启动的 pool 调用非空 batch 会抛 `std::runtime_error`；
  空 batch 直接返回空结果，不做启动检查。
- 一旦 batch 被接受，`run_batch()` 必定返回与输入等长的结果：单个 job 抛异常、job 始终没有取得
  worker（例如 batch 运行中另一个线程调用了 `shutdown_all()`）、或 job 的执行单元创建失败
  （线程创建失败等环境问题），都会通过 failure handler 转换成失败结果，而不是抛出。
- failure handler 接收的 worker id 是 `std::optional<std::size_t>`：`std::nullopt` 表示该 job
  从未分配到 worker（没取得 worker，或根本没被派发出去）；有值时一定是 `[0, worker_count())`
  内的真实 id。
- failure handler 不应抛异常：它抛出的异常会逃出 `run_batch()`。

pool 自身的边界：

- `shutdown_all()` 不等待正在执行的 job。此时 `Worker::stop()` 可能与 `Worker::run()` 并发发生，
  worker 实现需要自行保证这一点是安全的。
- `shutdown_all()` 之后再次 `start_all()` 会对所有 worker 重新调用 `start()`，即重启语义，
  worker 实现需要能接受重复 `start()`。
- `start_all()`、`run_batch()`、`shutdown_all()` 应由同一线程串行发起；`start_all()` 的启动检查不在
  同一临界区内，不保证并发调用安全。

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
- shutdown 与在飞 batch 并发；
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
