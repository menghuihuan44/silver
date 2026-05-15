# ============================================================================
# Silver Compiler Backend - 主构建文件
# ============================================================================
cmake_minimum_required(VERSION 3.16)
project(silver
    VERSION 0.1.0
    DESCRIPTION "Silver - Fast Cross-Platform Compiler Backend"
    HOMEPAGE_URL "https://github.com/menghuihuan44/silver"
    LANGUAGES C
)

# ============================================================================
# 项目配置选项
# ============================================================================

# 构建类型默认设置
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Release" CACHE STRING
        "Build type: Debug, Release, RelWithDebInfo, MinSizeRel" FORCE)
endif()

# 主要构建选项
option(SILVER_BUILD_SHARED "Build shared library (libsilver.so/dylib)" ON)
option(SILVER_BUILD_STATIC "Build static library (libsilver.a)" ON)
option(SILVER_BUILD_TESTS "Build test suite" ON)
option(SILVER_BUILD_BENCHMARKS "Build performance benchmarks" ON)
option(SILVER_BUILD_EXAMPLES "Build example programs" ON)
option(SILVER_BUILD_TOOLS "Build additional tools" ON)
option(SILVER_ENABLE_ASSERTIONS "Enable runtime assertions" OFF)
option(SILVER_ENABLE_PROFILING "Enable profiling support" OFF)
option(SILVER_WARNINGS_AS_ERRORS "Treat warnings as errors" OFF)
option(SILVER_USE_LIBC "Use system libc for support functions" ON)

# ============================================================================
# 全局编译设置
# ============================================================================

# C17标准
set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)

# 导出编译命令数据库（用于工具）
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# ============================================================================
# 平台检测
# ============================================================================

# 检测目标平台
if(CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64|AMD64|amd64")
    set(SILVER_HOST_ARCH "x86_64")
elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64|arm64|ARM64")
    set(SILVER_HOST_ARCH "arm64")
elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "riscv64")
    set(SILVER_HOST_ARCH "riscv64")
elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "i[3-6]86")
    set(SILVER_HOST_ARCH "x86")
elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "arm")
    set(SILVER_HOST_ARCH "arm")
else()
    set(SILVER_HOST_ARCH "unknown")
endif()

message(STATUS "Silver Host Architecture: ${SILVER_HOST_ARCH}")

# 检测操作系统
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    set(SILVER_HOST_OS "linux")
elseif(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
    set(SILVER_HOST_OS "macos")
elseif(CMAKE_SYSTEM_NAME STREQUAL "Windows")
    set(SILVER_HOST_OS "windows")
elseif(CMAKE_SYSTEM_NAME STREQUAL "FreeBSD")
    set(SILVER_HOST_OS "freebsd")
else()
    set(SILVER_HOST_OS "unknown")
endif()

message(STATUS "Silver Host OS: ${SILVER_HOST_OS}")

# ============================================================================
# 编译器检测和配置
# ============================================================================

# 编译器特定选项
if(CMAKE_C_COMPILER_ID MATCHES "GCC")
    message(STATUS "Compiler: GCC ${CMAKE_C_COMPILER_VERSION}")
    set(SILVER_COMPILER_GCC ON)
elseif(CMAKE_C_COMPILER_ID MATCHES "Clang")
    message(STATUS "Compiler: Clang ${CMAKE_C_COMPILER_VERSION}")
    set(SILVER_COMPILER_CLANG ON)
elseif(MSVC)
    message(STATUS "Compiler: MSVC ${MSVC_VERSION}")
    set(SILVER_COMPILER_MSVC ON)
endif()

# ============================================================================
# 编译标志
# ============================================================================

# 通用编译标志
set(SILVER_COMMON_FLAGS "")

# 警告标志
if(SILVER_COMPILER_GCC OR SILVER_COMPILER_CLANG)
    list(APPEND SILVER_COMMON_FLAGS
        -Wall
        -Wextra
        -Wpedantic
        -Wshadow
        -Wstrict-prototypes
        -Wmissing-prototypes
        -Wold-style-definition
        -Wmissing-declarations
        -Wredundant-decls
        -Wnested-externs
        -Wpointer-arith
        -Wcast-align
        -Wwrite-strings
        -Wformat=2
        -Wformat-overflow=2
        -Wformat-truncation=2
        -Wimplicit-fallthrough
        -Wmissing-include-dirs
        -Wswitch-default
        -Wuninitialized
        -Wmaybe-uninitialized
        -Wstrict-overflow=4
        -Wduplicated-branches
        -Wduplicated-cond
        -Wnull-dereference
        -Wjump-misses-init
        -Wlogical-op
        -Wno-unused-parameter
        -Wno-missing-field-initializers
    )
    
    # 将警告视为错误
    if(SILVER_WARNINGS_AS_ERRORS)
        list(APPEND SILVER_COMMON_FLAGS -Werror)
    endif()
    
    # 断言控制
    if(SILVER_ENABLE_ASSERTIONS)
        list(APPEND SILVER_COMMON_FLAGS -DSILVER_ASSERTS=1)
    endif()
    
elseif(SILVER_COMPILER_MSVC)
    list(APPEND SILVER_COMMON_FLAGS
        /W4
        /wd4200  # nonstandard extension: zero-sized array
        /wd4201  # nonstandard extension: nameless struct/union
        /wd4204  # nonstandard extension: non-constant aggregate initializer
        /wd4221  # nonstandard extension: address of automatic
        /wd4996  # deprecated functions
    )
    
    if(SILVER_WARNINGS_AS_ERRORS)
        list(APPEND SILVER_COMMON_FLAGS /WX)
    endif()
    
    if(SILVER_ENABLE_ASSERTIONS)
        list(APPEND SILVER_COMMON_FLAGS /DSILVER_ASSERTS=1)
    endif()
endif()

# 优化标志
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    message(STATUS "Build Type: Debug (no optimization, debug symbols)")
    if(SILVER_COMPILER_GCC OR SILVER_COMPILER_CLANG)
        list(APPEND SILVER_COMMON_FLAGS -O0 -g3 -ggdb -fno-omit-frame-pointer)
    elseif(SILVER_COMPILER_MSVC)
        list(APPEND SILVER_COMMON_FLAGS /Od /Zi)
    endif()
    
elseif(CMAKE_BUILD_TYPE STREQUAL "Release")
    message(STATUS "Build Type: Release (full optimization)")
    if(SILVER_COMPILER_GCC OR SILVER_COMPILER_CLANG)
        list(APPEND SILVER_COMMON_FLAGS
            -O3
            -DNDEBUG
            -march=native
            -mtune=native
            -fomit-frame-pointer
            -fstrict-aliasing
            -ffast-math
            -funroll-loops
            -finline-functions
            -ftree-vectorize
        )
        # 链接时优化
        if(SILVER_COMPILER_GCC)
            list(APPEND SILVER_COMMON_FLAGS -flto)
        elseif(SILVER_COMPILER_CLANG)
            list(APPEND SILVER_COMMON_FLAGS -flto=thin)
        endif()
    elseif(SILVER_COMPILER_MSVC)
        list(APPEND SILVER_COMMON_FLAGS /O2 /GL /GS-)
    endif()
    
elseif(CMAKE_BUILD_TYPE STREQUAL "RelWithDebInfo")
    message(STATUS "Build Type: Release with Debug Info")
    if(SILVER_COMPILER_GCC OR SILVER_COMPILER_CLANG)
        list(APPEND SILVER_COMMON_FLAGS -O2 -g -DNDEBUG)
    elseif(SILVER_COMPILER_MSVC)
        list(APPEND SILVER_COMMON_FLAGS /O2 /Zi)
    endif()
    
elseif(CMAKE_BUILD_TYPE STREQUAL "MinSizeRel")
    message(STATUS "Build Type: Minimum Size Release")
    if(SILVER_COMPILER_GCC OR SILVER_COMPILER_CLANG)
        list(APPEND SILVER_COMMON_FLAGS -Os -DNDEBUG)
    elseif(SILVER_COMPILER_MSVC)
        list(APPEND SILVER_COMMON_FLAGS /O1)
    endif()
endif()

# 性能分析选项
if(SILVER_ENABLE_PROFILING)
    if(SILVER_COMPILER_GCC OR SILVER_COMPILER_CLANG)
        list(APPEND SILVER_COMMON_FLAGS -pg)
    endif()
    message(STATUS "Profiling support: ENABLED")
endif()

# 平台特定定义
if(SILVER_HOST_OS STREQUAL "linux")
    list(APPEND SILVER_COMMON_FLAGS -D_GNU_SOURCE -D_DEFAULT_SOURCE)
elseif(SILVER_HOST_OS STREQUAL "macos")
    list(APPEND SILVER_COMMON_FLAGS -D_DARWIN_C_SOURCE)
endif()

# ============================================================================
# 检查依赖
# ============================================================================

include(CheckIncludeFile)
include(CheckFunctionExists)
include(CheckSymbolExists)
include(CheckTypeSize)

# 检查头文件
check_include_file("stdatomic.h" HAVE_STDATOMIC_H)
check_include_file("stdalign.h" HAVE_STDALIGN_H)
check_include_file("stdnoreturn.h" HAVE_STDNORETURN_H)
check_include_file("threads.h" HAVE_THREADS_H)

if(HAVE_STDATOMIC_H)
    list(APPEND SILVER_COMMON_FLAGS -DHAVE_STDATOMIC=1)
endif()

# 检查函数和符号
if(SILVER_USE_LIBC)
    check_function_exists("memcpy" HAVE_MEMCPY)
    check_function_exists("memset" HAVE_MEMSET)
    check_function_exists("memcmp" HAVE_MEMCMP)
    check_function_exists("strlen" HAVE_STRLEN)
    check_function_exists("snprintf" HAVE_SNPRINTF)
    check_function_exists("vsnprintf" HAVE_VSNPRINTF)
    check_function_exists("isatty" HAVE_ISATTY)
    check_function_exists("fileno" HAVE_FILENO)
endif()

# 检查类型大小
check_type_size("long" SIZEOF_LONG)
check_type_size("long long" SIZEOF_LONG_LONG)
check_type_size("void*" SIZEOF_VOID_P)

# ============================================================================
# 源文件收集
# ============================================================================

# 支持库源文件
set(SILVER_SUPPORT_SOURCES
    src/support/arena.c
    src/support/error.c
    src/support/buffer.c
)

# IR系统源文件
set(SILVER_IR_SOURCES
    src/ir/ir_type.c
    src/ir/ir_value.c
    src/ir/ir_inst.c
    src/ir/ir_module.c
    src/ir/ir_builder.c
)

# 优化Pass源文件
set(SILVER_OPT_SOURCES
    src/opt/passes.c
    src/opt/const_fold.c
    src/opt/algebraic.c
    src/opt/strength.c
    src/opt/dce.c
    src/opt/copy_prop.c
)

# 代码生成源文件
set(SILVER_CODEGEN_SOURCES
    src/codegen/machine.c
    src/codegen/isel.c
    src/codegen/emitter.c
)

# 目标平台源文件
set(SILVER_TARGET_SOURCES
    # x86-64
    src/target/x86/x86.c
    src/target/x86/x86_isel.c
    src/target/x86/x86_encode.c
    # ARM64
    src/target/arm/arm.c
    src/target/arm/arm_isel.c
    src/target/arm/arm_encode.c
    # RISC-V64
    src/target/riscv/riscv.c
    src/target/riscv/riscv_isel.c
    src/target/riscv/riscv_encode.c
)

# 调试信息源文件
set(SILVER_DEBUG_SOURCES
    src/debug/dwarf.c
)

# 输出格式源文件
set(SILVER_LINK_SOURCES
    src/link/elf.c
    src/link/pe.c
    src/link/macho.c
)

# 主API源文件
set(SILVER_API_SOURCES
    src/silver.c
)

# 所有源文件
set(SILVER_ALL_SOURCES
    ${SILVER_SUPPORT_SOURCES}
    ${SILVER_IR_SOURCES}
    ${SILVER_OPT_SOURCES}
    ${SILVER_CODEGEN_SOURCES}
    ${SILVER_TARGET_SOURCES}
    ${SILVER_DEBUG_SOURCES}
    ${SILVER_LINK_SOURCES}
    ${SILVER_API_SOURCES}
)

# ============================================================================
# Silver 库目标
# ============================================================================

# 头文件目录
set(SILVER_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/include)

# 静态库
if(SILVER_BUILD_STATIC)
    add_library(silver_static STATIC ${SILVER_ALL_SOURCES})
    
    target_include_directories(silver_static PUBLIC
        $<BUILD_INTERFACE:${SILVER_INCLUDE_DIR}>
        $<INSTALL_INTERFACE:include>
    )
    
    target_compile_options(silver_static PRIVATE ${SILVER_COMMON_FLAGS})
    
    set_target_properties(silver_static PROPERTIES
        OUTPUT_NAME silver
        C_STANDARD 17
        C_STANDARD_REQUIRED ON
        C_EXTENSIONS ON
        POSITION_INDEPENDENT_CODE ON
    )
    
    # 链接时优化
    if(CMAKE_BUILD_TYPE STREQUAL "Release")
        if(SILVER_COMPILER_GCC OR SILVER_COMPILER_CLANG)
            set_target_properties(silver_static PROPERTIES
                INTERPROCEDURAL_OPTIMIZATION TRUE
            )
        endif()
    endif()
endif()

# 共享库
if(SILVER_BUILD_SHARED)
    add_library(silver_shared SHARED ${SILVER_ALL_SOURCES})
    
    target_include_directories(silver_shared PUBLIC
        $<BUILD_INTERFACE:${SILVER_INCLUDE_DIR}>
        $<INSTALL_INTERFACE:include>
    )
    
    target_compile_definitions(silver_shared PRIVATE
        SILVER_BUILDING_DLL
        SILVER_DLL_EXPORT
    )
    
    target_compile_options(silver_shared PRIVATE ${SILVER_COMMON_FLAGS})
    
    set_target_properties(silver_shared PROPERTIES
        OUTPUT_NAME silver
        C_STANDARD 17
        C_STANDARD_REQUIRED ON
        C_EXTENSIONS ON
        VERSION ${PROJECT_VERSION}
        SOVERSION ${PROJECT_VERSION_MAJOR}
    )
    
    # 链接时优化
    if(CMAKE_BUILD_TYPE STREQUAL "Release")
        if(SILVER_COMPILER_GCC OR SILVER_COMPILER_CLANG)
            set_target_properties(silver_shared PROPERTIES
                INTERPROCEDURAL_OPTIMIZATION TRUE
            )
        endif()
    endif()
endif()

# 选择默认库目标
if(SILVER_BUILD_STATIC)
    set(SILVER_LIB silver_static)
elseif(SILVER_BUILD_SHARED)
    set(SILVER_LIB silver_shared)
else()
    message(FATAL_ERROR "At least one library type must be built")
endif()

# 创建别名目标
add_library(silver::lib ALIAS ${SILVER_LIB})

# ============================================================================
# 命令行驱动程序
# ============================================================================

add_executable(silverc
    tools/driver/main.c
)

target_link_libraries(silverc PRIVATE ${SILVER_LIB})

target_include_directories(silverc PRIVATE
    ${SILVER_INCLUDE_DIR}
)

target_compile_options(silverc PRIVATE ${SILVER_COMMON_FLAGS})

set_target_properties(silverc PROPERTIES
    OUTPUT_NAME silverc
)

# ============================================================================
# 附加工具
# ============================================================================

if(SILVER_BUILD_TOOLS)
    # IR转储工具
    add_executable(silver_dump
        tools/dump/dump.c
    )
    target_link_libraries(silver_dump PRIVATE ${SILVER_LIB})
    target_include_directories(silver_dump PRIVATE ${SILVER_INCLUDE_DIR})
    
    # 性能分析工具
    add_executable(silver_analyze
        tools/analyzer/silver_analyze.c
    )
    target_link_libraries(silver_analyze PRIVATE ${SILVER_LIB} m)
    target_include_directories(silver_analyze PRIVATE ${SILVER_INCLUDE_DIR})
endif()

# ============================================================================
# 测试
# ============================================================================

if(SILVER_BUILD_TESTS)
    enable_testing()
    
    # 单元测试
    set(SILVER_UNIT_TESTS
        test_ir
        test_opt
        # test_codegen
        test_arena
        test_targets
    )
    
    # test_ir
    add_executable(test_ir tests/unit/test_ir.c)
    target_link_libraries(test_ir PRIVATE ${SILVER_LIB})
    target_include_directories(test_ir PRIVATE ${SILVER_INCLUDE_DIR})
    add_test(NAME test_ir COMMAND test_ir)
    
    # test_opt
    add_executable(test_opt tests/unit/test_opt.c)
    target_link_libraries(test_opt PRIVATE ${SILVER_LIB})
    target_include_directories(test_opt PRIVATE ${SILVER_INCLUDE_DIR})
    add_test(NAME test_opt COMMAND test_opt)
    
    # test_codegen
    add_executable(test_codegen tests/unit/test_codegen.c)
    target_link_libraries(test_codegen PRIVATE ${SILVER_LIB})
    target_include_directories(test_codegen PRIVATE ${SILVER_INCLUDE_DIR})
    add_test(NAME test_codegen COMMAND test_codegen)
    
    # test_arena
    add_executable(test_arena tests/unit/test_arena.c)
    target_link_libraries(test_arena PRIVATE ${SILVER_LIB})
    target_include_directories(test_arena PRIVATE ${SILVER_INCLUDE_DIR})
    add_test(NAME test_arena COMMAND test_arena)
    
    # test_targets
    add_executable(test_targets tests/unit/test_targets.c)
    target_link_libraries(test_targets PRIVATE ${SILVER_LIB})
    target_include_directories(test_targets PRIVATE ${SILVER_INCLUDE_DIR})
    add_test(NAME test_targets COMMAND test_targets)
    
    # 集成测试
    # test_full_pipeline
    add_executable(test_full_pipeline tests/integration/test_full_pipeline.c)
    target_link_libraries(test_full_pipeline PRIVATE ${SILVER_LIB})
    target_include_directories(test_full_pipeline PRIVATE ${SILVER_INCLUDE_DIR})
    add_test(NAME test_full_pipeline COMMAND test_full_pipeline)
    
    # test_platforms
    add_executable(test_platforms tests/integration/test_platforms.c)
    target_link_libraries(test_platforms PRIVATE ${SILVER_LIB})
    target_include_directories(test_platforms PRIVATE ${SILVER_INCLUDE_DIR})
    add_test(NAME test_platforms COMMAND test_platforms)
    
    # 设置测试属性
    foreach(test_name test_ir test_opt test_codegen test_arena test_targets
                     test_full_pipeline test_platforms)
        target_include_directories(${test_name} PRIVATE ${SILVER_INCLUDE_DIR})
        set_tests_properties(${test_name} PROPERTIES TIMEOUT 60)
    endforeach() 
endif()

# ============================================================================
# 基准测试
# ============================================================================

if(SILVER_BUILD_BENCHMARKS)
    # 编译速度基准测试
    add_executable(bench_compile_speed
        tests/benchmarks/bench_compile_speed.c
    )
    target_link_libraries(bench_compile_speed PRIVATE ${SILVER_LIB} m)
    target_include_directories(bench_compile_speed PRIVATE ${SILVER_INCLUDE_DIR})
    
    # 代码质量基准测试
    add_executable(bench_code_quality
        tests/benchmarks/bench_code_quality.c
    )
    target_link_libraries(bench_code_quality PRIVATE ${SILVER_LIB} m)
    target_include_directories(bench_code_quality PRIVATE ${SILVER_INCLUDE_DIR})
    
    # 注册为CTest测试
    add_test(NAME bench_compile_speed 
        COMMAND bench_compile_speed
    )
    add_test(NAME bench_code_quality 
        COMMAND bench_code_quality
    )
    
    # 基准测试属性
    set_tests_properties(bench_compile_speed bench_code_quality
        PROPERTIES
        TIMEOUT 300
        LABELS "benchmark"
        RUN_SERIAL TRUE
    )
endif()

# ============================================================================
# 示例程序
# ============================================================================

if(SILVER_BUILD_EXAMPLES)
    add_executable(example_hello examples/hello_world.c)
    target_link_libraries(example_hello PRIVATE ${SILVER_LIB})
    target_include_directories(example_hello PRIVATE ${SILVER_INCLUDE_DIR})
    
    add_executable(example_fib examples/fibonacci.c)
    target_link_libraries(example_fib PRIVATE ${SILVER_LIB})
    target_include_directories(example_fib PRIVATE ${SILVER_INCLUDE_DIR})
endif()

# ============================================================================
# 自定义构建目标
# ============================================================================

# 运行所有测试
add_custom_target(check
    COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure -j${NPROC} -V
    DEPENDS ${SILVER_UNIT_TESTS} test_full_pipeline
    COMMENT "Running all Silver tests"
    USES_TERMINAL
)

# 运行单元测试
add_custom_target(check-unit
    COMMAND ${CMAKE_CTEST_COMMAND} -L unit --output-on-failure
    DEPENDS ${SILVER_UNIT_TESTS}
    COMMENT "Running unit tests"
    USES_TERMINAL
)

# 运行集成测试
add_custom_target(check-integration
    COMMAND ${CMAKE_CTEST_COMMAND} -L integration --output-on-failure
    DEPENDS test_full_pipeline
    COMMENT "Running integration tests"
    USES_TERMINAL
)

# 运行基准测试
add_custom_target(benchmark
    COMMAND ${CMAKE_CTEST_COMMAND} -L benchmark --output-on-failure
    DEPENDS bench_compile_speed bench_code_quality
    COMMENT "Running performance benchmarks"
    USES_TERMINAL
)

# 代码格式化（如果安装了clang-format）
find_program(CLANG_FORMAT clang-format)
if(CLANG_FORMAT)
    file(GLOB_RECURSE SILVER_ALL_HEADERS 
        ${SILVER_INCLUDE_DIR}/*.h
        ${SILVER_INCLUDE_DIR}/*.h
    )
    
    add_custom_target(format
        COMMAND ${CLANG_FORMAT} -i ${SILVER_ALL_SOURCES} 
                                  tools/driver/main.c
                                  ${SILVER_ALL_HEADERS}
        COMMENT "Formatting source code with clang-format"
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    )
    
    add_custom_target(format-check
        COMMAND ${CLANG_FORMAT} --dry-run --Werror ${SILVER_ALL_SOURCES}
        COMMENT "Checking source code formatting"
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    )
endif()

# 静态分析
find_program(CPPCHECK cppcheck)
if(CPPCHECK)
    add_custom_target(analyze
        COMMAND ${CPPCHECK} 
            --enable=all
            --inconclusive
            --std=c11
            --suppress=missingIncludeSystem
            -I ${SILVER_INCLUDE_DIR}
            ${SILVER_ALL_SOURCES}
        COMMENT "Running static analysis with cppcheck"
    )
endif()

# 清理构建产物
add_custom_target(distclean
    COMMAND ${CMAKE_COMMAND} -E remove_directory ${CMAKE_BINARY_DIR}
    COMMENT "Cleaning all build artifacts"
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)

# ============================================================================
# 安装
# ============================================================================

include(GNUInstallDirs)

# 安装头文件
install(DIRECTORY include/silver
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
    FILES_MATCHING PATTERN "*.h"
)

# 安装库
if(SILVER_BUILD_STATIC)
    install(TARGETS silver_static
        EXPORT silver-targets
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    )
endif()

if(SILVER_BUILD_SHARED)
    install(TARGETS silver_shared
        EXPORT silver-targets
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    )
endif()

# 安装命令行工具
install(TARGETS silverc
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

# 安装附加工具
if(SILVER_BUILD_TOOLS)
    install(TARGETS silver_dump silver_analyze
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    )
endif()

# ============================================================================
# pkg-config支持
# ============================================================================

# 查找自定义pkg-config路径
set(PKG_CONFIG_INSTALL_DIR "${CMAKE_INSTALL_LIBDIR}/pkgconfig"
    CACHE STRING "pkg-config file install directory")

# 生成pkg-config文件
configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/silver.pc.in
    ${CMAKE_CURRENT_BINARY_DIR}/silver.pc
    @ONLY
)

install(FILES ${CMAKE_CURRENT_BINARY_DIR}/silver.pc
    DESTINATION ${PKG_CONFIG_INSTALL_DIR}
)

# ============================================================================
# CMake包配置
# ============================================================================

# 生成CMake包配置文件
include(CMakePackageConfigHelpers)

configure_package_config_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/cmake/silver-config.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/cmake/silver-config.cmake
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/silver
)

write_basic_package_version_file(
    ${CMAKE_CURRENT_BINARY_DIR}/cmake/silver-config-version.cmake
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
)

install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/cmake/silver-config.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/cmake/silver-config-version.cmake
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/silver
)

install(EXPORT silver-targets
    FILE silver-targets.cmake
    NAMESPACE silver::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/silver
)

# ============================================================================
# 打包
# ============================================================================

set(CPACK_PACKAGE_NAME "silver")
set(CPACK_PACKAGE_VERSION ${PROJECT_VERSION})
set(CPACK_PACKAGE_VENDOR "Silver Project")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Silver - Fast Cross-Platform Compiler Backend")
set(CPACK_PACKAGE_CONTACT "silver-dev@example.com")
set(CPACK_RESOURCE_FILE_LICENSE ${CMAKE_CURRENT_SOURCE_DIR}/LICENSE)
set(CPACK_RESOURCE_FILE_README ${CMAKE_CURRENT_SOURCE_DIR}/README.md)

# 根据平台生成不同的包格式
if(SILVER_HOST_OS STREQUAL "linux")
    set(CPACK_GENERATOR "TGZ;DEB;RPM")
elseif(SILVER_HOST_OS STREQUAL "macos")
    set(CPACK_GENERATOR "TGZ;DragNDrop")
elseif(SILVER_HOST_OS STREQUAL "windows")
    set(CPACK_GENERATOR "ZIP;NSIS")
else()
    set(CPACK_GENERATOR "TGZ")
endif()

include(CPack)

# ============================================================================
# 打印构建摘要
# ============================================================================

message(STATUS "")
message(STATUS "╔══════════════════════════════════════════════════════════════╗")
message(STATUS "║              Silver Compiler Backend v${PROJECT_VERSION}                ║")
message(STATUS "╚══════════════════════════════════════════════════════════════╝")
message(STATUS "")
message(STATUS "Build Configuration:")
message(STATUS "  Build type:           ${CMAKE_BUILD_TYPE}")
message(STATUS "  Host architecture:    ${SILVER_HOST_ARCH}")
message(STATUS "  Host OS:              ${SILVER_HOST_OS}")
message(STATUS "  Static library:       ${SILVER_BUILD_STATIC}")
message(STATUS "  Shared library:       ${SILVER_BUILD_SHARED}")
message(STATUS "  Tests:                ${SILVER_BUILD_TESTS}")
message(STATUS "  Benchmarks:           ${SILVER_BUILD_BENCHMARKS}")
message(STATUS "  Examples:             ${SILVER_BUILD_EXAMPLES}")
message(STATUS "  Tools:                ${SILVER_BUILD_TOOLS}")
message(STATUS "  Profiling:            ${SILVER_ENABLE_PROFILING}")
message(STATUS "  Install prefix:       ${CMAKE_INSTALL_PREFIX}")
message(STATUS "")
message(STATUS "Available targets:")
message(STATUS "  make all              - Build everything")
message(STATUS "  make silverc          - Build compiler driver only")
message(STATUS "  make check            - Run all tests")
message(STATUS "  make check-unit       - Run unit tests only")
message(STATUS "  make check-integration- Run integration tests only")
message(STATUS "  make benchmark        - Run performance benchmarks")
message(STATUS "  make install          - Install to ${CMAKE_INSTALL_PREFIX}")
message(STATUS "  make package          - Create distribution package")
message(STATUS "  make format           - Format source code (requires clang-format)")
message(STATUS "  make analyze          - Run static analysis (requires cppcheck)")
message(STATUS "")