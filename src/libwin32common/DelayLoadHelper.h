/***************************************************************************
 * ROM Properties Page shell extension. (libwin32common)                   *
 * DelayLoadHelper.h: DelayLoad helper functions and macros.               *
 *                                                                         *
 * Copyright (c) 2017-2023 by David Korth.                                 *
 * SPDX-License-Identifier: GPL-2.0-or-later                               *
 ***************************************************************************/

#pragma once

#if !defined(_WIN32) || !defined(_MSC_VER)
#  error DelayLoadHelper.h is MSVC and Win32 only at the moment.
#endif

// MSVC: Exception handling for /DELAYLOAD.
// TODO: Check for /DELAYLOAD usage.
#include "libwin32common/RpWin32_sdk.h"
#include <delayimp.h>
#include <excpt.h>
#include <errno.h>

// This isn't defined if compiling with MSVC 2012+
// when using the Windows 7 SDK.
#ifndef FACILITY_VISUALCPP
#  define FACILITY_VISUALCPP 109
#endif

#ifdef __cplusplus
extern "C" {
#endif

/**
 * DELAYLOAD_FILTER_FUNCTION_IMPL(): Implementation of the DelayLoad filter function.
 * Don't use this directly; it's used by DELAYLOAD_TEST_FUNCTION_IMPL*().
 * @param fn Function name.
 */
#define DELAYLOAD_FILTER_FUNCTION_IMPL(fn) \
static LONG WINAPI DelayLoad_filter_##fn(DWORD exceptioncode) { \
	if (exceptioncode == VcppException(ERROR_SEVERITY_ERROR, ERROR_MOD_NOT_FOUND)) { \
		return EXCEPTION_EXECUTE_HANDLER; \
	} \
	return EXCEPTION_CONTINUE_SEARCH; \
}

/**
 * DELAYLOAD_TEST_FUNCTION_IMPL0(): Implementation of the DelayLoad test function.
 * Creates a test function named DelayLoad_test_##fn, which should
 * be called prior to using the DLL.
 * @param fn Function name. Must take 0 arguments.
 */
#define DELAYLOAD_TEST_FUNCTION_IMPL0(fn) \
DELAYLOAD_FILTER_FUNCTION_IMPL(fn) \
static int DelayLoad_test_##fn(void) { \
	static int success = 0; \
	if (!success) { \
		__try { \
			(void)fn(); \
		} __except (DelayLoad_filter_##fn(GetExceptionCode())) { \
			return -ENOTSUP; \
		} \
		success = 1; \
	} \
	return 0; \
}

/**
 * DELAYLOAD_TEST_FUNCTION_IMPL2(): Implementation of the DelayLoad test function.
 * Creates a test function named DelayLoad_test_##fn, which should
 * be called prior to using the DLL.
 * @param fn Function name. Must take 1 argument.
 * @param arg0
 */
#define DELAYLOAD_TEST_FUNCTION_IMPL1(fn, arg0) \
DELAYLOAD_FILTER_FUNCTION_IMPL(fn) \
static int DelayLoad_test_##fn(void) { \
	static int success = 0; \
	if (!success) { \
		__try { \
			(void)fn(arg0); \
		} __except (DelayLoad_filter_##fn(GetExceptionCode())) { \
			return -ENOTSUP; \
		} \
		success = 1; \
	} \
	return 0; \
}

/**
 * DELAYLOAD_TEST_FUNCTION_IMPL2(): Implementation of the DelayLoad test function.
 * Creates a test function named DelayLoad_test_##fn, which should
 * be called prior to using the DLL.
 * @param fn Function name. Must take 2 arguments.
 * @param arg0
 * @param arg1
 */
#define DELAYLOAD_TEST_FUNCTION_IMPL2(fn, arg0, arg1) \
DELAYLOAD_FILTER_FUNCTION_IMPL(fn) \
static int DelayLoad_test_##fn(void) { \
	static int success = 0; \
	if (!success) { \
		__try { \
			(void)fn(arg0, arg1); \
		} __except (DelayLoad_filter_##fn(GetExceptionCode())) { \
			return -ENOTSUP; \
		} \
		success = 1; \
	} \
	return 0; \
}

/**
 * Explicit LoadLibrary() for delay-load.
 * Used by our own DLL loading functions, so the DLL whitelist check is skipped.
 * @param pdli Delay-load info
 * @return Library handle, or NULL on error.
 */
HMODULE WINAPI rp_LoadLibrary(LPCSTR pszModuleName);

#ifdef __cplusplus
}
#endif
