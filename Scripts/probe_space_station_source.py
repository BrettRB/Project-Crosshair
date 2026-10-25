"""Read-only survey of the playable Nuketown level before building the space-station copy."""
import collections
import unreal as ue
MAP = '/Game/Crosshair/Maps/L_Nuketown'
world = ue.EditorLoadingAndSavingUtils.load_map(MAP)
if not world:
    raise RuntimeError('Nuketown could not be loaded')
classes = collections.Counter()
meshes = collections.Counter()
materials = collections.Counter()
lo = [1e9] * 3
hi = [-1e9] * 3
for actor in ue.get_editor_subsystem(ue.EditorActorSubsystem).get_all_level_actors():
    classes[actor.get_class().get_name()] += 1
    for c in actor.get_components_by_class(ue.StaticMeshComponent):
        m = c.static_mesh
        if not m:
            continue
        path = m.get_path_name().split('.')[0]
        meshes[path] += 1
        for i in range(c.get_num_materials()):
            mat = c.get_material(i)
            if mat:
                materials[mat.get_path_name().split('.')[0]] += 1
        o, e = actor.get_actor_bounds(False)[0], actor.get_actor_bounds(False)[1]
        for k, (a, b) in enumerate([(o.x, e.x), (o.y, e.y), (o.z, e.z)]):
            lo[k] = min(lo[k], a - b)
            hi[k] = max(hi[k], a + b)
for k, v in classes.most_common():
    ue.log('PROBE_CLASS %d %s' % (v, k))
for k, v in meshes.most_common():
    ue.log('PROBE_MESH %d %s' % (v, k))
for k, v in materials.most_common():
    ue.log('PROBE_MAT %d %s' % (v, k))
ue.log('PROBE_BOUNDS %s %s' % (lo, hi))
