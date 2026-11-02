"""Read actual Blueprint camera defaults; no asset is changed or saved."""
import json
from datetime import datetime
from pathlib import Path

import unreal


def read(obj, name):
    return obj.get_editor_property(name)


world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
mode_class = read(world.get_world_settings(), "default_game_mode") or unreal.load_class(
    None, "/Game/ThirdPerson/Blueprints/BP_ThirdPersonGameMode.BP_ThirdPersonGameMode_C"
)
mode = unreal.get_default_object(mode_class)
pawn = unreal.get_default_object(read(mode, "default_pawn_class"))
controller = unreal.get_default_object(read(mode, "player_controller_class"))
boom = pawn.get_component_by_class(unreal.SpringArmComponent)
camera = pawn.get_component_by_class(unreal.CameraComponent)
manager_class = read(controller, "player_camera_manager_class")
manager = unreal.get_default_object(manager_class) if manager_class else None
report = {
    "map": world.get_path_name(),
    "pawn": pawn.get_class().get_path_name(),
    "controller": controller.get_class().get_path_name(),
    "spring_arm": boom.get_path_name(),
    "camera": camera.get_path_name(),
    "arm_length": read(boom, "target_arm_length"),
    "use_pawn_control_rotation": read(boom, "use_pawn_control_rotation"),
    "collision": read(boom, "do_collision_test"),
    "probe_size": read(boom, "probe_size"),
    "probe_channel": str(read(boom, "probe_channel")),
    "position_lag": read(boom, "enable_camera_lag"),
    "rotation_lag": read(boom, "enable_camera_rotation_lag"),
    "lag_speed": read(boom, "camera_lag_speed"),
    "lag_max_distance": read(boom, "camera_lag_max_distance"),
    "lag_substepping": read(boom, "use_camera_lag_substepping"),
    "lag_max_time_step": read(boom, "camera_lag_max_time_step"),
    "camera_uses_control_rotation": read(camera, "use_pawn_control_rotation"),
    "camera_manager_class": manager.get_class().get_path_name() if manager else None,
    "manager_class_pitch_min": read(manager, "view_pitch_min") if manager else None,
    "manager_class_pitch_max": read(manager, "view_pitch_max") if manager else None,
}
destination = Path(unreal.Paths.project_saved_dir()) / "Verification" / "Phase2_3"
destination.mkdir(parents=True, exist_ok=True)
filename = destination / ("CameraDefaults-" + datetime.now().strftime("%Y%m%d-%H%M%S") + ".json")
filename.write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding="utf-8")
unreal.log("CAMERA_DEFAULTS_INSPECTED " + str(filename))
unreal.log(json.dumps(report, ensure_ascii=False))
