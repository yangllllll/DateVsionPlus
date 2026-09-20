@echo off
REM ============================================================
REM  同步主程序 core/ 头文件到独立插件 SDK 包
REM  主程序 core/ 是唯一真身，SDK/include 是其副本。
REM  修改了 core 头文件后运行本脚本即可同步。
REM ============================================================

setlocal

set "SRC=%~dp0..\core"
set "DST=%~dp0..\..\OpenVisionPlusPluginSDK\include"

if not exist "%SRC%" (
    echo [错误] 未找到主程序头文件目录: %SRC%
    exit /b 1
)
if not exist "%DST%" (
    echo [错误] 未找到 SDK 目录: %DST%
    exit /b 1
)

echo 同步 %SRC%  -^>  %DST%

for %%F in (coreglobal.h pluginbase.h plugintypes.h plugininterface.h cvutils.h opencvcompat.h) do (
    copy /y "%SRC%\%%F" "%DST%\%%F" >nul
    echo   %%F
)

copy /y "%~dp0..\3rdparty\opencv.pri" "%~dp0..\..\OpenVisionPlusPluginSDK\3rdparty\opencv.pri" >nul
echo   opencv.pri

echo.
echo 完成。若主程序重新构建过，别忘了同步导入库：
echo   copy ^<主程序构建目录^>\debug\OpenVisionPlus.lib   ^<SDK^>\lib\debug\
echo   copy ^<主程序构建目录^>\release\OpenVisionPlus.lib ^<SDK^>\lib\release\

endlocal
