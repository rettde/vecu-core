/* SwcSensor.c — Sensor SWC: reads vehicle speed and brake from CAN.
 *
 * SPDX-License-Identifier: MIT OR Apache-2.0
 */

#include "Rte_SwcSensor.h"

static uint32 g_vehicle_speed = 0;
static uint32 g_brake_active  = 0;

void SwcSensor_Init(void) {
    g_vehicle_speed = 0;
    g_brake_active  = 0;
}

void SwcSensor_MainFunction(void) {
    Rte_Read_VehicleSpeed(&g_vehicle_speed);
    Rte_Read_BrakeActive(&g_brake_active);
}

uint32 SwcSensor_GetVehicleSpeed(void) { return g_vehicle_speed; }
uint32 SwcSensor_GetBrakeActive(void)  { return g_brake_active; }
