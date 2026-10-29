"""Launcher-aligned presentation for the separate Online Tk windows."""

# Direct-command imports share the repository's grouped helper paths.
import sys as _layout_sys
from pathlib import Path as _LayoutPath
_layout_tools = _LayoutPath(__file__).resolve().parents[1]
_layout_root = _layout_tools.parent
if (_layout_tools / 'project_paths.py').is_file():
    _layout_sys.path.insert(0, str(_layout_tools))
    from project_paths import configure as _configure_paths, source_file as repo_file
    _configure_paths()
else:
    # Standalone source fixtures retain their original flat-folder behavior.
    _layout_root = _LayoutPath(__file__).resolve().parents[1]
    def repo_file(root, area, name):
        return _LayoutPath(root) / area / name

import ctypes
from ctypes import wintypes
import weakref
import math
import tkinter as tk
from tkinter import ttk


def system_dark():
    # Match the launcher regardless of light/dark app preference. Respect the
    # Windows accessibility override, which retains native system colors.
    class Contrast(ctypes.Structure):
        _fields_ = [('size', wintypes.UINT), ('flags', wintypes.DWORD), ('scheme', wintypes.LPWSTR)]
    contrast = Contrast()
    contrast.size = ctypes.sizeof(contrast)
    return not (ctypes.windll.user32.SystemParametersInfoW(
        0x42, contrast.size, ctypes.byref(contrast), 0) and contrast.flags & 1)



class OnlineTheme:
    def __init__(self, window, read_dark=system_dark):
        self.root = window
        self.read_dark = read_dark
        self.windows = weakref.WeakSet()
        self.style = ttk.Style(window)
        self.original = self.style.theme_use()
        self.images = []
        if 'TSFPOnlineDark' not in self.style.theme_names():
            self.style.theme_create('TSFPOnlineDark', parent='clam')
            self.build_controls()
        self.dark = None
        self.timer = None
        self.track(window)
        window.bind('<Destroy>', self.closed, add='+')
        self.refresh()

    def surface(self, fill, border, width=40, height=30, radius=5, checked=False):
        """Small scalable UI elements; keep native ttk layout/input behavior."""
        image = tk.PhotoImage(master=self.root, width=width, height=height)
        colors = lambda value: tuple(int(value[i:i+2], 16) for i in (1, 3, 5))
        fill_rgb, edge_rgb, outside = colors(fill), colors(border), colors('#0b1420')
        rows = []
        for y in range(height):
            row = []
            for x in range(width):
                dx=abs(x-(width-1)/2)-(width/2-radius)
                dy=abs(y-(height-1)/2)-(height/2-radius)
                distance=math.hypot(max(dx,0),max(dy,0))+min(max(dx,dy),0)-radius
                if distance > 0: rgb=outside
                elif distance > -1: rgb=tuple(round(edge_rgb[i]*(-distance)+outside[i]*(1+distance)) for i in range(3))
                elif distance > -2: rgb=edge_rgb
                else: rgb=fill_rgb
                if checked:
                    # Rounded blue square with an anti-aliased white tick.
                    for ax,ay,bx,by in ((4,9,7,12),(7,12,13,5)):
                        vx,vy=bx-ax,by-ay
                        t=max(0,min(1,((x-ax)*vx+(y-ay)*vy)/(vx*vx+vy*vy)))
                        if math.hypot(x-(ax+t*vx),y-(ay+t*vy))<1.1: rgb=(255,255,255)
                row.append('#%02x%02x%02x'%rgb)
            rows.append('{'+ ' '.join(row)+'}')
        image.put(' '.join(rows));self.images.append(image)
        return image

    def build_controls(self):
        previous=self.style.theme_use();self.style.theme_use('TSFPOnlineDark')
        normal=self.surface('#1b2c3e','#3b5268')
        hot=self.surface('#27445f','#86cdff')
        pressed=self.surface('#101e2d','#3b5268')
        focus=self.surface('#1b2c3e','#86cdff')
        disabled=self.surface('#101e2d','#2c4258')
        self.style.element_create('TSFP.Button','image',normal,('disabled',disabled),('pressed',pressed),('focus',focus),('active',hot),border=6,sticky='nswe')
        self.style.layout('TButton',[('TSFP.Button',{'sticky':'nswe','children':[('Button.padding',{'sticky':'nswe','children':[('Button.label',{'sticky':'nswe'})]})]})])
        def indicator(fill,edge,checked=False):
            box=self.surface(fill,edge,18,18,4,checked)
            padded=tk.PhotoImage(master=self.root,width=24,height=18)
            padded.put('#0b1420',to=(0,0,24,18))
            padded.tk.call(str(padded),'copy',str(box),'-to',0,0)
            self.images.append(padded)
            return padded
        unchecked=indicator('#101e2d','#7f97ae')
        check=indicator('#397aa8','#86cdff',True)
        check_hot=indicator('#4998ce','#8bd2ff',True)
        off_disabled=indicator('#101e2d','#3b5268')
        on_disabled=indicator('#3b5268','#7f97ae',True)
        self.style.element_create('TSFP.Check','image',unchecked,('disabled','selected',on_disabled),('disabled',off_disabled),('active','selected',check_hot),('selected',check),border=0,sticky='')
        self.style.layout('TCheckbutton',[('Checkbutton.padding',{'sticky':'nswe','children':[('TSFP.Check',{'side':'left','sticky':''}),('Checkbutton.focus',{'side':'left','sticky':'w','children':[('Checkbutton.label',{'sticky':'nswe'})]})]})])
        field=self.surface('#101e2d','#3b5268')
        focused_field=self.surface('#101e2d','#86cdff')
        self.style.element_create('TSFP.Entry','image',field,('focus',focused_field),border=5,sticky='nswe')
        self.style.layout('TEntry',[('TSFP.Entry',{'sticky':'nswe','children':[('Entry.padding',{'sticky':'nswe','children':[('Entry.textarea',{'sticky':'nswe'})]})]})])
        self.style.element_create('TSFP.Tree','from','clam','Treeview.field')
        self.style.layout('Treeview',[('TSFP.Tree',{'sticky':'nswe','children':[('Treeview.padding',{'sticky':'nswe','children':[('Treeview.treearea',{'sticky':'nswe'})]})]})])
        self.style.element_create('TSFP.Heading','image',self.surface('#101e2d','#2c4258',40,28,0),border=3,sticky='nswe')
        self.style.layout('Treeview.Heading',[('TSFP.Heading',{'sticky':'nswe','children':[('Treeheading.padding',{'sticky':'nswe','children':[('Treeheading.text',{'sticky':'we'})]})]})])
        primary=self.surface('#96cff2','#d6edff')
        primary_hot=self.surface('#c3e5fc','#ffffff')
        primary_focus=self.surface('#96cff2','#ffffff')
        self.style.element_create('TSFP.Primary','image',primary,('disabled',disabled),
                                  ('pressed',primary_hot),('focus',primary_focus),('active',primary_hot),border=6,sticky='nswe')
        self.style.layout('Primary.TButton',[('TSFP.Primary',{'sticky':'nswe','children':[
            ('Button.padding',{'sticky':'nswe','children':[('Button.label',{'sticky':'nswe'})]})]})])
        self.style.configure('Primary.TButton',foreground='#091c2c')
        self.style.map('Primary.TButton',foreground=[('disabled','#7f97ae')])
        self.style.configure('TButton',padding=(12,6))
        self.style.configure('TCheckbutton',padding=(0,3))
        self.style.theme_use(previous)

    def track(self, window):
        self.windows.add(window)
        # The package keeps the generated icon beside this module; source builds
        # produce it with the other Windows resources. No user artwork is needed.
        icon = _LayoutPath(__file__).with_name('multiplayer.ico')
        if not icon.is_file():
            icon = _layout_root / 'build/online-icon/multiplayer.ico'
        if icon.is_file():
            window.iconbitmap(str(icon))
            if window is self.root:
                window.iconbitmap(default=str(icon))
        window.bind('<Map>', lambda event: window.after_idle(lambda: self.caption(window))
                    if event.widget == window else None, add='+')
        window.after_idle(lambda: self.caption(window))

    def caption(self, window):
        if not window.winfo_exists() or not window.winfo_viewable():
            return
        window.configure(background='#0b1420' if self.dark else 'SystemButtonFace')
        get_root = ctypes.windll.user32.GetAncestor
        get_root.argtypes = [wintypes.HWND, wintypes.UINT]
        get_root.restype = wintypes.HWND
        value = ctypes.c_int(bool(self.dark))
        try:
            handle = get_root(window.winfo_id(), 2)
            if not handle:
                return
            getter = ctypes.windll.dwmapi.DwmGetWindowAttribute
            getter.argtypes = [wintypes.HWND, wintypes.DWORD, ctypes.c_void_p, wintypes.DWORD]
            actual = ctypes.c_int(-1)
            if getter(handle, 20, ctypes.byref(actual), 4) >= 0 and actual.value == value.value:
                return
            setter = ctypes.windll.dwmapi.DwmSetWindowAttribute
            setter.argtypes = [wintypes.HWND, wintypes.DWORD, ctypes.c_void_p, wintypes.DWORD]
            setter(handle, 20, ctypes.byref(value), 4)
        except (OSError, AttributeError):
            pass

    def refresh(self):
        if self.timer:
            self.root.after_cancel(self.timer)
            self.timer = None
        dark = self.read_dark()
        if dark != self.dark:
            self.dark = dark
            if dark:
                self.style.theme_use('TSFPOnlineDark')
                self.style.configure('.', font=('Segoe UI',10), background='#0b1420', foreground='#e4edf5',
                                     fieldbackground='#152a3d', bordercolor='#3b5268',
                                     lightcolor='#2c4258', darkcolor='#0b1420', insertcolor='#e4edf5')
                self.style.configure('Treeview', background='#0b1420', fieldbackground='#0b1420', foreground='#e4edf5')
                self.style.configure('TEntry', fieldbackground='#152a3d', foreground='#e4edf5', insertcolor='#e4edf5')
                self.style.configure('TButton', background='#152a3d')
                self.style.map('TButton', background=[('active', '#27445f')], foreground=[('disabled', '#7f97ae')])
                self.style.map('TEntry', fieldbackground=[('readonly', '#152a3d')], foreground=[('readonly', '#e4edf5')])
                self.style.map('Treeview', background=[('selected', '#1d3a55')], foreground=[('selected', '#ffffff')])
                self.style.map('TCheckbutton', background=[('active', '#152a3d')], foreground=[('disabled', '#7f97ae')])
                self.style.configure('Treeview.Heading', background='#152a3d', foreground='#b4c7d8', font=('Segoe UI',10,'bold'), padding=(10,8))
                self.style.configure('Treeview', rowheight=32)
                self.style.configure('TEntry', padding=(8,7))
                self.style.configure('Card.TFrame', background='#152a3d', borderwidth=1, relief='solid')
                self.style.configure('Surface.TFrame', background='#152a3d')
                self.style.configure('Card.TLabel', background='#152a3d')
                self.style.configure('Muted.TLabel', foreground='#b4c7d8')
                self.style.configure('CardMuted.TLabel', background='#152a3d', foreground='#b4c7d8')
                self.style.configure('Eyebrow.TLabel', foreground='#86cdff')
                self.style.configure('TNotebook', background='#0b1420', borderwidth=0)
                self.style.configure('TNotebook.Tab', padding=(18,10), background='#101e2d', foreground='#b4c7d8')
                self.style.map('TNotebook.Tab', background=[('selected','#1d3a55'),('active','#27445f')], foreground=[('selected','#86cdff')])
                self.style.configure('TCheckbutton', background='#152a3d')
                self.style.map('TCheckbutton', background=[('active','#152a3d')], foreground=[('disabled','#7f97ae')])
                self.style.map('Treeview.Heading', background=[('active', '#27445f')])
            else:
                # Restore native Windows colors, including high-contrast themes.
                self.style.theme_use(self.original)
            self.style.configure('Title.TLabel', font=('Segoe UI',26,'bold'))
            self.style.configure('Section.Card.TLabel', font=('Segoe UI',16,'bold'))
            self.style.configure('Eyebrow.TLabel', font=('Segoe UI',10,'bold'))
            for window in list(self.windows):
                if window.winfo_exists():
                    window.configure(background='#0b1420' if dark else 'SystemButtonFace')
        # Tk can create/reset its native wrapper on first show or re-show even
        # when the app preference hasn't changed. Repair that wrapper as well.
        for window in list(self.windows):
            self.caption(window)
        self.timer = self.root.after(1000, self.refresh)

    def closed(self, event):
        if event.widget == self.root and self.timer:
            self.root.after_cancel(self.timer)
            self.timer = None


def fit_window(window, width, height):
    """Use Tk's font scaling and keep the initial window inside the work area."""
    scale = max(1.0, float(window.tk.call('tk', 'scaling')) / (96 / 72))
    width = min(round(width * scale), window.winfo_screenwidth() - 80)
    height = min(round(height * scale), window.winfo_screenheight() - 100)
    window.geometry(f'{width}x{height}')
    window.minsize(min(width, round(1000 * scale)), min(height, round(760 * scale)))


def wrapped_label(parent, text=None, variable=None, style='Muted.TLabel', **kwargs):
    label = ttk.Label(parent, text=text, textvariable=variable, style=style, wraplength=300, width=1, **kwargs)
    def wrap(event):
        width = max(100, event.width - 40)
        if int(label.cget('wraplength')) != width:
            label.configure(wraplength=width)
            # A canvas fixes its child's allocated height, so changing wrapped
            # text can change the requested height without a Configure event.
            label.after_idle(lambda: label.winfo_toplevel().event_generate('<<OnlineContentChanged>>'))
    parent.bind('<Configure>', wrap, add='+')
    return label


def scrollable_panel(window, parent, padding, min_width):
    """Keep every form reachable on small displays and with enlarged fonts."""
    viewport = ttk.Frame(parent)
    viewport.columnconfigure(0, weight=1)
    viewport.rowconfigure(0, weight=1)
    canvas = tk.Canvas(viewport, highlightthickness=0, borderwidth=0)
    canvas.grid(row=0, column=0, sticky='nsew')
    vertical = ttk.Scrollbar(viewport, orient='vertical', command=canvas.yview)
    horizontal = ttk.Scrollbar(viewport, orient='horizontal', command=canvas.xview)
    canvas.configure(yscrollcommand=vertical.set, xscrollcommand=horizontal.set)
    content = ttk.Frame(canvas, padding=padding)
    content_id = canvas.create_window(0, 0, window=content, anchor='nw')
    def layout_viewport(event=None):
        # Wrapped label requests retain the previous viewport width. Do not
        # feed those requests back into the canvas after maximize/restore.
        scale = max(1.0, float(window.tk.call('tk', 'scaling')) / (96 / 72))
        width = max(canvas.winfo_width(), round(min_width * scale))
        height = max(canvas.winfo_height(), content.winfo_reqheight())
        canvas.itemconfigure(content_id, width=width, height=height)
        canvas.configure(scrollregion=(0, 0, width, height),
                         background=ttk.Style(window).lookup('TFrame', 'background'))
        if height > canvas.winfo_height():
            vertical.grid(row=0, column=1, sticky='ns')
        else:
            vertical.grid_remove()
        if width > canvas.winfo_width():
            horizontal.grid(row=1, column=0, sticky='ew')
        else:
            horizontal.grid_remove()
    canvas.bind('<Configure>', layout_viewport)
    content.bind('<Configure>', layout_viewport)
    canvas.bind('<<ThemeChanged>>', layout_viewport)
    window.bind('<<OnlineContentChanged>>', layout_viewport, add='+')
    # Keyboard traversal also reveals controls on small screens or at high DPI.
    def reveal(event):
        widget = event.widget
        if not str(widget).startswith(str(content) + '.'):
            return
        y = widget.winfo_rooty() - content.winfo_rooty()
        top = canvas.canvasy(0)
        bottom = top + canvas.winfo_height()
        if y < top or y + widget.winfo_height() > bottom:
            canvas.yview_moveto(max(0, y - 12) / max(1, content.winfo_height()))
        x = widget.winfo_rootx() - content.winfo_rootx()
        left = canvas.canvasx(0)
        if x < left or x + widget.winfo_width() > left + canvas.winfo_width():
            canvas.xview_moveto(max(0, x - 12) / max(1, content.winfo_width()))
    window.bind('<FocusIn>', reveal, add='+')
    return viewport, content


def build_browser(window, values, actions):
    """Build the real view without starting network, identity or save operations."""
    fit_window(window, 1080, 860)
    shell = ttk.Frame(window, padding=(24, 16))
    shell.pack(fill='both', expand=True)
    shell.columnconfigure(0, weight=1)
    shell.rowconfigure(1, weight=1)
    header = ttk.Frame(shell)
    header.grid(row=0, column=0, sticky='ew', pady=(0, 18))
    ttk.Label(header, text='TIMESPLITTERS  /  FUTURE PERFECT', style='Eyebrow.TLabel').pack(anchor='w')
    ttk.Label(header, text='Multiplayer', style='Title.TLabel').pack(anchor='w', pady=(3, 2))
    ttk.Label(header, text='Find your friends. Start a match.', style='Muted.TLabel').pack(anchor='w')

    tabs = ttk.Notebook(shell)
    tabs.grid(row=1, column=0, sticky='nsew')
    viewport, play = scrollable_panel(window, tabs, (0, 16, 0, 0), 950)
    directory_viewport, directory = scrollable_panel(window, tabs, 20, 480)
    tabs.add(viewport, text='Play online')
    tabs.add(directory_viewport, text='Directory settings')
    play.columnconfigure(0, weight=3, uniform='panels')
    play.columnconfigure(1, weight=2, uniform='panels')
    play.rowconfigure(0, weight=1)

    join = ttk.Frame(play, style='Card.TFrame', padding=18)
    join.grid(row=0, column=0, sticky='nsew', padx=(0, 12))
    join.columnconfigure(0, weight=1)
    join.rowconfigure(6, weight=1)
    ttk.Label(join, text='Join a friend', style='Section.Card.TLabel').grid(row=0, column=0, sticky='w')
    wrapped_label(join, 'Paste the full invitation your host shared with you.',
                  style='CardMuted.TLabel').grid(row=1, column=0, sticky='ew', pady=(6, 14))
    ttk.Label(join, text='Invitation / lobby code', style='Card.TLabel').grid(row=2, column=0, sticky='w')
    code = ttk.Entry(join, textvariable=values['code'])
    code.grid(row=3, column=0, sticky='ew', pady=(6, 8))
    code.bind('<Return>', lambda event: actions['join_code']())
    ttk.Button(join, text='Join by code', style='Primary.TButton', command=actions['join_code']).grid(
        row=4, column=0, sticky='ew')
    ttk.Label(join, text='PUBLIC LOBBIES', style='CardMuted.TLabel').grid(row=5, column=0, sticky='w', pady=(24, 8))
    listing = ttk.Frame(join)
    listing.grid(row=6, column=0, sticky='nsew')
    listing.columnconfigure(0, weight=1)
    listing.rowconfigure(0, weight=1)
    browser = ttk.Treeview(listing, columns=('name', 'players', 'code'), show='headings', height=4, selectmode='browse')
    for column, title, width in [('name', 'Lobby', 220), ('players', 'PCs', 55), ('code', 'Join code', 100)]:
        browser.heading(column, text=title)
        browser.column(column, width=width, minwidth=45, stretch=column == 'name', anchor='w')
    browser.grid(row=0, column=0, sticky='nsew')
    scrollbar = ttk.Scrollbar(listing, orient='vertical', command=browser.yview)
    scrollbar.grid(row=0, column=1, sticky='ns')
    browser.configure(yscrollcommand=scrollbar.set)
    browser.bind('<Return>', lambda event: actions['join_selected']())
    buttons = ttk.Frame(join, style='Surface.TFrame')
    buttons.grid(row=7, column=0, sticky='ew', pady=(10, 0))
    ttk.Button(buttons, text='Refresh', command=actions['refresh']).pack(side='left')
    ttk.Button(buttons, text='Join selected', command=actions['join_selected']).pack(side='right')
    wrapped_label(join, 'Public lobbies use the address in Directory settings. Refresh to find matches.',
                  style='CardMuted.TLabel').grid(row=8, column=0, sticky='ew', pady=(10, 0))

    host = ttk.Frame(play, style='Card.TFrame', padding=18)
    host.grid(row=0, column=1, sticky='nsew')
    host.columnconfigure(0, weight=1)
    ttk.Label(host, text='Host a match', style='Section.Card.TLabel').grid(row=0, column=0, sticky='w')
    wrapped_label(host, 'Create a lobby and share an invitation.', style='CardMuted.TLabel').grid(
        row=1, column=0, sticky='ew', pady=(6, 14))
    ttk.Label(host, text='Lobby name', style='Card.TLabel').grid(row=2, column=0, sticky='w')
    ttk.Entry(host, textvariable=values['name']).grid(row=3, column=0, sticky='ew', pady=(6, 12))
    options = ttk.Frame(host, style='Surface.TFrame')
    options.grid(row=4, column=0, sticky='ew')
    ttk.Label(options, text='UDP port', style='Card.TLabel').pack(side='left', padx=(0, 8))
    ttk.Entry(options, textvariable=values['port'], width=7).pack(side='left')
    ttk.Checkbutton(options, text='List publicly', variable=values['listed']).pack(side='right')
    ttk.Checkbutton(host, text='Start directory automatically', variable=values['automatic']).grid(
        row=5, column=0, sticky='w', pady=(12, 0))
    ttk.Checkbutton(host, text='Detect public IPv4 automatically', variable=values['detect_ip']).grid(
        row=6, column=0, sticky='w')
    ttk.Label(host, text='Public IPv4 (via ipify)', style='CardMuted.TLabel').grid(
        row=7, column=0, sticky='w', pady=(10, 6))
    ip_row = ttk.Frame(host, style='Surface.TFrame')
    ip_row.grid(row=8, column=0, sticky='ew')
    ip_row.columnconfigure(0, weight=1)
    ttk.Entry(ip_row, textvariable=values['public_ip'], width=14).grid(row=0, column=0, sticky='ew')
    ttk.Button(ip_row, text='Detect', command=actions['detect']).grid(row=0, column=1, padx=(8, 0))
    wrapped_label(host, 'First time hosting? Forward TCP 30820 and your UDP port to this PC. Guests need no port forwarding.',
                  style='CardMuted.TLabel').grid(row=9, column=0, sticky='ew', pady=(10, 8))
    host.rowconfigure(10, weight=1)
    ttk.Button(host, text='Start New Match', style='Primary.TButton', command=actions['host']).grid(
        row=11, column=0, sticky='ew')

    directory.columnconfigure(0, weight=1)
    ttk.Label(directory, text='Shared directory', style='Title.TLabel').grid(row=0, column=0, sticky='w')
    wrapped_label(directory, 'Advanced connection settings for public browsing or a separately hosted directory. '
        'Full invitations fill these in automatically; automatic hosting configures its own directory.').grid(
        row=1, column=0, sticky='ew', pady=(8, 24))
    for row, label, key in [(2, 'Directory address', 'address'), (4, 'Certificate SHA-256', 'pin')]:
        ttk.Label(directory, text=label).grid(row=row, column=0, sticky='w')
        ttk.Entry(directory, textvariable=values[key]).grid(row=row+1, column=0, sticky='ew', pady=(8, 20))
    wrapped_label(directory, 'For a self-signed certificate, paste the fingerprint shared by the directory owner. '
        'To host using this directory, turn off “Start directory automatically” on the Play online tab.').grid(
        row=6, column=0, sticky='ew')

    footer = ttk.Frame(shell)
    footer.grid(row=2, column=0, sticky='ew', pady=(16, 0))
    footer.columnconfigure(0, weight=1)
    status_area = ttk.Frame(footer)
    status_area.grid(row=0, column=0, sticky='ew', padx=(0, 16))
    wrapped_label(status_area, variable=values['status']).pack(fill='x')
    ttk.Button(footer, text='Return to game', command=actions['cancel']).grid(row=0, column=1)
    code.focus_set()
    return browser


def build_invitation(window, invitation_text, code, port):
    fit_window(window, 720, 400)
    viewport, content = scrollable_panel(window, window, 24, 500)
    viewport.pack(fill='both', expand=True)
    ttk.Label(content, text='ONLINE  /  LOBBY READY', style='Eyebrow.TLabel').pack(anchor='w')
    ttk.Label(content, text='Bring your friends.', style='Title.TLabel').pack(anchor='w', pady=(6, 10))
    wrapped_label(content, 'Share this invitation. Your friends can paste it into Invitation / lobby code.').pack(fill='x')
    entry = ttk.Entry(content)
    entry.insert(0, invitation_text)
    entry.configure(state='readonly')
    entry.pack(fill='x', pady=16)
    copied = tk.StringVar(master=window, value=f'Join code: {code}')
    def copy():
        window.clipboard_clear()
        window.clipboard_append(invitation_text)
        copied.set('Invitation copied. Send it to your friends.')
    ttk.Button(content, text='Copy invitation', style='Primary.TButton', command=copy).pack(anchor='w')
    ttk.Label(content, textvariable=copied, style='Eyebrow.TLabel').pack(anchor='w', pady=(12, 8))
    wrapped_label(content, f'Router forwarding: TCP 30820 and UDP {port} to this PC.\n'
        'Return to the game and select Start New Match to choose your map and mode.').pack(fill='x')
    ttk.Button(content, text='Return to game', command=window.destroy).pack(side='bottom', anchor='e', pady=(12, 0))


def preview():
    """Offline visual preview; no game files, saved settings or network calls."""
    import argparse
    parser = argparse.ArgumentParser(description=preview.__doc__)
    parser.add_argument('--invitation', action='store_true')
    args = parser.parse_args()
    window = tk.Tk()
    window.title('TimeSplitters Multiplayer — offline preview')
    theme = OnlineTheme(window)
    if args.invitation:
        build_invitation(window, 'Example invitation — ask your host for a real one', 'EXAMPLE', 30821)
    else:
        values = {key: tk.StringVar(master=window, value=value) for key, value in dict(
            address='https://127.0.0.1:30820', pin='', name='TimeSplitters lobby', port='30821',
            code='', public_ip='', status='Offline preview — sample lobbies, no connection.').items()}
        values.update({key: tk.BooleanVar(master=window, value=True) for key in ('listed','automatic','detect_ip')})
        def sample_action():
            values['status'].set('Preview only — open Online in the game to connect.')
        actions = dict.fromkeys(('refresh','join_selected','join_code','host','detect'), sample_action)
        actions['cancel'] = window.destroy
        browser = build_browser(window, values, actions)
        browser.insert('', 'end', values=('Friday night arcade (example)', '2/4', 'EXAMPLE'))
        browser.insert('', 'end', values=('Team deathmatch (example)', '1/4', 'SAMPLE'))
    window.mainloop()


if __name__ == '__main__':
    preview()
