/**
 * Xbox Input Compatibility Layer
 *
 * Translates the Xbox controller API to the host's pads through
 * recomp_pad.h (SDL3, or XInput on Windows behind RECOMP_PAD_API=xinput).
 * Handles the structural differences between the Xbox gamepad (analog
 * buttons as bytes, separate trigger channels) and the host (digital face
 * buttons, trigger axes).
 *
 * The titles' own input goes through the bindings (input_bindings.c) and
 * the XAPI replacement in src/hle; this is the older direct API, kept for
 * anything that still calls it.
 */

#include "xinput_xbox.h"
#include "recomp_pad.h"
#include <string.h>

#ifndef ERROR_SUCCESS
#define ERROR_SUCCESS 0u
#endif
#ifndef ERROR_DEVICE_NOT_CONNECTED
#define ERROR_DEVICE_NOT_CONNECTED 1167u
#endif

static BOOL  g_connected[XBOX_MAX_CONTROLLERS];
static DWORD g_packet[XBOX_MAX_CONTROLLERS];

void xbox_InputInit(void)
{
    DWORD i;
    for (i = 0; i < XBOX_MAX_CONTROLLERS; i++) {
        RecompPadState st;
        g_connected[i] = recomp_pad_read((int)i, &st) ? TRUE : FALSE;
    }
}

DWORD xbox_InputGetState(DWORD dwPort, XBOX_INPUT_STATE *pState)
{
    RecompPadState st;

    if (dwPort >= XBOX_MAX_CONTROLLERS || !pState)
        return ERROR_DEVICE_NOT_CONNECTED;
    if (!recomp_pad_read((int)dwPort, &st)) {
        g_connected[dwPort] = FALSE;
        return ERROR_DEVICE_NOT_CONNECTED;
    }
    g_connected[dwPort] = TRUE;

    memset(pState, 0, sizeof(XBOX_INPUT_STATE));
    pState->dwPacketNumber = ++g_packet[dwPort];
    /* The low byte is the D-pad, START, BACK and the stick clicks, which
     * the Xbox reports as digital bits; the rest are analog buttons. */
    pState->Gamepad.wButtons = st.buttons & 0x00FF;
    pState->Gamepad.bAnalogButtons[XBOX_BUTTON_A] = (st.buttons & RECOMP_PAD_A) ? 255 : 0;
    pState->Gamepad.bAnalogButtons[XBOX_BUTTON_B] = (st.buttons & RECOMP_PAD_B) ? 255 : 0;
    pState->Gamepad.bAnalogButtons[XBOX_BUTTON_X] = (st.buttons & RECOMP_PAD_X) ? 255 : 0;
    pState->Gamepad.bAnalogButtons[XBOX_BUTTON_Y] = (st.buttons & RECOMP_PAD_Y) ? 255 : 0;
    pState->Gamepad.bAnalogButtons[XBOX_BUTTON_BLACK] =
        (st.buttons & RECOMP_PAD_LEFT_SHOULDER) ? 255 : 0;
    pState->Gamepad.bAnalogButtons[XBOX_BUTTON_WHITE] =
        (st.buttons & RECOMP_PAD_RIGHT_SHOULDER) ? 255 : 0;
    pState->Gamepad.bAnalogButtons[XBOX_BUTTON_LTRIGGER] = st.left_trigger;
    pState->Gamepad.bAnalogButtons[XBOX_BUTTON_RTRIGGER] = st.right_trigger;
    pState->Gamepad.sThumbLX = st.lx;
    pState->Gamepad.sThumbLY = st.ly;
    pState->Gamepad.sThumbRX = st.rx;
    pState->Gamepad.sThumbRY = st.ry;
    return ERROR_SUCCESS;
}

DWORD xbox_InputSetState(DWORD dwPort, const XBOX_VIBRATION *pVibration)
{
    if (dwPort >= XBOX_MAX_CONTROLLERS || !pVibration)
        return ERROR_DEVICE_NOT_CONNECTED;
    if (!g_connected[dwPort])
        return ERROR_DEVICE_NOT_CONNECTED;
    recomp_pad_rumble((int)dwPort, pVibration->wLeftMotorSpeed, pVibration->wRightMotorSpeed);
    return ERROR_SUCCESS;
}

BOOL xbox_InputIsConnected(DWORD dwPort)
{
    if (dwPort >= XBOX_MAX_CONTROLLERS) return FALSE;
    return g_connected[dwPort];
}

DWORD xbox_InputGetCapabilities(DWORD dwPort, DWORD dwFlags, XBOX_INPUT_CAPABILITIES *pCaps)
{
    RecompPadState st;

    (void)dwFlags;
    if (dwPort >= XBOX_MAX_CONTROLLERS || !pCaps)
        return ERROR_DEVICE_NOT_CONNECTED;
    if (!recomp_pad_read((int)dwPort, &st))
        return ERROR_DEVICE_NOT_CONNECTED;
    memset(pCaps, 0, sizeof(XBOX_INPUT_CAPABILITIES));
    pCaps->Type = 1;        /* XINPUT_DEVTYPE_GAMEPAD */
    pCaps->SubType = 1;     /* XINPUT_DEVSUBTYPE_GAMEPAD */
    return ERROR_SUCCESS;
}
