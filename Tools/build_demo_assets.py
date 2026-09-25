"""Build the distributable sample in an isolated copy of the author's UE project.

Requires .lam-demo-staging in project root. Moves assets in that COPY only.
Keep the original /Game setup untouched; prepare a separate project copy first.
"""
import json
import pathlib
import unreal

root = pathlib.Path(unreal.Paths.project_dir()).resolve()
if not (root / '.lam-demo-staging').exists():
    raise RuntimeError('Run this only in the isolated project prepared for demo migration.')
inputs = pathlib.Path((root / '.lam-demo-staging').read_text(encoding='utf-8').strip())
target = '/LAMAudio2Expression/Demo'
assets = unreal.AssetToolsHelpers.get_asset_tools()
registry = unreal.AssetRegistryHelpers.get_asset_registry()
registry.search_all_assets(True)
samples = ['F1_anger_regular_31', 'F1_disgust_regular_38', 'F1_fear_regular_23',
           'F1_happy_regular_38', 'F1_sad_regular_10', 'F1_surprise_regular_11']
mapping = {
    '/Game/SkeletalMesh/Face52': target + '/Character/Face52',
    '/Game/SkeletalMesh/Face52_Skeleton': target + '/Character/Face52_Skeleton',
    '/Game/SkeletalMesh/Face52_Skeleton_AnimBlueprint': target + '/Blueprints/ABP_Face52',
    '/Game/Examples/BP_LAMPlayback': target + '/Blueprints/BP_AudioToFace',
}
for name in samples:
    mapping['/Game/Developers/thoshino/' + name] = target + '/Audio/' + name

renames = []
for source, dest in mapping.items():
    if unreal.EditorAssetLibrary.does_asset_exist(dest):
        continue
    obj = unreal.load_asset(source)
    if not obj:
        raise RuntimeError('Missing source asset: ' + source)
    renames.append(unreal.AssetRenameData(obj, dest.rsplit('/', 1)[0], dest.rsplit('/', 1)[1]))
if renames and not assets.rename_assets(renames):
    raise RuntimeError('Could not remap demo assets')

mesh = unreal.load_asset(target + '/Character/Face52')
# The original FBX referenced an unsaved physics asset. The portrait never simulates physics.
mesh.set_editor_property('physics_asset', None)

textures = {}
material_specs = json.loads((inputs / 'materials-source.json').read_text(encoding='utf-8'))
for index in sorted({m['texture'] for m in material_specs if m['texture'] is not None}):
    name = f'T_VRoid_{index:02d}'
    path = target + '/Character/Textures/' + name
    if not unreal.EditorAssetLibrary.does_asset_exist(path):
        task = unreal.AssetImportTask()
        task.filename = str(inputs / 'textures' / (name + '.png'))
        task.destination_path = target + '/Character/Textures'
        task.destination_name = name
        task.automated = True
        task.save = True
        assets.import_asset_tasks([task])
        if not task.imported_object_paths:
            raise RuntimeError('Texture import failed: ' + name)
    textures[index] = unreal.load_asset(path)

materials = {}
for spec in material_specs:
    name = 'M_' + spec['name']
    path = target + '/Character/Materials/' + name
    mat = unreal.load_asset(path) if unreal.EditorAssetLibrary.does_asset_exist(path) else assets.create_asset(
        name, target + '/Character/Materials', unreal.Material, unreal.MaterialFactoryNew())
    unreal.MaterialEditingLibrary.delete_all_material_expressions(mat)
    mat.set_editor_property('two_sided', spec['double_sided'])
    mat.set_editor_property('used_with_skeletal_mesh', True)
    mat.set_editor_property('used_with_morph_targets', True)
    mat.set_editor_property('blend_mode', unreal.BlendMode.BLEND_MASKED)
    mat.set_editor_property('opacity_mask_clip_value', .15 if spec['alpha'] == 'BLEND' else spec['cutoff'])
    tex = unreal.MaterialEditingLibrary.create_material_expression(mat, unreal.MaterialExpressionTextureSample)
    tex.set_editor_property('texture', textures[spec['texture']])
    unreal.MaterialEditingLibrary.connect_material_property(tex, 'RGB', unreal.MaterialProperty.MP_BASE_COLOR)
    unreal.MaterialEditingLibrary.connect_material_property(tex, 'A', unreal.MaterialProperty.MP_OPACITY_MASK)
    rough = unreal.MaterialEditingLibrary.create_material_expression(mat, unreal.MaterialExpressionConstant)
    rough.set_editor_property('r', .8)
    unreal.MaterialEditingLibrary.connect_material_property(rough, '', unreal.MaterialProperty.MP_ROUGHNESS)
    unreal.MaterialEditingLibrary.recompile_material(mat)
    unreal.EditorAssetLibrary.save_loaded_asset(mat, only_if_is_dirty=False)
    materials[spec['name']] = mat
slots = mesh.materials
for index, slot in enumerate(slots):
    slot.material_interface = materials[str(slot.material_slot_name)]
    slots[index] = slot
# The Python property invokes SetMaterials, updating UE 5.8's serialized cache.
# set_editor_property would only change the transient Materials array.
mesh.materials = slots
unreal.EditorAssetLibrary.save_loaded_asset(mesh, only_if_is_dirty=False)

waves = [unreal.load_asset(target + '/Audio/' + name) for name in samples]
for wave in waves:
    wave.set_editor_property('sound_asset_compression_type', unreal.SoundAssetCompressionType.BINK_AUDIO)
    wave.set_editor_property('loading_behavior', unreal.SoundWaveLoadingBehavior.LOAD_ON_DEMAND)
    unreal.EditorAssetLibrary.save_loaded_asset(wave)

factory = unreal.BlueprintFactory()
factory.set_editor_property('parent_class', unreal.LAMDemoPreview)
bp_path = target + '/Blueprints/BP_FaceDemo'
bp = unreal.load_asset(bp_path) if unreal.EditorAssetLibrary.does_asset_exist(bp_path) else assets.create_asset(
    'BP_FaceDemo', target + '/Blueprints', unreal.Blueprint, factory)
cdo = unreal.get_default_object(bp.generated_class())
cdo.set_editor_property('samples', waves)
cdo.set_editor_property('sample_labels', ['Anger', 'Disgust', 'Fear', 'Happiness', 'Sadness', 'Surprise'])
face = cdo.get_editor_property('face')
face.set_skeletal_mesh_asset(mesh)
face.set_anim_instance_class(unreal.load_asset(target + '/Blueprints/ABP_Face52').generated_class())
unreal.BlueprintEditorLibrary.compile_blueprint(bp)
unreal.EditorAssetLibrary.save_loaded_asset(bp, only_if_is_dirty=False)

level = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
if not level.new_level(target + '/Maps/LAM_FaceDemo'):
    raise RuntimeError('Could not create demo level')
actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
demo = actors.spawn_actor_from_class(bp.generated_class(), unreal.Vector(0, 0, 0))
demo.set_actor_label('LAM Face Demo - choose voice 1-6')
world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
world.get_world_settings().set_editor_property('default_game_mode', unreal.LAMDemoPreviewGameMode)
for name, rotation, intensity, color in [
    ('Key', unreal.Rotator(pitch=-20, yaw=-55, roll=0), 2.0, unreal.LinearColor(1, .92, .85)),
    ('Fill', unreal.Rotator(pitch=-15, yaw=-140, roll=0), 1.0, unreal.LinearColor(.65, .8, 1)),
    ('Rim', unreal.Rotator(pitch=-35, yaw=90, roll=0), 2.0, unreal.LinearColor(.7, 1, .95)),
]:
    light = actors.spawn_actor_from_class(unreal.DirectionalLight, unreal.Vector(0, 0, 180), rotation)
    light.set_actor_label(name)
    component = light.get_component_by_class(unreal.DirectionalLightComponent)
    component.set_mobility(unreal.ComponentMobility.MOVABLE)
    component.set_intensity(intensity)
    component.set_light_color(color)
post = actors.spawn_actor_from_class(unreal.PostProcessVolume, unreal.Vector())
post.set_editor_property('unbound', True)
settings = post.get_editor_property('settings')
settings.set_editor_property('override_auto_exposure_min_brightness', True)
settings.set_editor_property('override_auto_exposure_max_brightness', True)
settings.set_editor_property('auto_exposure_min_brightness', 1.0)
settings.set_editor_property('auto_exposure_max_brightness', 1.0)
post.set_editor_property('settings', settings)
level.save_current_level()
unreal.EditorAssetLibrary.save_directory(target, only_if_is_dirty=False, recursive=True)

options = unreal.AssetRegistryDependencyOptions(include_soft_package_references=True, include_hard_package_references=True,
    include_searchable_names=False, include_soft_management_references=False, include_hard_management_references=False)
report = []
for asset in registry.get_assets_by_path(target, recursive=True):
    dependencies = [str(x) for x in (registry.get_dependencies(asset.package_name, options) or [])]
    forbidden = [x for x in dependencies if x.startswith('/Game/') or x == '/Script/LAMDemo']
    if forbidden:
        raise RuntimeError(f'Nonportable demo dependency: {asset.package_name}: {forbidden}')
    report.append({'asset': str(asset.package_name), 'dependencies': dependencies})
(inputs / 'demo-dependencies.json').write_text(json.dumps(report, indent=2), encoding='utf-8')
unreal.log('LAM plugin demo built; no /Game or /Script/LAMDemo dependencies.')
