/* vecu_status.h — Status codes for the vECU ABI (ADR-001).
 *
 * Must stay in sync with vecu_abi::status in lib.rs.
 * Convention: 0 = success, negative = error.
 *
 * SPDX-License-Identifier: MIT OR Apache-2.0
 */

#ifndef VECU_STATUS_H
#define VECU_STATUS_H

#define VECU_OK               0   /* Success.                               */
#define VECU_VERSION_MISMATCH (-1) /* ABI version not supported by plugin.  */
#define VECU_INVALID_ARGUMENT (-2) /* Null or invalid argument.             */
#define VECU_INIT_FAILED      (-3) /* Module initialisation failed.         */
#define VECU_NOT_SUPPORTED    (-4) /* Requested capability not supported.   */
#define VECU_MODULE_ERROR     (-5) /* Generic module-level error.           */

#endif /* VECU_STATUS_H */
