"""
The binding UI: four controllers, a device for each, and every Xbox control.

tkinter, because the toolkit's tools are Python and this has to run from the
same checkout with nothing installed. Every widget is a plain tk one with its
colours set by hand: ttk themes on Windows will not let a widget be black.

The look is the original console's -- a black field, dark grey blades, one
Xbox green for structure and a brighter dashboard green for whatever has
focus. The blade column on the left is the dashboard's, and the row of
controls is one green underline per group rather than boxes, which is how the
console's menus separated things.
"""

import os
import tkinter as tk
from tkinter import messagebox

from . import bindings, devices

BLACK = "#000000"
PANEL = "#0D0D0D"
PANEL_HI = "#171717"
EDGE = "#242424"
GREEN = "#107C10"          # the Xbox brand green: structure, borders, fills
GREEN_DEEP = "#0A3D0A"     # the same green, sunk into black, for backgrounds
GLOW = "#9BE04B"           # the dashboard's brighter green: focus and values
TEXT = "#DCE6DC"
TEXT_DIM = "#718071"       # a green-grey, for labels and secondary text

FONT = ("Segoe UI", 10)
FONT_SMALL = ("Segoe UI", 8)
FONT_BOLD = ("Segoe UI Semibold", 10)
FONT_HEAD = ("Segoe UI Light", 20)


def _shorten(text, limit=52):
    """A path that fits the header. The tail is the part worth reading."""
    return text if len(text) <= limit else "..." + text[-(limit - 3):]


def _label(parent, text, **kw):
    options = dict(bg=parent["bg"], fg=TEXT, font=FONT, anchor="w")
    options.update(kw)
    return tk.Label(parent, text=text, **options)


def _button(parent, text, command, accent=False, width=None):
    """A flat button in the console's palette: green on black, glow on hover."""
    face = GREEN_DEEP if accent else PANEL_HI
    button = tk.Button(
        parent, text=text, command=command, font=FONT_BOLD if accent else FONT,
        bg=face, fg=GLOW if accent else TEXT, activebackground=GREEN,
        activeforeground=BLACK, relief="flat", bd=0, padx=12, pady=5,
        highlightthickness=1, highlightbackground=GREEN if accent else EDGE,
        highlightcolor=GREEN, cursor="hand2")
    if width:
        button.configure(width=width)
    button.bind("<Enter>", lambda _e: button.configure(bg=GREEN, fg=BLACK))
    button.bind("<Leave>", lambda _e: button.configure(
        bg=face, fg=GLOW if accent else TEXT))
    return button


class Scroller(tk.Canvas):
    """A scrollbar drawn by hand.

    tk.Scrollbar on Windows keeps the system's light trough whatever colours
    it is given, which is a white stripe down a black window. This is the same
    widget in two colours: a green bar in a black channel.
    """

    def __init__(self, parent, target):
        super().__init__(parent, width=10, bg=BLACK, highlightthickness=0, bd=0)
        self.target = target
        self.first, self.last = 0.0, 1.0
        self.thumb = self.create_rectangle(2, 0, 9, 40, fill=GREEN, outline="")
        self.bind("<Configure>", lambda _e: self.redraw())
        self.bind("<Button-1>", self.drag)
        self.bind("<B1-Motion>", self.drag)

    def set(self, first, last):
        self.first, self.last = float(first), float(last)
        self.redraw()

    def redraw(self):
        height = self.winfo_height()
        top = self.first * height
        bottom = max(self.last * height, top + 24)
        self.coords(self.thumb, 2, top, 9, min(bottom, height))
        self.itemconfigure(self.thumb,
                           state="hidden" if self.last - self.first >= 1.0
                           else "normal")

    def drag(self, event):
        height = max(self.winfo_height(), 1)
        span = self.last - self.first
        self.target.yview_moveto(max(0.0, min(1.0, event.y / height - span / 2)))


class CaptureDialog(tk.Toplevel):
    """"Press a key or button now" -- the only way a binding is set.

    Keys arrive as events; a pad has to be polled, so the dialog polls the
    controller's own pad every 40 ms and compares against the state it had
    when the dialog opened. Nothing is captured until something changes,
    which is what stops a stick resting off centre from binding itself.
    """

    def __init__(self, parent, title, pad_index, on_done):
        super().__init__(parent)
        self.on_done = on_done
        self.pad_index = pad_index
        self.baseline = devices.read_pad(pad_index) if pad_index is not None else None
        self.done = False

        self.overrideredirect(True)
        self.configure(bg=GREEN)
        frame = tk.Frame(self, bg=PANEL, padx=28, pady=22)
        frame.pack(padx=2, pady=2)
        _label(frame, title.upper(), fg=TEXT_DIM, font=FONT_SMALL).pack(anchor="w")
        _label(frame, "Press a key or a button", fg=GLOW,
               font=("Segoe UI Light", 16)).pack(anchor="w", pady=(6, 2))
        hint = "Esc cancels."
        if pad_index is None:
            hint += "  This controller is on the keyboard, so pads are ignored."
        else:
            hint += "  Gamepad %d is being watched too." % (pad_index + 1)
        _label(frame, hint, fg=TEXT_DIM, font=FONT_SMALL).pack(anchor="w")
        row = tk.Frame(frame, bg=PANEL)
        row.pack(anchor="w", pady=(16, 0))
        _button(row, "Clear this binding", lambda: self.finish(""), width=18).pack(side="left")
        _button(row, "Cancel", lambda: self.finish(None), width=10).pack(side="left", padx=(8, 0))

        self.update_idletasks()
        x = parent.winfo_rootx() + (parent.winfo_width() - self.winfo_width()) // 2
        y = parent.winfo_rooty() + (parent.winfo_height() - self.winfo_height()) // 3
        self.geometry("+%d+%d" % (max(x, 0), max(y, 0)))
        self.transient(parent)
        self.grab_set()
        self.focus_force()
        self.bind("<KeyPress>", self.on_key)
        self.poll()

    def on_key(self, event):
        if event.keysym == "Escape":
            return self.finish(None)
        if event.keysym in ("Shift_L", "Shift_R", "Control_L", "Control_R",
                            "Alt_L", "Alt_R") and event.keycode == 0:
            return None
        return self.finish(devices.key_source_from_event(event))

    def poll(self):
        if self.done or self.pad_index is None:
            return
        self.baseline, source = devices.pad_source_for(self.pad_index, self.baseline)
        if source:
            self.finish(source)
        else:
            self.after(40, self.poll)

    def finish(self, source):
        if self.done:
            return None
        self.done = True
        self.grab_release()
        self.destroy()
        self.on_done(source)
        return "break"


class App:
    def __init__(self, root, path=None):
        self.root = root
        self.path = path or bindings.config_path()
        self.config = bindings.load(self.path)
        self.port = 1
        self.dirty = False
        self.blades = {}
        self.device_buttons = []

        root.title("xboxrecomp - controller bindings")
        root.configure(bg=BLACK)
        root.geometry("1080x760")
        root.minsize(900, 560)

        self.build_header()
        body = tk.Frame(root, bg=BLACK)
        body.pack(fill="both", expand=True, padx=18, pady=(0, 10))
        self.build_blades(body)
        self.build_panel(body)
        self.build_footer()
        self.select_port(1)

    # ---- chrome ----------------------------------------------------------

    def build_header(self):
        head = tk.Frame(self.root, bg=BLACK)
        head.pack(fill="x", padx=18, pady=(16, 10))
        _label(head, "XBOXRECOMP", fg=GLOW, font=FONT_HEAD).pack(side="left")
        _label(head, "  CONTROLLER BINDINGS", fg=TEXT_DIM,
               font=("Segoe UI", 11)).pack(side="left", pady=(8, 0))
        self.path_label = _label(head, "", fg=TEXT_DIM, font=FONT_SMALL,
                                 anchor="e")
        self.path_label.pack(side="right", pady=(10, 0))
        tk.Frame(self.root, bg=GREEN, height=2).pack(fill="x", padx=18)

    def build_blades(self, parent):
        column = tk.Frame(parent, bg=BLACK, width=230)
        column.pack(side="left", fill="y", pady=16)
        column.pack_propagate(False)
        for port in range(1, bindings.PORTS + 1):
            blade = tk.Frame(column, bg=PANEL, cursor="hand2")
            blade.pack(fill="x", pady=(0, 6))
            bar = tk.Frame(blade, bg=EDGE, width=4)
            bar.pack(side="left", fill="y")
            inner = tk.Frame(blade, bg=PANEL, padx=14, pady=12)
            inner.pack(side="left", fill="both", expand=True)
            title = _label(inner, "CONTROLLER %d" % port, fg=TEXT, font=FONT_BOLD)
            title.pack(anchor="w")
            device = _label(inner, "", fg=TEXT_DIM, font=FONT_SMALL)
            device.pack(anchor="w")
            self.blades[port] = (blade, bar, inner, title, device)
            for widget in (blade, inner, title, device):
                widget.bind("<Button-1>", lambda _e, p=port: self.select_port(p))
        note = tk.Frame(column, bg=PANEL, padx=14, pady=12)
        note.pack(fill="x", pady=(10, 0))
        _label(note, "A port with no device is reported empty, exactly as an "
                     "empty socket was on the console.", fg=TEXT_DIM,
               font=FONT_SMALL, wraplength=185, justify="left").pack(anchor="w")

    def build_panel(self, parent):
        panel = tk.Frame(parent, bg=PANEL)
        panel.pack(side="left", fill="both", expand=True, padx=(12, 0), pady=16)

        device_row = tk.Frame(panel, bg=PANEL, padx=18, pady=14)
        device_row.pack(fill="x")
        _label(device_row, "INPUT DEVICE", fg=TEXT_DIM,
               font=FONT_SMALL).pack(anchor="w")
        self.device_row = tk.Frame(device_row, bg=PANEL)
        self.device_row.pack(fill="x", pady=(6, 0))
        _button(device_row, "Rescan", self.refresh_devices).pack(anchor="e", pady=(8, 0))
        tk.Frame(panel, bg=GREEN, height=1).pack(fill="x", padx=18)

        holder = tk.Frame(panel, bg=PANEL)
        holder.pack(fill="both", expand=True, padx=(18, 8), pady=(12, 12))
        # A width, so the list of controls does not decide how wide the
        # window opens; it scrolls and stretches instead.
        self.canvas = tk.Canvas(holder, bg=PANEL, highlightthickness=0, bd=0,
                                width=560, height=300)
        scroll = Scroller(holder, self.canvas)
        self.rows = tk.Frame(self.canvas, bg=PANEL)
        self.rows.bind("<Configure>", lambda _e: self.canvas.configure(
            scrollregion=self.canvas.bbox("all")))
        self.window = self.canvas.create_window((0, 0), window=self.rows, anchor="nw")
        self.canvas.bind("<Configure>", lambda e: self.canvas.itemconfigure(
            self.window, width=e.width))
        self.canvas.configure(yscrollcommand=scroll.set)
        self.canvas.pack(side="left", fill="both", expand=True)
        scroll.pack(side="right", fill="y")
        self.canvas.bind_all("<MouseWheel>", lambda e: self.canvas.yview_scroll(
            -1 * (e.delta // 120), "units"))

    def build_footer(self):
        tk.Frame(self.root, bg=GREEN, height=1).pack(fill="x", padx=18)
        foot = tk.Frame(self.root, bg=BLACK, padx=18, pady=12)
        foot.pack(fill="x")
        self.status = _label(foot, "", fg=TEXT_DIM, font=FONT_SMALL)
        self.status.pack(side="left")
        _button(foot, "Save", self.save, accent=True, width=10).pack(side="right")
        _button(foot, "Reset all four", self.reset_all).pack(side="right", padx=(0, 8))
        _button(foot, "Reset this controller", self.reset_controller).pack(
            side="right", padx=(0, 8))

    # ---- state -----------------------------------------------------------

    @property
    def controller(self):
        return self.config["controllers"][self.port - 1]

    def select_port(self, port):
        self.port = port
        for number, (blade, bar, inner, title, device) in self.blades.items():
            chosen = number == port
            face = PANEL_HI if chosen else PANEL
            blade.configure(bg=face)
            inner.configure(bg=face)
            bar.configure(bg=GLOW if chosen else EDGE)
            title.configure(bg=face, fg=GLOW if chosen else TEXT)
            entry = self.config["controllers"][number - 1]
            device.configure(bg=face,
                             text=bindings.device_label(entry["device"]))
        self.refresh_devices()
        self.rebuild_rows()
        self.refresh_status()

    def refresh_devices(self):
        for widget in self.device_row.winfo_children():
            widget.destroy()
        self.device_buttons = []
        current = self.controller["device"]
        # Three to a row: six devices side by side runs off the panel, and a
        # dropdown would hide which pads are actually there.
        for index, (value, label) in enumerate(devices.device_choices()):
            chosen = value == current
            button = tk.Button(
                self.device_row, text=label, font=FONT,
                bg=GREEN_DEEP if chosen else PANEL_HI,
                fg=GLOW if chosen else TEXT, activebackground=GREEN,
                activeforeground=BLACK, relief="flat", bd=0, padx=10, pady=6,
                highlightthickness=1, cursor="hand2",
                highlightbackground=GLOW if chosen else EDGE,
                command=lambda v=value: self.set_device(v))
            button.grid(row=index // 3, column=index % 3, sticky="ew",
                        padx=(0, 6), pady=(0, 6))
            self.device_buttons.append(button)
        for column in range(3):
            self.device_row.grid_columnconfigure(column, weight=1, uniform="dev")
        if not devices.available():
            _label(self.device_row, "no XInput on this machine",
                   fg=TEXT_DIM, font=FONT_SMALL).grid(row=2, column=0,
                                                      columnspan=3, sticky="w")

    def set_device(self, value):
        self.controller["device"] = value
        self.dirty = True
        self.select_port(self.port)

    def rebuild_rows(self):
        for widget in self.rows.winfo_children():
            widget.destroy()
        group = None
        for name, label, control_group in bindings.CONTROLS:
            if control_group != group:
                group = control_group
                header = tk.Frame(self.rows, bg=PANEL)
                header.pack(fill="x", pady=(14, 4))
                _label(header, group.upper(), fg=GLOW,
                       font=FONT_SMALL).pack(anchor="w")
                tk.Frame(header, bg=GREEN_DEEP, height=1).pack(fill="x", pady=(3, 0))
            self.build_row(name, label)

    def build_row(self, name, label):
        sources = self.controller["bindings"].get(name, [])
        row = tk.Frame(self.rows, bg=PANEL)
        row.pack(fill="x", pady=1)
        _label(row, label, width=20).pack(side="left", padx=(2, 0))
        value = " + ".join(bindings.source_label(s) for s in sources) or "unbound"
        field = tk.Label(row, text=value, bg=PANEL_HI,
                         fg=GLOW if sources else TEXT_DIM, font=FONT,
                         anchor="w", padx=10, pady=5, cursor="hand2")
        field.pack(side="left", fill="x", expand=True)
        field.bind("<Button-1>", lambda _e: self.capture(name, label, replace=True))
        field.bind("<Enter>", lambda _e: field.configure(bg=GREEN_DEEP))
        field.bind("<Leave>", lambda _e: field.configure(bg=PANEL_HI))
        _button(row, "Bind", lambda: self.capture(name, label, replace=True),
                width=5).pack(side="left", padx=(6, 0))
        _button(row, "+", lambda: self.capture(name, label, replace=False),
                width=2).pack(side="left", padx=(4, 0))

    def capture(self, name, label, replace):
        pad_index = bindings.pad_index(self.controller["device"])

        def done(source):
            if source is None:
                return
            current = list(self.controller["bindings"].get(name, []))
            if source == "":
                current = []
            elif replace:
                current = [source]
            elif source not in current:
                current.append(source)
            self.controller["bindings"][name] = current[:4]
            self.dirty = True
            self.rebuild_rows()
            self.refresh_status()

        title = "Controller %d  -  %s" % (self.port, label)
        CaptureDialog(self.root, title, pad_index, done)

    # ---- actions ---------------------------------------------------------

    def reset_controller(self):
        self.config["controllers"][self.port - 1] = bindings.default_controller(self.port)
        self.dirty = True
        self.select_port(self.port)

    def reset_all(self):
        self.config = bindings.default_config()
        self.dirty = True
        self.select_port(self.port)

    def save(self):
        try:
            written = bindings.save(self.config, self.path)
        except OSError as error:
            messagebox.showerror("xboxrecomp", "Could not save:\n%s" % error)
            return
        self.dirty = False
        self.refresh_status("Saved to %s" % written)

    def refresh_status(self, message=None):
        exists = os.path.exists(self.path)
        state = "unsaved changes" if self.dirty else (
            "saved" if exists else "not written yet")
        self.status.configure(text=message or
                              "Controller %d  -  %s  -  %s"
                              % (self.port,
                                 bindings.device_label(self.controller["device"]),
                                 state))
        self.path_label.configure(text=_shorten(self.path))


def main(argv=None):
    argv = list(argv or [])
    path = argv[0] if argv else None
    root = tk.Tk()
    App(root, path)
    root.mainloop()
    return 0
