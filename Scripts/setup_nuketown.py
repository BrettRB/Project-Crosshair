"""Prepare a playable copy of the author-supplied UE 5.3 Nuketown environment.
Run in this project with UnrealEditor-Cmd -run=pythonscript -script=<this file>.
Requires the original Content folders and BlackOps_NukeTown_V2.umap at their
original package paths. Never overwrites the practice map or the source level.
See docs/nuketown-import.md for provenance and missing dependency details.
"""
import unreal as ue

DESTINATION = '/Game/Crosshair/Maps/L_Nuketown'
assets = ue.EditorAssetLibrary
if assets.does_asset_exist(DESTINATION):
    raise RuntimeError('Playable Nuketown already exists; refusing to overwrite designer edits')
world = ue.EditorLoadingAndSavingUtils.load_map('/Game/BlackOps_NukeTown_V2')
if not world:
    raise RuntimeError('Original Nuketown map failed to load')
actors = ue.get_editor_subsystem(ue.EditorActorSubsystem)
all_actors = list(actors.get_all_level_actors())
# Source-engine units are inches; character movement uses Unreal centimeters.
# Rebase the source ground near Z=0, above the existing out-of-world reset limit.
for actor in all_actors:
    if not actor.get_attach_parent_actor():
        p = actor.get_actor_location()
        s = actor.get_actor_scale3d()
        actor.set_actor_location(ue.Vector(p.x * 2.54, p.y * 2.54, p.z * 2.54 + 1397), False, False)
        actor.set_actor_scale3d(ue.Vector(s.x * 2.54, s.y * 2.54, s.z * 2.54))

# Missing surface textures are not bundled with the author's material instances.
# Apply explicit, local fallback materials only to those imported surface slots.
# Original supplied textures/materials elsewhere stay in use.
parent_path = '/Game/Crosshair/Maps/NuketownMaterials/M_NuketownSurface'
parent = assets.load_asset(parent_path) if assets.does_asset_exist(parent_path) else assets.duplicate_asset('/Game/Crosshair/Targets/M_Target', parent_path)
parent.set_editor_property('used_with_nanite', True)
ue.MaterialEditingLibrary.recompile_material(parent)
assets.save_loaded_asset(parent, False)
tools = ue.AssetToolsHelpers.get_asset_tools()
fallbacks = {}
colors = {
    'asphalt': (.045, .05, .055), 'grass': (.075, .16, .035),
    'sand': (.38, .30, .18), 'brick': (.30, .13, .075),
    'roof': (.12, .075, .055), 'wood': (.27, .16, .075),
    'plank': (.32, .23, .12), 'wallpaper': (.42, .40, .28),
    'plaster': (.65, .61, .46), 'metal': (.16, .18, .19),
    'carpet': (.21, .18, .11), 'marble': (.60, .60, .57),
    'concrete': (.28, .28, .25), 'stone': (.26, .24, .19)
}
changed_meshes = set()
replacement_slots = 0
for actor in all_actors:
    for component in actor.get_components_by_class(ue.StaticMeshComponent):
        mesh = component.static_mesh
        if not mesh:
            continue
        if mesh.get_path_name().startswith(('/Game/BlackOPSIK/', '/Game/MDL/', '/Game/ArchViz/')):
            component.set_collision_profile_name('BlockAll')
            component.set_collision_enabled(ue.CollisionEnabled.QUERY_AND_PHYSICS)
            if mesh.get_path_name() not in changed_meshes:
                body = mesh.get_editor_property('body_setup')
                if body:
                    body.set_editor_property('collision_trace_flag', ue.CollisionTraceFlag.CTF_USE_COMPLEX_AS_SIMPLE)
                    if not assets.save_loaded_asset(mesh, False):
                        raise RuntimeError('Could not save collision for ' + mesh.get_path_name())
                changed_meshes.add(mesh.get_path_name())
        for slot in range(component.get_num_materials()):
            material = component.get_material(slot)
            if not material or not material.get_path_name().startswith('/Game/Megascans/Surfaces/'):
                continue
            key = next((k for k in colors if k in material.get_name().lower()), 'concrete')
            if key not in fallbacks:
                path = '/Game/Crosshair/Maps/NuketownMaterials/MI_' + key
                instance = assets.load_asset(path) if assets.does_asset_exist(path) else tools.create_asset(
                    'MI_' + key, '/Game/Crosshair/Maps/NuketownMaterials', ue.MaterialInstanceConstant, ue.MaterialInstanceConstantFactoryNew())
                ue.MaterialEditingLibrary.set_material_instance_parent(instance, parent)
                ue.MaterialEditingLibrary.set_material_instance_vector_parameter_value(instance, 'Color', ue.LinearColor(*colors[key], 1))
                assets.save_loaded_asset(instance, False)
                fallbacks[key] = instance
            component.set_material(slot, fallbacks[key])
            replacement_slots += 1
# Promotional cameras/sequences must not override gameplay view or playback.
for actor in all_actors:
    if isinstance(actor, (ue.CineCameraActor, ue.LevelSequenceActor, ue.PostProcessVolume, ue.PlayerStart)):
        actors.destroy_actor(actor)

sun = actors.spawn_actor_from_class(ue.DirectionalLight, ue.Vector(0, 0, 3000), ue.Rotator(-45, -35, 0))
sun.set_actor_label('Crosshair daylight')
sun.light_component.set_editor_property('mobility', ue.ComponentMobility.MOVABLE)
sun.light_component.set_editor_property('intensity', 5.0)
sky = actors.spawn_actor_from_class(ue.SkyLight, ue.Vector(0, 0, 2500))
sky.light_component.set_editor_property('mobility', ue.ComponentMobility.MOVABLE)
sky.light_component.set_editor_property('intensity', 1.0)
sky.light_component.set_editor_property('real_time_capture', True)
actors.spawn_actor_from_class(ue.SkyAtmosphere, ue.Vector())
# Clear ground beside the house; verify floor and free capsule in the runtime test.
start = actors.spawn_actor_from_class(ue.PlayerStart, ue.Vector(2000, -2000, 190), ue.Rotator(0, 180, 0))
start.set_actor_label('Crosshair Nuketown start')
game = assets.load_asset('/Game/Crosshair/Player/BP_PracticeGameMode')
world.get_world_settings().set_editor_property('default_game_mode', game.generated_class())
if not ue.EditorLoadingAndSavingUtils.save_map(world, DESTINATION):
    raise RuntimeError('Could not save playable Nuketown')
ue.log('NUKETOWN_SETUP_OK meshes=%d fallback_slots=%d' % (len(changed_meshes), replacement_slots))
