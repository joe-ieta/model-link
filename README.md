# ModelLink

跨平台模型下载工具，支持从魔塔社区 (ModelScope) 和 HuggingFace 下载模型文件。

## 功能

- 输入模型标识 (如 `Qwen/Qwen3.6-27B`) 自动识别并获取文件列表
- 模型标识会同时检查魔塔社区和 HuggingFace，优先选择 GGUF 文件；两个站点都没有 GGUF 时再选择其他格式
- 多文件并发下载（默认 3 线程，界面可调整 1-8）
- 下载速度使用最近 5 秒滑动窗口平均值
- 断点续传，自动处理服务端不支持 Range 的情况
- 下载全部、下载选中、单行按钮下载单个文件
- 下载文件自动保存到系统下载目录的 `model_link_<模型标识>` 文件夹
- 下载全部会清空当前模型下载目录；下载选中/单文件会复用目录并保留已下载文件

## 构建

需要 Qt6、CMake 3.16+、MSVC 2022 (Windows) 或 GCC (Linux)。

Ubuntu 示例：

```bash
sudo apt update
sudo apt install cmake g++ qt6-base-dev qt6-tools-dev
```

```bash
cmake -S . -B build -DCMAKE_PREFIX_PATH=/path/to/Qt6
cmake --build build --config Release
ctest --test-dir build -C Release
```

默认测试不访问外网。需要运行 ModelScope/HuggingFace 集成测试时：

```bash
MODEL_LINK_RUN_NETWORK_TESTS=1 ctest --test-dir build -C Release
```

稳定测试包含本地 HTTP 服务模拟的断点续传场景，覆盖 `206 Partial Content` 和服务端忽略 Range 返回 `200 OK` 的情况。

## Windows 部署

```bat
windeployqt build\Release\ModelLink.exe --no-translations
```

## Linux 说明

程序默认使用 Qt 系统风格和系统下载目录。若需要分发独立包，可结合 `linuxdeployqt` 或 AppImage 工具链。运行环境需要 Qt6 Widgets/Network/TLS 插件可用。

## 许可证

MIT
