# 相机离线回归测试

本工程直接编译 UVC 和大华采集封装，使用合成图像与内存中的 SDK 函数替身，
不会枚举或打开摄像头，不需要安装大华 SDK。

在已初始化 MSVC 环境的终端中，从 SDK 根目录创建独立构建目录：

```powershell
New-Item -ItemType Directory -Force build/camera-tests
Set-Location build/camera-tests
qmake ../../examples/uvccamera/tests/camera_tests.pro "CONFIG+=release" "CONFIG-=debug"
nmake /nologo
```

运行前将所用 Qt 和 OpenCV 的 `bin` 目录加入当前终端的 `PATH`。直接运行
`release/camera_tests.exe` 会执行两个测试类。需要保存完整日志时，分别运行，
避免 QtTest 的第二个测试类覆盖第一个类的输出文件：

```powershell
$env:CAMERA_TEST_SUITE = 'uvc'
./release/camera_tests.exe -o uvc-results.txt,txt
$env:CAMERA_TEST_SUITE = 'dahua'
./release/camera_tests.exe -o dahua-results.txt,txt
Remove-Item Env:CAMERA_TEST_SUITE
```

覆盖范围：帧所有权及下游写入隔离、并发帧序号一致性、慢读者保护和缓冲区复用、超时和停止唤醒，
SDK 直接写入最终图像、连续取流不被打断、临时取流及帧资源异常清理、
灰度行填充、数据截断和超时重试。

UVC 的合成基准预热后交替运行旧版与新版路径，报告 7 轮中位数。
每轮使用 80 张 1920×1080 BGR 图，计入读取目标缓冲区的分配和填充、
帧发布，排除真实设备 I/O、解码和消费者的深拷贝。
该数字仅描述这段内存路径，不能当作摄像头帧率或完整流程性能。
