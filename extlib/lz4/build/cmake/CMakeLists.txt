# CMake support for LZ4
#
# To the extent possible under law, the author(s) have dedicated all
# copyright and related and neighboring rights to this software to
# the public domain worldwide. This software is distributed without
# any warranty.
#
# For details, see <http://creativecommons.org/publicdomain/zero/1.0/>.


cmake_minimum_required(VERSION 3.5)

set(LZ4_TOP_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../..")

# Parse version information
function(parse_lz4_version VERSION_TYPE)
    file(STRINGS "${LZ4_TOP_SOURCE_DIR}/lib/lz4.h" version_line REGEX "^#define LZ4_VERSION_${VERSION_TYPE} +([0-9]+).*$")
    string(REGEX REPLACE "^#define LZ4_VERSION_${VERSION_TYPE} +([0-9]+).*$" "\\1" version_number "${version_line}")
    set(LZ4_VERSION_${VERSION_TYPE} ${version_number} PARENT_SCOPE)
endfunction()

foreach(version_type IN ITEMS MAJOR MINOR RELEASE)
    parse_lz4_version(${version_type})
endforeach()

set(LZ4_VERSION_STRING "${LZ4_VERSION_MAJOR}.${LZ4_VERSION_MINOR}.${LZ4_VERSION_RELEASE}")
mark_as_advanced(LZ4_VERSION_STRING LZ4_VERSION_MAJOR LZ4_VERSION_MINOR LZ4_VERSION_RELEASE)

message(STATUS "Creating build script for LZ4 version: ${LZ4_VERSION_STRING}")

project(LZ4 VERSION ${LZ4_VERSION_STRING} LANGUAGES C)


IF(0) # rom-properties: Always disabled.
option(LZ4_BUILD_CLI "Build lz4 program" ON)
ENDIF(0) # rom-properties
set(LZ4_BUILD_CLI OFF)


# Determine if LZ4 is being built as part of another project.
# If LZ4 is bundled in another project, we don't want to install anything.
# Default behavior can be overridden by setting the LZ4_BUNDLED_MODE variable.
if(NOT DEFINED LZ4_BUNDLED_MODE)
  get_directory_property(LZ4_IS_SUBPROJECT PARENT_DIRECTORY)
  if(LZ4_IS_SUBPROJECT)
    set(LZ4_BUNDLED_MODE ON)
  else()
    set(LZ4_BUNDLED_MODE OFF)
  endif()
endif()
mark_as_advanced(LZ4_BUNDLED_MODE)

# CPack
if(NOT LZ4_BUNDLED_MODE AND NOT CPack_CMake_INCLUDED)
  set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "LZ4 compression library")
  set(CPACK_PACKAGE_DESCRIPTION_FILE "${LZ4_TOP_SOURCE_DIR}/README.md")
  set(CPACK_RESOURCE_FILE_LICENSE "${LZ4_TOP_SOURCE_DIR}/LICENSE")
  set(CPACK_PACKAGE_VERSION_MAJOR ${LZ4_VERSION_MAJOR})
  set(CPACK_PACKAGE_VERSION_MINOR ${LZ4_VERSION_MINOR})
  set(CPACK_PACKAGE_VERSION_PATCH ${LZ4_VERSION_RELEASE})
  include(CPack)
endif(NOT LZ4_BUNDLED_MODE AND NOT CPack_CMake_INCLUDED)

# Allow people to choose whether to build shared or static libraries
# via the BUILD_SHARED_LIBS option unless we are in bundled mode, in
# which case we always use static libraries.
IF(0) # rom-properties: Disable this; we'll control it ourserlves in extlib.
include(CMakeDependentOption)
CMAKE_DEPENDENT_OPTION(BUILD_SHARED_LIBS "Build shared libraries" ON "NOT LZ4_BUNDLED_MODE" OFF)
CMAKE_DEPENDENT_OPTION(BUILD_STATIC_LIBS "Build static libraries" OFF "BUILD_SHARED_LIBS" ON)
ENDIF(0) # rom-properties

if(NOT BUILD_SHARED_LIBS AND NOT BUILD_STATIC_LIBS)
  message(FATAL_ERROR "Both BUILD_SHARED_LIBS and BUILD_STATIC_LIBS have been disabled")
endif()

set(LZ4_LIB_SOURCE_DIR "${LZ4_TOP_SOURCE_DIR}/lib")
set(LZ4_PROG_SOURCE_DIR "${LZ4_TOP_SOURCE_DIR}/programs")

include_directories("${LZ4_LIB_SOURCE_DIR}")

# CLI sources
file(GLOB LZ4_SOURCES
     "${LZ4_LIB_SOURCE_DIR}/*.c")
file(GLOB LZ4_CLI_SOURCES
     "${LZ4_PROG_SOURCE_DIR}/*.c")
list(APPEND LZ4_CLI_SOURCES ${LZ4_SOURCES}) # LZ4_CLI always use liblz4 sources directly.

# Whether to use position independent code for the static library.  If
# we're building a shared library this is ignored and PIC is always
# used.
IF(0) # rom-properties: Always enable this.
option(LZ4_POSITION_INDEPENDENT_LIB "Use position independent code for static library (if applicable)" ON)
ENDIF(0) # rom-properties
set(LZ4_POSITION_INDEPENDENT_LIB ON)

# rom-properties
IF(MSVC)
	# Append 'd' to the DLL filename for debug builds.
	SET(CMAKE_DEBUG_POSTFIX "d")
ENDIF(MSVC)

# liblz4
include(GNUInstallDirs)
set(LZ4_LIBRARIES_BUILT)
if(BUILD_SHARED_LIBS)
  IF(WIN32)
    # rom-properties: Add the Win32 resource file.
    SET(LZ4_RC_FILE "${LZ4_TOP_SOURCE_DIR}/build/VS2022/liblz4-dll/liblz4-dll.rc")
    SET(LZ4_SOURCES ${LZ4_SOURCES} ${LZ4_RC_FILE})
  ENDIF(WIN32)
  add_library(lz4_shared SHARED ${LZ4_SOURCES})
  target_include_directories(lz4_shared
    PUBLIC $<BUILD_INTERFACE:${LZ4_LIB_SOURCE_DIR}>
    INTERFACE $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)
  set_target_properties(lz4_shared PROPERTIES
    OUTPUT_NAME lz4
    SOVERSION "${LZ4_VERSION_MAJOR}"
    VERSION "${LZ4_VERSION_STRING}")
  if(MSVC)
    # rom-properties: Add LZ4_DLL_IMPORT=1 in interface flags.
    target_compile_definitions(lz4_shared
	PRIVATE -DLZ4_DLL_EXPORT=1
	INTERFACE -DLZ4_DLL_IMPORT=1
	)
  endif()
  list(APPEND LZ4_LIBRARIES_BUILT lz4_shared)
  # rom-properties: Add the include directory, split debug, etc.
  target_include_directories(lz4_shared PUBLIC "${LZ4_LIB_SOURCE_DIR}")
  DO_SPLIT_DEBUG(lz4_shared)
  SET_WINDOWS_SUBSYSTEM(lz4_shared WIN32)
endif()
if(BUILD_STATIC_LIBS)
  # rom-properties: Hide all symbols so they're not exported by libromdata.
  CMAKE_POLICY(SET CMP0063 NEW)
  SET(CMAKE_C_VISIBILITY_PRESET "hidden")
  SET(CMAKE_VISIBILITY_INLINES_HIDDEN ON)

  set(STATIC_LIB_NAME lz4)
  if (MSVC AND BUILD_SHARED_LIBS)
    set(STATIC_LIB_NAME lz4_static)
  endif()
  add_library(lz4_static STATIC ${LZ4_SOURCES})
  target_include_directories(lz4_static
    PUBLIC $<BUILD_INTERFACE:${LZ4_LIB_SOURCE_DIR}>
    INTERFACE $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)
  set_target_properties(lz4_static PROPERTIES
    OUTPUT_NAME ${STATIC_LIB_NAME}
    POSITION_INDEPENDENT_CODE ${LZ4_POSITION_INDEPENDENT_LIB})
  list(APPEND LZ4_LIBRARIES_BUILT lz4_static)
  # rom-properties: Hide all symbols so they're not exported by libromdata.
  target_compile_definitions(lz4_static PUBLIC LZ4LIB_VISIBILITY=)
endif()

# Add unified target.
add_library(lz4 INTERFACE)
list(APPEND LZ4_LIBRARIES_BUILT lz4)
if(BUILD_SHARED_LIBS)
  target_link_libraries(lz4 INTERFACE lz4_shared)
  set(LZ4_LINK_LIBRARY lz4_shared) # rom-properties
else()
  target_link_libraries(lz4 INTERFACE lz4_static)
  set(LZ4_LINK_LIBRARY lz4_static) # rom-properties
endif()

# xxhash namespace
if(BUILD_SHARED_LIBS)
  target_compile_definitions(lz4_shared PRIVATE
    XXH_NAMESPACE=LZ4_)
endif()
if(BUILD_STATIC_LIBS)
  target_compile_definitions(lz4_static PRIVATE
    XXH_NAMESPACE=LZ4_)
endif()

# lz4
if (LZ4_BUILD_CLI)
  set(LZ4_PROGRAMS_BUILT lz4cli)
  add_executable(lz4cli ${LZ4_CLI_SOURCES})
  set_target_properties(lz4cli PROPERTIES OUTPUT_NAME lz4)
endif()

# Extra warning flags
if(MSVC)
  set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} /W4")
else()
  include (CheckCCompilerFlag)
  foreach (flag
      # GCC-style
      -pedantic-errors
      -Wall
      -Wextra
      -Wundef
      -Wcast-qual
      -Wcast-align
      -Wshadow
      -Wswitch-enum
      -Wdeclaration-after-statement
      -Wstrict-prototypes
      -Wpointer-arith)

    # Because https://gcc.gnu.org/wiki/FAQ#wnowarning
    string(REGEX REPLACE "\\-Wno\\-(.+)" "-W\\1" flag_to_test "${flag}")
    string(REGEX REPLACE "[^a-zA-Z0-9]+" "_" test_name "CFLAG_${flag_to_test}")

    check_c_compiler_flag("${ADD_COMPILER_FLAGS_PREPEND} ${flag_to_test}" ${test_name})

    if(${test_name})
      set(CMAKE_C_FLAGS_DEBUG "${flag} ${CMAKE_C_FLAGS_DEBUG}")
    endif()

    unset(test_name)
    unset(flag_to_test)
  endforeach (flag)
endif()


if(NOT LZ4_BUNDLED_MODE)
  install(TARGETS ${LZ4_PROGRAMS_BUILT}
    BUNDLE	DESTINATION "${CMAKE_INSTALL_BINDIR}"
    RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}")
  install(TARGETS ${LZ4_LIBRARIES_BUILT}
    EXPORT lz4Targets
    LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}"
    ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}"
    RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}")
  install(FILES
    "${LZ4_LIB_SOURCE_DIR}/lz4.h"
    "${LZ4_LIB_SOURCE_DIR}/lz4hc.h"
    "${LZ4_LIB_SOURCE_DIR}/lz4frame.h"
    "${LZ4_LIB_SOURCE_DIR}/lz4file.h"
    DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}")
  install(FILES "${LZ4_PROG_SOURCE_DIR}/lz4.1"
    DESTINATION "${CMAKE_INSTALL_MANDIR}/man1")
  install(FILES "${CMAKE_CURRENT_BINARY_DIR}/liblz4.pc"
    DESTINATION "${CMAKE_INSTALL_LIBDIR}/pkgconfig")

  include(CMakePackageConfigHelpers)
  write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/lz4ConfigVersion.cmake"
    VERSION ${LZ4_VERSION_STRING}
    COMPATIBILITY SameMajorVersion)

  set(LZ4_PKG_INSTALLDIR "${CMAKE_INSTALL_LIBDIR}/cmake/lz4")
  configure_package_config_file(
    "${CMAKE_CURRENT_LIST_DIR}/lz4Config.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/lz4Config.cmake"
    INSTALL_DESTINATION ${LZ4_PKG_INSTALLDIR})
  export(EXPORT lz4Targets
    FILE ${CMAKE_CURRENT_BINARY_DIR}/lz4Targets.cmake
    NAMESPACE LZ4::)

  install(EXPORT lz4Targets
    FILE lz4Targets.cmake
    NAMESPACE LZ4::
    DESTINATION ${LZ4_PKG_INSTALLDIR})
  install(FILES
      ${CMAKE_CURRENT_BINARY_DIR}/lz4Config.cmake
      ${CMAKE_CURRENT_BINARY_DIR}/lz4ConfigVersion.cmake
    DESTINATION ${LZ4_PKG_INSTALLDIR})

  # install lz4cat and unlz4 symlinks on Unix systems
  if(UNIX AND LZ4_BUILD_CLI)
    foreach(cli_tool IN ITEMS lz4cat unlz4)
      # Create a custom target for the symlink creation
      add_custom_target("create_${cli_tool}_symlink" ALL
        COMMAND ${CMAKE_COMMAND} -E create_symlink
          $<TARGET_FILE_NAME:lz4cli> ${cli_tool}
        COMMENT "Creating symlink for ${cli_tool}"
        VERBATIM)

      # Install the symlink into the binary installation directory
      install(FILES "${CMAKE_CURRENT_BINARY_DIR}/${cli_tool}"
        DESTINATION ${CMAKE_INSTALL_BINDIR}
        RENAME ${cli_tool})
    endforeach()

    # create manpage aliases
    foreach(f lz4cat unlz4)
      file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/${f}.1" ".so man1/lz4.1\n")
      install(FILES "${CMAKE_CURRENT_BINARY_DIR}/${f}.1"
        DESTINATION "${CMAKE_INSTALL_MANDIR}/man1")
    endforeach()
  endif(UNIX AND LZ4_BUILD_CLI)
endif(NOT LZ4_BUNDLED_MODE)

# rom-properties: Install the LZ4 shared library in the package.
IF(NOT SKIP_INSTALL_LIBRARIES)
	INSTALL(TARGETS ${LZ4_LINK_LIBRARY}
		#EXPORT lz4
		RUNTIME DESTINATION "${INSTALL_BIN_DIR}"
		#ARCHIVE DESTINATION "${INSTALL_LIB_DIR}"
		FRAMEWORK DESTINATION "${INSTALL_LIB_DIR}")

	IF(INSTALL_DEBUG AND BUILD_SHARED_LIBS)
		# FIXME: Generator expression $<TARGET_PROPERTY:${_target},PDB> didn't work with CPack-3.6.1.
		# TODO: Test on MinGW.
		# FIXME: Generator PDB expression doesn't have the debug postfix for some reason,
		# so cpack fails in debug builds if we get the PDB property.
		INCLUDE(DirInstallPaths)
		SET(PDB_FILENAME_D "$<TARGET_FILE_DIR:${LZ4_LINK_LIBRARY}>/lz4${CMAKE_DEBUG_POSTFIX}.pdb")
		SET(PDB_FILENAME_R "$<TARGET_FILE_DIR:${LZ4_LINK_LIBRARY}>/lz4${CMAKE_RELEASE_POSTFIX}.pdb")
		INSTALL(FILES "${PDB_FILENAME_D}" "${PDB_FILENAME_R}"
			DESTINATION "${DIR_INSTALL_DLL_DEBUG}"
			COMPONENT "debug"
			OPTIONAL
			)
		UNSET(PDB_FILENAME_D)
		UNSET(PDB_FILENAME_R)
	ENDIF(INSTALL_DEBUG AND BUILD_SHARED_LIBS)
ENDIF(NOT SKIP_INSTALL_LIBRARIES)

# pkg-config
set(PREFIX "${CMAKE_INSTALL_PREFIX}")

if("${CMAKE_INSTALL_FULL_LIBDIR}" STREQUAL "${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_LIBDIR}")
  set(LIBDIR "\${prefix}/${CMAKE_INSTALL_LIBDIR}")
else()
  set(LIBDIR "${CMAKE_INSTALL_FULL_LIBDIR}")
endif()

if("${CMAKE_INSTALL_FULL_INCLUDEDIR}" STREQUAL "${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_INCLUDEDIR}")
  set(INCLUDEDIR "\${prefix}/${CMAKE_INSTALL_INCLUDEDIR}")
else()
  set(INCLUDEDIR "${CMAKE_INSTALL_FULL_INCLUDEDIR}")
endif()

# for liblz4.pc substitution
set(VERSION ${LZ4_VERSION_STRING})
configure_file(${LZ4_LIB_SOURCE_DIR}/liblz4.pc.in liblz4.pc @ONLY)
