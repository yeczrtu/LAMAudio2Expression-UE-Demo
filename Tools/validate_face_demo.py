"""Resave migrated face assets and validate that all demo content lives in /Game."""
import json
import pathlib
import re
import unreal

root = pathlib.Path(unreal.Paths.project_dir()).resolve()
target = '/Game/LAMFaceDemo'
registry = unreal.AssetRegistryHelpers.get_asset_registry()
registry.search_all_assets(True)
level = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
assert level.load_level(target + '/Maps/LAM_FaceDemo')
assets = registry.get_assets_by_path(target, recursive=True)
if len(assets) != 44:
    raise RuntimeError(f'Expected 44 demo assets, found {len(assets)}')
for asset in assets:
    obj = asset.get_asset()
    if not obj or not unreal.EditorAssetLibrary.save_loaded_asset(obj, only_if_is_dirty=False):
        raise RuntimeError('Cannot save ' + str(asset.package_name))
assert unreal.LAMDemoBlueprintLibrary.validate_face_demo_blueprints()
actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors()
assert len([a for a in actors if isinstance(a, unreal.DirectionalLight)]) == 1
mesh = unreal.load_asset(target + '/Character/Face52')
assert len(mesh.materials) == 14 and all(m.material_interface for m in mesh.materials)
source = (root / 'Plugins/LAMAudio2Expression/Source/LAMAudio2Expression/Private/LAMTypes.cpp').read_text(encoding='utf-8')
names = re.findall(r'TEXT\("([a-zA-Z]+)"\)', source.split('return Names;')[0])
assert len(names) == 52 and set(names).issubset({m.get_name() for m in mesh.get_editor_property('morph_targets')})
options = unreal.AssetRegistryDependencyOptions(include_soft_package_references=True, include_hard_package_references=True,
    include_searchable_names=False, include_soft_management_references=False, include_hard_management_references=False)
rows = []
for asset in assets:
    dependencies = [str(x) for x in (registry.get_dependencies(asset.package_name, options) or [])]
    forbidden = [x for x in dependencies if x in ('/Script/LAMDemo', '/Script/LAMDemoEditor') or x.startswith('/LAMAudio2Expression/Demo') or
                 (x.startswith('/Game/') and not x.startswith(target + '/'))]
    if forbidden:
        raise RuntimeError(f'Unexpected dependency: {asset.package_name}: {forbidden}')
    rows.append({'asset': str(asset.package_name), 'dependencies': dependencies})
(root / 'Docs/Validation/face-demo-dependencies.json').write_text(json.dumps(rows, indent=2), encoding='utf-8')
unreal.log('LAM_SEPARATE_DEMO_VALIDATED assets=44 materials=14 ARKit=52 pure_blueprint=True DirectionalLights=1')
