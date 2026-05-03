# ModelLink

跨平台模型下载工具，支持从魔塔社区 (ModelScope) 和 HuggingFace 下载模型文件。

## 功能

- 输入模型标识 (如 `Qwen/Qwen3.6-27B`) 自动识别并获取文件列表
- 优先从魔塔社区获取，失败自动回退到 HuggingFace
- GGUF 格式文件优先显示
- 多文件并发下载（默认 3 线程）
- 滑动窗口实时速度显示
- 断点续传
- 双击单文件下载

## 构建

需要 Qt6、CMake 3.16+、MSVC 2022 (Windows) 或 GCC (Linux)。

```bash
cmake -S . -B build -DCMAKE_PREFIX_PATH=/path/to/Qt6
cmake --build build --config Release
ctest --test-dir build -C Release
```

## 许可证

MIT
