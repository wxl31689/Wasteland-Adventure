"""Inspect actual animation assets and Blueprint graphs without modifying resources."""
import json
from datetime import datetime
from pathlib import Path

import unreal


def path(obj):
    return obj.get_path_name() if obj else None


def prop(obj, name):
    try:
        value = obj.get_editor_property(name)
        if isinstance(value, unreal.Object):
            return path(value)
        return value if isinstance(value, (str, bool, int, float, type(None))) else str(value)
    except Exception as error:
        return {"unavailable": str(error)}


mode_class = unreal.load_class(None, "/Game/ThirdPerson/Blueprints/BP_ThirdPersonGameMode.BP_ThirdPersonGameMode_C")
mode = unreal.get_default_object(mode_class)
pawn = unreal.get_default_object(mode.get_editor_property("default_pawn_class"))
mesh = pawn.get_component_by_class(unreal.SkeletalMeshComponent)
anim_class = mesh.get_editor_property("anim_class")
blueprint = unreal.load_asset(anim_class.get_path_name().removesuffix("_C"))
report = {
    "pawn_class": path(pawn.get_class()),
    "mesh": prop(mesh, "skeletal_mesh_asset"),
    "anim_class": path(anim_class),
    "blueprint": path(blueprint),
    "blueprint_type": path(blueprint.get_class()),
    "blueprint_functions": [name for name in dir(blueprint) if not name.startswith("_")],
    "editor_library_functions": [name for name in dir(unreal.BlueprintEditorLibrary) if not name.startswith("_")],
    "blueprint_properties": {name: prop(blueprint, name) for name in ("parent_class", "target_skeleton", "ubergraph_pages", "function_graphs", "implemented_interfaces")},
    "assets": [],
}
for name in unreal.EditorAssetLibrary.list_assets("/Game/Characters/Mannequins/Anims/Unarmed", recursive=True, include_folder=False):
    asset = unreal.load_asset(name)
    item = {"path": name, "class": path(asset.get_class()), "skeleton": prop(asset, "skeleton")}
    if isinstance(asset, unreal.AnimSequence):
        item["duration"] = asset.get_play_length()
        item["enable_root_motion"] = prop(asset, "enable_root_motion")
        item["root_lock"] = prop(asset, "root_motion_root_lock")
        item["force_root_lock"] = prop(asset, "force_root_lock")
    if isinstance(asset, unreal.BlendSpace):
        item["axis"] = prop(asset, "blend_parameters")
        item["samples"] = prop(asset, "sample_data")
    report["assets"].append(item)
destination = Path(unreal.Paths.project_saved_dir()) / "Verification" / "Phase2_4"
destination.mkdir(parents=True, exist_ok=True)
filename = destination / ("AnimationInspection-" + datetime.now().strftime("%Y%m%d-%H%M%S") + ".json")
filename.write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding="utf-8")
unreal.log("ANIMATION_INSPECTION_SAVED " + str(filename))
unreal.get_editor_subsystem(unreal.AssetEditorSubsystem).open_editor_for_assets([blueprint])
