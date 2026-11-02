"""One-time, hash-guarded UE asset configuration for phase 2.4. No external assets."""
import hashlib
import json
from pathlib import Path
import unreal

root = Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()))
evidence = root / 'Saved/Verification/Phase2_4'
manifest = json.loads((evidence / 'Baseline/ContentHashes.json').read_text(encoding='utf-8-sig'))
expected = {entry['Path'].replace('\\', '/'): entry['SHA256'].upper() for entry in manifest}
existing = (
    'Content/Characters/Mannequins/Anims/Unarmed/BS_Idle_Walk_Run.uasset',
    'Content/ThirdPerson/Blueprints/BP_ThirdPersonCharacter.uasset',
    'Content/Characters/Mannequins/Anims/Unarmed/Jump/MM_Dash.uasset',
)
for relative in existing:
    actual = hashlib.sha256((root / relative).read_bytes()).hexdigest().upper()
    if actual != expected[relative]:
        raise RuntimeError('Asset changed since inspected baseline: ' + relative)

destination = '/Game/ThirdPerson/Animations/AS_Dodge_InPlace'
if unreal.EditorAssetLibrary.does_asset_exist(destination):
    raise RuntimeError('Do not overwrite an existing animation: ' + destination)
source = unreal.load_asset('/Game/Characters/Mannequins/Anims/Unarmed/Jump/MM_Dash')
dodge = unreal.EditorAssetLibrary.duplicate_asset(source.get_path_name(), destination)
if not isinstance(dodge, unreal.AnimSequence):
    raise RuntimeError('Unreal failed to duplicate the inspected animation sequence.')
dodge.modify()
dodge.set_editor_property('enable_root_motion', False)
dodge.set_editor_property('force_root_lock', True)
dodge.set_editor_property('root_motion_root_lock', unreal.RootMotionRootLock.REF_POSE)
unreal.EditorAssetLibrary.save_loaded_asset(dodge, only_if_is_dirty=False)

space = unreal.load_asset('/Game/Characters/Mannequins/Anims/Unarmed/BS_Idle_Walk_Run')
space.modify()
samples = list(space.get_editor_property('sample_data'))
# Preserve numeric values before changing the axis; PostEditChange may adjust samples.
original = [(sample.get_editor_property('sample_value').x,
             sample.get_editor_property('sample_value').y,
             sample.get_editor_property('sample_value').z,
             sample.get_editor_property('rate_scale')) for sample in samples]
parameters = list(space.get_editor_property('blend_parameters'))
parameters[1].set_editor_property('max', 750.0)
parameters[1].set_editor_property('grid_num', 3)
space.set_editor_property('blend_parameters', parameters)
changes = []
for sample, (x, old_speed, z, old_rate) in zip(samples, original):
    if abs(old_speed - 300.0) < 0.01:
        speed = 500.0
    elif abs(old_speed - 600.0) < 0.01:
        speed = 750.0
    else:
        continue
    rate = old_rate * speed / old_speed
    sample.set_editor_property('sample_value', unreal.Vector(x, speed, z))
    sample.set_editor_property('rate_scale', rate)
    changes.append({'asset': sample.get_editor_property('animation').get_path_name(), 'old_speed': old_speed, 'speed': speed, 'rate_scale': rate})
if not changes:
    raise RuntimeError('Inspected template BlendSpace sample rows were not found.')
space.set_editor_property('sample_data', samples)
unreal.EditorAssetLibrary.save_loaded_asset(space, only_if_is_dirty=False)
world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
unreal.SystemLibrary.execute_console_command(world, 'WastelandAdventure.Animation.RebuildBlendSpace')

blueprint = unreal.load_asset('/Game/ThirdPerson/Blueprints/BP_ThirdPersonCharacter')
blueprint.modify()
default = unreal.get_default_object(unreal.BlueprintEditorLibrary.generated_class(blueprint))
default.modify()
default.set_editor_property('dodge_animation', dodge)
unreal.BlueprintEditorLibrary.compile_blueprint(blueprint)
unreal.EditorAssetLibrary.save_loaded_asset(blueprint, only_if_is_dirty=False)
default = unreal.get_default_object(unreal.BlueprintEditorLibrary.generated_class(blueprint))
if default.get_editor_property('dodge_animation') != dodge:
    raise RuntimeError('Character Blueprint did not retain its animation reference.')
if dodge.get_editor_property('enable_root_motion') or not dodge.get_editor_property('force_root_lock'):
    raise RuntimeError('In-place animation configuration was not retained.')
report = {'configured': True, 'source_unchanged': source.get_path_name(), 'dodge_animation': dodge.get_path_name(), 'duration': dodge.get_play_length(), 'enable_root_motion': False, 'force_root_lock': True, 'blendspace_changes': changes, 'animation_blueprint_reused': default.get_component_by_class(unreal.SkeletalMeshComponent).get_editor_property('anim_class').get_path_name()}
(evidence / 'AssetConfiguration.json').write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding='utf-8')
unreal.log('PHASE2_4_ASSETS_CONFIGURED ' + str(evidence / 'AssetConfiguration.json'))
