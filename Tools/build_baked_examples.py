"""Run after LAM.Bake.GenerateAndRegenerate with -LAMBakeSaveFixtures."""
import unreal

clip = unreal.load_asset('/Game/Audio/speech_stream_LAMClip')
if not clip or not unreal.LAMEditorLibrary.create_baked_example(clip):
    raise RuntimeError('Bake speech_stream and create the static playback example first')

# A model-free map for the existing runtime playback-control harness.
path = '/Game/Examples/LAM_BakedTest'
if not unreal.EditorAssetLibrary.does_asset_exist(path):
    level = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    if not level.new_level(path):
        raise RuntimeError('Could not create the baked playback test map')
    world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    world.get_world_settings().set_editor_property('default_game_mode', unreal.LAMDemoGameMode)
    if not level.save_current_level():
        raise RuntimeError('Could not save the baked playback test map')
unreal.log('Baked playback example and test map are ready')
