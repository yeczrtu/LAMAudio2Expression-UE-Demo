"""Create the cooked inline-concurrency fixture in the demo project only."""
import unreal

path = '/Game/Audio/inline_concurrency'
wave = unreal.load_asset(path) if unreal.EditorAssetLibrary.does_asset_exist(path) else unreal.EditorAssetLibrary.duplicate_asset('/Game/Audio/one_inline', path)
assert wave, 'Run import_assets.py first'
wave.set_editor_property('override_concurrency', True)
settings = wave.get_editor_property('concurrency_overrides')
settings.set_editor_property('max_count', 1)
settings.set_editor_property('resolution_rule', unreal.MaxConcurrentResolutionRule.PREVENT_NEW)
wave.set_editor_property('concurrency_overrides', settings)
unreal.EditorAssetLibrary.save_loaded_asset(wave)
