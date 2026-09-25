"""UE Python commandlet: generate examples only in a fresh .work release copy.

Invoke this file from the source repository, targeting the copied uproject.
Exclude Content/Examples and Content/LAMDemo.umap when making the disposable copy.
Restore its model uasset from the verified BuildPlugin output afterwards because
CreateExamples saves the model's runtime selection as part of asset generation.
"""
from pathlib import Path
import unreal

repository = Path(__file__).resolve().parent.parent
project = Path(unreal.Paths.project_dir()).resolve()
assert project.is_relative_to(repository / '.work'), 'Only a disposable .work project is allowed'
for name in ['Content/LAMDemo.umap', 'Content/Examples/BP_LAMPlayback.uasset',
             'Content/Examples/ABP_LAMCurves.uasset']:
    assert not (project / name).exists(), 'Refusing to overwrite existing example: ' + name
unreal.AssetRegistryHelpers.get_asset_registry().scan_paths_synchronous(['/Game'], True)
assert unreal.LAMEditorLibrary.create_examples()
level = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
assert level.new_level('/Game/LAMDemo')
actor = unreal.get_editor_subsystem(unreal.EditorActorSubsystem).spawn_actor_from_class(
    unreal.LAMDemoActor, unreal.Vector(0, 0, 0))
actor.set_editor_property('sound', unreal.load_asset('/Game/Audio/speech_stream'))
actor.set_editor_property('model', unreal.load_asset('/LAMAudio2Expression/Models/LAM_A2E'))
assert level.save_current_level()
unreal.log('Generated release examples without modifying the working project')
