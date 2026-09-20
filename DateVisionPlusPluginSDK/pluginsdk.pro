# -----------------------------------------------------------------------------
# OpenVisionPlus 插件 SDK —— Qt Creator 工程入口
#
# Qt Creator: 文件 -> 打开文件或项目 -> 选择本文件 (pluginsdk.pro)
#
# 这是一个 subdirs 聚合工程，一次打开即可看到并构建 SDK 自带的全部示例工程。
# 编写自己的插件时，复制 templates/myplugin/ 后在本文件的 SUBDIRS 中加一行即可。
# -----------------------------------------------------------------------------

TEMPLATE = subdirs
CONFIG  += ordered

SUBDIRS += \
    examples/userpluginsample \