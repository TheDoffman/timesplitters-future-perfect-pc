"""Theme real Tk widgets without changing the user's Windows preferences."""


import sys
import json
import subprocess
from pathlib import Path

import pytest

def run_tk_check(name, *args):
    # Each Online application owns one Tcl interpreter. Repeated Tk teardown
    # in pytest's long-lived process can break later Tcl initialization;
    # keep real widgets/assertions, but give each lifecycle its own process.
    try:
        result = subprocess.run([
            sys.executable, '-c',
            'import faulthandler,json,runpy,sys; faulthandler.dump_traceback_later(20,repeat=True); '
            'runpy.run_path(sys.argv[1])[sys.argv[2]](*json.loads(sys.argv[3]))',
            str(Path(__file__).resolve()), name, json.dumps(args),
        ], capture_output=True, text=True, timeout=60)
    except subprocess.TimeoutExpired as error:
        # TimeoutExpired keeps captured bytes even with text=True. Surface the
        # child's delayed stacks; the default short traceback hides them.
        def decoded(value):
            return value.decode('utf-8', errors='replace') if isinstance(value, bytes) else (value or '')
        pytest.fail(f'{name}{args} timed out after 60 seconds\n'
                    + decoded(error.stdout) + decoded(error.stderr), pytrace=False)
    assert result.returncode == 0, result.stdout + result.stderr


def test_tk_timeout_preserves_child_stack(monkeypatch):
    def timeout(*args, **kwargs):
        raise subprocess.TimeoutExpired(args[0], 60, output=b'child progress\n',
                                        stderr=b'Timeout (0:00:20)!\n  File "tkinter", line 7 in update\n')
    monkeypatch.setattr(subprocess, 'run', timeout)
    with pytest.raises(pytest.fail.Exception, match='(?s)child progress.*line 7 in update'):
        run_tk_check('fixture')



def test_viewport_converges_after_repeated_geometry_notifications(monkeypatch):
    sys.path.insert(0, str(Path(__file__).resolve().parents[2] / 'tools/network'))
    import online_theme
    from types import SimpleNamespace
    pending, canvases = [], []

    class Widget:
        def __init__(self, *args, **kwargs):
            self.bindings = {}
        def columnconfigure(self, *args, **kwargs): pass
        def rowconfigure(self, *args, **kwargs): pass
        def grid(self, *args, **kwargs): pass
        def grid_remove(self): pass
        def bind(self, event, callback, **kwargs): self.bindings[event] = callback
        def winfo_reqheight(self): return 400

    class Canvas(Widget):
        def __init__(self, *args, **kwargs):
            super().__init__(); self.width = 800; self.requests = 0
            canvases.append(self)
        def winfo_width(self): return self.width
        def winfo_height(self): return 300
        def configure(self, **kwargs): pass
        def create_window(self, *args, window, **kwargs):
            self.content = window; return 1
        def itemconfigure(self, *args, **kwargs):
            self.requests += 1
            pending.append(self.content.bindings['<Configure>'])
        def yview(self, *args): pass
        def xview(self, *args): pass

    monkeypatch.setattr(online_theme.tk, 'Canvas', Canvas)
    monkeypatch.setattr(online_theme.ttk, 'Frame', Widget)
    monkeypatch.setattr(online_theme.ttk, 'Scrollbar', lambda *a, **k: SimpleNamespace(
        set=lambda *a: None, grid=lambda **k: None, grid_remove=lambda: None))
    monkeypatch.setattr(online_theme.ttk, 'Style', lambda *a: SimpleNamespace(lookup=lambda *a: '#111111'))
    window = Widget(); window.tk = SimpleNamespace(call=lambda *a: 96 / 72)
    online_theme.scrollable_panel(window, Widget(), 0, 950)
    canvas = canvases[0]
    for width in (800, 1200, 800):
        canvas.width = width
        pending.append(canvas.bindings['<Configure>'])
        for _ in range(10):
            if not pending: break
            pending.pop(0)()
        assert not pending, 'Unchanged geometry must settle even if Tk sends Configure again'
    assert canvas.requests == 3  # Real viewport changes still reach the canvas.

@pytest.mark.skipif(sys.platform not in ('win32', 'darwin'), reason='Desktop Online adapter')
@pytest.mark.parametrize('original_theme', [None, 'clam'])
def test_online_theme_switch_and_close(original_theme):
    run_tk_check('_check_online_theme_switch_and_close', original_theme)

def _check_online_theme_switch_and_close(original_theme):
    import tkinter as tk
    from tkinter import ttk
    sys.path.insert(0,str(Path(__file__).resolve().parents[2]/'tools/network'))
    from online_theme import OnlineTheme
    window=tk.Tk();window.withdraw();dark=[False]
    if original_theme: ttk.Style(window).theme_use(original_theme)
    theme=OnlineTheme(window,lambda:dark[0])
    popup=tk.Toplevel(window);popup.withdraw();theme.track(popup)
    entry=ttk.Entry(popup,state='readonly');entry.pack()
    original=theme.original
    try:
        dark[0]=True;theme.refresh();window.update_idletasks()
        assert theme.style.theme_use()=='TSFPOnlineDark'
        assert theme.style.layout('TButton')[0][0]=='TSFP.Button'
        assert 'Button.padding' in str(theme.style.layout('TButton'))
        assert 'TSFP.Check' in str(theme.style.layout('TCheckbutton'))
        assert theme.style.lookup('TEntry','foreground',('readonly',))=='#e4edf5'
        assert theme.style.lookup('Treeview','background')=='#0b1420'
        assert popup.cget('background')=='#0b1420'
        dark[0]=False;theme.refresh()
        assert theme.style.theme_use()==original
        assert popup.cget('background') == ('SystemButtonFace' if sys.platform == 'win32' else theme.style.lookup('TFrame', 'background'))
    finally:
        window.destroy()
    assert theme.timer is None


@pytest.mark.skipif(sys.platform not in ('win32', 'darwin'), reason='Desktop Online adapter')
@pytest.mark.parametrize('scale', [1, 1.5, 2])
def test_control_corners_and_scrollbar_input(scale):
    run_tk_check('_check_control_corners_and_scrollbar_input', scale)


def _check_control_corners_and_scrollbar_input(scale):
    import tkinter as tk
    from tkinter import ttk
    sys.path.insert(0, str(Path(__file__).resolve().parents[2] / 'tools/network'))
    from online_theme import OnlineTheme
    root = tk.Tk()
    root.tk.call('tk', 'scaling', scale * 96 / 72)
    theme = OnlineTheme(root, lambda: True)
    try:
        surface = theme.surface('#101e2d', '#3b5268')
        assert surface.transparency_get(0, 0)
        assert not surface.transparency_get(20, 15)
        for style, background in [('TEntry', '#0b1420'), ('TButton', '#0b1420'),
                ('Primary.TButton', '#0b1420'), ('Card.TEntry', '#152a3d'),
                ('Card.TButton', '#152a3d'), ('Card.Primary.TButton', '#152a3d')]:
            for state in ((), ('active',), ('pressed',), ('disabled',), ('focus',)):
                assert theme.style.lookup(style, 'background', state) == background
        for orientation in ('vertical', 'horizontal'):
            calls = []
            bar = ttk.Scrollbar(root, orient=orientation, command=lambda *args: calls.append(args))
            bar.pack(fill='y' if orientation == 'vertical' else 'x')
            bar.set(.25, .5)
            root.update()
            width, height = bar.winfo_width(), bar.winfo_height()
            size = width if orientation == 'vertical' else height
            assert size >= round(18 * scale) - 1
            for delta, direction in [(-1, 'up' if orientation == 'vertical' else 'left'),
                                     (1, 'down' if orientation == 'vertical' else 'right')]:
                x = width//2 if orientation == 'vertical' else (size//2 if delta < 0 else width-size//2)
                y = height//2 if orientation == 'horizontal' else (size//2 if delta < 0 else height-size//2)
                assert bar.identify(x, y).endswith(direction+'arrow')
                bar.event_generate('<ButtonPress-1>', x=x, y=y)
                bar.event_generate('<ButtonRelease-1>', x=x, y=y)
                root.update()
                assert calls[-1] == ('scroll', str(delta), 'units')
            bar.destroy()
    finally:
        root.destroy()


@pytest.mark.skipif(sys.platform != 'win32', reason='Windows DWM adapter')
def test_caption_after_map_reshow_and_native_reset():
    run_tk_check('_check_caption_after_map_reshow_and_native_reset')

def _check_caption_after_map_reshow_and_native_reset():
    import ctypes
    from ctypes import wintypes as w
    import tkinter as tk
    sys.path.insert(0,str(Path(__file__).resolve().parents[2]/'tools/network'))
    from online_theme import OnlineTheme
    root=tk.Tk();root.withdraw();root.geometry('300x120')
    dark=[True];theme=OnlineTheme(root,lambda:dark[0])
    popup=tk.Toplevel(root);popup.withdraw();theme.track(popup)
    ancestor=ctypes.windll.user32.GetAncestor;ancestor.argtypes=[w.HWND,w.UINT];ancestor.restype=w.HWND
    get=ctypes.windll.dwmapi.DwmGetWindowAttribute;get.argtypes=[w.HWND,w.DWORD,ctypes.c_void_p,w.DWORD]
    set_attribute=ctypes.windll.dwmapi.DwmSetWindowAttribute;set_attribute.argtypes=get.argtypes
    def current(window):
        value=ctypes.c_int(-1)
        assert get(ancestor(window.winfo_id(),2),20,ctypes.byref(value),4)>=0
        return value.value
    try:
        for window in (root,popup):
            window.deiconify();root.update();root.update_idletasks()
            assert current(window)==1
            window.withdraw();root.update();window.deiconify();root.update()
            assert current(window)==1
            value=ctypes.c_int(0)
            assert set_attribute(ancestor(window.winfo_id(),2),20,ctypes.byref(value),4)>=0
            theme.refresh()  # Preference unchanged, native wrapper reset.
            assert current(window)==1
        dark[0]=False;theme.refresh()
        assert current(root)==0 and current(popup)==0
    finally:
        root.destroy()


@pytest.mark.skipif(sys.platform not in ('win32', 'darwin'), reason='Desktop Online adapter')
@pytest.mark.parametrize('scale, branded', [(1, True), (1.5, True), (2, True), (1, False)])
def test_browser_layout_and_commands(scale, branded):
    run_tk_check('_check_browser_layout_and_commands', scale, branded)


def _check_browser_layout_and_commands(scale, branded):
    import tkinter as tk
    from tkinter import ttk
    sys.path.insert(0, str(Path(__file__).resolve().parents[2] / 'tools/network'))
    from online_theme import OnlineTheme, build_browser, build_invitation
    root = tk.Tk()
    # Reproduce the hosted Windows runner's small display on large developer monitors.
    root.winfo_screenwidth = lambda: 1280
    root.winfo_screenheight = lambda: 768
    root.tk.call('tk', 'scaling', scale * 96 / 72)
    theme = OnlineTheme(root, lambda: branded)
    values = {key: tk.StringVar(master=root, value=value) for key, value in dict(
        address='https://127.0.0.1:30820', pin='', name='Example', port='30821',
        code='EXAMPLE', public_ip='', status='Ready').items()}
    values.update({key: tk.BooleanVar(master=root, value=True) for key in ('listed', 'automatic', 'detect_ip')})
    calls = []
    actions = {key: lambda key=key: calls.append(key) for key in (
        'refresh', 'join_selected', 'join_code', 'host', 'detect', 'cancel')}
    browser = build_browser(root, values, actions)
    def children(widget):
        for child in widget.winfo_children():
            yield child
            yield from children(child)
    def check_bounds():
        root.update()
        for child in children(root):
            if not child.winfo_ismapped():
                continue
            parent = child.master
            if isinstance(parent, tk.Canvas):
                continue  # Embedded content is reachable through its scrollbars.
            assert child.winfo_x() >= 0 and child.winfo_y() >= 0, str(child)
            assert child.winfo_x() + child.winfo_width() <= parent.winfo_width() + 2, (
                str(child), child.winfo_x(), child.winfo_width(), parent.winfo_width())
            assert child.winfo_y() + child.winfo_height() <= parent.winfo_height() + 2, (
                str(child), child.winfo_y(), child.winfo_height(), parent.winfo_height())
            if isinstance(child, (ttk.Button, ttk.Checkbutton, ttk.Label)):
                assert child.winfo_width() >= child.winfo_reqwidth() - 2, (str(child), child.cget('text'))
                assert child.winfo_height() >= child.winfo_reqheight() - 2, (str(child), child.cget('text'))
    try:
        check_bounds()
        for text, action in [('Refresh', 'refresh'), ('Join selected', 'join_selected'),
                ('Join by code', 'join_code'), ('Start New Match', 'host'),
                ('Detect', 'detect'), ('Return to game', 'cancel')]:
            button = next(w for w in children(root) if isinstance(w, ttk.Button) and w.cget('text') == text)
            button.invoke()
            assert calls[-1] == action
        assert values['code'].get() == 'EXAMPLE' and values['port'].get() == '30821'
        tabs = next(w for w in children(root) if isinstance(w, ttk.Notebook))
        tabs.select(1)
        check_bounds()
        tabs.select(0)
        # Growing and shrinking must not retain the labels' old wrap width.
        root.geometry('2400x1100')
        root.update()
        root.geometry('1080x860')
        check_bounds()
        # Minimum supported size must keep controls reachable.
        width, height = root.minsize()
        root.geometry(f'{width}x{height}')
        check_bounds()
        # Simulate a short laptop viewport without changing system display settings.
        root.minsize(0, 0)
        root.geometry('1000x620')
        check_bounds()
        host_button = next(w for w in children(root) if isinstance(w, ttk.Button)
                           and w.cget('text') == 'Start New Match')
        host_button.focus_force()
        root.update()
        canvas = next(w for w in children(root) if isinstance(w, tk.Canvas))
        assert canvas.winfo_rooty() <= host_button.winfo_rooty()
        assert host_button.winfo_rooty() + host_button.winfo_height() <= canvas.winfo_rooty() + canvas.winfo_height()
        tabs.select(1)
        check_bounds()
        directory_view = root.nametowidget(tabs.tabs()[1])
        certificate = [w for w in children(directory_view) if isinstance(w, ttk.Entry)][-1]
        certificate.focus_force()
        root.update()
        directory_canvas = next(w for w in children(directory_view) if isinstance(w, tk.Canvas))
        assert directory_canvas.winfo_rooty() <= certificate.winfo_rooty()
        assert certificate.winfo_rooty() + certificate.winfo_height() <= directory_canvas.winfo_rooty() + directory_canvas.winfo_height()
        popup = tk.Toplevel(root)
        popup.winfo_screenwidth = lambda: 1280
        popup.winfo_screenheight = lambda: 768
        theme.track(popup)
        build_invitation(popup, 'Example invitation', 'EXAMPLE', 30821)
        root.update()
        # Popup descendants are checked separately because Toplevel positions
        # are screen-relative, not relative to their logical owner.
        for child in children(popup):
            if isinstance(child, ttk.Scrollbar) and not child.winfo_ismapped():
                continue
            assert child.winfo_ismapped()
            if not isinstance(child.master, tk.Canvas):
                assert child.winfo_y() + child.winfo_height() <= child.master.winfo_height() + 2
        return_button = next(w for w in children(popup) if isinstance(w, ttk.Button) and w.cget('text') == 'Return to game')
        return_button.focus_force()
        root.update()
        popup_canvas = next(w for w in children(popup) if isinstance(w, tk.Canvas))
        assert popup_canvas.winfo_rooty() <= return_button.winfo_rooty()
        assert return_button.winfo_rooty() + return_button.winfo_height() <= popup_canvas.winfo_rooty() + popup_canvas.winfo_height()
        popup.destroy()
    finally:
        root.destroy()
