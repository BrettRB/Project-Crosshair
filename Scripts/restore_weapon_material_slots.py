"""Restore existing semantic weapon finishes after a geometry-only reimport."""
import unreal as ue
a=ue.EditorAssetLibrary
for name in ['AR','SMG','Sniper']:
 d=a.load_asset('/Game/Crosshair/Weapons/DA_'+name);m=d.get_editor_property('presentation_mesh')
 for i,s in enumerate(m.static_materials):
  finish={'Paint':'M_PaintedWeapon','Metal':'M_WeaponMetal','Rubber':'M_WeaponRubber','Glass':'M_OpticGlass'}[str(s.material_slot_name)]
  material=a.load_asset('/Game/Crosshair/Weapons/Finishes/'+finish);assert material
  m.set_material(i,material)
 assert a.save_loaded_asset(m,False)
 # Reimports can reorder slots: rebuild only skin slot references, preserving shaders/IDs.
 skins=[]
 for old in d.get_editor_property('skins'):
  skin=old.copy();instance=a.load_asset('/Game/Crosshair/Weapons/Finishes/MI_'+str(skin.id));assert instance
  skin.set_editor_property('materials',[instance if str(s.material_slot_name)=='Paint' else None for s in m.static_materials]);skins.append(skin)
 d.set_editor_property('skins',skins);assert a.save_loaded_asset(d,False)
ue.log('CROSSHAIR_COMBAT_WEAPON_SURFACES_OK')
