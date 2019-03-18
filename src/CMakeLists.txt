##
# @file CMakeLists.txt
# @brief 
# 
# @author gyz
# @email mni_gyz@163.com
#
include_directories(${CMAKE_SOURCE_DIR}/include)

set(LIB_BPT_SRC
    BPlusTree.cc
)

add_library(bptree ${LIB_BPT_SRC})