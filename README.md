# OrderedConcurrentPool

OrderedConcurrentPool 是一个 C++17 header-only 有序并发 worker pool。

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
cmake/                                   CMake package 配置模板
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
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
cmake --build build
ctest --test-dir build --output-on-failure
```

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

## 许可证

当前尚未指定开源许可证。

在仓库所有者添加明确许可证前，本项目不应被视为已授权公开分发或第三方复用。

