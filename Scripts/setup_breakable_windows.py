"""Convert only imported house glass panes; preserve meshes, materials and frames.
Run inside Unreal Editor, after compiling ACrosshairWindow. Safe to rerun.
"""
import math
from pathlib import Path
import unreal as ue

LEVEL = '/Game/Crosshair/Maps/L_Nuketown'
ue.EditorLoadingAndSavingUtils.load_map(LEVEL)
actors = ue.get_editor_subsystem(ue.EditorActorSubsystem)
window_class = ue.load_class(None, '/Script/Project_Crosshair.CrosshairWindow')
assert window_class
output = Path(ue.Paths.project_saved_dir()) / 'WindowGeometry'
output.mkdir(parents=True, exist_ok=True)
converted = 0
for source in list(actors.get_all_level_actors()):
    if not source.get_actor_label().startswith('func_breakable_'):
        continue
    components = source.get_components_by_class(ue.StaticMeshComponent)
    if len(components) != 1:
        continue
    component = components[0]
    materials = component.get_materials()
    paths = [m.get_path_name().lower() if m else '' for m in materials]
    if not any('glass' in p for p in paths) or not all('glass' in p or 'toolsnodraw' in p for p in paths):
        continue
    mesh = component.static_mesh
    if not mesh:
        continue
    # OBJ exporter maps Unreal XYZ to XZY. Measure the actual plane rather
    # than its axis-aligned bounds: house rotations are baked into vertices.
    filename = output / (source.get_actor_label()+'.obj')
    task = ue.AssetExportTask()
    task.object = mesh
    task.exporter = ue.StaticMeshExporterOBJ()
    task.filename = str(filename)
    task.automated = True
    task.prompt = False
    task.replace_identical = True
    assert ue.Exporter.run_asset_export_task(task), mesh.get_path_name()
    vertices = set()
    for line in filename.read_text().splitlines():
        if line.startswith('v '):
            x,z,y = map(float,line.split()[1:4])
            vertices.add((x,y,z))
    assert len(vertices) >= 4
    mx = sum(v[0] for v in vertices)/len(vertices)
    my = sum(v[1] for v in vertices)/len(vertices)
    xx = sum((v[0]-mx)**2 for v in vertices)
    yy = sum((v[1]-my)**2 for v in vertices)
    xy = sum((v[0]-mx)*(v[1]-my) for v in vertices)
    angle = .5*math.atan2(2*xy,xx-yy)
    sx,sy = math.cos(angle),math.sin(angle)
    nx,ny = -sy,sx
    widths = [x*sx+y*sy for x,y,z in vertices]
    depths = [x*nx+y*ny for x,y,z in vertices]
    heights = [z for x,y,z in vertices]
    midw,midd = (min(widths)+max(widths))/2,(min(depths)+max(depths))/2
    center = ue.Vector(sx*midw+nx*midd,sy*midw+ny*midd,(min(heights)+max(heights))/2)
    window = actors.spawn_actor_from_class(window_class,source.get_actor_location(),source.get_actor_rotation())
    assert window
    window.set_actor_label('Window_'+source.get_actor_label())
    window.set_editor_property('tags',[ue.Name('CrosshairHouseWindow')])
    glass = window.get_editor_property('glass')
    glass.set_static_mesh(mesh)
    glass.set_world_transform(component.get_world_transform(),False,True)
    glass.set_collision_profile_name('BlockAll')
    for i,material in enumerate(materials):
        glass.set_material(i,material)
    window.set_editor_property('configured_opening',True)
    window.set_editor_property('opening_center',center)
    window.set_editor_property('opening_normal',ue.Vector(nx,ny,0))
    window.set_editor_property('opening_half_width',(max(widths)-min(widths))/2)
    window.set_editor_property('opening_half_height',(max(heights)-min(heights))/2)
    assert actors.destroy_actor(source)
    converted += 1
    ue.log('CROSSHAIR_WINDOW_CONVERTED '+window.get_actor_label())
windows = [a for a in actors.get_all_level_actors() if isinstance(a,ue.CrosshairWindow)]
assert windows, 'No house windows found'
assert ue.EditorLoadingAndSavingUtils.save_map(ue.EditorLevelLibrary.get_editor_world(),LEVEL)
ue.log('CROSSHAIR_WINDOWS_OK converted=%d total=%d'%(converted,len(windows)))
