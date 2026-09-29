# Changelog

## Unreleased

### 修正

- 修正 `run_batch()` 在 job 未取得 worker（例如 batch 运行中另一个线程调用 `shutdown_all()`）时
  抛出异常并丢弃整批结果的问题：这类 job 现在通过 failure handler 转换成失败结果。
  一旦 batch 被接受，`run_batch()` 必定返回与输入等长的结果；
- 新增 `unassigned_worker_id()`，用于标识未取得 worker 的失败结果；
- 新增 shutdown 与在飞 batch 并发的契约测试。

### 变更

- 增加 GitHub Actions CI；
- 增加 Release build / test 自动验证；
- 增加 CMake install package 与外部 consumer smoke test；
- 增加 AddressSanitizer / ThreadSanitizer 验证；
- 将本地构建目录文档约定整理为 `.build/*`，避免根目录堆积临时 build 产物。
- 补充 README 的 benchmark 参考数据、线程模型说明、契约与边界说明以及下游使用说明。

## v0.1.0 - 2026-08-06

首个本地发布版本。

### 已包含

- C++17 header-only `OrderedConcurrentPool`；
- CMake target：`ocp::ordered_concurrent_pool`；
- CMake install/export package；
- MIT License；
- GoogleTest 契约测试；
- 最小 batch 示例；
- 最小 benchmark 程序。

### 当前边界

- 固定数量 worker；
- batch job 并发执行；
- 输出严格保序；
- per-job exception 由调用方 failure handler 转换；
- startup failure cleanup；
- repeatable shutdown；
- 不支持动态扩缩容、取消、优先级、retry、worker 自动重建或分布式调度。
