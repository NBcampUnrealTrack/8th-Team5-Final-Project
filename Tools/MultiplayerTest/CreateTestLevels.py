"""Run with UnrealEditor-Cmd -run=pythonscript -script=<this file>
-EnablePlugins=PythonScriptPlugin,EditorScriptingUtilities.
"""
import unreal

folder = "/Game/Levels/Test_CW/Multiplayer"
for name, class_name in [
    ("L_MultiplayerTestMenu", "HBMultiplayerTestMenuGameMode"),
    ("L_MultiplayerTestLobby", "HBMultiplayerTestLobbyGameMode"),
]:
    package = f"{folder}/{name}"
    if unreal.EditorAssetLibrary.does_asset_exist(package):
        unreal.log(f"Already exists: {package}; preserving it.")
        continue
    world = unreal.EditorLoadingAndSavingUtils.new_blank_map(False)
    mode = unreal.load_class(None, f"/Script/HeavenBreach.{class_name}")
    if not mode:
        raise RuntimeError(f"Missing compiled class: {class_name}")
    world.get_world_settings().set_editor_property("default_game_mode", mode)
    if not unreal.EditorLoadingAndSavingUtils.save_map(world, package):
        raise RuntimeError(f"Could not save {package}")
    unreal.log(f"Created: {package}")
