# -----------------------------------------------------------------------------
# WebSocket 图像流推送插件（Qt Plugin -> DLL）
#   对应 Python 版本: user_plugins/websocket_server.py
#
# 本 DLL 导出 1 个工具：
#   websocket_server  图像流推送
#     输入：input（图像）
#     输出：output（原始图像透传）、client_count（当前客户端数）
#     参数：监听端口 / JPEG质量 / 自动启动 / 发送帧率
#
# 运行机制：
#   - 内置轻量 WebSocket 服务器（QTcpServer + 手工握手），只依赖 Qt Network，
#     不需要额外安装 QtWebSockets 模块；
#   - execute() 只负责保存当前图像并输出客户端数，真正的编码/广播由定时器
#     按「发送帧率」持续进行，因此即使流程暂停，已连接的客户端仍能收到最后一帧；
#   - 双击节点可打开状态对话框，查看端口/客户端数并手动启停。
#
# 客户端接入示例（浏览器控制台）：
#   const ws = new WebSocket("ws://127.0.0.1:9000");
#   ws.onmessage = e => { document.getElementById("img").src = "data:image/jpeg;base64," + e.data; };
#
# 构建与部署：
#   1) 用 Qt Creator 打开本文件，选择【与主程序完全相同】的 Qt 套件与构建配置
#   2) 构建（导入库由 pluginsdk.pri 自动链接）
#   3) 得到 websocketserver.dll，输出到 <本目录>/bin/plugins
#   4) 把它拷贝到 DateVisionPlus.exe 所在目录的 plugins/ 子目录，
#      启动主程序即可在「输入输出」分类中看到「图像流推送」
# -----------------------------------------------------------------------------

include($$PWD/../../pluginsdk.pri)

TARGET   = websocketserver
TEMPLATE = lib

# WebSocket 服务器基于 QTcpServer / QTcpSocket
QT += network

SOURCES += \
    websocketserverplugin.cpp

HEADERS += \
    websocketserverplugin.h

# 想让 DLL 直接生成到主程序可加载的位置，可在此覆盖（include 之后生效）：
# DESTDIR = <主程序构建目录>/release/plugins
