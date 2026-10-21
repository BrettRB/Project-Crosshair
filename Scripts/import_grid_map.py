"""Import the user's Sketchfab 2K GLB through Unreal Interchange, without touching other maps.
Run with UnrealEditor-Cmd -run=pythonscript -script=<absolute script path>.
Set CROSSHAIR_GRID_SOURCE to override the original downloaded source location.
See docs/grid-map.md for attribution, gameplay changes and verification.
"""
import os
from pathlib import Path
import unreal as ue
SOURCE=Path(os.environ.get('CROSSHAIR_GRID_SOURCE',str(Path.home()/'Downloads/call_of_duty_black_ops_1_grid_inspired_map.glb')))
DEST='/Game/Crosshair/Maps/L_Grid'
CONTENT='/Game/Crosshair/Maps/Grid'
assets=ue.EditorAssetLibrary
if assets.does_asset_exist(DEST):
 raise RuntimeError('Grid already exists; refusing to overwrite designer edits')
if not SOURCE.is_file(): raise RuntimeError('Missing GLB: '+str(SOURCE))
world=ue.EditorLoadingAndSavingUtils.new_blank_map(False)
manager=ue.InterchangeManager.get_interchange_manager_scripted()
params=ue.ImportAssetParameters()
params.set_editor_property('is_automated',True)
params.set_editor_property('replace_existing',True)
if not manager.import_scene(CONTENT,ue.InterchangeManager.create_source_data(str(SOURCE)),params):
 raise RuntimeError('Grid GLB scene import failed')
actors=ue.get_editor_subsystem(ue.EditorActorSubsystem)
meshes=set(); count=0; bounds=[]
for actor in actors.get_all_level_actors():
 for component in actor.get_components_by_class(ue.StaticMeshComponent):
  mesh=component.static_mesh
  if not mesh: continue
  count+=1
  component.set_mobility(ue.ComponentMobility.STATIC)
  component.set_collision_profile_name('BlockAll')
  component.set_collision_enabled(ue.CollisionEnabled.QUERY_AND_PHYSICS)
  if mesh.get_path_name() not in meshes:
   # Many source pieces carry large local-space bounds under scaled scene nodes.
   # Use lower-resolution Lumen distance fields; render/collision triangles stay intact.
   mesh_editor=ue.get_editor_subsystem(ue.StaticMeshEditorSubsystem) or ue.get_default_object(ue.StaticMeshEditorSubsystem)
   settings=mesh_editor.get_lod_build_settings(mesh,0)
   world_scale=component.get_world_scale()
   settings.set_editor_property('distance_field_resolution_scale',min(0.25,max(0.001,abs(world_scale.x),abs(world_scale.y),abs(world_scale.z))))
   mesh_editor.set_lod_build_settings(mesh,0,settings)
   body=mesh.get_editor_property('body_setup')
   if body:
    body.set_editor_property('collision_trace_flag',ue.CollisionTraceFlag.CTF_USE_COMPLEX_AS_SIMPLE)
    body.set_editor_property('double_sided_geometry',True)
   meshes.add(mesh.get_path_name())
   if not assets.save_loaded_asset(mesh,False): raise RuntimeError('Could not save '+mesh.get_path_name())
  origin,extent=actor.get_actor_bounds(False)
  bounds.append((extent.x*extent.y,actor.get_actor_label(),(origin.x,origin.y,origin.z),(extent.x,extent.y,extent.z)))
if count<50: raise RuntimeError('Expected a full Grid scene, found '+str(count)+' mesh components')
for row in sorted(bounds,reverse=True)[:25]: ue.log('GRID_BOUNDS '+str(row))
for cls,pos,rot in [(ue.DirectionalLight,ue.Vector(0,0,15000),ue.Rotator(-40,-35,0)),(ue.SkyLight,ue.Vector(0,0,10000),ue.Rotator()),(ue.SkyAtmosphere,ue.Vector(),ue.Rotator())]:
 a=actors.spawn_actor_from_class(cls,pos,rot)
 if cls==ue.DirectionalLight:
  a.light_component.set_editor_property('mobility',ue.ComponentMobility.MOVABLE)
  a.light_component.set_editor_property('intensity',5)
 if cls==ue.SkyLight:
  a.light_component.set_editor_property('mobility',ue.ComponentMobility.MOVABLE)
  a.light_component.set_editor_property('intensity',1)
  a.light_component.set_editor_property('real_time_capture',True)
start=actors.spawn_actor_from_class(ue.PlayerStart,ue.Vector(0,0,300),ue.Rotator())
start.set_actor_label('Crosshair Grid start')
world.get_world_settings().set_editor_property('default_game_mode',assets.load_asset('/Game/Crosshair/Player/BP_PracticeGameMode').generated_class())
if not assets.save_directory(CONTENT,False,True): raise RuntimeError('Could not save imported Grid assets')
if not ue.EditorLoadingAndSavingUtils.save_map(world,DEST): raise RuntimeError('Could not save Grid map')
ue.log('CROSSHAIR_GRID_IMPORT_OK components=%d meshes=%d'%(count,len(meshes)))
