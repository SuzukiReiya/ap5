"""Run by UE's Python commandlet, never by the system Python interpreter."""
import unreal

MAP_PATH = "/Game/Generated/Minimal"


def main():
    # Preserve an existing map; no user assets are overwritten.
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
