"""UEのPythonコマンドレットで、検証用マップと単色背景材質を生成する。"""
import unreal

MAP_PATH = "/Game/Generated/Minimal"
BACKGROUND_PATH = "/Game/Generated/M_Background"


def create_background():
    # 既存のマップがある場合も、不足している背景材質だけは生成する。
    if unreal.EditorAssetLibrary.does_asset_exist(BACKGROUND_PATH):
        return
    unreal.EditorAssetLibrary.make_directory("/Game/Generated")
    material = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
        "M_Background", "/Game/Generated", unreal.Material, unreal.MaterialFactoryNew())
    if material is None:
        raise RuntimeError("背景材質を作成できませんでした")
    material.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
    material.set_editor_property("two_sided", True)
    color = unreal.MaterialEditingLibrary.create_material_expression(
        material, unreal.MaterialExpressionConstant3Vector)
    if color is None:
        raise RuntimeError("背景色のノードを作成できませんでした")
    color.set_editor_property("constant", unreal.LinearColor(0.055, 0.09, 0.14, 1.0))
    if not unreal.MaterialEditingLibrary.connect_material_property(
            color, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR):
        raise RuntimeError("背景材質の色を接続できませんでした")
    unreal.MaterialEditingLibrary.recompile_material(material)
    if not unreal.EditorAssetLibrary.save_loaded_asset(material):
        raise RuntimeError("背景材質を保存できませんでした")
    unreal.log("AP5_BACKGROUND_READY: 青灰色の単色背景材質を生成しました")



def main():
    create_background()
    # 既存のマップとユーザーの編集を保持する。
    if unreal.EditorAssetLibrary.does_asset_exist(MAP_PATH):
        unreal.log("AP5_MAP_READY: existing map preserved")
        return
    world = unreal.EditorLoadingAndSavingUtils.new_blank_map(False)
    if world is None:
        raise RuntimeError("Could not create the initial world")
    unreal.EditorAssetLibrary.make_directory("/Game/Generated")
    if not unreal.EditorLoadingAndSavingUtils.save_map(world, MAP_PATH):
        raise RuntimeError("Could not save the initial map")
    unreal.log("AP5_MAP_READY: empty map created; gameplay actors are spawned by C++")


main()
