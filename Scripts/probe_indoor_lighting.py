import unreal as ue
world=ue.EditorLoadingAndSavingUtils.load_map('/Game/Crosshair/Maps/L_Nuketown')
actors=ue.get_editor_subsystem(ue.EditorActorSubsystem)
for a in actors.get_all_level_actors():
 if isinstance(a,(ue.Light,ue.PostProcessVolume)):
  ue.log('LIGHT '+a.get_actor_label()+' '+str(a.get_actor_location()))
  if isinstance(a,ue.Light):
   c=a.light_component
   ue.log('VALUES intensity='+str(c.get_editor_property('intensity'))+' mobility='+str(c.get_editor_property('mobility')))
 if isinstance(a,ue.CrosshairWindow):
  center,extent=a.get_actor_bounds(False);ue.log('WINDOW '+str(center)+' '+str(extent))
ue.log('CROSSHAIR_LIGHT_PROBE_OK')
