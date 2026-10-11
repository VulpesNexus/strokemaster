# Portable build of StrokeMaster's host-independent geometry cores and their tests, for g++ or
# clang on any platform. It needs no InDesign SDK and no MSVC: the cores are pure C++17 over the
# vendored Clipper2. run.py remains the Windows (MSVC) entry point; see tests/README.md.
#
#   cmake -S tests/native-repaint -B build/portable-cores
#   cmake --build build/portable-cores
#   ctest --test-dir build/portable-cores --output-on-failure
cmake_minimum_required(VERSION 3.16)
project(StrokeMasterGeometryCores LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(ISE_WERROR "Treat warnings as errors, like run.py's /WX" ON)

set(ISE_REPO ${CMAKE_CURRENT_SOURCE_DIR}/../..)
set(ISE_SOURCE ${ISE_REPO}/plugin/source)

# The counterpart of run.py's /W4 /WX. Clang reports the capture of the constant `precision`
# in IseRegionCore.cpp as unneeded; the source keeps it so the MSVC build is unchanged.
set(ISE_WARNINGS -Wall -Wextra)
if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    list(APPEND ISE_WARNINGS -Wno-unused-lambda-capture)
endif()
if(ISE_WERROR)
    list(APPEND ISE_WARNINGS -Werror)
endif()

# IseMiterCore.h, IsePatternCore.h, and IseRegionCore.h with Clipper2 (built with USINGZ by
# IseClipper2.cpp, as in the plugin).
add_executable(MiterCoreTests
    MiterCoreTests.cpp
    ${ISE_SOURCE}/IseRegionCore.cpp
    ${ISE_SOURCE}/IseClipper2.cpp)
target_include_directories(MiterCoreTests PRIVATE ${ISE_REPO}/plugin/third_party/clipper2/include)
target_compile_options(MiterCoreTests PRIVATE ${ISE_WARNINGS})

# IseStrokeBehindCore.h, header-only.
add_executable(StrokeBehindCoreTests StrokeBehindCoreTests.cpp)
target_compile_options(StrokeBehindCoreTests PRIVATE ${ISE_WARNINGS})

enable_testing()
add_test(NAME MiterCoreTests COMMAND MiterCoreTests)
add_test(NAME StrokeBehindCoreTests COMMAND StrokeBehindCoreTests)
