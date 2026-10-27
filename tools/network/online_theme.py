"""Windows app-mode palette for the separate Online Tk windows."""

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
    import winreg
    class Contrast(ctypes.Structure):
        _fields_ = [('size', wintypes.UINT), ('flags', wintypes.DWORD), ('scheme', wintypes.LPWSTR)]
    contrast = Contrast()
    contrast.size = ctypes.sizeof(contrast)
    if ctypes.windll.user32.SystemParametersInfoW(0x42, contrast.size, ctypes.byref(contrast), 0) and contrast.flags & 1:
        return False
    try:
        with winreg.OpenKey(winreg.HKEY_CURRENT_USER, r'Software\Microsoft\Windows\CurrentVersion\Themes\Personalize') as key:
            return winreg.QueryValueEx(key, 'AppsUseLightTheme')[0] == 0
    except OSError:
        return False


class OnlineTheme:
    def __init__(self, window, read_dark=system_dark):
        self.root = window
        self.read_dark = read_dark
        self.windows = weakref.WeakSet()
        self.style = ttk.Style(window)
        self.original = self.style.theme_use()
        self.images = []
        if 'TSFPOnlineDark' not in self.style.theme_names():
            self.style.theme_create('TSFPOnlineDark', parent='vista' if 'vista' in self.style.theme_names() else self.original)
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
        fill_rgb, edge_rgb, outside = colors(fill), colors(border), colors('#202020')
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
        normal=self.surface('#333333','#505050')
        hot=self.surface('#3d3d3d','#646464')
        pressed=self.surface('#292929','#484848')
        focus=self.surface('#333333','#60b9fa')
        disabled=self.surface('#292929','#3a3a3a')
        self.style.element_create('TSFP.Button','image',normal,('disabled',disabled),('pressed',pressed),('focus',focus),('active',hot),border=6,sticky='nswe')
        self.style.layout('TButton',[('TSFP.Button',{'sticky':'nswe','children':[('Button.padding',{'sticky':'nswe','children':[('Button.label',{'sticky':'nswe'})]})]})])
        def indicator(fill,edge,checked=False):
            box=self.surface(fill,edge,18,18,4,checked)
            padded=tk.PhotoImage(master=self.root,width=24,height=18)
            padded.put('#202020',to=(0,0,24,18))
            padded.tk.call(str(padded),'copy',str(box),'-to',0,0)
            self.images.append(padded)
            return padded
        unchecked=indicator('#2b2b2b','#aaaaaa')
        check=indicator('#087fc4','#60b9fa',True)
        check_hot=indicator('#1899dd','#8bd2ff',True)
        off_disabled=indicator('#292929','#555555')
        on_disabled=indicator('#555555','#777777',True)
        self.style.element_create('TSFP.Check','image',unchecked,('disabled','selected',on_disabled),('disabled',off_disabled),('active','selected',check_hot),('selected',check),border=0,sticky='')
        self.style.layout('TCheckbutton',[('Checkbutton.padding',{'sticky':'nswe','children':[('TSFP.Check',{'side':'left','sticky':''}),('Checkbutton.focus',{'side':'left','sticky':'w','children':[('Checkbutton.label',{'sticky':'nswe'})]})]})])
        field=self.surface('#2b2b2b','#606060')
        focused_field=self.surface('#2b2b2b','#60b9fa')
        self.style.element_create('TSFP.Entry','image',field,('focus',focused_field),border=5,sticky='nswe')
        self.style.layout('TEntry',[('TSFP.Entry',{'sticky':'nswe','children':[('Entry.padding',{'sticky':'nswe','children':[('Entry.textarea',{'sticky':'nswe'})]})]})])
        self.style.element_create('TSFP.Tree','from','clam','Treeview.field')
        self.style.layout('Treeview',[('TSFP.Tree',{'sticky':'nswe','children':[('Treeview.padding',{'sticky':'nswe','children':[('Treeview.treearea',{'sticky':'nswe'})]})]})])
        self.style.element_create('TSFP.Heading','image',self.surface('#2b2b2b','#404040',40,28,0),border=3,sticky='nswe')
        self.style.layout('Treeview.Heading',[('TSFP.Heading',{'sticky':'nswe','children':[('Treeheading.padding',{'sticky':'nswe','children':[('Treeheading.text',{'sticky':'we'})]})]})])
        self.style.configure('TButton',padding=(8,4))
        self.style.configure('TCheckbutton',padding=(0,3))
        self.style.theme_use(previous)

    def track(self, window):
        self.windows.add(window)
        window.bind('<Map>', lambda event: window.after_idle(lambda: self.caption(window))
                    if event.widget == window else None, add='+')
        window.after_idle(lambda: self.caption(window))

    def caption(self, window):
        if not window.winfo_exists() or not window.winfo_viewable():
            return
        window.configure(background='#202020' if self.dark else 'SystemButtonFace')
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
                self.style.configure('.', background='#202020', foreground='#f0f0f0',
                                     fieldbackground='#303030', bordercolor='#606060',
                                     lightcolor='#404040', darkcolor='#202020', insertcolor='#f0f0f0')
                self.style.configure('Treeview', background='#202020', fieldbackground='#202020', foreground='#f0f0f0')
                self.style.configure('TEntry', fieldbackground='#303030', foreground='#f0f0f0', insertcolor='#f0f0f0')
                self.style.configure('TButton', background='#303030')
                self.style.map('TButton', background=[('active', '#454545')], foreground=[('disabled', '#999999')])
                self.style.map('TEntry', fieldbackground=[('readonly', '#303030')], foreground=[('readonly', '#f0f0f0')])
                self.style.map('Treeview', background=[('selected', '#325a80')], foreground=[('selected', '#ffffff')])
                self.style.map('TCheckbutton', background=[('active', '#303030')], foreground=[('disabled', '#999999')])
                self.style.configure('Treeview.Heading', background='#303030', foreground='#f0f0f0')
                self.style.map('Treeview.Heading', background=[('active', '#454545')])
            else:
                # Restore native Windows colors, including high-contrast themes.
                self.style.theme_use(self.original)
            for window in list(self.windows):
                if window.winfo_exists():
                    window.configure(background='#202020' if dark else 'SystemButtonFace')
        # Tk can create/reset its native wrapper on first show or re-show even
        # when the app preference hasn't changed. Repair that wrapper as well.
        for window in list(self.windows):
            self.caption(window)
        self.timer = self.root.after(1000, self.refresh)

    def closed(self, event):
        if event.widget == self.root and self.timer:
            self.root.after_cancel(self.timer)
            self.timer = None
