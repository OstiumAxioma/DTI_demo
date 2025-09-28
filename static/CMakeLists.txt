cmake_minimum_required(VERSION 3.20)
project(DTIFiberLib CXX)

# 加载本地配置文件（如果存在）
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/config.local.cmake")
    message(STATUS "Loading local configuration from config.local.cmake")
    include("${CMAKE_CURRENT_SOURCE_DIR}/config.local.cmake")
elseif(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/config.cmake")
    message(STATUS "Loading configuration from config.cmake")
    include("${CMAKE_CURRENT_SOURCE_DIR}/config.cmake")
endif()

# 设置C++标准
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 解决MSVC编译器字符编码警告
if(MSVC)
    add_compile_options("/source-charset:utf-8")
endif()

# Qt5配置 - 自动查找或使用环境变量
if(NOT DEFINED Qt5_DIR)
    # 尝试常见的Qt安装路径
    set(Qt5_PATHS
        "C:/Qt/Qt5.14.2/5.14.2/msvc2017_64"
        "C:/Qt/5.14.2/msvc2017_64"
        "C:/Qt5/5.14.2/msvc2017_64"
        "D:/Qt/Qt5.14.2/5.14.2/msvc2017_64"
        "D:/Qt/5.14.2/msvc2017_64"
        "$ENV{Qt5_DIR}"
        "$ENV{QT_DIR}"
        "$ENV{QTDIR}"
    )
    
    foreach(path ${Qt5_PATHS})
        if(EXISTS "${path}/lib/cmake/Qt5")
            set(Qt5_DIR "${path}/lib/cmake/Qt5")
            message(STATUS "Found Qt5 at: ${Qt5_DIR}")
            break()
        endif()
    endforeach()
endif()

# 如果还是找不到Qt5，提示用户设置
if(NOT DEFINED Qt5_DIR)
    message(WARNING "Qt5 not found automatically. Please set Qt5_DIR or CMAKE_PREFIX_PATH")
    message(WARNING "You can set it by: cmake -DQt5_DIR=<path_to_qt5>/lib/cmake/Qt5")
endif()

# VTK配置 - 自动查找或使用环境变量
if(NOT DEFINED VTK_DIR)
    # 尝试常见的VTK安装路径
    set(VTK_PATHS
        "D:/code/vtk8.2.0/VTK-8.2.0/lib/cmake/vtk-8.2"
        "C:/VTK/lib/cmake/vtk-8.2"
        "C:/Program Files/VTK/lib/cmake/vtk-8.2"
        "$ENV{VTK_DIR}"
        "$ENV{VTK_ROOT}/lib/cmake/vtk-8.2"
    )
    
    foreach(path ${VTK_PATHS})
        if(EXISTS "${path}/VTKConfig.cmake")
            set(VTK_DIR "${path}")
            message(STATUS "Found VTK at: ${VTK_DIR}")
            break()
        endif()
    endforeach()
endif()

# 如果还是找不到VTK，提示用户设置
if(NOT DEFINED VTK_DIR)
    message(FATAL_ERROR "VTK not found. Please set VTK_DIR or install VTK\n"
                        "You can set it by: cmake -DVTK_DIR=<path_to_vtk>/lib/cmake/vtk-8.2")
endif()

find_package(VTK 8.2 REQUIRED)

# 包含VTK使用文件（VTK 8.2必需）
include(${VTK_USE_FILE})

# 设置输出目录
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

# 收集源文件
set(SOURCES
    src/DTIFiberLib.cpp
    src/TrkFileReader.cpp
    src/FiberBundleRenderer.cpp
)

set(HEADERS
    header/DTIFiberLib.h
    header/TrkFileReader.h
    header/FiberBundleRenderer.h
)

# 创建静态库
add_library(DTIFiberLib STATIC ${SOURCES} ${HEADERS})

# 设置包含目录
target_include_directories(DTIFiberLib PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/header
    ${VTK_INCLUDE_DIRS}
)

# 链接VTK库
target_link_libraries(DTIFiberLib PUBLIC ${VTK_LIBRARIES})

# Windows特定设置
if(WIN32)
    # 定义预处理器宏
    target_compile_definitions(DTIFiberLib PRIVATE
        _WINDOWS
        _UNICODE
        UNICODE
    )
    
    # 设置编译选项
    target_compile_options(DTIFiberLib PRIVATE
        /W3     # 警告级别3
        /EHsc   # 启用C++异常处理
        /MP     # 多处理器编译
    )
    
    # Release配置优化
    target_compile_options(DTIFiberLib PRIVATE
        $<$<CONFIG:Release>:/O2>    # 最大优化
        $<$<CONFIG:Release>:/GL>    # 全程序优化
    )
    
    # Debug配置
    target_compile_options(DTIFiberLib PRIVATE
        $<$<CONFIG:Debug>:/Od>      # 禁用优化
        $<$<CONFIG:Debug>:/Zi>      # 生成调试信息
    )
endif()

# 显示配置信息
message(STATUS "Building DTIFiberLib Static Library with VTK support")
message(STATUS "VTK_VERSION: ${VTK_VERSION}")
message(STATUS "VTK_USE_FILE: ${VTK_USE_FILE}")
message(STATUS "Output directory: ${CMAKE_ARCHIVE_OUTPUT_DIRECTORY}")