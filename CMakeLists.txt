execute_process(
  COMMAND ${CMAKE_SOURCE_DIR}/build/convert_compile_commands.exe
  WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
  RESULT_VARIABLE _ret
)

cmake_minimum_required(VERSION 3.30)
project(CppModulesSample LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 启用模块编译
set(CMAKE_EXPERIMENTAL_CXX_MODULE_DYNDEP 1)
set(CMAKE_EXPERIMENTAL_CXX_MODULE_CMAKE_API 1)

# 生成编译命令数据库
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_executable(main
    main.cpp
)

# 添加模块接口单元
target_sources(main
    PRIVATE
        FILE_SET CXX_MODULES FILES
            hello.ixx
)

# 将msvc风格的compile_commands.json转为clangd风格的工具
add_executable(convert_compile_commands
    tools/convert_compile_commands.cpp
)

# MSVC 配置
if(MSVC)
    target_compile_options(main PRIVATE /experimental:module /Zc:preprocessor /std:c++latest)
endif()
