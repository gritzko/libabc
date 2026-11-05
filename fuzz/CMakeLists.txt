cmake_minimum_required(VERSION 3.20)
project(librdx C CXX)

set(CMAKE_C_STANDARD 23)
#  FUZZ-26B04IqDqU: C++ targets diff against STL; abc headers need GNU typeof
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_EXTENSIONS ON)

set(CMAKE_EXPORT_COMPILE_COMMANDS on)

include_directories(../..)
include_directories(..)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${FUZZ_CXX_FLAGS}")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${FUZZ_C_FLAGS}")
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} ${FUZZ_LDD_FLAGS}")

#  FUZZ-26B04IZDFD: clang on Alpine without gcc selects no GCC install (no
#  crtbegin.o), so it misses the libstdc++ headers and, on compiler-rt, the unwinder
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

add_executable(HEAPfuzz HEAP.cpp ../RON.c)
add_executable(HASHfuzz HASH.cpp)
add_executable(SWISSfuzz SWISS.cpp)

add_executable(HASHdfuzz HASHd.c)

add_executable(ZINTfuzz ZINT.c "${CMAKE_CURRENT_SOURCE_DIR}/../ZINT.c")

add_executable(ZINT2fuzz ZINT2.c)
target_link_libraries(ZINT2fuzz abc)

add_executable(SORTfuzz SORT.cpp ../SORT.c ../RON.c)

add_executable(DIFFfuzz DIFF.c ../RON.c)

# ABC-017: mirror the root-dir suppression for the ragel output
set_source_files_properties(../URI.rl.c PROPERTIES
    COMPILE_OPTIONS "-Wno-unused-variable;-Wno-unused-const-variable")

add_executable(URIfuzz URI.c ../URI.c ../URI.rl.c)
#  URISplitPath now walks the path via abc/PATH's PATHu8sDrainNE
#  (URI-004), so the fuzz target needs PATH.c + its UTF8/ryu deps —
#  pulled on demand from the abc library (URI symbols stay instrumented
#  via the explicit ../URI.c above; the lib's copies are not pulled).
target_link_libraries(URIfuzz abc)

add_executable(SLOGfuzz SLOG.c)
target_link_libraries(SLOGfuzz abc)

add_executable(NFAfuzz NFA.c ../RON.c)

#  FUZZ-26B04IqDqU: HITx/BOXx are C only, so a C half (HIT.c) and STL (.cpp)
add_executable(HITfuzz HIT.cpp HIT.c ../RON.c)

add_executable(BOXfuzz BOX.cpp BOX.c ../RON.c)

add_executable(QSORTfuzz QSORT.cpp)
target_link_libraries(QSORTfuzz abc)

#  ABC-004: u1 bitmap set-algebra vs a reference set model.
add_executable(BITfuzz BIT.c ../RON.c)

#  PUPS-26B04CThJv: the PUPS kv64 house vs a plain array model, two lanes.
add_executable(PUPSfuzz PUPS.c)
target_link_libraries(PUPSfuzz abc)

#add_executable(SST SST.c)
#target_link_options(SST PRIVATE "${FUZZ_LDD_FLAGS}")
#target_compile_options(SST PRIVATE "${FUZZ_C_FLAGS}")

#  FUZZ-26B04IO_g6: TARGETfuzz replays the local, uncommitted corpus/TARGET/
#  once, no fuzzing; lsan.supp mutes a musl libFuzzer leak
file(GLOB FUZZ_CORPORA CONFIGURE_DEPENDS LIST_DIRECTORIES true
    ${CMAKE_CURRENT_SOURCE_DIR}/corpus/*)
foreach (dir ${FUZZ_CORPORA})
    get_filename_component(t ${dir} NAME)
    if (TARGET ${t}fuzz)
        add_test(NAME ${t}fuzz COMMAND ${t}fuzz -runs=0 -timeout=25 ${dir})
        set_tests_properties(${t}fuzz PROPERTIES LABELS fuzz ENVIRONMENT
            "ASAN_OPTIONS=allow_addr2line=1;LSAN_OPTIONS=suppressions=${CMAKE_CURRENT_SOURCE_DIR}/lsan.supp")
    endif ()
endforeach ()
