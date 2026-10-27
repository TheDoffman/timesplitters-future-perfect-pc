"""Shared layout/theme edits must reach the Mac adapter without a second edit."""


import importlib.util
from pathlib import Path
import tempfile
import unittest
import xml.etree.ElementTree as ET

ROOT = Path(__file__).resolve().parents[2]
SOURCE = ROOT / 'tools/launcher'
spec = importlib.util.spec_from_file_location('launcher_layout', SOURCE / 'macos/generate_layout.py')
layout = importlib.util.module_from_spec(spec)
spec.loader.exec_module(layout)

class SharedLauncherUi(unittest.TestCase):
    def test_home_label_and_spacing_change_propagate(self):
        with tempfile.TemporaryDirectory() as directory:
            source, target = Path(directory)/'Home.xaml', Path(directory)/'Home.axaml'
            tree = ET.parse(SOURCE/'Launcher.xaml')
            button = next(n for n in tree.iter() if n.attrib.get('{'+layout.X+'}Name') == 'RollbackButton')
            button.set('Content', 'Shared button revision')
            button.set('Margin', '3,5,17,11')
            template = tree.getroot().find('.//{'+layout.W+'}ControlTemplate[@TargetType="Button"]/{'+layout.W+'}Border')
            template.set('CornerRadius', '8')
            tree.write(source)
            layout.generate(source, target)
            result = next(n for n in ET.parse(target).iter() if n.attrib.get('{'+layout.X+'}Name') == 'RollbackButton')
            self.assertEqual(result.attrib['Content'], 'Shared button revision')
            self.assertEqual(result.attrib['Margin'], '3,5,17,11')
            self.assertIn('CornerRadius="8"', target.read_text(encoding='utf-8'))
            self.assertNotIn('NativeHomeStyles', target.read_text(encoding='utf-8'))

    def test_theme_template_geometry_and_states_change_together(self):
        with tempfile.TemporaryDirectory() as directory:
            source, target = Path(directory)/'Theme.xaml', Path(directory)/'Theme.axaml'
            tree = ET.parse(SOURCE/'PageTheme.xaml')
            button = tree.getroot().find('{'+layout.W+'}Style[@TargetType="Button"]')
            button.find('{'+layout.W+'}Setter[@Property="Padding"]').set('Value', '21,13')
            face = button.find('.//{'+layout.W+'}Border')
            face.set('CornerRadius', '9')
            disabled = button.find('.//{'+layout.W+'}Trigger[@Property="IsEnabled"]/{'+layout.W+'}Setter')
            disabled.set('Value','0.25')
            tree.write(source)
            layout.generate_theme(source,target)
            result = target.read_text(encoding='utf-8')
            self.assertIn('Property="Padding" Value="21,13"', result)
            self.assertIn('CornerRadius="9"', result)
            self.assertIn('Button:disabled /template/ Border#Face', result)
            self.assertIn('Property="Opacity" Value="0.25"', result)

    def test_both_builds_compile_all_shared_pages_and_ui_tests(self):
        windows = (ROOT/'tools/build/build-launcher.ps1').read_text(encoding='utf-8')
        mac = (SOURCE/'macos/NativeLauncher.csproj').read_text(encoding='utf-8')
        self.assertIn("-Filter '*Page.cs'",windows)
        self.assertIn('../*Page.cs',mac)
        self.assertIn('launcher_shared_ui.cs',windows)
        self.assertIn('launcher_shared_ui.cs',mac)
        self.assertFalse((SOURCE/'macos/NativeSettings.cs').exists())

if __name__ == '__main__':
    unittest.main()
