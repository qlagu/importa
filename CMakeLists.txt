# execute_process(
#   COMMAND ${CMAKE_SOURCE_DIR}/build/convert_compile_commands.exe
#   WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
#   RESULT_VARIABLE _ret
# )

cmake_minimum_required(VERSION 3.30)
project(CppModulesSample LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 启用模块编译
set(CMAKE_EXPERIMENTAL_CXX_MODULE_DYNDEP 1)
set(CMAKE_EXPERIMENTAL_CXX_MODULE_CMAKE_API 1)

# 生成编译命令数据库
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# modules
add_library(stdx STATIC)
target_sources(stdx
    PUBLIC
        FILE_SET CXX_MODULES
        TYPE CXX_MODULES
        FILES
            modules/stdx/std.ixx
)

add_library(toolchains STATIC)
target_sources(toolchains
    PUBLIC
        FILE_SET CXX_MODULES
        TYPE CXX_MODULES
        FILES
            modules/toolchains/toolchains.ixx
    PRIVATE
        modules/toolchains/toolchains.cpp
)
target_link_libraries(toolchains 
    PUBLIC
        stdx
        executor
)

add_library(executor STATIC)
target_sources(executor
    PUBLIC
        FILE_SET CXX_MODULES
        FILES
            modules/executor/executor.ixx
    PRIVATE
        modules/executor/executor.cpp
)
target_link_libraries(executor 
    PUBLIC 
        stdx
)

add_library(module_processor STATIC)
target_sources(module_processor
    PUBLIC
        FILE_SET CXX_MODULES
        FILES
            modules/module_processor/module_processor.ixx
    PRIVATE
        modules/module_processor/module_processor.cpp
)
target_link_libraries(module_processor
    PUBLIC 
        stdx
        executor
        toolchains
)
# tests

add_executable(tests
    tests/test.cpp
)
target_link_libraries(tests 
    PRIVATE 
        stdx
)

add_executable(tests_toolchains
    tests/toolchains.cpp
)
target_link_libraries(tests_toolchains 
    PRIVATE 
        executor
        toolchains
        stdx
)

add_executable(tests_executor
    tests/executor.cpp
)
target_link_libraries(tests_executor 
    PRIVATE 
        executor
        stdx
)

add_executable(tests_moduleProcessor
    tests/moduleProcessor.cpp
)
target_link_libraries(tests_moduleProcessor 
    PRIVATE 
        module_processor
        executor
        toolchains
        stdx
)

# 将msvc风格的compile_commands.json转为clangd风格的工具
add_executable(convert_compile_commands
    #tools/convert_compile_commands.cpp
    tools/impa_gen.cpp
)

# 为所有目标统一添加 /utf-8 选项
function(add_utf8_options_to_target target_name)
    if(MSVC)
        target_compile_options(${target_name} PRIVATE /utf-8)
    endif()
endfunction()

add_utf8_options_to_target(stdx)
add_utf8_options_to_target(toolchains)
add_utf8_options_to_target(executor)
add_utf8_options_to_target(tests)
add_utf8_options_to_target(tests_toolchains)
add_utf8_options_to_target(tests_executor)
add_utf8_options_to_target(convert_compile_commands)
add_utf8_options_to_target(module_processor)

