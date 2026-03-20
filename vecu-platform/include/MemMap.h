/* MemMap.h -- Universal no-op MemMap for host (vECU) compilation.
 *
 * On the real target this file maps BSW memory sections to linker
 * sections via #pragma directives. For host compilation all section
 * pragmas are no-ops -- the host linker handles placement automatically.
 *
 * AUTOSAR design: MemMap.h is included multiple times (once per
 * START_SEC / STOP_SEC pair). It intentionally has NO include guard.
 * Each inclusion simply undefines the section macro -- a no-op.
 *
 * SPDX-License-Identifier: MIT OR Apache-2.0
 */

/* Intentionally no include guard -- AUTOSAR MemMap is multi-inclusion. */
