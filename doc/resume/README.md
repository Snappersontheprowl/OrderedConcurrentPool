# OrderedConcurrentPool 简历材料

本文档用于将 `OrderedConcurrentPool` 项目整理为校招简历与面试材料。推荐根据投递岗位选择 4-6 条放入简历正文，面试时再展开后半部分。

## 简历正文推荐版

**项目名称：OrderedConcurrentPool - C++17 Header-only 有序并发 Worker Pool**

**l 项目简介：** 设计并实现一个 C++17 header-only 有序并发 worker pool，用于将一批输入任务并发分发到固定数量 worker 执行，并保证输出结果严格保持输入顺序。项目聚焦“小而可靠”的并发基础设施能力，为上层仿真、批处理和参数扫描类任务提供可复用的调度组件。

**l 并发调度：** 基于 `std::async`、`std::mutex`、`std::condition_variable` 与 worker lease 机制实现固定 worker 数量下的任务并发执行，通过可用 worker 队列控制资源占用，避免每个任务独占业务对象导致的生命周期混乱。

**l 保序结果：** 将 job index 与 result slot 显式绑定，使任务即使乱序完成也能写回原始输入位置，从调度层保证 batch 输出稳定保序，降低调用方在批量仿真、数据后处理和结果聚合阶段的额外排序成本。

**l 异常隔离：** 设计 `FailureHandler` 回调将单个 job 的异常转换为业务失败结果，避免局部任务失败污染整批执行；同时在 worker startup 失败时清理已启动 worker，保证启动阶段异常能够向调用方传播并保持资源状态可收敛。

**l 生命周期管理：** 抽象 `Worker<Job, Result>` 接口，统一约束 `start/run/stop` 生命周期；支持 `shutdown_all()` 重复调用，并在析构阶段自动关闭 worker，降低调用方遗漏清理带来的并发资源泄漏风险。

**l 工程化交付：** 以 header-only 方式提供公开 API，导出 CMake target `ocp::ordered_concurrent_pool`，支持 `add_subdirectory` 与 `find_package` 两种消费方式；补充 example、benchmark、MIT License、CHANGELOG 与 CMake install/export package，形成可被外部 CMake 项目直接集成的轻量级库。

**l 质量验证：** 使用 GoogleTest 编写契约测试，覆盖非法构造参数、worker 启动失败清理、启动前运行拒绝、空 batch、乱序完成下的保序结果、job 异常转换、重复 shutdown 与 stress batch 等场景；CI 中补充 Release build、外部 consumer smoke test、AddressSanitizer 与 ThreadSanitizer 验证，提升并发库基础可靠性。

## 精简版

如果简历空间较紧，可以压缩为以下 3 条：

**l 项目简介：** 实现一个 C++17 header-only 有序并发 worker pool，支持固定数量 worker 并发执行 batch job，并保证结果严格按输入顺序返回，可作为上层仿真、批处理与参数扫描任务的通用调度基础组件。

**l 核心设计：** 基于 `std::async`、互斥锁、条件变量和 worker lease 机制管理 worker 获取与释放，通过 job index 绑定 result slot 解决任务乱序完成后的结果保序问题，并用 `FailureHandler` 将单任务异常转换为业务失败结果。

**l 工程验证：** 提供 header-only API、CMake target `ocp::ordered_concurrent_pool`、install/export package、example 与 benchmark；使用 GoogleTest 覆盖启动失败清理、异常隔离、重复 shutdown、保序输出等契约，并通过 Release build、consumer smoke test、ASan/TSan CI 验证。

## 面试展开要点

### 1. 为什么做这个项目

可以这样回答：

```text
我之前在上层工程里会遇到一类批量任务：每个任务可以并发执行，但最终结果必须和输入顺序一致。直接使用普通线程池或手写 async 往往会把并发调度、结果排序、异常处理、worker 生命周期混在业务代码里，所以我把这部分抽象成一个很小的 header-only C++17 库。
```

重点强调：

- 这是基础设施项目，不是业务 demo；
- 目标不是做“大而全”的线程池，而是解决固定 worker、batch 执行、结果保序这一类明确问题；
- 项目边界清楚：不做动态扩缩容、优先级、取消、重试、协程或分布式调度。

### 2. 核心难点怎么讲

推荐按 3 个点讲：

**结果保序：** 并发任务完成顺序不可控，因此不能按完成顺序 push result，而是为每个 job 保留原始 index，让对应异步任务只写自己的 `results[job_index]`。

**worker 复用：** worker 可能封装昂贵资源，例如仿真 session、临时目录或外部进程上下文，所以不能简单为每个 job 新建 worker。项目用 available worker queue 控制 worker 租借和归还。

**异常边界：** job 异常属于单任务失败，不应该直接中断整批任务；worker 启动失败属于 pool 无法进入可用状态，需要传播异常并清理已启动 worker。这两类异常边界必须分开处理。

### 3. 可以被追问的实现细节

**为什么选择 header-only：**

```text
项目核心是模板类 `OrderedConcurrentPool<Job, Result>`，Job 和 Result 都由调用方定义。做成 header-only 可以避免模板实例化和链接发布的问题，也方便被上层 CMake 项目直接集成。
```

**为什么需要 `FailureHandler`：**

```text
库本身不理解业务失败结果长什么样，所以不在内部硬编码错误结构，而是把 worker id、job 和 exception_ptr 交给调用方转换为 Result。这让调度层保持业务无关。
```

**为什么 `shutdown_all()` 可重复调用：**

```text
并发资源清理经常会出现在显式 shutdown、异常回滚和析构兜底多个路径里。把 shutdown 设计成幂等接口，可以降低调用方为了避免重复 stop 而维护额外状态的复杂度。
```

**如何验证并发正确性：**

```text
测试里使用不同 delay 构造乱序完成场景，检查结果仍按输入顺序返回；通过 job 抛异常验证 failure handler 不影响其他任务；通过启动失败测试验证 stop 清理路径；再用 stress batch 做基础压力覆盖。CI 中进一步使用 ThreadSanitizer 辅助检查数据竞争。
```

## 中文简历可粘贴版本

```text
OrderedConcurrentPool - C++17 Header-only 有序并发 Worker Pool

项目简介：设计并实现一个 C++17 header-only 有序并发 worker pool，用于将 batch job 并发分发到固定数量 worker 执行，并保证输出结果严格保持输入顺序，为上层仿真、批处理和参数扫描任务提供可复用调度组件。

并发调度：基于 std::async、std::mutex、std::condition_variable 与 worker lease 机制实现 worker 获取/释放和资源复用，通过可用 worker 队列限制并发度，避免业务 worker 生命周期与任务调度逻辑耦合。

结果保序：将 job index 与 result slot 显式绑定，使任务即使乱序完成也能写回输入对应位置，从调度层保证 batch 输出稳定保序，降低调用方额外排序和结果聚合成本。

异常与生命周期：设计 FailureHandler 将单个 job 异常转换为业务失败结果，避免局部失败污染整批任务；统一抽象 Worker<Job, Result> 的 start/run/stop 生命周期，支持启动失败清理和 shutdown_all() 重复调用。

工程化验证：提供 CMake target ocp::ordered_concurrent_pool、install/export package、example 与 benchmark；使用 GoogleTest 覆盖非法构造、启动失败、保序输出、异常转换、重复 shutdown、stress batch 等契约，并通过 Release build、consumer smoke test、ASan/TSan CI 验证。
```

## 英文简历版本

```text
OrderedConcurrentPool - C++17 Header-only Ordered Concurrent Worker Pool

Project Overview: Designed and implemented a C++17 header-only worker pool that executes batch jobs concurrently with a fixed number of reusable workers while preserving output order according to the input sequence. The library provides a lightweight scheduling primitive for simulation, batch processing, and parameter sweep workloads.

Concurrent Scheduling: Built the worker acquisition and release mechanism with std::async, std::mutex, std::condition_variable, and an RAII-style worker lease. The design limits concurrency through an available-worker queue and keeps business worker lifetimes separated from task scheduling.

Ordered Results: Bound each job index to a dedicated result slot so out-of-order task completion still produces deterministic input-order output, reducing sorting and aggregation complexity for callers.

Failure Handling and Lifecycle: Introduced a caller-defined FailureHandler to convert per-job exceptions into domain-specific failed results without aborting the entire batch. Defined a Worker<Job, Result> lifecycle with start/run/stop, startup-failure cleanup, destructor fallback, and repeatable shutdown_all().

Engineering and Validation: Packaged the library as ocp::ordered_concurrent_pool with CMake install/export support, examples, benchmarks, MIT License, and changelog. Added GoogleTest contract tests for invalid construction, startup failure, ordered output, exception conversion, repeated shutdown, and stress batches, with CI coverage for Release builds, external consumer smoke tests, AddressSanitizer, and ThreadSanitizer.
```

## 简历使用建议

- 投 C++ 后端、基础架构、EDA/CAD 工具链方向时，保留“并发调度、生命周期、CMake 包化、sanitizer”这些关键词。
- 投算法或仿真平台方向时，可以强调它服务于批量仿真、参数扫描、结果聚合等上层工作流。
- 不建议写“性能提升 xx%”或“吞吐提升 xx 倍”，除非后续补充了可复现实验和对照基线。
- 面试时要主动说明项目边界：它不是通用线程池框架，而是一个小型、可验证、可集成的有序 batch worker pool。
