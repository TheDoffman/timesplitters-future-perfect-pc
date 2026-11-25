"""Translate the shared Windows home layout into Avalonia's equivalent syntax.

The source of geometry, labels, artwork placement and colors stays Launcher.xaml.
Only UI framework syntax and host wording are adapted here; no game policy.
"""
from pathlib import Path
import sys
import xml.etree.ElementTree as ET
W='http://schemas.microsoft.com/winfx/2006/xaml/presentation'
A='https://github.com/avaloniaui'
X='http://schemas.microsoft.com/winfx/2006/xaml'
ET.register_namespace('',A);ET.register_namespace('x',X)
def local(tag):return tag.rsplit('}',1)[-1]
def generate(source,destination):
 root=ET.parse(source).getroot();root.set('{'+X+'}Class','TimeSplitters.Launcher.NativeWindow')
 navigation_focus=root.find(".//{"+W+"}Style[@{"+X+"}Key='NavigationFocus']//{"+W+"}Border")
 styles=ET.Element('{'+A+'}Window.Styles')
 ET.SubElement(styles,'{'+A+'}StyleInclude',{'Source':'/PageTheme.axaml'})
 for resource in list(root.find('{'+W+'}Window.Resources')):
  if local(resource.tag)!='Style':continue
  key=resource.attrib.get('{'+X+'}Key');target=resource.attrib['TargetType']
  style=ET.SubElement(styles,'{'+A+'}Style',{'Selector':target+('.'+key if key else '')})
  for setter in resource:
   if setter.attrib.get('Property') in ('Template','FocusVisualStyle','StrokeLineJoin','StrokeEndLineCap'):continue
   if 'Value' not in setter.attrib:continue
   if setter.attrib['Property'] in ('StrokeStartLineCap','StrokeEndLineCap'):setter.set('Property','StrokeLineCap')
   if setter.attrib['Property']=='Stroke':setter.set('Value','#B4C7D8')
   style.append(setter)
  if target=='Button':
   if navigation_focus is not None:
    focus_style=ET.SubElement(styles,'{'+A+'}Style',{'Selector':target+('.'+key if key else '')+':focus-visible /template/ Border#Face'})
    ET.SubElement(focus_style,'{'+A+'}Setter',{'Property':'BorderBrush','Value':navigation_focus.attrib['BorderBrush']})
   template=resource.find('.//{'+W+'}ControlTemplate')
   if template is not None:
    converted=template_styles(styles,template,target+('.'+key if key else ''))
    setter=ET.SubElement(style,'{'+A+'}Setter',{'Property':'Template'});setter.append(converted)
    # An implicit WPF style replaces, rather than merges, theme defaults.
    ET.SubElement(style,'{'+A+'}Setter',{'Property':'MinHeight','Value':'0'})
  root.find('{'+W+'}Window.Resources').remove(resource)
 # Avalonia's default button alignment and expander theme differ from WPF.
 for selector,values in [('TabItem',{'FontSize':'13','Padding':'16,12'}),('Button',{'HorizontalAlignment':'Stretch','CornerRadius':'3'}),('Expander',{'HorizontalAlignment':'Stretch','Background':'Transparent'}),('ComboBox',{'Background':'#102031','BorderBrush':'#416685','Foreground':'#E4EDF5'})]:
  style=ET.SubElement(styles,'{'+A+'}Style',{'Selector':selector})
  for key,value in values.items():ET.SubElement(style,'{'+A+'}Setter',{'Property':key,'Value':value})
 root.insert(1,styles)
 root.find('{'+W+'}Grid').set('Background','#0B1420')
 for element in root.iter():
  if element.tag.startswith('{'+W+'}'):element.tag='{'+A+'}'+local(element.tag)
  for key,value in list(element.attrib.items()):
   if key in ('SnapsToDevicePixels','CanContentScroll','ToolTipService.ShowOnDisabled','RenderOptions.BitmapScalingMode','StrokeLineJoin'):del element.attrib[key]
   elif key=='Visibility':del element.attrib[key];element.set('IsVisible','True' if value=='Visible' else 'False')
   elif key=='ToolTip':del element.attrib[key];element.set('ToolTip.Tip',value.replace('File Explorer','Finder'))
   elif key=='Style':del element.attrib[key];element.set('Classes',value.removeprefix('{StaticResource ').removesuffix('}'))
   elif key in ('StrokeStartLineCap','StrokeEndLineCap'):del element.attrib[key];element.set('StrokeLineCap',value)
   elif key in ('StartPoint','EndPoint'):element.set(key,','.join(str(float(n)*100)+'%' for n in value.split(',')))
   elif key=='MinHeight' and value.startswith('{Binding'):element.set(key,'360')
   elif key=='StrokeDashArray':element.set(key,value.replace(' ',','))
   elif key=='FontFamily':element.set(key,'Georgia, Liberation Serif, DejaVu Serif' if value=='Georgia' else 'Menlo' if value=='Consolas' else 'Arial')
   elif value=='WINDOWS x64':element.set(key,'macOS ARM64')
   elif value=='NATIVE PC PORT':element.set(key,'NATIVE MAC PORT')
 ET.indent(root)
 Path(destination).write_text(ET.tostring(root,encoding='unicode')+'\n',encoding='utf-8')
def simple_theme(source,destination):
 root=ET.parse(source).getroot();styles=ET.Element('{'+A+'}Styles')
 for resource in root:
  if local(resource.tag)!='Style' or '{'+X+'}Key' in resource.attrib:continue
  target=resource.attrib['TargetType'];style=ET.SubElement(styles,'{'+A+'}Style',{'Selector':target})
  for setter in resource:
   if 'Value' not in setter.attrib or setter.attrib.get('Property') in ('Template','FocusVisualStyle','ScrollViewer.HorizontalScrollBarVisibility','ScrollViewer.VerticalScrollBarVisibility'):continue
   ET.SubElement(style,'{'+A+'}Setter',dict(setter.attrib))
 ET.indent(styles);Path(destination).write_text(ET.tostring(styles,encoding='unicode')+'\n',encoding='utf-8')

# Control templates translate framework syntax, retaining the shared source's
# geometry, colors, padding, hover/pressed/focus/disabled values.
def template_node(node):
 import copy
 node=copy.deepcopy(node)
 for item in node.iter():
  item.tag='{'+A+'}'+local(item.tag)
  item.attrib.pop('TargetType',None)
  if local(item.tag)=='ContentPresenter':
   item.attrib.pop('ContentSource',None)
   item.set('Content','{TemplateBinding Content}')
   item.set('ContentTemplate','{TemplateBinding ContentTemplate}')
   item.set('Foreground','{TemplateBinding Foreground}')
  if 'RenderTransformOrigin' in item.attrib:item.set('RenderTransformOrigin','50%,50%')
 return node

def template_styles(styles,template,selector):
 import copy
 template=copy.deepcopy(template)
 triggers=template.find('{'+W+'}ControlTemplate.Triggers')
 states={'IsMouseOver':':pointerover','IsPressed':':pressed','IsKeyboardFocused':':focus-visible','IsEnabled':':disabled','IsChecked':':checked'}
 if triggers is not None:
  template.remove(triggers)
  for trigger in triggers:
   pseudo=states[trigger.attrib['Property']]
   for setter in trigger:
    name=setter.attrib.get('TargetName');target=next((n for n in template.iter() if n.attrib.get('{'+X+'}Name')==name),None)
    target_selector=selector+pseudo+(' /template/ '+local(target.tag)+'#'+name if target is not None else '')
    style=ET.SubElement(styles,'{'+A+'}Style',{'Selector':target_selector})
    converted=template_node(setter);converted.attrib.pop('TargetName',None);style.append(converted)
 return template_node(template)

def generate_theme(source,destination):
 simple_theme(source,destination)
 source_root=ET.parse(source).getroot();styles=ET.parse(destination).getroot()
 # Framework defaults differ: WPF stretches fields, Avalonia aligns them left.
 for selector in ('ComboBox','TextBox'):
  style=ET.SubElement(styles,'{'+A+'}Style',{'Selector':selector})
  ET.SubElement(style,'{'+A+'}Setter',{'Property':'HorizontalAlignment','Value':'Stretch'})
 for source_style in source_root:
  target=source_style.attrib.get('TargetType')
  if target=='Button':
   # WPF's keyboard-only focus adorner maps to native :focus-visible.
   focus=source_root.find(".//{"+W+"}Style[@{"+X+"}Key='NavigationFocus']//{"+W+"}Border")
   if focus is not None:
    style=ET.SubElement(styles,'{'+A+'}Style',{'Selector':'Button:focus-visible /template/ Border#Face'})
    ET.SubElement(style,'{'+A+'}Setter',{'Property':'BorderBrush','Value':focus.attrib['BorderBrush']})
   template=source_style.find('.//{'+W+'}ControlTemplate')
   converted=template_styles(styles,template,'Button')
   style=ET.SubElement(styles,'{'+A+'}Style',{'Selector':'Button'})
   setter=ET.SubElement(style,'{'+A+'}Setter',{'Property':'Template'});setter.append(converted)
  elif target=='Expander':
   toggle=source_style.find('.//{'+W+'}ControlTemplate[@TargetType="ToggleButton"]')
   resources=ET.Element('{'+A+'}Styles.Resources');styles.insert(0,resources)
   theme=ET.SubElement(resources,'{'+A+'}ControlTheme',{'{'+X+'}Key':'SharedExpanderHeader','TargetType':'ToggleButton'})
   converted=template_styles(theme,toggle,'^')
   setter=ET.SubElement(theme,'{'+A+'}Setter',{'Property':'Template'});setter.append(converted)
   # Map parent Header to the native toggle Content, and IsExpanded to IsChecked.
   style=ET.SubElement(styles,'{'+A+'}Style',{'Selector':'Expander'})
   for key,value in {'MinHeight':'0','MinWidth':'0','Padding':'0','BorderThickness':'0','VerticalAlignment':'Top'}.items():ET.SubElement(style,'{'+A+'}Setter',{'Property':key,'Value':value})
   setter=ET.SubElement(style,'{'+A+'}Setter',{'Property':'Template'})
   template=ET.SubElement(setter,'{'+A+'}ControlTemplate');panel=ET.SubElement(template,'{'+A+'}StackPanel')
   ET.SubElement(panel,'{'+A+'}ToggleButton',{'Theme':'{StaticResource SharedExpanderHeader}','HorizontalContentAlignment':'Stretch','HorizontalAlignment':'Stretch','Content':'{TemplateBinding Header}','ContentTemplate':'{TemplateBinding HeaderTemplate}','Foreground':'{TemplateBinding Foreground}','FontSize':'{TemplateBinding FontSize}','IsChecked':'{TemplateBinding IsExpanded, Mode=TwoWay}'})
   ET.SubElement(panel,'{'+A+'}ContentPresenter',{'Content':'{TemplateBinding Content}','ContentTemplate':'{TemplateBinding ContentTemplate}','IsVisible':'{TemplateBinding IsExpanded}'})
 ET.indent(styles);Path(destination).write_text(ET.tostring(styles,encoding='unicode')+'\n',encoding='utf-8')

if __name__=="__main__":generate(sys.argv[1],sys.argv[2])
