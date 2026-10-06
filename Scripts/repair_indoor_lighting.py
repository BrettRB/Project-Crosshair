"""Adjust only existing playable Nuketown interior lights; preserve exterior/source maps."""
import unreal as ue
world=ue.EditorLoadingAndSavingUtils.load_map('/Game/Crosshair/Maps/L_Nuketown')
actors=ue.get_editor_subsystem(ue.EditorActorSubsystem)
count=0
for actor in actors.get_all_level_actors():
 if not isinstance(actor,(ue.PointLight,ue.RectLight)): continue
 c=actor.light_component
 c.set_editor_property('mobility',ue.ComponentMobility.MOVABLE)
 c.set_editor_property('intensity_units',ue.LightUnits.LUMENS)
 c.set_editor_property('intensity',500.0 if isinstance(actor,ue.PointLight) else 650.0)
 c.set_editor_property('attenuation_radius',850.0)
 c.set_editor_property('indirect_lighting_intensity',1.15)
 c.set_editor_property('cast_shadows',True)
 c.set_editor_property('use_temperature',True)
 c.set_editor_property('temperature',5000.0)
 if isinstance(actor,ue.PointLight):
  c.set_editor_property('source_radius',12.0)
  c.set_editor_property('use_inverse_squared_falloff',True)
 else:
  c.set_editor_property('source_width',120.0)
  c.set_editor_property('source_height',70.0)
 count+=1
assert count==11,count
assert ue.EditorLoadingAndSavingUtils.save_map(world,'/Game/Crosshair/Maps/L_Nuketown')
world=ue.EditorLoadingAndSavingUtils.load_map('/Game/Crosshair/Maps/L_Nuketown')
verified=0
for actor in actors.get_all_level_actors():
 if isinstance(actor,(ue.PointLight,ue.RectLight)):
  c=actor.light_component
  assert c.get_editor_property('mobility')==ue.ComponentMobility.MOVABLE
  assert c.get_editor_property('intensity')>=500
  verified+=1
assert verified==11
ue.log('CROSSHAIR_INDOOR_LIGHTS_OK corrected='+str(verified))
