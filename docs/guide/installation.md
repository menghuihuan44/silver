# 安装

## 系统要求

- **CMake** >= 3.16
- **C 编译器**：GCC >= 8.0, Clang >= 10.0, 或 MSVC 2019+
- **操作系统**：Linux / macOS / Windows / FreeBSD

## 从源码构建

```bash
git clone https://github.com/menghuihuan44/silver.git
cd silver
mkdir build && cd build

# Release 构建（推荐）
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build . -j$(nproc)

# Debug 构建
cmake .. -DCMAKE_BUILD_TYPE=Debug
cmake --build . -j$(nproc)
```

## 安装到系统

```bash
cd build
sudo cmake --install . --prefix /usr/local
```

安装后可用：

```bash
silverc --version
```

## 构建选项

可通过 `-D` 标志传递给 CMake：

| 选项 | 默认值 | 说明 |
|------|--------|------|
| `SILVER_BUILD_SHARED` | ON | 构建共享库 |
| `SILVER_BUILD_STATIC` | ON | 构建静态库 |
| `SILVER_BUILD_TESTS` | ON | 构建测试套件 |
| `SILVER_BUILD_BENCHMARKS` | ON | 构建性能基准 |
| `SILVER_BUILD_EXAMPLES` | ON | 构建示例程序 |
| `SILVER_BUILD_TOOLS` | ON | 构建附加工具 |
| `SILVER_WARNINGS_AS_ERRORS` | OFF | 警告视为错误 |
| `SILVER_ENABLE_PROFILING` | OFF | 启用性能分析支持 |

示例：

```bash
cmake .. -DCMAKE_BUILD_TYPE=Release -DSILVER_BUILD_TESTS=OFF
```

## 链接到你的项目

**pkg-config**：

```bash
pkg-config --cflags --libs silver
```

**CMake**：

```cmake
find_package(silver REQUIRED)
target_link_libraries(your_app silver::silver)
```

**手动**：

```bash
gcc -I/usr/local/include -o your_app your_app.c -lsilver
```