"""Explicitly rebuild the shipped demo Blueprint graphs and update the existing map.

Run in UE Editor via Tools > Execute Python Script, or UnrealEditor-Cmd -run=pythonscript.
This overwrites BP_FaceDemo, BP_FaceDemoHUD and BP_FaceDemoGameMode; do not use on customized graphs.
"""
import unreal
import uuid

base = '/Game/LAMFaceDemo'
assets = unreal.AssetToolsHelpers.get_asset_tools()
for name in ('SM_Dialogue', 'SM_Alternate'):
    path = base + '/Audio/' + name
    if not unreal.EditorAssetLibrary.does_asset_exist(path):
        mix = assets.create_asset(name, base + '/Audio', unreal.SoundSubmix, unreal.SoundSubmixFactory())
        assert mix and unreal.EditorAssetLibrary.save_loaded_asset(mix)

level = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
# Unload the demo map before recompiling its Actor class.
assert level.new_level('/Temp/LAMBlueprintAuthoring_' + uuid.uuid4().hex)
assert unreal.LAMDemoBlueprintLibrary.rebuild_face_demo_blueprints(), 'Blueprint graph build failed'
assert level.load_level(base + '/Maps/LAM_FaceDemo')
actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
bp = unreal.load_asset(base + '/Blueprints/BP_FaceDemo')
demo_actors = []
lights = []
for actor in actors.get_all_level_actors():
    if actor.get_class() == bp.generated_class():
        demo_actors.append(actor)
    if isinstance(actor, unreal.DirectionalLight):
        lights.append(actor)
for actor in demo_actors:
    actors.destroy_actor(actor)
demo = actors.spawn_actor_from_class(bp.generated_class(), unreal.Vector())
demo.set_actor_label('BP Face Demo - open Blueprint to follow the nodes')
demo.tags = ['LAMBlueprintDemo']
for light in lights:
    actors.destroy_actor(light)
light = actors.spawn_actor_from_class(unreal.DirectionalLight, unreal.Vector(0, 0, 180), unreal.Rotator(pitch=-20, yaw=-55))
light.set_actor_label('Directional Light')
component = light.get_component_by_class(unreal.DirectionalLightComponent)
component.set_mobility(unreal.ComponentMobility.MOVABLE)
component.set_intensity(2.0)
component.set_light_color(unreal.LinearColor(1, .95, .9))
mode = unreal.load_asset(base + '/Blueprints/BP_FaceDemoGameMode')
world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
world.get_world_settings().set_editor_property('default_game_mode', mode.generated_class())
assert level.save_current_level()
assert unreal.LAMDemoBlueprintLibrary.validate_face_demo_blueprints()
assert len([a for a in actors.get_all_level_actors() if isinstance(a, unreal.DirectionalLight)]) == 1
unreal.log('LAM_BLUEPRINT_DEMO_BUILT DirectionalLights=1')
