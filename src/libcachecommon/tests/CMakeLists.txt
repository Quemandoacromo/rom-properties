# libcachecommon test suite
PROJECT(libcachecommon-tests LANGUAGES CXX)

# Top-level src directory
INCLUDE_DIRECTORIES(${CMAKE_CURRENT_SOURCE_DIR}/../..)
INCLUDE_DIRECTORIES(${CMAKE_CURRENT_BINARY_DIR}/../..)

# MSVC: Set /DEPENDENTLOADFLAG:0xA00 for unit tests.
IF(COMMAND SET_DEPENDENT_LOAD_FLAG_GTEST)
	SET_DEPENDENT_LOAD_FLAG_GTEST()
ENDIF(COMMAND SET_DEPENDENT_LOAD_FLAG_GTEST)

# LibCacheCommon::filterCacheKey() test.
ADD_EXECUTABLE(FilterCacheKeyTest FilterCacheKeyTest.cpp)
TARGET_LINK_LIBRARIES(FilterCacheKeyTest PRIVATE rptest cachecommon rptext)
IF(WIN32)
	TARGET_LINK_LIBRARIES(FilterCacheKeyTest PRIVATE win32common)
ELSE(WIN32)
	TARGET_LINK_LIBRARIES(FilterCacheKeyTest PRIVATE unixcommon)
ENDIF(WIN32)
DO_SPLIT_DEBUG(FilterCacheKeyTest)
SET_WINDOWS_SUBSYSTEM(FilterCacheKeyTest CONSOLE)
SET_WINDOWS_ENTRYPOINT(FilterCacheKeyTest wmain OFF)
ADD_TEST(NAME FilterCacheKeyTest COMMAND FilterCacheKeyTest --gtest_brief)

# Delay-load shell32.dll and ole32.dll to prevent a performance penalty due to gdi32.dll.
# Reference: https://randomascii.wordpress.com/2018/12/03/a-not-called-function-can-cause-a-5x-slowdown/
# This is also needed when disabling direct Win32k syscalls,
# since loading gdi32.dll will crash in that case.
# NOTE: ole32.dll is indirectly linked through libwin32common. (CoTaskMemFree())
INCLUDE(../../libwin32common/DelayLoadHelper.cmake)
ADD_DELAYLOAD_FLAGS(FilterCacheKeyTest shell32.dll ole32.dll)
