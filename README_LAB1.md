# Cubexyz RUCBase Lab1 存储管理

本次独立重做版本，小组 Cubexyz，组长倪浩宇，成员林锦龙、王麒瑞、陈宇。
新私有仓库：https://github.com/haohao6666666/Cubexyz-RUCBase-Lab1-20261011

## 小组分工

| 成员 | 负责内容 | 贡献比例 |
| --- | --- | --- |
| 倪浩宇（组长） | 缓冲池实现、模块集成、性能分析与报告统稿 | 40% |
| 林锦龙 | 环境配置、磁盘管理与文件 I/O 验证 | 20% |
| 王麒瑞 | LRU 替换器、候选顺序与边界测试 | 20% |
| 陈宇 | 记录操作、记录扫描与数据一致性验证 | 20% |

## 实现范围

从官方基线 `6849e3f2f08f9a096803c1695e5a270067269d4f` 重新克隆，实现磁盘管理、LRU、缓冲池、记录增删改查和扫描。保留原始公有接口、布局、子模块及许可证。后续索引、查询、事务和恢复任务的 TODO 不在本次交付范围。

磁盘采用位置读写与短 I/O 循环；LRU 维护唯一候选与互斥访问；换页在读取成功后提交新状态；记录操作使用作用域释放，扫描不长期占用页面引用。

## 环境和复现

Ubuntu 24.04.4 LTS ARM64，GCC 13.3.0，CMake 3.28.3，GDB 15.1，C++20。
macOS / Windows 推荐按 `.devcontainer` 配置使用课程容器。
GoogleTest 子模块固定为 `1336c4b6d1a6f4bc6beebccb920e5ff858889292`。

```sh
git clone --recurse-submodules https://github.com/haohao6666666/Cubexyz-RUCBase-Lab1-20261011.git
cd Cubexyz-RUCBase-Lab1-20261011
./scripts/verify_lab1.sh debug
./scripts/verify_lab1.sh sanitizers
./scripts/benchmark_lab1.sh
```

私有仓库需要有权限的 GitHub 账号。前两条验证命令各执行官方 21 例和新增 10 例；性能入口生成 105 条数据。
若只验证课程任务，执行 `cmake --preset debug`、`cmake --build --preset debug -j 4`、`ctest --preset lab1`。
不要对仍未实现的 Lab2–Lab4 运行结果作完成声明。

## 验证证据

`evidence/20261011/` 保存本次环境、构建、官方功能、补充边界、sanitizer、GDB、性能和干净检出的日志。截图为实际日志在浏览器中的视图，结构图为辅助绘图并对照源码核验，性能图由原始 CSV 生成。

新增用例覆盖部分页 I/O、四线程共享 fd、全部帧固定、脏页和零页保存、读取失败恢复、异常路径释放、1 万次操作的独立 map 核对、中间空页、槽复用和记录页分配失败。

初次 LeakSanitizer 发现官方 MultipleFilesTest 的 32 个模拟数组共 16 MiB 未释放。只将测试数组改为 `unique_ptr<char[]>` 并调整辅助函数 const 引用，未改变断言、文件数或工作负载。保留失败日志，修复后全量重测通过。

## 性能口径

Release 构建，固定种子 `20261011`，21 组配置各重复 5 次。
页面实验为 2048 页、每轮 100000 次访问、32/128/512 帧，顺序、均匀随机、约 90% 请求访问前 64 页的热点模式。
记录实验为 64 字节、128 帧、10000/50000/100000 条，独立测量插入、扫描并读取、更新、删除。
计时外准备数据、生成序列及打印，运行后核对内容或校验和。
每轮新建数据库缓存，操作系统文件缓存由准备阶段预热。记录计时包含过程中淘汰写回，不含关闭时最终刷盘；数值不是冷磁盘或事务提交吞吐量。

在有 matplotlib 的 Python 环境执行：

```sh
python3 scripts/plot_lab1.py evidence/20261011
```

正常关闭重开不等于崩溃恢复。公开测试通过不代表教师隐藏用例或最终成绩。
