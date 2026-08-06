# Changelog

## Unreleased

- 增加 GitHub Actions CI；
- 增加 Release build / test 自动验证；
- 增加 CMake install package 与外部 consumer smoke test；
- 增加 AddressSanitizer / ThreadSanitizer 验证。

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
