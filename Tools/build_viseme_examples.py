"""UE Python: create editable presets and a Face52 vowel AnimBP; preserve existing assets."""
import unreal

mesh = unreal.load_asset('/Game/LAMFaceDemo/Character/Face52')
assert mesh, 'The demo Face52 mesh is required for these examples.'
names = {m.get_name() for m in mesh.get_editor_property('morph_targets')}
assert all('Fcl_MTH_' + vowel in names for vowel in 'AIUEO')
assert unreal.LAMEditorLibrary.create_oculus_examples(mesh)
unreal.log('LAM_VISEME_EXAMPLES_READY /Game/LAMVisemeExamples/ABP_LAMVisemes')
