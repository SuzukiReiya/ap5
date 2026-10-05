"""UEに依存しない整合性検証。ゲームのビルド・描画は検証しない。"""
import json
from pathlib import Path
import runpy
import sys
import types
import unittest
from unittest.mock import Mock, patch

ROOT = Path(__file__).resolve().parents[1]


class ProjectTests(unittest.TestCase):
    def test_project_and_plugins(self):
        project = json.loads((ROOT / "Ap5.uproject").read_text(encoding="utf-8-sig"))
        self.assertEqual(project["EngineAssociation"], "5.6")
        self.assertEqual(project["Modules"][0]["Name"], "Ap5")
        plugins = {p["Name"]: p for p in project["Plugins"]}
        for name in ("PythonScriptPlugin", "EditorScriptingUtilities"):
            self.assertTrue(plugins[name]["Enabled"])
            self.assertEqual(plugins[name]["TargetAllowList"], ["Editor"])

    def test_generated_map_and_class_paths(self):
        config = (ROOT / "Config/DefaultEngine.ini").read_text(encoding="utf-8-sig")
        self.assertIn("GameDefaultMap=/Game/Generated/Minimal", config)
        self.assertIn("GlobalDefaultGameMode=/Script/Ap5.Ap5GameMode", config)
        self.assertIn("/Game/Generated/Minimal", (ROOT / "Scripts/CreateMap.py").read_text(encoding="utf-8-sig"))
        self.assertIn("/Game/Generated/Minimal", (ROOT / "Config/DefaultGame.ini").read_text(encoding="utf-8-sig"))

    def test_expected_files(self):
        for relative in ("Source/Ap5.Target.cs", "Source/Ap5Editor.Target.cs",
                         "Source/Ap5/Ap5.Build.cs", "Source/Ap5/Ap5GameMode.h",
                         "Scripts/Setup.ps1", "Scripts/BuildAndRun.ps1", "Scripts/Package.ps1"):
            self.assertTrue((ROOT / relative).is_file(), relative)

    def test_native_tools_fail_closed(self):
        common = (ROOT / "Scripts/Common.ps1").read_text(encoding="utf-8-sig")
        self.assertIn("$toolExitCode = $LASTEXITCODE", common)
        self.assertIn("if ($toolExitCode -ne 0)", common)
        for script in ("Setup.ps1", "BuildAndRun.ps1", "Package.ps1"):
            self.assertIn("exit 1", (ROOT / "Scripts" / script).read_text(encoding="utf-8-sig"))


class MapGenerationTests(unittest.TestCase):
    def run_script(self, exists=False, world=object(), save=True):
        unreal = types.ModuleType("unreal")
        unreal.EditorAssetLibrary = Mock()
        unreal.EditorAssetLibrary.does_asset_exist.return_value = exists
        unreal.EditorLoadingAndSavingUtils = Mock()
        unreal.EditorLoadingAndSavingUtils.new_blank_map.return_value = world
        unreal.EditorLoadingAndSavingUtils.save_map.return_value = save
        unreal.log = Mock()
        with patch.dict(sys.modules, {"unreal": unreal}):
            runpy.run_path(str(ROOT / "Scripts/CreateMap.py"))
        return unreal

    def test_new_map_saved(self):
        unreal = self.run_script()
        unreal.EditorLoadingAndSavingUtils.save_map.assert_called_once()
        self.assertEqual(unreal.EditorLoadingAndSavingUtils.save_map.call_args.args[1],
                         "/Game/Generated/Minimal")

    def test_existing_map_preserved(self):
        unreal = self.run_script(exists=True)
        unreal.EditorLoadingAndSavingUtils.new_blank_map.assert_not_called()
        unreal.EditorLoadingAndSavingUtils.save_map.assert_not_called()

    def test_world_failure(self):
        with self.assertRaisesRegex(RuntimeError, "create"):
            self.run_script(world=None)

    def test_save_failure(self):
        with self.assertRaisesRegex(RuntimeError, "save"):
            self.run_script(save=False)


if __name__ == "__main__":
    unittest.main()
