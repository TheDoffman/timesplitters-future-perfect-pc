/*
 * usb_gamepad.c -- an Xbox controller, as the console's USB stack expects it.
 *
 * Descriptors and the input report, from the USB 2.0 specification for the
 * standard requests and from the device's own published interface class for
 * the rest. The gamepad is not a HID device: it reports interface class 0x58
 * subclass 0x42, which is Microsoft's own, and its report has a fixed layout
 * rather than one described by a HID report descriptor. That is why there is
 * no report descriptor here and why nothing asks for one.
 *
 * Input comes from the host through the existing xbox_input layer, so a real
 * pad plugged into the PC drives this one.
 */
#include <stdlib.h>
#include <stdio.h>
#include "usb_gamepad.h"

#include <string.h>

/* ---- descriptors ------------------------------------------------------- */

static const uint8_t s_device_desc[18] = {
    18,             /* bLength                                    */
    0x01,           /* bDescriptorType: DEVICE                    */
    0x10, 0x01,     /* bcdUSB 1.10                                */
    0x00,           /* bDeviceClass: per interface                */
    0x00,           /* bDeviceSubClass                            */
    0x00,           /* bDeviceProtocol                            */
    0x08,           /* bMaxPacketSize0: 8                         */
    0x5E, 0x04,     /* idVendor  0x045E Microsoft                 */
    0x89, 0x02,     /* idProduct 0x0289 Controller S              */
    0x21, 0x01,     /* bcdDevice                                  */
    0x00,           /* iManufacturer: none                        */
    0x00,           /* iProduct: none                             */
    0x00,           /* iSerialNumber: none                        */
    0x01            /* bNumConfigurations                         */
};

/* Configuration, interface and both endpoints, in the one block a
 * GET_DESCRIPTOR(CONFIGURATION) returns. wTotalLength covers all of it. */
static const uint8_t s_config_desc[32] = {
    /* configuration */
    9, 0x02, 32, 0x00, 0x01, 0x01, 0x00, 0x80, 50,
    /* interface: class 0x58 subclass 0x42, the Xbox gamepad's own */
    9, 0x04, 0x00, 0x00, 0x02, 0x58, 0x42, 0x00, 0x00,
    /* endpoint 0x81 IN, interrupt, 32 bytes, 4 ms */
    7, 0x05, 0x81, 0x03, 0x20, 0x00, 0x04,
    /* endpoint 0x02 OUT, interrupt, 32 bytes, 4 ms -- rumble */
    7, 0x05, 0x02, 0x03, 0x20, 0x00, 0x04
};

static uint8_t s_address;
static uint8_t s_configuration;

uint8_t usb_gamepad_address(void) { return s_address; }

/* ---- control transfers ------------------------------------------------- */

#define REQ_GET_STATUS         0x00
#define REQ_CLEAR_FEATURE      0x01
#define REQ_SET_FEATURE        0x03
#define REQ_SET_ADDRESS        0x05
#define REQ_GET_DESCRIPTOR     0x06
#define REQ_GET_CONFIGURATION  0x08
#define REQ_SET_CONFIGURATION  0x09
#define REQ_SET_INTERFACE      0x0B

#define DESC_DEVICE            0x01
#define DESC_CONFIGURATION     0x02
#define DESC_STRING            0x03

static int copy_out(uint8_t *out, int max, const uint8_t *src, int len,
                    uint16_t wLength)
{
    /* A device sends the smaller of what was asked for and what it has. */
    if (len > (int)wLength) len = (int)wLength;
    if (len > max)          len = max;
    if (len > 0)            memcpy(out, src, (size_t)len);
    return len;
}

int usb_gamepad_control(const UsbSetup *setup, uint8_t *out, int max)
{
    int is_in = (setup->bmRequestType & 0x80) != 0;
    int type  = (setup->bmRequestType >> 5) & 3;   /* 0 standard, 1 class */

    if (type == 0) {
        switch (setup->bRequest) {
        case REQ_GET_DESCRIPTOR:
            switch (setup->wValue >> 8) {
            case DESC_DEVICE:
                return copy_out(out, max, s_device_desc,
                                (int)sizeof s_device_desc, setup->wLength);
            case DESC_CONFIGURATION:
                return copy_out(out, max, s_config_desc,
                                (int)sizeof s_config_desc, setup->wLength);
            case DESC_STRING:
                /* No string descriptors. Stalling is the correct answer and
                 * the one a host expects; returning an empty descriptor gets
                 * read as a malformed one. */
                return -1;
            default:
                return -1;
            }

        case REQ_SET_ADDRESS:
            s_address = (uint8_t)(setup->wValue & 0x7F);
            return 0;                    /* zero-length status stage */

        case REQ_SET_CONFIGURATION:
            s_configuration = (uint8_t)(setup->wValue & 0xFF);
            return 0;

        case REQ_GET_CONFIGURATION:
            if (!is_in || max < 1) return -1;
            out[0] = s_configuration;
            return 1;

        case REQ_GET_STATUS:
            /* Bus-powered, no remote wakeup. */
            if (!is_in || max < 2) return -1;
            out[0] = 0; out[1] = 0;
            return 2;

        case REQ_CLEAR_FEATURE:
        case REQ_SET_FEATURE:
        case REQ_SET_INTERFACE:
            return 0;

        default:
            return -1;
        }
    }

    /* Class requests. The Xbox pad answers a vendor-defined capabilities
     * request on the interface; anything else is not ours to guess at. */
    return -1;
}

/* ---- the input report -------------------------------------------------- */

/* The host's own pad, through the layer that maps one to XInput and applies
 * the user's bindings. A real controller plugged into the PC drives this
 * emulated one, and so does a keyboard if that is what port 1 is bound to.
 * This device is the one on port 1; the OHCI side has no others yet. */
#include "../input/xinput_xbox.h"
#include "../input/input_bindings.h"

/*
 * The Xbox report is 20 bytes and fixed:
 *
 *   0      report id, always 0
 *   1      length, always 20
 *   2      digital buttons: dpad, start, back, thumb clicks
 *   3      reserved
 *   4..11  analog buttons A B X Y Black White, then the two triggers
 *   12..19 four signed 16-bit stick axes, little endian
 */

/* Say that the title is polling, and optionally press something.
 *
 * Two separate questions, answered in one place because both need the report
 * path. "Is it waiting for input" cannot be settled by the absence of USB
 * logging when there is no USB logging; and a title sitting on a press-start
 * screen cannot be advanced by an unattended run however long it is given.
 *
 * RECOMP_FAKE_INPUT=start        holds START, pressed and released on a cycle
 * RECOMP_FAKE_INPUT=a            the A button
 * RECOMP_FAKE_INPUT=start,a      cycles through them in turn
 *
 * RECOMP_FAKE_INPUT_MS=<n>       how long each press and gap lasts (default
 *                                500, so a press every second)
 *
 * Pressed and released rather than held: a menu advances on the edge, and a
 * button that is never released reads as one press forever.
 */
static struct { const char *name; int digital; unsigned bit; } FAKE_BUTTONS[] = {
    { "start", 0, XBOX_GAMEPAD_START      },
    { "back",  0, XBOX_GAMEPAD_BACK       },
    { "up",    0, XBOX_GAMEPAD_DPAD_UP    },
    { "down",  0, XBOX_GAMEPAD_DPAD_DOWN  },
    { "left",  0, XBOX_GAMEPAD_DPAD_LEFT  },
    { "right", 0, XBOX_GAMEPAD_DPAD_RIGHT },
    { "a",     1, XBOX_BUTTON_A           },
    { "b",     1, XBOX_BUTTON_B           },
    { "x",     1, XBOX_BUTTON_X           },
    { "y",     1, XBOX_BUTTON_Y           },
};

static void fake_input_apply(uint8_t *out)
{
    static int checked;
    static char spec[64];
    const char *env;
    unsigned long period;
    unsigned long long now;
    const char *p;
    int index = 0, want, i;

    if (!checked) {
        checked = 1;
        env = getenv("RECOMP_FAKE_INPUT");
        if (env) {
            strncpy(spec, env, sizeof spec - 1);
            fprintf(stderr, "  [PAD] synthetic input: %s\n", spec);
            fflush(stderr);
        }
    }
    if (!spec[0])
        return;

    env = getenv("RECOMP_FAKE_INPUT_MS");
    period = env ? strtoul(env, NULL, 0) : 0;
    if (!period)
        period = 500;

    now = (unsigned long long)GetTickCount64();

    /* Odd half of the cycle is the gap, so every press has an edge. */
    if ((now / period) % 2 == 0)
        return;

    /* Which button this cycle: the list is walked in turn so a sequence like
     * "start,a" can get through a title screen and then a menu. */
    {
        int count = 1;
        for (p = spec; *p; p++)
            if (*p == ',')
                count++;
        want = (int)((now / (period * 2)) % (unsigned long long)count);
    }

    p = spec;
    while (index < want && (p = strchr(p, ',')) != NULL) {
        p++;
        index++;
    }
    if (!p)
        return;

    for (i = 0; i < (int)(sizeof FAKE_BUTTONS / sizeof FAKE_BUTTONS[0]); i++) {
        size_t n = strlen(FAKE_BUTTONS[i].name);
        if (strncmp(p, FAKE_BUTTONS[i].name, n) != 0)
            continue;
        if (p[n] && p[n] != ',')
            continue;
        if (FAKE_BUTTONS[i].digital) {
            out[4 + FAKE_BUTTONS[i].bit] = 0xFF;   /* full pressure */
        } else {
            unsigned bits = (unsigned)(out[2] | (out[3] << 8));
            bits |= FAKE_BUTTONS[i].bit;
            out[2] = (uint8_t)(bits & 0xFF);
            out[3] = (uint8_t)((bits >> 8) & 0xFF);
        }
        return;
    }
}

int usb_gamepad_report(uint8_t *out, int max)
{
    XBOX_GAMEPAD pad;
    const XBOX_GAMEPAD *g = &pad;
    int i;

    if (max < 20)
        return 0;

    /* Unconditional, and bounded: whether the title reads the pad at all is
     * the first question about input, and there was no way to answer it. */
    {
        static unsigned polls;
        if (polls++ < 3 || polls % 1000 == 0) {
            fprintf(stderr, "  [PAD] report #%u requested\n", polls);
            fflush(stderr);
        }
    }

    memset(out, 0, 20);
    out[0] = 0;
    out[1] = 20;

    /* A disconnected host pad is not an error here: the device is present on
     * the bus either way, it just reports nothing pressed. */
    if (!recomp_bindings_sample(0, &pad))
        return 20;

    out[2] = (uint8_t)(g->wButtons & 0xFF);
    out[3] = (uint8_t)((g->wButtons >> 8) & 0xFF);
    for (i = 0; i < 8; i++)
        out[4 + i] = g->bAnalogButtons[i];
    out[12] = (uint8_t)(g->sThumbLX & 0xFF);
    out[13] = (uint8_t)((g->sThumbLX >> 8) & 0xFF);
    out[14] = (uint8_t)(g->sThumbLY & 0xFF);
    out[15] = (uint8_t)((g->sThumbLY >> 8) & 0xFF);
    out[16] = (uint8_t)(g->sThumbRX & 0xFF);
    out[17] = (uint8_t)((g->sThumbRX >> 8) & 0xFF);
    out[18] = (uint8_t)(g->sThumbRY & 0xFF);
    out[19] = (uint8_t)((g->sThumbRY >> 8) & 0xFF);
    fake_input_apply(out);
    return 20;
}
