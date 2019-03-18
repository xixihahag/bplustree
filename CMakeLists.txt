##
# @file CMakeLists.txt
# @brief 
# 
# @author gyz
# @email mni_gyz@163.com
#
cmake_minimum_required(VERSION 2.8)
project(bptree)

# c/c++按照11表展
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED OFF)
set(CMAKE_C_EXTENSIONS OFF)
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# 设置编译结果输出路径
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG "${PROJECT_BINARY_DIR}/bin")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE "${PROJECT_BINARY_DIR}/bin")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_DEBUG "${PROJECT_BINARY_DIR}/bin")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_RELEASE "${PROJECT_BINARY_DIR}/bin")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY_DEBUG "${PROJECT_BINARY_DIR}/bin")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY_RELEASE "${PROJECT_BINARY_DIR}/bin")

# set(CMAKE_C_FLAGS                     "-Wall -Werror -Wno-attributes")
set(CMAKE_C_FLAGS                     "-g")
set(CMAKE_C_FLAGS_DEBUG               "-g -DDEBUG") #-fstack-protector-all
set(CMAKE_C_FLAGS_RELEASE             "-O3")
set(CMAKE_C_FLAGS_MINSIZEREL          "-Os")
set(CMAKE_C_RELWITHDEBINFO_RELEASE    "-O2")
# set(CMAKE_CXX_FLAGS                   "-Wall -Werror -Wno-attributes -ftemplate-backtrace-limit=0 -fno-exceptions -std=\"c++11\"")
set(CMAKE_CXX_FLAGS                   "-g")
set(CMAKE_CXX_FLAGS_DEBUG             "-g -DDEBUG")
set(CMAKE_CXX_FLAGS_RELEASE           "-O3 -fno-strict-aliasing")
set(CMAKE_CXX_FLAGS_MINSIZEREL        "-Os")
set(CMAKE_CXX_RELWITHDEBINFO_RELEASE  "-O2")

add_subdirectory(src)
add_subdirectory(tests)