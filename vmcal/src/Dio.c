/* Dio.c — Virtual Digital I/O Driver (ADR-002 / Virtual-MCAL).
 *
 * RAM-backed implementation. Channel and port values are stored in
 * static arrays, providing deterministic read/write behavior.
 *
 * SPDX-License-Identifier: MIT OR Apache-2.0
 */

#include "Dio.h"
#include <string.h>

static Dio_LevelType     g_channels[DIO_MAX_CHANNELS];
static Dio_PortLevelType g_ports[DIO_MAX_PORTS];

Dio_LevelType Dio_ReadChannel(Dio_ChannelType ChannelId) {
    if (ChannelId >= DIO_MAX_CHANNELS) { return STD_LOW; }
    return g_channels[ChannelId];
}

void Dio_WriteChannel(Dio_ChannelType ChannelId, Dio_LevelType Level) {
    if (ChannelId >= DIO_MAX_CHANNELS) { return; }
    g_channels[ChannelId] = (Level != STD_LOW) ? STD_HIGH : STD_LOW;
}

Dio_PortLevelType Dio_ReadPort(Dio_PortType PortId) {
    if (PortId >= DIO_MAX_PORTS) { return 0u; }
    return g_ports[PortId];
}

void Dio_WritePort(Dio_PortType PortId, Dio_PortLevelType Level) {
    if (PortId >= DIO_MAX_PORTS) { return; }
    g_ports[PortId] = Level;
}

void Dio_FlipChannel(Dio_ChannelType ChannelId) {
    if (ChannelId >= DIO_MAX_CHANNELS) { return; }
    g_channels[ChannelId] = (g_channels[ChannelId] == STD_LOW) ? STD_HIGH : STD_LOW;
}
