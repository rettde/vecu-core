/* ComM.c — Communication Manager (simplified stub).
 *
 * Minimal AUTOSAR ComM state machine for Level-3 vECU.
 * Per-user mode request, per-channel arbitration (highest wins).
 *
 * SPDX-License-Identifier: MIT OR Apache-2.0
 */

#include "ComM.h"
#include <string.h>

static boolean       g_initialized = FALSE;
static ComM_ModeType g_user_request[COMM_MAX_USERS];
static boolean       g_channel_allowed[COMM_MAX_CHANNELS];
static ComM_ModeType g_current_mode = COMM_NO_COMMUNICATION;
static uint8         g_num_users    = 0;
static uint8         g_num_channels = 0;

void ComM_Init(const ComM_ConfigType* ConfigPtr) {
    memset(g_user_request, COMM_NO_COMMUNICATION, sizeof(g_user_request));
    memset(g_channel_allowed, FALSE, sizeof(g_channel_allowed));
    g_current_mode = COMM_NO_COMMUNICATION;
    if (ConfigPtr != NULL) {
        g_num_users    = (ConfigPtr->numUsers < COMM_MAX_USERS) ? ConfigPtr->numUsers : COMM_MAX_USERS;
        g_num_channels = (ConfigPtr->numChannels < COMM_MAX_CHANNELS) ? ConfigPtr->numChannels : COMM_MAX_CHANNELS;
    } else {
        g_num_users    = 1;
        g_num_channels = 1;
    }
    g_initialized = TRUE;
}

void ComM_DeInit(void) {
    g_current_mode = COMM_NO_COMMUNICATION;
    g_initialized = FALSE;
}

Std_ReturnType ComM_RequestComMode(ComM_UserHandleType User, ComM_ModeType ComMode) {
    if (!g_initialized || User >= g_num_users) { return E_NOT_OK; }
    g_user_request[User] = ComMode;
    return E_OK;
}

Std_ReturnType ComM_GetCurrentComMode(ComM_UserHandleType User, ComM_ModeType* ComMode) {
    if (!g_initialized || ComMode == NULL || User >= g_num_users) { return E_NOT_OK; }
    *ComMode = g_current_mode;
    return E_OK;
}

void ComM_CommunicationAllowed(uint8 Channel, boolean Allowed) {
    if (Channel < COMM_MAX_CHANNELS) {
        g_channel_allowed[Channel] = Allowed;
    }
}

void ComM_MainFunction(void) {
    if (!g_initialized) { return; }

    ComM_ModeType highest = COMM_NO_COMMUNICATION;
    uint8 i;
    for (i = 0; i < g_num_users; i++) {
        if (g_user_request[i] > highest) {
            highest = g_user_request[i];
        }
    }

    boolean any_allowed = FALSE;
    for (i = 0; i < g_num_channels; i++) {
        if (g_channel_allowed[i]) {
            any_allowed = TRUE;
            break;
        }
    }

    if (!any_allowed && highest == COMM_FULL_COMMUNICATION) {
        highest = COMM_SILENT_COMMUNICATION;
    }

    g_current_mode = highest;
}
