cmake_minimum_required(VERSION 3.16)

project(DeviceConfigTool VERSION 1.0 LANGUAGES CXX)

# ============================================================
# HID Remapper 上位机（Qt / C++）
# 目标：连接 JiKong K1 (RP2350) 固件，通过 USB HID Feature Report
# 读取/编辑键盘按键映射，并下发+持久化到设备。
# 键盘 UI 复刻自 config-tool-web 的“捕获触发键”键盘 SVG 布局。
# ============================================================

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

# --- Qt (优先 Qt6，回退 Qt5) ---
find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Widgets Svg)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Widgets Svg)

# --- hidapi ---
# 探测顺序：
#   1) 项目内自带的预编译包 <项目>/hidapi（Windows MSVC：include/ + x64/）；
#   2) 通过 -DHIDAPI_ROOT 指定的路径；
#   3) 系统 pkg-config（Linux/macOS）。
set(HIDAPI_FOUND FALSE)
set(BUNDLED_HIDAPI "${CMAKE_CURRENT_SOURCE_DIR}/hidapi")
if(WIN32 AND EXISTS "${BUNDLED_HIDAPI}/include/hidapi.h" AND EXISTS "${BUNDLED_HIDAPI}/x64/hidapi.lib")
    set(HIDAPI_BUNDLED TRUE)
    set(HIDAPI_INCLUDE_DIRS "${BUNDLED_HIDAPI}/include")
    set(HIDAPI_LIBRARIES "${BUNDLED_HIDAPI}/x64/hidapi.lib")
    set(HIDAPI_DLL "${BUNDLED_HIDAPI}/x64/hidapi.dll")
    set(HIDAPI_FOUND TRUE)
    message(STATUS "使用项目内预编译 hidapi: ${BUNDLED_HIDAPI}/x64")
endif()

if(NOT HIDAPI_FOUND)
    find_package(PkgConfig QUIET)
    if(PkgConfig_FOUND)
        if(WIN32)
            pkg_check_modules(HIDAPI QUIET hidapi)
        else()
            pkg_check_modules(HIDAPI QUIET hidapi-hidraw hidapi-libusb hidapi)
        endif()
    endif()
endif()

set(SOURCES
    src/main.cpp
    src/mainwindow.cpp
    src/keyboardwidget.cpp
    src/keyboardlayout.cpp
    src/device.cpp
    src/protocol.cpp
    src/usages.cpp
    src/inputworker.cpp
    src/resources.qrc
)

set(HEADERS
    src/mainwindow.h
    src/keyboardwidget.h
    src/keyboardlayout.h
    src/device.h
    src/protocol.h
    src/usages.h
    src/inputworker.h
)

add_executable(hid-remapper-qt ${SOURCES} ${HEADERS})

target_link_libraries(hid-remapper-qt PRIVATE Qt${QT_VERSION_MAJOR}::Widgets Qt${QT_VERSION_MAJOR}::Svg)

if(HIDAPI_FOUND)
    target_include_directories(hid-remapper-qt PRIVATE ${HIDAPI_INCLUDE_DIRS})
    target_link_libraries(hid-remapper-qt PRIVATE ${HIDAPI_LIBRARIES})
    target_link_directories(hid-remapper-qt PRIVATE ${HIDAPI_LIBRARY_DIRS})
else()
    # 回退：期望用户通过 -DHIDAPI_ROOT 指定 hidapi 安装路径。
    if(DEFINED HIDAPI_ROOT)
        target_include_directories(hid-remapper-qt PRIVATE "${HIDAPI_ROOT}/include")
        find_library(HIDAPI_LIB NAMES hidapi hidapi-hidraw hidapi-libusb
            PATHS "${HIDAPI_ROOT}/lib" "${HIDAPI_ROOT}/bin")
        if(HIDAPI_LIB)
            target_link_libraries(hid-remapper-qt PRIVATE ${HIDAPI_LIB})
        else()
            message(FATAL_ERROR "在 HIDAPI_ROOT 下未找到 hidapi 库文件")
        endif()
    else()
        message(FATAL_ERROR
            "未找到 hidapi。请安装 hidapi 开发包，或通过 -DHIDAPI_ROOT=<路径> 指定。\n"
            "  Windows(vcpkg): vcpkg install hidapi 并加 -DCMAKE_TOOLCHAIN_FILE=...\n"
            "  Linux: sudo apt install libhidapi-dev\n"
            "  macOS: brew install hidapi")
    endif()
endif()

if(WIN32)
    set_target_properties(hid-remapper-qt PROPERTIES WIN32_EXECUTABLE TRUE)
    # Raw Input / 窗口 API 依赖 user32；标题栏配色依赖 dwmapi。
    target_link_libraries(hid-remapper-qt PRIVATE user32 dwmapi)

    if(MSVC)
        # 源码与执行字符集均按 UTF-8 处理：源文件为 UTF-8（无 BOM），
        # 若不指定 /utf-8，MSVC 会按系统 ANSI 代码页(GBK)误读中文字面量导致乱码。
        target_compile_options(hid-remapper-qt PRIVATE /utf-8)

        # --- 反作弊友好的编译/链接加固 ---
        # /DYNAMICBASE: ASLR；/HIGHENTROPYVA: 64 位高熵 ASLR；/NXCOMPAT: DEP。
        target_link_options(hid-remapper-qt PRIVATE
            /DYNAMICBASE /HIGHENTROPYVA /NXCOMPAT)
        # Release / MinSizeRel 下不生成 PDB、不在 exe 内嵌调试目录，减少多余特征。
        # /DEBUG:NONE 已确保链接器不产出 PDB，故编译期无需再刻意关闭 /Zi。
        target_link_options(hid-remapper-qt PRIVATE
            $<$<CONFIG:Release>:/DEBUG:NONE>
            $<$<CONFIG:MinSizeRel>:/DEBUG:NONE>)
        # 控制流保护（CFG），进一步降低无名进程的可疑度。
        target_compile_options(hid-remapper-qt PRIVATE /guard:cf)
        target_link_options(hid-remapper-qt PRIVATE /guard:cf)
        # 以普通用户权限运行（asInvoker），仅在需要时由用户手动提权。
        # 显式嵌入 asInvoker manifest，避免安装程序检测启发式自动请求管理员。
        target_sources(hid-remapper-qt PRIVATE
            "${CMAKE_CURRENT_SOURCE_DIR}/app.manifest"
            "${CMAKE_CURRENT_SOURCE_DIR}/version.rc")
    endif()

    # 中性化可执行文件名：去除 "remapper" 等重映射类工具特征字符串。
    set_target_properties(hid-remapper-qt PROPERTIES OUTPUT_NAME "DeviceConfigTool")
endif()

# 构建后把 hidapi.dll 复制到可执行文件旁边，保证运行时能加载。
if(DEFINED HIDAPI_DLL AND EXISTS "${HIDAPI_DLL}")
    add_custom_command(TARGET hid-remapper-qt POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${HIDAPI_DLL}" "$<TARGET_FILE_DIR:hid-remapper-qt>"
        COMMENT "复制 hidapi.dll 到输出目录")
endif()
