import unreal as ue
a=ue.EditorAssetLibrary
# Enable a world representation for observing the back-carried weapon, hidden from its owner.
bp=a.load_asset('/Game/Crosshair/Player/BP_PracticeCharacter');cdo=ue.get_default_object(bp.generated_class());body=cdo.get_editor_property('mesh')
body.set_skinned_asset_and_update(a.load_asset('/Game/Characters/Mannequins/Meshes/SKM_Manny_Simple'));body.set_editor_property('relative_location',ue.Vector(0,0,-96));body.set_editor_property('relative_rotation',ue.Rotator(pitch=0,yaw=-90,roll=0));body.set_visibility(True);body.set_owner_no_see(True)
cdo.set_editor_property('body_idle_animation',a.load_asset('/Game/Characters/Mannequins/Anims/Unarmed/MM_Idle'))
for i in range(2):body.set_material(i,a.load_asset('/Game/Crosshair/Weapons/Finishes/M_PaintedWeapon'))
assert a.save_loaded_asset(bp,False)
ue.log('CROSSHAIR_CARRY_BODY_OK')
