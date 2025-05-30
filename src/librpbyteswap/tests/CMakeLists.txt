# librpbyteswap test suite
CMAKE_POLICY(SET CMP0048 NEW)
IF(POLICY CMP0063)
	# CMake 3.3: Enable symbol visibility presets for all
	# target types, including static libraries and executables.
	CMAKE_POLICY(SET CMP0063 NEW)
ENDIF(POLICY CMP0063)
PROJECT(librpbyteswap-tests LANGUAGES CXX)

# Top-level src directory
INCLUDE_DIRECTORIES(${CMAKE_CURRENT_SOURCE_DIR}/../..)
INCLUDE_DIRECTORIES(${CMAKE_CURRENT_BINARY_DIR}/../..)

# MSVC: Set /DEPENDENTLOADFLAG:0xA00 for unit tests.
IF(COMMAND SET_DEPENDENT_LOAD_FLAG_GTEST)
	SET_DEPENDENT_LOAD_FLAG_GTEST()
ENDIF(COMMAND SET_DEPENDENT_LOAD_FLAG_GTEST)

# ByteswapTest
ADD_EXECUTABLE(ByteswapTest
	ByteswapTest.cpp
	ByteswapTest_data.hpp)
TARGET_LINK_LIBRARIES(ByteswapTest PRIVATE rptest rpbyteswap)
DO_SPLIT_DEBUG(ByteswapTest)
SET_WINDOWS_SUBSYSTEM(ByteswapTest CONSOLE)
SET_WINDOWS_ENTRYPOINT(ByteswapTest wmain OFF)
ADD_TEST(NAME ByteswapTest COMMAND ByteswapTest --gtest_brief --gtest_filter=-*benchmark*)

# BitstuffTest
ADD_EXECUTABLE(BitstuffTest BitstuffTest.cpp)
TARGET_LINK_LIBRARIES(BitstuffTest PRIVATE rptest rpbyteswap)
DO_SPLIT_DEBUG(BitstuffTest)
SET_WINDOWS_SUBSYSTEM(BitstuffTest CONSOLE)
SET_WINDOWS_ENTRYPOINT(BitstuffTest wmain OFF)
ADD_TEST(NAME BitstuffTest COMMAND BitstuffTest --gtest_brief)
