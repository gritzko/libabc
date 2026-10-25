cmake_minimum_required(VERSION 3.20)
project(libabc C)

set(CMAKE_C_STANDARD 23)

set(CMAKE_EXPORT_COMPILE_COMMANDS on)

# ABC-017: warnings on for all targets (subdirs inherit);
# unused-parameter waived: callback signatures are fixed by their APIs
add_compile_options(-Wall -Wextra -Wno-unused-parameter)

# ABC-017: ragel-generated sources carry unused consts/vars by design
set(ABC_RAGEL_OUTPUTS ANSI.rl.c JSON.rl.c URI.rl.c HTTP.rl.c LEX.rl.c)
set_source_files_properties(${ABC_RAGEL_OUTPUTS} PROPERTIES
    COMPILE_OPTIONS "-Wno-unused-variable;-Wno-unused-const-variable")

enable_testing()

# ABC-017: gate for the network tests (test/CMakeLists.txt); CI's
# inet-asan lane turns it on — loopback-only, no live-internet deps
option(WITH_INET "build network (loopback) tests" OFF)

# wasm32-wasi build: abc-core only, emulated mman/signal/clocks/getpid,
# no sodium/curl/lz4, no CLIs, tests, bench or fuzz
option(WASM_MODE "build abc-core only, for wasm32-wasi" OFF)
if (WASM_MODE)
    add_compile_definitions(_WASI_EMULATED_MMAN _WASI_EMULATED_SIGNAL _WASI_EMULATED_PROCESS_CLOCKS _WASI_EMULATED_GETPID)
endif()

#  BOOT-26A25Jhvsy: OFF builds abc without CURL.c/CURL.h, no libcurl
#  needed or linked, and drops the CURL tests (POLtest drives CURL too)
option(WITH_CURL "build CURL.c (libcurl) into abc" ON)

option(WITH_ASAN "build with ASAN" OFF)
if (WITH_ASAN)
    set(ASAN_FLAGS "-g -O1 -fsanitize=address -fno-omit-frame-pointer -fno-optimize-sibling-calls")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} ${ASAN_FLAGS}")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${ASAN_FLAGS}")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${ASAN_FLAGS}")
endif ()

option(WITH_FUZZ "build with libFuzzer" OFF)
if (WITH_FUZZ)
    set(FUZZ_FLAGS "-fsanitize=fuzzer")
    set(FUZZ_CXX_FLAGS "${FUZZ_FLAGS}")
    set(FUZZ_C_FLAGS "${FUZZ_FLAGS}")
    set(FUZZ_LDD_FLAGS "${FUZZ_FLAGS}")
    add_compile_options(-fsanitize=fuzzer-no-link)
    # ABC-017: non-fuzz binaries need the sancov runtime linked in too
    add_link_options(-fsanitize=fuzzer-no-link)
endif ()

set(DOG_BIN_DIR ${CMAKE_BINARY_DIR}/bin)

include_directories(.. . ryu)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}")
if (NOT WASM_MODE)
find_library(sodium NAMES sodium HINTS /usr /usr/local /opt/homebrew)
find_path(SODIUM_INCLUDE_DIR NAMES sodium.h HINTS /usr/include /usr/local/include /opt/homebrew/include)
#  curl is a HARD dependency of the full `abc` unless WITH_CURL is OFF:
#  CURL.c is part of it, and the test tree links `abc` unconditionally.  A
#  QUIET find turned a missing libcurl-dev into ~45 `cannot find -labc` link
#  errors, with the real cause buried in one STATUS line.  Fail at configure
#  time, naming the package.
#  Ubuntu/Debian: libcurl4-gnutls-dev (CI) or libcurl4-openssl-dev.
#  macOS: brew install curl.
if (WITH_CURL)
    find_package(CURL REQUIRED)
endif()
find_package(PkgConfig QUIET)
if (PkgConfig_FOUND)
    pkg_check_modules(LZ4 liblz4)
    if (WITH_ROCKS)
        pkg_check_modules(ROCKSDB REQUIRED rocksdb)
    endif()
endif()
# Fallback lz4 detection for hosts without pkg-config (cf. sodium above)
if (NOT LZ4_FOUND)
    find_library(LZ4_LIBRARIES NAMES lz4 HINTS /usr /usr/local /opt/homebrew)
    find_path(LZ4_INCLUDE_DIRS NAMES lz4.h HINTS /usr/include /usr/local/include /opt/homebrew/include)
    if (LZ4_LIBRARIES AND LZ4_INCLUDE_DIRS)
        set(LZ4_FOUND TRUE)
    endif()
endif()
endif() # NOT WASM_MODE

list(APPEND ABC_HEADERS
S.h
Sx.h
01.h
ABC.h
ANSI.h
B.h
BIN.h
Bx.h
BUF.h
FILE.h
HASH.h
HASHx.h
HEAPx.h
PUPS.h
PUPx.h
HEX.h
HEXx.h
INT.h
LSM.h
JSON.h
JSON.rl.h
LEX.h
MMAP.h
NEST.h
OK.h
PAGE.h
PRO.h
SHA.h
SKIP.h
SKIPx.h
SLOG.h
SORT.h
TEST.h
TLV.h
TTY.h
UTF8.h
ZINT.h
URI.h
NET.h
UDP.h
HTTP.h
CURL.h
PACK.h
PATH.h
BSD.h
)

list(APPEND ABC_SOURCES
ANSI.c
ANSI.rl.c
NEST.c
FILE.c
INT.c
JSON.c
JSON.rl.c
LEX.c
LEX.rl.c
LSM.c
MMAP.c
PAGE.c
POL.c
PUPS.c
RON.c
SLOG.c
SORT.c
URI.rl.c
URI.c
NET.c
HTTP.rl.c
HTTP.c
TCP.c
TLV.c
TTY.c
UDP.c
UTF8.c
ZINT.c
CURL.c
PACK.c
PATH.c
BSD.c
ryu/d2s.c
)

if (NOT WITH_CURL)
list(REMOVE_ITEM ABC_HEADERS CURL.h)
list(REMOVE_ITEM ABC_SOURCES CURL.c)
endif()

if (WITH_ROCKS)
list(APPEND ABC_HEADERS ROCK.h)
list(APPEND ABC_SOURCES ROCK.c)
endif()

# Minimal abc-core: no heavy deps — for standalone tools like spot
set(ABC_CORE_SOURCES ${ABC_SOURCES})
list(REMOVE_ITEM ABC_CORE_SOURCES CURL.c NET.c HTTP.c HTTP.rl.c TCP.c UDP.c POL.c PACK.c ROCK.c)
if (WASM_MODE)  # WASI has no sys/wait.h, termios.h
    list(REMOVE_ITEM ABC_CORE_SOURCES FILE.c ANSI.c PUPS.c)  # PUPS needs FILE.c (mmap)
endif()
add_library(abc-core STATIC ${ABC_HEADERS} ${ABC_CORE_SOURCES})
if (WASM_MODE)
    # users of abc-core include B.h etc., so they need the same defines
    target_compile_definitions(abc-core INTERFACE _WASI_EMULATED_MMAN _WASI_EMULATED_SIGNAL _WASI_EMULATED_PROCESS_CLOCKS _WASI_EMULATED_GETPID)
    target_link_libraries(abc-core INTERFACE wasi-emulated-mman wasi-emulated-signal wasi-emulated-process-clocks wasi-emulated-getpid)
    return()
endif()

# Full abc: curl is required above (unless WITH_CURL is OFF); lz4 and sodium are optional.
# BOOT-26A23Apv3i: without lz4 (Alpine: lz4-dev) PACK.c and the PACK
# tests drop out; without sodium (libsodium-dev) the SHA/NACL tests do.
# Everything else builds and tests.  ABC_SKIPPED names what was left out.
set(ABC_SKIPPED "")
if (NOT LZ4_FOUND)
    list(REMOVE_ITEM ABC_SOURCES PACK.c)
    list(REMOVE_ITEM ABC_HEADERS PACK.h)
    list(APPEND ABC_SKIPPED "PACK (no lz4)")
endif()
if (NOT sodium OR NOT SODIUM_INCLUDE_DIR)
    set(sodium "")
    list(APPEND ABC_SKIPPED "SHA NACL (no sodium)")
endif()
add_library(abc STATIC ${ABC_HEADERS} ${ABC_SOURCES})
if (WITH_CURL)
    target_link_libraries(abc CURL::libcurl)
else()
    list(APPEND ABC_SKIPPED "CURL (WITH_CURL OFF)")
endif()
if (LZ4_FOUND)
    target_include_directories(abc PUBLIC ${LZ4_INCLUDE_DIRS})
    target_link_directories(abc PUBLIC ${LZ4_LIBRARY_DIRS})
    target_link_libraries(abc ${LZ4_LIBRARIES})
endif()
if (sodium)
    target_include_directories(abc PUBLIC ${SODIUM_INCLUDE_DIR})
    target_link_libraries(abc ${sodium})
endif()
if (WITH_ROCKS)
    target_include_directories(abc PUBLIC ${ROCKSDB_INCLUDE_DIRS})
    target_link_directories(abc PUBLIC ${ROCKSDB_LIBRARY_DIRS})
    target_link_libraries(abc ${ROCKSDB_LIBRARIES})
endif()
if (ABC_SKIPPED)
    message(STATUS "abc: skipped: ${ABC_SKIPPED}")
endif()
file(WRITE ${CMAKE_BINARY_DIR}/skipped.txt "${ABC_SKIPPED}\n")

add_executable(ok64 OK.cli.c)
target_link_libraries(ok64 abc-core)

add_executable(lex LEX.cli.c)
target_link_libraries(lex abc-core)

add_executable(nfagrep nfagrep.c)
target_link_libraries(nfagrep abc-core)

add_subdirectory(test)

if (WITH_BENCH)
add_subdirectory(bench)
endif()

if (WITH_FUZZ)
add_subdirectory(fuzz)
endif()
