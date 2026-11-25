"""Translate the canonical appearance helpers' framework calls, never launcher policy.

Like generate_layout, this keeps the approved layout in one source. Generated
files stay in the build directory and compile in both Avalonia host projects.
"""
from pathlib import Path
import re

def generate(source, destination):
    parts = []
    imports = '''using System;
using System.Linq;
using System.Reflection;
using System.Collections.Generic;
using Avalonia;
using Avalonia.Controls;
using Avalonia.Controls.Primitives;
using Avalonia.Layout;
using Avalonia.LogicalTree;
using Avalonia.Media;
using Avalonia.Automation;
using TimeSplitters.Launcher;
using DependencyObject = Avalonia.Controls.Control;
using FrameworkElement = Avalonia.Controls.Control;
using UIElement = Avalonia.Controls.Control;
using Brush = Avalonia.Media.IBrush;
using FontWeights = Avalonia.Media.FontWeight;
'''
    for name in ('MinimalSettings', 'MinimalTools', 'Restyle'):
        text = (Path(source) / (name + '.cs')).read_text(encoding='utf-8-sig')
        text = re.sub(r'^using .*;\s*', '', text, flags=re.M)
        text = re.sub(r'\(Brush\)new BrushConverter\(\)\.ConvertFromString\((\w+)\)', r'Ui.Brush(\1)', text)
        text = text.replace('LogicalTreeHelper.GetChildren(root)', 'root.GetLogicalChildren()')
        text = text.replace('System.Windows.Automation.AutomationProperties', 'AutomationProperties')
        text = re.sub(r'ToolTip=(?:"[^"]*"|names\[index\]|help.Text),', '', text)  # explicit help is assigned below for the back action
        text = text.replace('help.ToolTip??help.Text', 'ToolTip.GetTip(help)??help.Text')
        text = text.replace('caption.ToolTip=ToolTip.GetTip(help)??help.Text;', 'ToolTip.SetTip(caption,ToolTip.GetTip(help)??help.Text);')
        text = re.sub(r',ToolTip=(?:"[^"]*"|help.Text)(?=})', '', text)
        text = text.replace('toolsButton.Click+=', 'Ui.Tip(toolsButton,"Return to Launcher tools. Your unsaved settings stay in this session.");toolsButton.Click+=')
        text = text.replace('VerticalScrollBarVisibility=ScrollBarVisibility.Auto,HorizontalScrollBarVisibility=ScrollBarVisibility.Disabled,', '')
        text = text.replace('scroll.ScrollToHome()', 'Ui.ScrollHome(scroll)')
        text = text.replace('e.Source==', 'e.Source==')
        text = text.replace('overlay.Visibility!=Visibility.Visible', '!overlay.IsVisible')
        text = text.replace('System.ComponentModel.DependencyPropertyDescriptor.FromProperty(UIElement.VisibilityProperty,typeof(Border)).AddValueChanged(overlay,(s,e)=>syncModal());', 'overlay.PropertyChanged+=(s,e)=>{if(e.Property==Control.IsVisibleProperty)syncModal();};')
        text = re.sub(r'(\w+)\.Visibility=(\w+)\.Visibility==Visibility.Visible\?Visibility.Collapsed:Visibility.Visible', r'\1.IsVisible=!\2.IsVisible', text)
        text = re.sub(r'([\w.\[\]()]+)\.Visibility=([^;]+?)\?Visibility.Visible:Visibility.Collapsed', r'\1.IsVisible=\2', text)
        text = re.sub(r'([\w.\[\]()]+)\.Visibility=Visibility.Collapsed', r'\1.IsVisible=false', text)
        text = re.sub(r'([\w.\[\]()]+)\.Visibility=Visibility.Visible', r'\1.IsVisible=true', text)
        text = text.replace('if(progress.Visibility==Visibility.Hidden)progress.Visibility=Visibility.Collapsed;', 'if(!progress.IsVisible)progress.IsVisible=false;')
        text = text.replace('if(progress.Visibility==Visibility.Hidden)progress.IsVisible=false;', 'if(!progress.IsVisible)progress.IsVisible=false;')
        if name == 'Restyle':
            text = text.replace('var ctrl=root as Control;', 'var ctrl=root as TemplatedControl;')
        text = text.replace('.OfType<Control>()', '.OfType<TemplatedControl>()')
        text = text.replace('SolidColorBrush', 'ISolidColorBrush')
        text = text.replace('button.Background==Brushes.Transparent', '(button.Background is ISolidColorBrush transparent && transparent.Color.A==0)')
        parts.append(text)
    Path(destination).write_text(imports + '\n'.join(parts), encoding='utf-8')
