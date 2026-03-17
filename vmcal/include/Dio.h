/* Dio.h — Virtual Digital I/O Driver API (ADR-002 / Virtual-MCAL).
 *
 * RAM-backed drop-in replacement for Dio_* MCAL driver.
 *
 * SPDX-License-Identifier: MIT OR Apache-2.0
 */

#ifndef VMCAL_DIO_H
#define VMCAL_DIO_H

#include "Std_Types.h"

typedef uint16 Dio_ChannelType;
typedef uint8  Dio_PortType;
typedef uint8  Dio_LevelType;
typedef uint8  Dio_PortLevelType;

#define DIO_MAX_CHANNELS 256u
#define DIO_MAX_PORTS     32u

#define STD_LOW   ((Dio_LevelType)0x00u)
#define STD_HIGH  ((Dio_LevelType)0x01u)

Dio_LevelType     Dio_ReadChannel(Dio_ChannelType ChannelId);
void              Dio_WriteChannel(Dio_ChannelType ChannelId, Dio_LevelType Level);
Dio_PortLevelType Dio_ReadPort(Dio_PortType PortId);
void              Dio_WritePort(Dio_PortType PortId, Dio_PortLevelType Level);
void              Dio_FlipChannel(Dio_ChannelType ChannelId);

#endif /* VMCAL_DIO_H */
