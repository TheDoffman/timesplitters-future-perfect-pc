"""
What is plugged in, and what is being pressed on it.

XInput through ctypes: the UI needs to list the pads that answer right now,
and, while capturing a binding, to notice the first control that moves. No
DirectInput and no raw HID -- an Xbox-layout pad is what XInput reports, and
anything else on Windows is either seen by XInput or needs a translation
layer this toolkit does not have.

Everything here degrades to "no pads" rather than raising, so the UI runs on a
machine with no XInput at all (and so the tests can import it anywhere).
"""

import ctypes
import os

from . import bindings

MAX_PADS = 4

# XInput's own left-stick deadzone, used only to decide that a stick has been
# pushed far enough to count as a capture.
CAPTURE_AXIS_THRESHOLD = 16000
CAPTURE_TRIGGER_THRESHOLD = 100

_XINPUT_DLLS = ("xinput1_4.dll", "xinput1_3.dll", "xinput9_1_0.dll")

_PAD_BITS = [
    ("pad:dpad_up", 0x0001), ("pad:dpad_down", 0x0002),
    ("pad:dpad_left", 0x0004), ("pad:dpad_right", 0x0008),
    ("pad:start", 0x0010), ("pad:back", 0x0020),
    ("pad:lthumb", 0x0040), ("pad:rthumb", 0x0080),
    ("pad:lshoulder", 0x0100), ("pad:rshoulder", 0x0200),
    ("pad:a", 0x1000), ("pad:b", 0x2000),
    ("pad:x", 0x4000), ("pad:y", 0x8000),
]


class _Gamepad(ctypes.Structure):
    _fields_ = [
        ("wButtons", ctypes.c_ushort),
        ("bLeftTrigger", ctypes.c_ubyte),
        ("bRightTrigger", ctypes.c_ubyte),
        ("sThumbLX", ctypes.c_short),
        ("sThumbLY", ctypes.c_short),
        ("sThumbRX", ctypes.c_short),
        ("sThumbRY", ctypes.c_short),
    ]


class _State(ctypes.Structure):
    _fields_ = [("dwPacketNumber", ctypes.c_uint), ("Gamepad", _Gamepad)]


_library = None
_looked = False


def _xinput():
    global _library, _looked
    if _looked:
        return _library
    _looked = True
    if os.name == "nt":
        for name in _XINPUT_DLLS:
            try:
                _library = ctypes.windll.LoadLibrary(name)
                break
            except OSError:
                continue
    return _library


def available():
    """Is there an XInput to ask at all?"""
    return _xinput() is not None


def read_pad(index):
    """One pad's state as a plain dict, or None if that slot is empty."""
    library = _xinput()
    if library is None or not 0 <= index < MAX_PADS:
        return None
    state = _State()
    try:
        if library.XInputGetState(index, ctypes.byref(state)) != 0:
            return None
    except OSError:
        return None
    pad = state.Gamepad
    return {
        "buttons": pad.wButtons,
        "lt": pad.bLeftTrigger,
        "rt": pad.bRightTrigger,
        "lx": pad.sThumbLX, "ly": pad.sThumbLY,
        "rx": pad.sThumbRX, "ry": pad.sThumbRY,
    }


def connected_pads():
    """The XInput slots that answer right now, as a list of indices."""
    return [i for i in range(MAX_PADS) if read_pad(i) is not None]


def device_choices():
    """Every device the user may pick, as (value, label) for the UI.

    The keyboard is always offered; a pad is labelled with whether it is
    there, because a binding for a pad that is unplugged at the moment is
    still a binding worth keeping.
    """
    present = set(connected_pads())
    choices = [("keyboard", "Keyboard")]
    for index in range(MAX_PADS):
        state = "connected" if index in present else "absent"
        # The runtime reads pads through SDL3 by default, which numbers
        # them in the order they were connected; this tool can only ask
        # XInput, so "connected" is XInput's view of the same slot.
        choices.append(("gamepad:%d" % index, "Gamepad %d - %s" % (index + 1, state)))
    choices.append(("none", "Nothing"))
    return choices


def pad_pressed(state, baseline):
    """The first pad source pressed since `baseline`, or None.

    Compared against a baseline taken when capture started so a stick already
    held off centre, or a trigger resting high, does not capture itself.
    """
    if state is None or baseline is None:
        return None
    changed = state["buttons"] & ~baseline["buttons"]
    for source, bit in _PAD_BITS:
        if changed & bit:
            return source
    for axis, source in (("lt", "pad:lt"), ("rt", "pad:rt")):
        if state[axis] > CAPTURE_TRIGGER_THRESHOLD and baseline[axis] <= CAPTURE_TRIGGER_THRESHOLD:
            return source
    for axis in ("lx", "ly", "rx", "ry"):
        value, was = state[axis], baseline[axis]
        if value > CAPTURE_AXIS_THRESHOLD and was <= CAPTURE_AXIS_THRESHOLD:
            return "pad:%s+" % axis
        if value < -CAPTURE_AXIS_THRESHOLD and was >= -CAPTURE_AXIS_THRESHOLD:
            return "pad:%s-" % axis
    return None


def pad_source_for(index, baseline):
    """Poll one pad once during capture: (new baseline, source or None)."""
    state = read_pad(index)
    if state is None:
        return baseline, None
    if baseline is None:
        return state, None
    return state, pad_pressed(state, baseline)


def key_source_from_event(event):
    """A "key:" source from a tkinter key event.

    On Windows event.keycode is the virtual-key code, which is exactly what
    the runtime reads, so punctuation and keys with no name still bind.
    """
    keysym = (event.keysym or "").upper()
    alias = {
        "RETURN": "RETURN", "KP_ENTER": "RETURN", "BACKSPACE": "BACK",
        "PRIOR": "PAGEUP", "NEXT": "PAGEDOWN", "SHIFT_L": "LSHIFT",
        "SHIFT_R": "RSHIFT", "CONTROL_L": "LCTRL", "CONTROL_R": "RCTRL",
        "ALT_L": "LALT", "ALT_R": "RALT", "CAPS_LOCK": "CAPSLOCK",
        "ESC": "ESCAPE",
    }
    name = alias.get(keysym, keysym)
    if name not in bindings.KEY_NAMES:
        name = None
    return bindings.key_source(event.keycode, name)
