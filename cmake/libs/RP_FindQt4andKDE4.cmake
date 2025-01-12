# Find Qt4 and KDE4.
MACRO(FIND_QT4_AND_KDE4)
	# NOTE: KDE4's .cmake files overwrite the per-configuration CFLAGS/CXXFLAGS variables.
	# We'll need to preserve them here.
	FOREACH(_config RELWITHDEBINFO RELEASE DEBUG DEBUGFULL PROFILE DEBIAN)
		SET(OLD_CMAKE_C_FLAGS_${_config}   "${CMAKE_C_FLAGS_${_config}}")
		SET(OLD_CMAKE_CXX_FLAGS_${_config} "${CMAKE_CXX_FLAGS_${_config}}")
	ENDFOREACH(_config)

	SET(ENV{QT_SELECT} qt4)
	SET(QT_DEFAULT_MAJOR_VERSION 4)
	SET(QT_NO_CREATE_VERSIONLESS_TARGETS TRUE)

	SET(QT4_NO_LINK_QTMAIN 1)
	FIND_PACKAGE(Qt4 4.6.0 ${REQUIRE_KDE4} COMPONENTS QtCore QtGui QtDBus)
	IF(QT4_FOUND)
		# Make sure KDE4's CMake files don't create an uninstall rule.
		SET(_kde4_uninstall_rule_created TRUE)

		# Find KDE4.
		# - 4.7.0: KMessageWidget
		FIND_PACKAGE(KDE4 4.7.0 ${REQUIRE_KDE4})
		IF(NOT KDE4_FOUND)
			# KDE4 not found.
			SET(BUILD_KDE4 OFF CACHE INTERNAL "Build the KDE4 plugin." FORCE)
		ENDIF(NOT KDE4_FOUND)
		IF(PLUGIN_INSTALL_DIR)
			SET(KDE4_PLUGIN_INSTALL_DIR "${PLUGIN_INSTALL_DIR}")
			UNSET(PLUGIN_INSTALL_DIR)
			UNSET(PLUGIN_INSTALL_DIR CACHE)
		ELSE(PLUGIN_INSTALL_DIR)
			# PLUGIN_INSTALL_DIR might not be set in some cases.
			# Use KDE4's documented default value.
			SET(KDE4_PLUGIN_INSTALL_DIR "${KDE4_LIB_INSTALL_DIR}/kde4")
		ENDIF(PLUGIN_INSTALL_DIR)

		# Get rid of the explicit C90 setting.
		STRING(REPLACE "-std=iso9899:1990" "" CMAKE_C_FLAGS "${CMAKE_C_FLAGS}")

		# Get rid of -fno-exceptions.
		STRING(REPLACE "-fno-exceptions" "" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")

		IF(QT_QTDBUS_LIBRARY)
			SET(HAVE_QtDBus 1)
			IF(ENABLE_ACHIEVEMENTS)
				# QtDBus is used for notifications.
				# TODO: Make notifications optional.
				SET(HAVE_QtDBus_NOTIFY 1)
			ENDIF(ENABLE_ACHIEVEMENTS)
		ENDIF(QT_QTDBUS_LIBRARY)
	ELSE(QT4_FOUND)
		# Qt4 not found.
		SET(BUILD_KDE4 OFF CACHE INTERNAL "Build the KDE4 plugin." FORCE)
	ENDIF(QT4_FOUND)

	# NOTE: KDE4's .cmake files overwrite the per-configuration CFLAGS/CXXFLAGS variables.
	# We'll need to preserve them here.
	FOREACH(_config RELWITHDEBINFO RELEASE DEBUG DEBUGFULL PROFILE DEBIAN)
		SET(CMAKE_C_FLAGS_${_config}   "${OLD_CMAKE_C_FLAGS_${_config}}")
		SET(CMAKE_CXX_FLAGS_${_config} "${OLD_CMAKE_CXX_FLAGS_${_config}}")

		UNSET(OLD_CMAKE_C_FLAGS_${_config})
		UNSET(OLD_CMAKE_CXX_FLAGS_${_config})
	ENDFOREACH(_config)
ENDMACRO(FIND_QT4_AND_KDE4)
