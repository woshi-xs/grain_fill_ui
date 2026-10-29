cmake_minimum_required(VERSION 3.19)
# 软件名只写在这里。改名后重新配置 CMake 即可
project(GrainFilling
    VERSION 1.0.0
     DESCRIPTION "grain filling"
    LANGUAGES CXX)

find_package(Qt6 6.5 REQUIRED COMPONENTS Core Widgets)
find_package(OpenCV REQUIRED)
find_package(spdlog REQUIRED)
find_package(tomlplusplus REQUIRED)

qt_standard_project_setup()

# qt_standard_project_setup() 只开 AUTOMOC/AUTOUIC，qrc 需要显式开 AUTORCC
set(CMAKE_AUTORCC ON)

# 递归扫描 src 下所有 cpp/h/ui
file(GLOB_RECURSE SOURCES
    src/*.cpp
    src/*.h
    src/*.hpp
    src/*.ui
)

qt_add_executable(
    ${PROJECT_NAME}
    WIN32 MACOSX_BUNDLE
    ${SOURCES}
    src/resources.qrc
    resources.rc
)

# 头文件路径：src作为根，代码内直接 #include "ui/widget.h"
target_include_directories(
    ${PROJECT_NAME}
    PRIVATE
    src
    ui
)

target_link_libraries(${PROJECT_NAME}
    PRIVATE
        Qt::Core
        Qt::Widgets
        ${OpenCV_LIBS}
        spdlog::spdlog
        tomlplusplus::tomlplusplus
)

#导出软件版本号和软件名字让cpp能捕获
target_compile_definitions(${PROJECT_NAME} PRIVATE
    APP_VERSION=\"${PROJECT_VERSION}\"
    APP_NAME=\"${PROJECT_NAME}\"
)


install(TARGETS ${PROJECT_NAME}
    BUNDLE  DESTINATION .
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

qt_generate_deploy_app_script(
    TARGET ${PROJECT_NAME}
    OUTPUT_SCRIPT deploy_script
    NO_UNSUPPORTED_PLATFORM_ERROR
)
install(SCRIPT ${deploy_script})
