"""Remove collision only from raised ArchViz grass in the playable Nuketown copy."""
import unreal as ue
MAP = '/Game/Crosshair/Maps/L_Nuketown'
PREFIX = '/Game/ArchViz/Grass/Grass_grass_'
world = ue.EditorLoadingAndSavingUtils.load_map(MAP)
if not world:
    raise RuntimeError('Playable Nuketown could not be loaded')
count = 0
for actor in ue.get_editor_subsystem(ue.EditorActorSubsystem).get_all_level_actors():
    for component in actor.get_components_by_class(ue.StaticMeshComponent):
        mesh = component.static_mesh
        if mesh and mesh.get_path_name().startswith(PREFIX):
            component.modify()
            component.set_collision_profile_name('NoCollision')
            component.set_collision_enabled(ue.CollisionEnabled.NO_COLLISION)
            count += 1
            ue.log('GRASS_REPAIR '+mesh.get_path_name())
if count != 4:
    raise RuntimeError('Expected four raised grass foliage components, found '+str(count))
if not ue.EditorLoadingAndSavingUtils.save_map(world, MAP):
    raise RuntimeError('Playable map could not be saved')
# Reload verifies persisted component state, without changing source foliage/ground assets.
ue.EditorLoadingAndSavingUtils.load_map('/Game/Crosshair/Maps/L_Practice')
ue.EditorLoadingAndSavingUtils.load_map(MAP)
checked = 0
for actor in ue.get_editor_subsystem(ue.EditorActorSubsystem).get_all_level_actors():
    for c in actor.get_components_by_class(ue.StaticMeshComponent):
        if c.static_mesh and c.static_mesh.get_path_name().startswith(PREFIX):
            if c.get_collision_enabled() != ue.CollisionEnabled.NO_COLLISION:
                raise RuntimeError('Grass collision returned after reload')
            checked += 1
if checked != count:
    raise RuntimeError('Foliage components changed after reload')
ue.log('NUKETOWN_GRASS_REPAIR_OK count='+str(count))
