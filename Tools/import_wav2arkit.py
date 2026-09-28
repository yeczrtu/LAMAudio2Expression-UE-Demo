"""UE Python commandlet: import the verified CPU model. Add -LAMWav2ARKitTests for fixtures."""
from pathlib import Path
import sys
import unreal

sys.path.insert(0, str(Path(__file__).resolve().parent))
from setup_wav2arkit import MODEL_DIR, verify_files

verify_files()  # Fail before creating assets if either original file is missing or altered.


def import_asset(source, destination, name, model=False):
    task = unreal.AssetImportTask()
    task.filename = str(source)
    task.destination_path = destination
    task.destination_name = name
    task.automated = True
    task.replace_existing = True
    task.save = False
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    if not task.imported_object_paths:
        raise RuntimeError(f'Import failed: {source}')
    asset = unreal.load_asset(task.imported_object_paths[0])
    if model:
        if not unreal.LAMEditorLibrary.configure_cpu_model(asset):
            raise RuntimeError(f'CPU configuration failed: {source}')
    else:
        asset.set_editor_property('sound_asset_compression_type', unreal.SoundAssetCompressionType.BINK_AUDIO)
        asset.set_editor_property('loading_behavior', unreal.SoundWaveLoadingBehavior.FORCE_INLINE)
    if not unreal.EditorAssetLibrary.save_loaded_asset(asset):
        raise RuntimeError(f'Save failed: {source}')


import_asset(MODEL_DIR / 'wav2arkit_cpu.onnx', '/LAMAudio2Expression/Models', 'Wav2ARKit_CPU', model=True)
if '-LAMWav2ARKitTests' in unreal.SystemLibrary.get_command_line():
    for source in sorted((MODEL_DIR / 'fixtures').glob('*.onnx')):
        import_asset(source, '/Game/Wav2ARKitTests/Models', source.stem, model=True)
    for source in sorted((MODEL_DIR / 'fixtures').glob('*.wav')):
        import_asset(source, '/Game/Wav2ARKitTests', source.stem)
unreal.log('Wav2ARKit imported for CPU. Select Wav2ARKit_CPU in LAM Audio2Expression project settings.')
