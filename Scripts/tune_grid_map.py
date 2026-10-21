"""Scoped Grid distance-field tuning and asset audit; preserves scene placement/materials."""
import unreal as ue
assets=ue.EditorAssetLibrary
world=ue.EditorLoadingAndSavingUtils.load_map('/Game/Crosshair/Maps/L_Grid')
if not world: raise RuntimeError('Grid is not installed')
mesh_editor=ue.get_editor_subsystem(ue.StaticMeshEditorSubsystem) or ue.get_default_object(ue.StaticMeshEditorSubsystem)
mesh_scales={}
for actor in ue.get_editor_subsystem(ue.EditorActorSubsystem).get_all_level_actors():
 for component in actor.get_components_by_class(ue.StaticMeshComponent):
  if component.static_mesh:
   scale=component.get_world_scale()
   name=component.static_mesh.get_path_name()
   mesh_scales[name]=max(mesh_scales.get(name,0.001),abs(scale.x),abs(scale.y),abs(scale.z))
meshes=[];textures=[];materials=0
for path in assets.list_assets('/Game/Crosshair/Maps/Grid',True,False):
 asset=assets.load_asset(path)
 if isinstance(asset,ue.StaticMesh):
  settings=mesh_editor.get_lod_build_settings(asset,0)
  settings.set_editor_property('distance_field_resolution_scale',min(0.25,mesh_scales.get(asset.get_path_name(),0.25)))
  mesh_editor.set_lod_build_settings(asset,0,settings)
  if not assets.save_loaded_asset(asset,False): raise RuntimeError('Cannot save '+path)
  meshes.append(asset)
 elif isinstance(asset,ue.Texture2D):
  textures.append((asset.get_name(),asset.blueprint_get_size_x(),asset.blueprint_get_size_y()))
 elif isinstance(asset,ue.MaterialInterface): materials+=1
if len(meshes)!=590 or len(textures)!=30 or materials!=38: raise RuntimeError('Unexpected Grid asset counts')
for name,x,y in textures: ue.log('GRID_TEXTURE %s %dx%d'%(name,x,y))
ue.log('CROSSHAIR_GRID_TUNING_OK meshes=%d textures=%d materials=%d'%(len(meshes),len(textures),materials))
