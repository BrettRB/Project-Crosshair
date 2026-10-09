"""Original tactical clothing and bone-attached pads. Leaves template rig/assets intact."""
from pathlib import Path
import unreal as ue
assets=ue.EditorAssetLibrary; edit=ue.MaterialEditingLibrary; tools=ue.AssetToolsHelpers.get_asset_tools()
folder='/Game/Crosshair/Player/Combat'
assets.make_directory(folder)
old=folder+'/M_CombatArms'
m=assets.load_asset(old) if assets.does_asset_exist(old) else assets.duplicate_asset('/Game/Crosshair/Player/Arms/M_FirstPersonArms',old)
assert m
# Keep the existing reference-pose opacity mask; replace mannequin color/normal/metal.
def node(cls): return edit.create_material_expression(m,cls)
def custom(code,inputs,kind):
 n=node(ue.MaterialExpressionCustom); n.set_editor_property('code',code); n.set_editor_property('output_type',kind)
 params=[]
 for name in inputs:
  value=ue.CustomInput();value.set_editor_property('input_name',name);params.append(value)
 n.set_editor_property('inputs',params)
 return n
def link(a,b,pin):
 assert edit.connect_material_expressions(a,'',b,pin),pin
pos=node(ue.MaterialExpressionPreSkinnedPosition); interp=node(ue.MaterialExpressionVertexInterpolator); link(pos,interp,'')
uv=node(ue.MaterialExpressionTextureCoordinate)
color=custom('float hand=smoothstep(32.0,37.0,abs(P.x)); float patch=step(.25,sin(P.x*.22+sin(P.z*.17))*sin(P.z*.19+P.y*.12)); float3 cloth=lerp(float3(.03,.045,.02),float3(.10,.085,.045),patch); float weave=.96+.04*sin(UV.x*2400)*sin(UV.y*2200); return lerp(cloth,float3(.009,.012,.009),hand)*weave;',['P','UV'],ue.CustomMaterialOutputType.CMOT_FLOAT3)
link(interp,color,'P');link(uv,color,'UV');assert edit.connect_material_property(color,'',ue.MaterialProperty.MP_BASE_COLOR)
normal=custom('return normalize(float3(sin(UV.x*2400)*.07,sin(UV.y*2200)*.07,1));',['UV'],ue.CustomMaterialOutputType.CMOT_FLOAT3)
link(uv,normal,'UV');edit.connect_material_property(normal,'',ue.MaterialProperty.MP_NORMAL)
for prop,value in [(ue.MaterialProperty.MP_METALLIC,0),(ue.MaterialProperty.MP_ROUGHNESS,.86),(ue.MaterialProperty.MP_SPECULAR,.25)]:
 n=node(ue.MaterialExpressionConstant);n.set_editor_property('r',value);edit.connect_material_property(n,'',prop)
edit.recompile_material(m);assert assets.save_loaded_asset(m,False)
# Reuse the established original geometry writer/import workflow without running weapon imports.
script=Path(ue.Paths.project_dir())/'Scripts/create_weapon_models.py'
namespace={};exec(script.read_text(encoding='utf-8-sig').split('source=ROOT/')[0],namespace)
model=namespace['Model']()
# Rounded molded pad with a separate padded backing and four stitched strap bands.
model.loft([(-5,2.7,.6,0),(-3,3.4,1,0),(3,3.4,1,0),(5,2.7,.6,0)],2,24)
model.loft([(-4,2.3,.6,-.8),(0,2.9,.9,-.8),(4,2.3,.6,-.8)],0,24)
for x in [-3.2,3.2]: model.box((x,0,.6),(1.1,6.7,.35),2,.12)
source=Path(ue.Paths.project_dir())/'ContentSource/Character';source.mkdir(parents=True,exist_ok=True)
(source/'Crosshair.mtl').write_text('newmtl Paint\nKd .1 .1 .05\nnewmtl Metal\nKd .05 .05 .05\nnewmtl Rubber\nKd .02 .02 .02\nnewmtl Glass\nKd .01 .01 .01\n')
obj=source/'SM_ElbowPad.obj';model.write(obj)
options=ue.FbxImportUI();options.import_materials=False;options.import_textures=False;options.automated_import_should_detect_type=False;options.mesh_type_to_import=ue.FBXImportType.FBXIT_STATIC_MESH
options.static_mesh_import_data.combine_meshes=True;options.static_mesh_import_data.auto_generate_collision=False
options.static_mesh_import_data.normal_import_method=ue.FBXNormalImportMethod.FBXNIM_IMPORT_NORMALS
task=ue.AssetImportTask();task.filename=str(obj);task.destination_path=folder;task.destination_name='SM_ElbowPad';task.automated=True;task.replace_existing=True;task.save=True;task.options=options
tools.import_asset_tasks([task]);mesh=assets.load_asset(folder+'/SM_ElbowPad');assert mesh
for i,slot in enumerate(mesh.static_materials):
 mesh.set_material(i,assets.load_asset('/Game/Crosshair/Weapons/Finishes/'+('M_PaintedWeapon' if str(slot.material_slot_name)=='Paint' else 'M_WeaponRubber')))
assert assets.save_loaded_asset(mesh,False)
bp=assets.load_asset('/Game/Crosshair/Player/BP_PracticeCharacter');cdo=ue.get_default_object(bp.generated_class())
cdo.set_editor_property('first_person_arm_materials',[m,m]);cdo.get_editor_property('combat_appearance').set_editor_property('elbow_pad',mesh)
assert assets.save_loaded_asset(bp,False)
ue.log('CROSSHAIR_COMBAT_CHARACTER_OK')
