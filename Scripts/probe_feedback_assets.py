import unreal as ue
actors=ue.get_editor_subsystem(ue.EditorActorSubsystem)
ue.EditorLoadingAndSavingUtils.load_map('/Game/Crosshair/Maps/L_Practice')
for a in actors.get_all_level_actors():
 if isinstance(a,ue.CrosshairDummy):
  m=a.get_editor_property('model')
  ue.log('DUMMY_PROBE '+str((a.get_actor_label(),str(a.get_actor_transform()),str(m.get_relative_transform()))))
ue.EditorLoadingAndSavingUtils.load_map('/Game/Crosshair/Maps/L_Nuketown')
for a in actors.get_all_level_actors():
 for c in a.get_components_by_class(ue.StaticMeshComponent):
  mesh=c.static_mesh
  if not mesh: continue
  mats=[m.get_path_name() if m else '' for m in c.get_materials()]
  if any(x in (mesh.get_path_name()+' '.join(mats)).lower() for x in ['flower','plant','bush']):
   ue.log('FLOWER_PROBE '+str((a.get_actor_label(),mesh.get_path_name(),mats,str(c.get_collision_enabled()),str(c.get_world_transform()))))
