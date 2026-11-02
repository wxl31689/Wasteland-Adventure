"""Read the real loaded UE Blueprint defaults without changing or saving assets.

Run in the Unreal Editor console: py "<absolute path>/InspectCharacter.py"
The resulting JSON is local evidence, not a gameplay test result.
"""
import json
from pathlib import Path

import unreal


def path(value):
    return value.get_path_name() if value else None


def read(obj, name):
    return obj.get_editor_property(name)


editor = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
world = editor.get_editor_world()
override = read(world.get_world_settings(), "default_game_mode")
game_mode_class = override or unreal.load_class(
    None, "/Game/ThirdPerson/Blueprints/BP_ThirdPersonGameMode.BP_ThirdPersonGameMode_C"
)
game_mode = unreal.get_default_object(game_mode_class)
pawn_class = read(game_mode, "default_pawn_class")
controller_class = read(game_mode, "player_controller_class")
pawn = unreal.get_default_object(pawn_class)
controller = unreal.get_default_object(controller_class)
movement = read(pawn, "character_movement")
mesh = pawn.get_component_by_class(unreal.SkeletalMeshComponent)
boom = pawn.get_component_by_class(unreal.SpringArmComponent)
camera = pawn.get_component_by_class(unreal.CameraComponent)
contexts = list(read(controller, "default_mapping_contexts")) + list(
    read(controller, "mobile_excluded_mapping_contexts")
)
report = {
    "map": path(world),
    "game_mode_override": path(override),
    "game_mode_class": path(game_mode_class),
    "pawn_class": path(pawn_class),
    "uses_existing_native_character": isinstance(
        pawn, unreal.get_type_from_class(unreal.load_class(None, "/Script/WastelandAdventure.WastelandAdventureCharacter"))
    ),
    "controller_class": path(controller_class),
    "max_walk_speed": read(movement, "max_walk_speed"),
    "sprint_settings": {
        "walk_speed": read(pawn, "walk_speed"),
        "sprint_speed": read(pawn, "sprint_speed"),
        "is_sprinting": read(pawn, "is_sprinting"),
    },
    "jump_z_velocity": read(movement, "jump_z_velocity"),
    "air_control": read(movement, "air_control"),
    "braking_deceleration_walking": read(movement, "braking_deceleration_walking"),
    "mesh": path(read(mesh, "skeletal_mesh_asset")),
    "animation_blueprint_class": path(read(mesh, "anim_class")),
    "spring_arm_length": read(boom, "target_arm_length"),
    "spring_arm_collision": read(boom, "do_collision_test"),
    "spring_arm_uses_control_rotation": read(boom, "use_pawn_control_rotation"),
    "camera": path(camera),
    "input_actions": {
        name: path(read(pawn, name))
        for name in ("move_action", "jump_action", "look_action", "mouse_look_action")
    },
    "mapping_contexts": [path(context) for context in contexts],
    "context_details": [
        {
            "context": path(context),
            "mappings": [
                {"action": path(read(mapping, "action")), "key": str(unreal.InputLibrary.key_get_display_name(read(mapping, "key")))}
                for mapping in read(read(context, "default_key_mappings"), "mappings")
            ],
        }
        for context in contexts
    ],
}
destination = Path(unreal.Paths.project_saved_dir()) / "Verification" / "Phase2_1" / "CharacterInspection.json"
destination.parent.mkdir(parents=True, exist_ok=True)
destination.write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding="utf-8")
unreal.log("CHARACTER_INSPECTION_SAVED " + str(destination))
unreal.log(json.dumps(report, ensure_ascii=False))
