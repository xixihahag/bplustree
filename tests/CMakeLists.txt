##
# @file CMakeLists.txt
# @brief 
# 
# @author gyz
# @email mni_gyz@163.com
#
include_directories(${CMAKE_SOURCE_DIR}/include)

set(TEST
    main.cc
)

add_executable(ptest ${TEST})
# add_dependencies(ptest bptree)
target_link_libraries(ptest bptree)