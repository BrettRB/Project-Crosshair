"""Repair only placed practice targets and decorative Nuketown flowers."""
import unreal as ue
actors=ue.get_editor_subsystem(ue.EditorActorSubsystem)
practice='/Game/Crosshair/Maps/L_Practice'
nuke='/Game/Crosshair/Maps/L_Nuketown'
world=ue.EditorLoadingAndSavingUtils.load_map(practice)
count=0
for a in actors.get_all_level_actors():
 if isinstance(a,ue.CrosshairDummy):
  a.set_actor_rotation(ue.Rotator(pitch=0,yaw=180,roll=0),True)
  count+=1
assert count==3,count
assert ue.EditorLoadingAndSavingUtils.save_map(world,practice)
world=ue.EditorLoadingAndSavingUtils.load_map(nuke)
flowers=0
prefix='/Game/BlackOPSIK/2/models_bo1_nuketown_outside_mc_t5_foliage_flowers'
for a in actors.get_all_level_actors():
 for c in a.get_components_by_class(ue.StaticMeshComponent):
  if c.static_mesh and c.static_mesh.get_path_name().startswith(prefix):
   c.modify(); c.set_collision_profile_name('NoCollision')
   c.set_collision_enabled(ue.CollisionEnabled.NO_COLLISION); flowers+=1
assert flowers==82,flowers
assert ue.EditorLoadingAndSavingUtils.save_map(world,nuke)
ue.EditorLoadingAndSavingUtils.load_map(practice)
for a in actors.get_all_level_actors():
 if isinstance(a,ue.CrosshairDummy):
  r=a.get_actor_rotation(); assert abs(r.pitch)<.01 and abs(r.roll)<.01
ue.EditorLoadingAndSavingUtils.load_map(nuke)
verified=0
for a in actors.get_all_level_actors():
 for c in a.get_components_by_class(ue.StaticMeshComponent):
  if c.static_mesh and c.static_mesh.get_path_name().startswith(prefix):
   assert c.get_collision_enabled()==ue.CollisionEnabled.NO_COLLISION; verified+=1
assert verified==flowers
ue.log('CROSSHAIR_FEEDBACK_ASSETS_OK upright=%d flowers=%d'%(count,flowers))
