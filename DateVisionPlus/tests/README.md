# 性能与回归验证

各 `.pro` 为独立测试工程，使用与主程序一致的 Qt / OpenCV / 编译器。建议用独立构建目录，避免测试与主程序输出混用。

| 工程 | 覆盖内容 | 本次状态 |
| --- | --- | --- |
| `executionengine_test.pro` | 拓扑与环检测、端口路由、图变更、停止、异常、图像引用、调度基准 | 已运行通过 |
| `executionworker_test.pro` | 真实工作线程、排队取消、图复用和替换、进度信号、重复任务拒绝 | 已运行通过 |
| `outputpanel_test.pro` | 表项复用、变化值更新、日志定时刷新、表格基准 | 已运行通过 |
| `image_hotpaths_test.pro` | 图像转换像素与所有权、ROI 步长、Blob 等价、Measure 输入隔离 | 已运行通过 |
| `runtime_test.pro` | 主窗口编辑保护、图缓存失效、对话框期间远程触发、连续停止、TCP 格式响应 | 已编译，尚未运行，留待最终验收 |

`runtime_test` 使用测试插件并关闭用户 DLL 扫描；TCP 用例在本机临时端口连接，不访问相机或 PLC。

## 构建和运行

在已配置 MSVC、Qt 和 OpenCV 的开发命令行，从项目目录执行。例如：

```bat
mkdir build\runtime_checks
cd build\runtime_checks
qmake ..\..\tests\runtime_test.pro -spec win32-msvc "CONFIG+=release" "CONFIG-=debug"
jom -j 4
set QT_QPA_PLATFORM=offscreen
release\runtime_test.exe
```

其他工程用相同方式分别构建；运行前确保对应 Qt 的 `bin` 在 PATH 中。`image_hotpaths_test` 是普通断言测试程序，其余使用 Qt Test。Qt Test 基准可传 `-median 5`。不要用 Debug 测试结果比较性能。

## 已测局部基准

2026-10-13，本机 Qt 6.11.2 / MSVC Release / OpenCV 5.0；旧实现取自提交 `6b2b888`。下表表示局部操作，不代表相机采集、算法和通信组成的完整检测流程同比提速。

| 场景 | 原实现 | 优化实现 |
| --- | ---: | ---: |
| 128 节点简单链，每轮调度与路由 | 0.30 ms | 0.060 ms |
| 512 节点简单链，每轮调度与路由 | 1.2 ms | 0.28 ms |
| 100 节点 × 10 端口，表格内容相同 | 1.1 ms | 0.25 ms |
| 100 节点 × 10 端口，全部输出值变化 | 0.98 ms | 0.48 ms |
| 3840 × 2160 BGRA 转 QImage | 17.27 ms | 7.79 ms |

调度比较采用旧版每轮 `setup + execute` 与新版固定图复用计划的 `execute`，与主窗口优化前后的调用方式一致。测试插件只做简单标量路由，未计入真实视觉算法。

图像转换使用单 OpenCV 线程、5 次预热、交替执行顺序、31 个样本中位数。Qt 表格基准使用 offscreen 平台、`-median 5`。不同硬件和流程应重新测量。

本机记录位于忽略的构建目录：

- `build/engine_checks/engine-results.txt`
- `build/engine_baseline/baseline-results.txt`
- `build/engine_baseline/baseline-compatibility.txt`
- `build/outputpanel_tests/baseline.txt`、`optimized.txt`
- `build/executionworker_tests/results.txt`

## 最终验收重点

1. 用实际项目测试单次、连续运行与停止，确认最终图像、节点状态及数值结果。
2. 停止后增删节点、修改连线与参数，再运行，确认缓存失效正确。
3. 连续运行时确认界面可响应，且流程编辑、保存和参数窗口被阻止。
4. 用实际 TCP 客户端确认触发与表达式结果；参数对话框打开时应返回忙碌状态。
5. 使用真实相机及第三方插件检查线程归属和采集行为；本次自动测试未覆盖这些硬件路径。
