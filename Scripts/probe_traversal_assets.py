import unreal as ue
ue.EditorLoadingAndSavingUtils.load_map('/Game/Crosshair/Maps/L_Nuketown')
actors=ue.get_editor_subsystem(ue.EditorActorSubsystem)
for a in actors.get_all_level_actors():
 for c in a.get_components_by_class(ue.StaticMeshComponent):
  mesh=c.static_mesh
  if not mesh: continue
  mats=[m.get_path_name() if m else '' for m in c.get_materials()]
  if any('glass' in m.lower() or 'window' in m.lower() for m in mats):
   origin,extent=a.get_actor_bounds(False)
   ue.log('WINDOW_PROBE '+str((a.get_actor_label(),a.get_path_name(),mesh.get_path_name(),mats,str(c.get_world_transform()),str(origin),str(extent))))
mesh=ue.load_asset('/Game/Characters/Mannequins/Meshes/SKM_Manny_Simple')
ue.log('TARGET_MODEL '+str(mesh.get_editor_property('physics_asset')))
