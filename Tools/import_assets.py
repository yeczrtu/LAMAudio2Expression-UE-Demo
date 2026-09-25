"""Run with UnrealEditor-Cmd LAMDemo.uproject -run=pythonscript -script=..."""
import pathlib
import runpy
import unreal
root=pathlib.Path(unreal.Paths.project_dir()).resolve()
tasks=[]
def add(src,dest,name):
    t=unreal.AssetImportTask(); t.filename=str(src); t.destination_path=dest; t.destination_name=name
    t.automated=True; t.replace_existing=True; t.save=True; tasks.append(t)
add(root/'.work/export/LAM_A2E.onnx','/LAMAudio2Expression/Models','LAM_A2E')
for f in sorted((root/'.work/fixtures').glob('*.wav')): add(f,'/Game/Audio',f.stem)
unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks(tasks)
for t in tasks:
    if not t.imported_object_paths: raise RuntimeError('Import failed: '+t.filename)
for f in sorted((root/'.work/fixtures').glob('*.wav')):
    wave=unreal.load_asset('/Game/Audio/'+f.stem)
    wave.set_editor_property('sound_asset_compression_type',unreal.SoundAssetCompressionType.BINK_AUDIO)
    wave.set_editor_property('loading_behavior',unreal.SoundWaveLoadingBehavior.FORCE_INLINE if f.stem.endswith('inline') else unreal.SoundWaveLoadingBehavior.LOAD_ON_DEMAND)
    unreal.EditorAssetLibrary.save_loaded_asset(wave)
runpy.run_path(str(root/'Tools/build_playback_test_assets.py'))
level=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
level.new_level('/Game/LAMDemo')
actors=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
demo=actors.spawn_actor_from_class(unreal.LAMDemoActor,unreal.Vector(0,0,0))
demo.set_editor_property('sound',unreal.load_asset('/Game/Audio/speech_stream'))
demo.set_editor_property('model',unreal.load_asset('/LAMAudio2Expression/Models/LAM_A2E'))
level.save_current_level()
if not unreal.LAMEditorLibrary.create_examples(): raise RuntimeError('Blueprint example generation failed')
unreal.log('LAM assets and demo map imported successfully')
