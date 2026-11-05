cmake_minimum_required(VERSION 3.20)
project(librdx C CXX)

set(CMAKE_C_STANDARD 23)
find_package(benchmark)
set(CMAKE_EXPORT_COMPILE_COMMANDS on)

#set(CMAKE_CXX_FLAGS ${CMAKE_CXX_FLAGS} -fPIC -fno-exceptions)
set(BENCH_CXX_FLAGS ${CMAKE_CXX_FLAGS} -O3 -v)
set(BENCH_LDD_FLAGS benchmark::benchmark)

include_directories(..)
#find_package(sodium CONFIG)

add_executable(HEAPbench HEAP.cpp)
target_compile_options(HEAPbench PRIVATE ${BENCH_CXX_FLAGS})
target_link_libraries(HEAPbench PRIVATE ${BENCH_LDD_FLAGS})

add_executable(SORTbench SORT.cpp ../SORT.c)
target_compile_options(SORTbench PRIVATE ${BENCH_CXX_FLAGS})
target_link_libraries(SORTbench PRIVATE ${BENCH_LDD_FLAGS})

add_executable(QSORTbench QSORT.cpp)
target_compile_options(QSORTbench PRIVATE ${BENCH_CXX_FLAGS})
target_link_libraries(QSORTbench PRIVATE ${BENCH_LDD_FLAGS})

#  BENCH-26B0559PCK: BOX and PUPS vs std::set; C++20 for `requires`, GNU typeof for abc
#  headers; libstdc++ lookup and libunwind as fuzz/CMakeLists.txt FUZZ-26B04IZDFD
include(CheckIncludeFileCXX)
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)
check_include_file_cxx(unordered_map ABC_HAVE_STL)
file(GLOB ABC_STL_CFG /usr/include/c++/*/*/bits/c++config.h)
if (NOT ABC_HAVE_STL AND ABC_STL_CFG)
    list(GET ABC_STL_CFG -1 ABC_STL_TRI)
    cmake_path(GET ABC_STL_TRI PARENT_PATH ABC_STL_TRI)
    cmake_path(GET ABC_STL_TRI PARENT_PATH ABC_STL_TRI)
    cmake_path(GET ABC_STL_TRI PARENT_PATH ABC_STL_VER)
    include_directories(SYSTEM ${ABC_STL_VER} ${ABC_STL_TRI})
endif()
if ("${CMAKE_CXX_FLAGS} ${CMAKE_EXE_LINKER_FLAGS}" MATCHES "compiler-rt")
    add_link_options(--unwindlib=libunwind)
endif()

find_package(absl CONFIG QUIET)
foreach(B BOX PUPS)
    add_executable(${B}bench ${B}.cpp BOXb.c PUPb.c)
    set_target_properties(${B}bench PROPERTIES CXX_STANDARD 20 CXX_EXTENSIONS ON)
    target_compile_options(${B}bench PRIVATE -O3)
    target_link_libraries(${B}bench PRIVATE ${BENCH_LDD_FLAGS} abc-core)
    if (absl_FOUND AND EXISTS /usr/include/absl/container/btree_set.h)
        target_compile_definitions(${B}bench PRIVATE ABC_HAVE_ABSL)
        target_link_libraries(${B}bench PRIVATE absl::btree)
    endif()
endforeach()
