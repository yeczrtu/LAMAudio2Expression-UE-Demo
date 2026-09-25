"""One-time migration of the formerly bundled face demo into this demo project."""
import pathlib
import unreal
root=pathlib.Path(unreal.Paths.project_dir()).resolve()
if not (root/'LAMDemo.uproject').exists():
    raise RuntimeError('Use the LAMDemo project')
registry=unreal.AssetRegistryHelpers.get_asset_registry()
registry.search_all_assets(True)
source='/LAMAudio2Expression/Demo'
target='/Game/LAMFaceDemo'
entries=registry.get_assets_by_path(source,recursive=True)
if len(entries)!=40:
    raise RuntimeError(f'Expected 40 demo assets, found {len(entries)}')
renames=[]
for entry in entries:
    dest=str(entry.package_name).replace(source,target,1)
    if unreal.EditorAssetLibrary.does_asset_exist(dest):
        raise RuntimeError('Destination already exists: '+dest)
    renames.append(unreal.AssetRenameData(entry.get_asset(),dest.rsplit('/',1)[0],dest.rsplit('/',1)[1]))
if not unreal.AssetToolsHelpers.get_asset_tools().rename_assets(renames):
    raise RuntimeError('Demo migration failed')
unreal.EditorAssetLibrary.save_directory(target,only_if_is_dirty=False,recursive=True)
unreal.log('LAM_FACE_DEMO_MOVED '+str(len(renames)))
