/* etl_profile.h — ETL configuration for vecu single-threaded build.
 *
 * Based on OpenBSW's referenceApp/etl_profile/etl_profile.h but
 * configured for a deterministic, single-threaded vECU environment.
 *
 * SPDX-License-Identifier: MIT OR Apache-2.0
 */

#ifndef ETL_PROFILE_H
#define ETL_PROFILE_H

#define ETL_TARGET_DEVICE_GENERIC
#define ETL_TARGET_OS_NONE

#define ETL_NO_STL

#include <initializer_list>
#include <tuple>

#define ETL_FORCE_STD_INITIALIZER_LIST

#define ETL_NO_LIBC_WCHAR_H

#define ETL_USING_BUILTIN_MEMCPY  0
#define ETL_USING_BUILTIN_MEMMOVE 0
#define ETL_USING_BUILTIN_MEMSET  0
#define ETL_USING_BUILTIN_MEMCMP  0
#define ETL_USING_BUILTIN_MEMCHR  0

#define ETL_MINIMAL_ERRORS
#define ETL_USE_ASSERT_FUNCTION

#define ETL_CHRONO_HIGH_RESOLUTION_CLOCK_DURATION etl::chrono::nanoseconds
#define ETL_CHRONO_SYSTEM_CLOCK_DURATION          etl::chrono::microseconds
#define ETL_CHRONO_STEADY_CLOCK_DURATION          etl::chrono::seconds

#endif /* ETL_PROFILE_H */
