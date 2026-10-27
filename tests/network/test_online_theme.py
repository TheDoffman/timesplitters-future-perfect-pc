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
    result = subprocess.run([
        sys.executable, '-c',
        'import json,runpy,sys; runpy.run_path(sys.argv[1])[sys.argv[2]](*json.loads(sys.argv[3]))',
        str(Path(__file__).resolve()), name, json.dumps(args),
    ], capture_output=True, text=True, timeout=60)
    assert result.returncode == 0, result.stdout + result.stderr

@pytest.mark.skipif(sys.platform != 'win32', reason='Windows Online adapter')
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
        assert theme.style.lookup('TEntry','foreground',('readonly',))=='#f0f0f0'
        assert theme.style.lookup('Treeview','background')=='#202020'
        assert popup.cget('background')=='#202020'
        dark[0]=False;theme.refresh()
        assert theme.style.theme_use()==original
        assert popup.cget('background')=='SystemButtonFace'
    finally:
        window.destroy()
    assert theme.timer is None


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
