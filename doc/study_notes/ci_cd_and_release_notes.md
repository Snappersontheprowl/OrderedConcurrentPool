# CI/CD 与 C++ 开源库发布学习笔记

记录日期：2026-08-06

关联项目：`OrderedConcurrentPool`

关联仓库：

```text
https://github.com/Snappersontheprowl/OrderedConcurrentPool
```

## 1. 这份笔记要解决什么问题

这是第一次系统接触“开源 C++ 库的发布规范、CI/CD、GitHub Actions、release tag、
sanitizer、CMake install/export package”等概念时的项目笔记。

本笔记不追求把所有 DevOps 概念一次讲完，而是围绕本项目已经真实做过的一组动作来理解：

```text
写代码
  -> 本地验证
  -> git commit
  -> push 到 GitHub
  -> GitHub Actions 自动构建和测试
  -> 用 tag 标记发布版本
  -> 让第三方 CMake 项目能够消费该库
```

当前掌握目标：

- 知道 CI/CD 分别是什么；
- 知道本项目现在做到了哪一步；
- 能看懂 `.github/workflows/ci.yml`；
- 能理解为什么 C++ 库需要 install / package / consumer smoke test；
- 能理解 ASan / TSan 为什么对并发库重要；
- 能复现本地验证命令；
- 知道遇到 push reject、sanitizer runtime 缺失、GitHub CLI 未登录时该怎么判断。

## 2. CI/CD 是什么

### 2.1 CI：Continuous Integration，持续集成

CI 的核心含义是：

```text
每次代码变化后，自动在干净环境里构建、测试、检查，尽早发现问题。
```

对于 `OrderedConcurrentPool` 这种 C++ header-only 并发库，CI 主要回答这些问题：

- 代码在 GitHub runner 上能不能 configure；
- 代码能不能编译；
- GoogleTest 契约测试能不能通过；
- CMake install/export package 是否真的可用；
- 外部项目能不能 `find_package(OrderedConcurrentPool)`；
- sanitizer 能不能发现内存错误或线程竞态。

一句话理解：

```text
CI 是自动门卫：每次代码进主线前后，它帮你检查项目是否仍然健康。
```

### 2.2 CD：Continuous Delivery / Continuous Deployment

CD 有两个常见含义。

Continuous Delivery，持续交付：

```text
每次通过 CI 后，自动生成可发布产物，让项目随时处于“可以发布”的状态。
```

Continuous Deployment，持续部署：

```text
每次通过 CI 后，自动部署到生产环境。
```

对于一个 C++ header-only 库来说，“部署到生产环境”通常不是主线需求。更贴近本项目的是
Continuous Delivery，例如：

- 自动生成 release archive；
- 自动上传 GitHub Release 附件；
- 自动生成源码包；
- 自动验证安装包；
- 自动发布到包管理器。

当前 `OrderedConcurrentPool` 已做的是 CI，不是完整 CD。

当前没有做的 CD 内容：

- 没有自动创建 GitHub Release 页面；
- 没有自动上传 release artifact；
- 没有发布到 vcpkg / Conan / Homebrew；
- 没有自动生成多平台二进制包。

这不是缺陷，而是阶段选择。对 `v0.1.0` 之后的项目来说，先补 CI 比急着做 CD 更划算。

## 3. 本项目现在处于什么状态

`OrderedConcurrentPool` 当前已经具备一个基础开源 C++ 库形态：

- MIT License；
- `README.md`；
- `CHANGELOG.md`；
- header-only API；
- CMake target：`ocp::ordered_concurrent_pool`；
- CMake install/export package；
- 最小 example；
- GoogleTest 契约测试；
- 最小 benchmark；
- GitHub `main`；
- `v0.1.0` tag；
- GitHub Actions CI。

当前最新主线提交：

```text
0524272 Add CI verification workflow
```

当前发布 tag：

```text
v0.1.0 -> f8847eb
```

注意：

- `v0.1.0` 是首个 MIT 发布版本；
- CI 是 `v0.1.0` 之后在 `main` 上增加的工程增强；
- 没有移动 `v0.1.0` tag；
- 如果之后想把 CI 也纳入正式 patch release，可以再打 `v0.1.1`。

## 4. 本次真实做过的一系列操作

### 4.1 准备 `v0.1.0`

先补齐发布基础文件：

- `LICENSE`：明确使用 MIT；
- `CHANGELOG.md`：记录首个版本；
- `benchmarks/ordered_pool_benchmark.cpp`：最小 benchmark；
- `README.md`：说明版本、许可证、benchmark、发布状态；
- `CMakeLists.txt`：增加 benchmark target 和 install docs。

本地验证：

```bash
cmake -S . -B .build/release-check -DCMAKE_BUILD_TYPE=Release -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
cmake --build .build/release-check --parallel
ctest --test-dir .build/release-check --output-on-failure
```

结果：

```text
100% tests passed, 0 tests failed out of 8
```

benchmark 验证：

```bash
cmake -S . -B .build/benchmark \
  -DCMAKE_BUILD_TYPE=Release \
  -DORDERED_CONCURRENT_POOL_BUILD_TESTS=OFF \
  -DORDERED_CONCURRENT_POOL_BUILD_EXAMPLES=OFF \
  -DORDERED_CONCURRENT_POOL_BUILD_BENCHMARKS=ON
cmake --build .build/benchmark --parallel
./.build/benchmark/ordered_concurrent_pool_benchmark
```

benchmark 输出是 CSV：

```text
worker_count,job_count,latency_ms,throw_every,elapsed_ms
```

这个 benchmark 只用于本机回归观察，不作为跨机器性能承诺。

install 验证：

```bash
cmake --install .build/release-check --prefix /tmp/ocp_release_install_...
```

安装结果包含：

```text
include/ocp/ordered_concurrent_pool.hpp
lib64/cmake/OrderedConcurrentPool/OrderedConcurrentPoolConfig.cmake
lib64/cmake/OrderedConcurrentPool/OrderedConcurrentPoolConfigVersion.cmake
lib64/cmake/OrderedConcurrentPool/OrderedConcurrentPoolTargets.cmake
share/doc/OrderedConcurrentPool/LICENSE
share/doc/OrderedConcurrentPool/README.md
share/doc/OrderedConcurrentPool/CHANGELOG.md
```

### 4.2 发布 `main` 和 `v0.1.0`

本地创建 tag：

```bash
git tag -a v0.1.0 -m "OrderedConcurrentPool v0.1.0"
```

第一次 push 时遇到：

```text
! [rejected] main -> main (fetch first)
```

含义：

```text
GitHub 远程 main 上已有本地没有的提交。
如果直接强推，会覆盖远程历史。
```

处理方式：

```bash
git fetch origin
git merge origin/main --allow-unrelated-histories
```

为什么需要 `--allow-unrelated-histories`：

```text
本地仓库和 GitHub 远程仓库各自初始化过，
两边没有共同祖先提交。
Git 默认不允许直接合并两条无共同历史的分支。
```

合并时 `LICENSE` 冲突。远程已有 MIT License，版权名是：

```text
Copyright (c) 2026 ZhengLecheng
```

最终保留了远程更具体的版权名。

然后重新 push：

```bash
git push origin main
git push origin v0.1.0
```

发布结果：

```text
main -> origin/main
v0.1.0 -> origin/v0.1.0
```

### 4.3 增加 GitHub Actions CI

新增文件：

```text
.github/workflows/ci.yml
```

新增 README badge：

[![CI](https://github.com/Snappersontheprowl/OrderedConcurrentPool/actions/workflows/ci.yml/badge.svg)](https://github.com/Snappersontheprowl/OrderedConcurrentPool/actions/workflows/ci.yml)


新增 `CHANGELOG.md` 的 `Unreleased` 记录：

- 增加 GitHub Actions CI；
- 增加 Release build / test 自动验证；
- 增加 CMake install package 与外部 consumer smoke test；
- 增加 AddressSanitizer / ThreadSanitizer 验证。

提交并推送：

```bash
git commit -m "Add CI verification workflow"
git push origin main
```

远程 CI 首轮运行：

```text
Run: 31111814593
Commit: 0524272
```

结果：

```text
Install package and external consumer: completed success
ThreadSanitizer: completed success
AddressSanitizer: completed success
Release build and tests: completed success
```

## 5. `.github/workflows/ci.yml` 怎么读

GitHub Actions 的核心结构可以这样理解：

```text
workflow
  -> event
  -> jobs
  -> steps
```

对应到本项目：

```yaml
name: CI
```

表示这个 workflow 叫 `CI`。

```yaml
on:
  push:
    branches:
      - main
    tags:
      - "v*"
  pull_request:
    branches:
      - main
```

表示这些情况会触发 CI：

- push 到 `main`；
- push `v*` 格式的 tag，例如 `v0.1.0`；
- 向 `main` 发起 pull request。

```yaml
jobs:
```

下面是多个独立 job。它们默认可以并行跑。

## 6. CI job 逐个解释

### 6.1 Release build and tests

目的：

```text
确认项目能在 Release 模式下构建并通过契约测试。
```

核心命令：

```bash
cmake -S . -B .build/ci-release \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
cmake --build .build/ci-release --parallel
ctest --test-dir .build/ci-release --output-on-failure
```

为什么要用 Release：

- Debug 适合开发；
- Release 更接近用户实际使用方式；
- 某些未定义行为可能只在优化后暴露。

### 6.2 Install package and external consumer

目的：

```text
验证项目安装后，能被第三方 CMake 项目正常 find_package 和链接。
```

这一项非常重要。很多 C++ 项目“在源码树里能编译”，但安装后别人用不了。

本项目要证明：

```cmake
find_package(OrderedConcurrentPool CONFIG REQUIRED)

target_link_libraries(your_target
  PRIVATE
    ocp::ordered_concurrent_pool
)
```

是真实可用的。

CI 里做了三件事：

第一，安装库：

```bash
cmake --install build --prefix "${RUNNER_TEMP}/ocp-install"
```

第二，检查关键安装文件存在：

```bash
test -f "${RUNNER_TEMP}/ocp-install/include/ocp/ordered_concurrent_pool.hpp"
test -f "${RUNNER_TEMP}/ocp-install/share/doc/OrderedConcurrentPool/LICENSE"
test -f "${RUNNER_TEMP}/ocp-install/share/doc/OrderedConcurrentPool/README.md"
test -f "${RUNNER_TEMP}/ocp-install/share/doc/OrderedConcurrentPool/CHANGELOG.md"
find "${RUNNER_TEMP}/ocp-install" -name OrderedConcurrentPoolConfig.cmake -type f | grep .
find "${RUNNER_TEMP}/ocp-install" -name OrderedConcurrentPoolTargets.cmake -type f | grep .
```

第三，临时创建一个外部 consumer 项目：

```text
ocp-consumer/
  CMakeLists.txt
  main.cpp
```

它不是项目源码的一部分，而是模拟“别人安装后怎么用”。

这是一个很好的工程判断标准：

```text
库项目不能只证明自己能编译，还要证明别人能消费。
```

### 6.3 AddressSanitizer

ASan 是 AddressSanitizer。

它主要检查：

- 越界访问；
- use-after-free；
- double free；
- 栈/堆内存错误。

本项目使用的配置：

```bash
cmake -S . -B .build/ci-asan-clang \
  -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_C_COMPILER=clang \
  -DCMAKE_CXX_COMPILER=clang++ \
  -DCMAKE_C_FLAGS="-fsanitize=address -fno-omit-frame-pointer" \
  -DCMAKE_CXX_FLAGS="-fsanitize=address -fno-omit-frame-pointer" \
  -DCMAKE_EXE_LINKER_FLAGS="-fsanitize=address"
cmake --build .build/ci-asan-clang --parallel
ctest --test-dir .build/ci-asan-clang --output-on-failure
```

### 6.4 ThreadSanitizer

TSan 是 ThreadSanitizer。

它主要检查：

- 数据竞争；
- 错误的线程同步；
- 共享状态未受保护访问。

对 `OrderedConcurrentPool` 这种并发库，TSan 很重要。

普通测试只能说明：

```text
这次运行结果看起来对。
```

TSan 可以进一步帮助判断：

```text
这个“对”是否建立在数据竞争或未定义行为上。
```

本项目使用的配置：

```bash
cmake -S . -B .build/ci-tsan-clang \
  -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_C_COMPILER=clang \
  -DCMAKE_CXX_COMPILER=clang++ \
  -DCMAKE_C_FLAGS="-fsanitize=thread -fno-omit-frame-pointer" \
  -DCMAKE_CXX_FLAGS="-fsanitize=thread -fno-omit-frame-pointer" \
  -DCMAKE_EXE_LINKER_FLAGS="-fsanitize=thread"
cmake --build .build/ci-tsan-clang --parallel
ctest --test-dir .build/ci-tsan-clang --output-on-failure
```

## 7. 为什么 sanitizer CI 用 clang

本机验证时，第一次尝试用 `g++` 跑 ASan，报错：

```text
/usr/bin/ld: cannot find /usr/lib64/libasan.so.5.0.0
```

含义：

```text
当前机器上的 GCC sanitizer runtime 不完整或版本不匹配。
```

随后改用 `clang++`，但 GoogleTest 会启用 C 语言项目，C 编译器仍默认走 `/usr/bin/cc`，
还是会触发 GCC ASan runtime 问题。

最终正确做法：

```text
同时指定 C 编译器和 C++ 编译器：

-DCMAKE_C_COMPILER=clang
-DCMAKE_CXX_COMPILER=clang++
```

这就是为什么 CI 里 sanitizer matrix 写成：

```yaml
c_compiler: clang
cxx_compiler: clang++
```

这个细节很实用：C++ 项目的测试依赖可能会启用 C 编译器，不能只指定 C++ 编译器。

## 8. 本地常用验证命令

### 8.1 普通开发验证

```bash
cmake -S . -B .build/debug -DCMAKE_BUILD_TYPE=Debug -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
cmake --build .build/debug --parallel
ctest --test-dir .build/debug --output-on-failure
```

### 8.2 Release 验证

```bash
cmake -S . -B .build/ci-release -DCMAKE_BUILD_TYPE=Release -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
cmake --build .build/ci-release --parallel
ctest --test-dir .build/ci-release --output-on-failure
```

### 8.3 install package 验证

```bash
cmake -S . -B .build/ci-install \
  -DCMAKE_BUILD_TYPE=Release \
  -DORDERED_CONCURRENT_POOL_BUILD_TESTS=OFF \
  -DORDERED_CONCURRENT_POOL_BUILD_EXAMPLES=OFF \
  -DORDERED_CONCURRENT_POOL_BUILD_BENCHMARKS=OFF
cmake --build .build/ci-install --parallel

ocp_install_dir=$(mktemp -d /tmp/ocp_ci_install_XXXXXX)
cmake --install .build/ci-install --prefix "$ocp_install_dir"
find "$ocp_install_dir" -maxdepth 6 -type f | sort
```

### 8.4 benchmark 验证

```bash
cmake -S . -B .build/benchmark \
  -DCMAKE_BUILD_TYPE=Release \
  -DORDERED_CONCURRENT_POOL_BUILD_TESTS=OFF \
  -DORDERED_CONCURRENT_POOL_BUILD_EXAMPLES=OFF \
  -DORDERED_CONCURRENT_POOL_BUILD_BENCHMARKS=ON
cmake --build .build/benchmark --parallel
./.build/benchmark/ordered_concurrent_pool_benchmark
```

### 8.5 ASan 验证

```bash
cmake -S . -B .build/ci-asan-clang \
  -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_C_COMPILER=clang \
  -DCMAKE_CXX_COMPILER=clang++ \
  -DCMAKE_C_FLAGS="-fsanitize=address -fno-omit-frame-pointer" \
  -DCMAKE_CXX_FLAGS="-fsanitize=address -fno-omit-frame-pointer" \
  -DCMAKE_EXE_LINKER_FLAGS="-fsanitize=address" \
  -DORDERED_CONCURRENT_POOL_BUILD_EXAMPLES=OFF \
  -DORDERED_CONCURRENT_POOL_BUILD_BENCHMARKS=OFF
cmake --build .build/ci-asan-clang --parallel
ctest --test-dir .build/ci-asan-clang --output-on-failure
```

### 8.6 TSan 验证

```bash
cmake -S . -B .build/ci-tsan-clang \
  -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_C_COMPILER=clang \
  -DCMAKE_CXX_COMPILER=clang++ \
  -DCMAKE_C_FLAGS="-fsanitize=thread -fno-omit-frame-pointer" \
  -DCMAKE_CXX_FLAGS="-fsanitize=thread -fno-omit-frame-pointer" \
  -DCMAKE_EXE_LINKER_FLAGS="-fsanitize=thread" \
  -DORDERED_CONCURRENT_POOL_BUILD_EXAMPLES=OFF \
  -DORDERED_CONCURRENT_POOL_BUILD_BENCHMARKS=OFF
cmake --build .build/ci-tsan-clang --parallel
ctest --test-dir .build/ci-tsan-clang --output-on-failure
```

## 9. 常见概念对照

| 概念 | 在本项目中的含义 |
|---|---|
| CI | GitHub Actions 自动 build / test / install / sanitizer |
| CD | 当前未做；后续可自动创建 release artifact |
| workflow | `.github/workflows/ci.yml` 这一整套自动化流程 |
| job | workflow 中一个独立任务，例如 Release build 或 TSan |
| step | job 里的一个具体动作，例如 `cmake --build` |
| runner | GitHub 提供的干净虚拟机，当前用 `ubuntu-latest` |
| badge | README 顶部显示 CI 状态的小图标 |
| tag | Git 中给某个提交打的版本标记，例如 `v0.1.0` |
| release | 面向使用者的版本口径；tag 是 release 的基础 |
| install/export package | CMake 安装后让外部项目 `find_package` 能找到库 |
| consumer smoke test | 创建临时外部项目验证安装包真的能被用 |
| ASan | 检查内存错误 |
| TSan | 检查线程竞态 |

## 10. 常见误区

### 误区 1：本机能编译就等于别人能用

不等于。

本机源码树内编译只能说明：

```text
当前 checkout 下可以 build。
```

但别人一般会：

```text
install
find_package
target_link_libraries
include header
```

所以 C++ 库需要 install consumer smoke test。

### 误区 2：CI 通过就代表没有 bug

不代表。

CI 只能证明：

```text
在当前测试覆盖、当前 runner、当前编译器和当前配置下，没有发现问题。
```

它不能证明：

- 所有输入都正确；
- 所有平台都正确；
- 性能一定好；
- 未来不会有竞态。

所以 CI 是底线，不是终点。

### 误区 3：benchmark 跑出数字就能写性能承诺

不能。

benchmark 数字依赖：

- 机器；
- CPU；
- 内存；
- 编译器；
- 编译参数；
- 系统负载；
- 测试样本；
- 重复次数。

当前 `OrderedConcurrentPool` 的 benchmark 只用于本机回归观察。

### 误区 4：所有项目都必须一开始做完整 CD

不需要。

对当前项目，合理顺序是：

```text
先 CI
再稳定 API
再考虑 release artifact / package manager
```

不要为了“看起来专业”过早引入复杂发布流水线。

## 11. 常见故障处理

### 11.1 `git push` 被拒绝：`fetch first`

现象：

```text
! [rejected] main -> main (fetch first)
```

原因：

```text
远程 main 有本地没有的提交。
```

安全处理：

```bash
git fetch origin
git log --oneline --decorate --graph --all --max-count=20
```

如果确认远程是独立初始化历史，可以合并：

```bash
git merge origin/main --allow-unrelated-histories
```

不要轻易 `git push --force`。强推会覆盖远程历史，除非你明确知道自己在做什么，并且这是项目允许的。

### 11.2 sanitizer 找不到 runtime

现象：

```text
/usr/bin/ld: cannot find /usr/lib64/libasan.so.5.0.0
```

原因：

```text
编译器 sanitizer runtime 缺失或版本不匹配。
```

本项目处理方式：

```text
sanitizer job 使用 clang / clang++。
```

同时指定：

```bash
-DCMAKE_C_COMPILER=clang
-DCMAKE_CXX_COMPILER=clang++
```

### 11.3 `gh run list` 要求登录

现象：

```text
To get started with GitHub CLI, please run: gh auth login
```

含义：

```text
本机 GitHub CLI 没有登录，不能通过 gh 查询 run。
```

替代方式：

- 用浏览器打开 Actions 页面；
- 对公开仓库，用 GitHub REST API 查询；
- 或配置 `GH_TOKEN`。

本次使用公开 API 查询了 Actions 状态。

## 12. 当前可以如何判断自己掌握了

不要用“我看过了”判断掌握。建议用下面几个小检查。

### 12.1 能解释

你能用自己的话解释：

- CI 和 CD 的区别；
- 为什么 C++ 库需要 install consumer smoke test；
- 为什么并发库要跑 TSan；
- 为什么 `v0.1.0` tag 不应该随便移动；
- 为什么 push 被拒绝时不能直接强推。

### 12.2 能定位

看到这些文件时，你能说出它们的作用：

```text
.github/workflows/ci.yml
README.md
CHANGELOG.md
LICENSE
CMakeLists.txt
cmake/OrderedConcurrentPoolConfig.cmake.in
```

### 12.3 能复现

你能独立跑：

```bash
cmake -S . -B .build/ci-release -DCMAKE_BUILD_TYPE=Release
cmake --build .build/ci-release --parallel
ctest --test-dir .build/ci-release --output-on-failure
```

并能解释每一行在做什么。

### 12.4 能修改

你能独立给 CI 增加一个 job，例如：

```text
只编译 benchmark，但不要求 benchmark 数字固定。
```

这是一个合适的下一步练习。

## 13. 后续学习路线

建议按这个顺序继续学：

1. Git 基础发布流程：
   - commit；
   - branch；
   - tag；
   - remote；
   - fetch / pull / push；
   - merge conflict。
2. CMake package：
   - target；
   - install；
   - export；
   - `find_package`；
   - `CMAKE_PREFIX_PATH`。
3. GitHub Actions：
   - workflow；
   - event；
   - job；
   - step；
   - matrix；
   - badge。
4. C++ 质量门禁：
   - unit test；
   - ASan；
   - TSan；
   - benchmark；
   - smoke test。
5. 发布工程：
   - GitHub Release；
   - release notes；
   - artifact；
   - package manager。

## 14. 本项目下一步可练习任务

如果要继续练习 CI/CD，建议从小任务开始：

### 任务 A：增加 benchmark compile-only CI

目标：

```text
CI 中确认 benchmark 能编译，但不比较性能数字。
```

原因：

```text
GitHub runner 性能不稳定，不能把 elapsed_ms 写成硬性断言。
```

验收：

- 新增 benchmark job；
- `ORDERED_CONCURRENT_POOL_BUILD_BENCHMARKS=ON`；
- 成功构建 `ordered_concurrent_pool_benchmark`；
- 不运行或只运行不校验具体时间。

### 任务 B：增加多编译器矩阵

目标：

```text
用 gcc 和 clang 都跑 Release build/test。
```

验收：

- matrix 包含 `g++` 和 `clang++`；
- 两个编译器下测试都通过。

### 任务 C：准备 `v0.1.1`

目标：

```text
把 CI 增强作为 patch release 记录。
```

验收：

- `CHANGELOG.md` 从 `Unreleased` 移到 `v0.1.1 - 日期`；
- 创建 tag `v0.1.1`；
- push tag；
- GitHub Actions 对 tag 触发成功。

当前建议：

```text
先不急着做 C，等 CI 稳定跑几次后再发 v0.1.1。
```

## 15. 一句话复盘

这次做的事情不是“给项目加了一个配置文件”，而是建立了一个最小但完整的工程质量闭环：

```text
本地验证
  -> commit
  -> push
  -> GitHub 自动构建测试
  -> 安装包消费验证
  -> sanitizer 检查
  -> 文档记录事实
```

对一个 C++ 并发基础库来说，这个闭环比继续堆功能更重要。
