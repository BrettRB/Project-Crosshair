"""Original soldier outfits/gear on a project-owned copy of the template rig.
Only creates/updates /Game/Crosshair/Soldiers and BP_Target's appearance configuration.
"""
from pathlib import Path
import math
import unreal as ue
a=ue.EditorAssetLibrary;tools=ue.AssetToolsHelpers.get_asset_tools();edit=ue.MaterialEditingLibrary
folder='/Game/Crosshair/Soldiers';a.make_directory(folder)
source=Path(ue.Paths.project_dir())/'ContentSource/Character/Soldiers';source.mkdir(parents=True,exist_ok=True)
namespace={};script=Path(ue.Paths.project_dir())/'Scripts/create_weapon_models.py'
exec(script.read_text(encoding='utf-8-sig').split('source=ROOT/')[0],namespace);Model=namespace['Model']
(source/'Crosshair.mtl').write_text('newmtl Paint\nKd .1 .1 .05\nnewmtl Metal\nKd .05 .05 .05\nnewmtl Rubber\nKd .02 .02 .02\nnewmtl Glass\nKd .01 .01 .01\n')

def upload(name,m):
 # OBJ uses a right-handed basis; Unreal's importer flips Y. Author the mirrored
 # source so imported geometry matches the mannequin reference-pose coordinates.
 m.parts=[([(x,-y,z) for x,y,z in verts],[tuple(reversed(face)) for face in faces],slot,smooth) for verts,faces,slot,smooth in m.parts]
 obj=source/(name+'.obj');m.write(obj)
 options=ue.FbxImportUI();options.import_materials=False;options.import_textures=False;options.automated_import_should_detect_type=False;options.mesh_type_to_import=ue.FBXImportType.FBXIT_STATIC_MESH
 options.static_mesh_import_data.combine_meshes=True;options.static_mesh_import_data.auto_generate_collision=False;options.static_mesh_import_data.normal_import_method=ue.FBXNormalImportMethod.FBXNIM_IMPORT_NORMALS
 task=ue.AssetImportTask();task.filename=str(obj);task.destination_path=folder+'/Gear';task.destination_name=name;task.automated=True;task.replace_existing=True;task.save=True;task.options=options
 tools.import_asset_tasks([task]);mesh=a.load_asset(folder+'/Gear/'+name);assert mesh
 for i,slot in enumerate(mesh.static_materials):
  mesh.set_material(i,a.load_asset('/Game/Crosshair/Weapons/Finishes/'+('M_WeaponRubber' if str(slot.material_slot_name)=='Rubber' else 'M_OpticGlass' if str(slot.material_slot_name)=='Glass' else 'M_PaintedWeapon')))
 assert a.save_loaded_asset(mesh,False);return mesh

def dome(m,rx,ry,rz,lower=-.15,mat=1):
 # Curved closed shell with a separate reinforced rim. Rings avoid faceted primitive helmets.
 rings=12;segments=40;verts=[]
 for k in range(rings+1):
  phi=.03+(math.pi*.53-.03)*k/rings
  for i in range(segments):
   t=i*math.tau/segments;verts.append((rx*math.sin(phi)*math.cos(t),ry*math.sin(phi)*math.sin(t),rz*math.cos(phi)+lower))
 faces=[tuple(reversed(range(segments))),tuple(range(rings*segments,(rings+1)*segments))]
 for k in range(rings):
  for i in range(segments):
   j=(i+1)%segments;faces.append((k*segments+i,k*segments+j,(k+1)*segments+j,(k+1)*segments+i))
 m.part(verts,faces,mat,True)
helmet=Model();dome(helmet,11.8,13.0,12)
helmet.box((0,11,0),(19,4,1.3),1,.3)
for x in [-10.8,10.8]:
 helmet.box((x,0,-1.5),(1.4,12,2.4),2,.4)
 for y in [-3,0,3]:helmet.box((x*1.02,y,-1.5),(1,1.2,1),1,.15)
helmet.box((0,12.5,3),(4.2,1.4,3.5),2,.25)
helmet=upload('SM_BallisticHelmet',helmet)
cap=Model();dome(cap,10.5,12,7,mat=0);cap.box((0,14,-.2),(19,9,.9),0,.4);cap=upload('SM_FieldCap',cap)
hat=Model();dome(hat,11,12,7,mat=0)
# Elliptical broad stitched brim.
for k in range(40):
 t=k*math.tau/40;t2=(k+1)*math.tau/40
 hat.part([(11*math.cos(t),12*math.sin(t),-.8),(16*math.cos(t),17*math.sin(t),-1.4),(16*math.cos(t2),17*math.sin(t2),-1.4),(11*math.cos(t2),12*math.sin(t2),-.8)],[(0,1,2,3)],0)
hat=upload('SM_ReconBoonie',hat)
goggles=Model();goggles.box((0,0,0),(20,3,6),2,.8)
for x in [-5.2,5.2]:goggles.box((x,1.7,0),(8.5,.5,4.3),3,.7)
goggles.box((0,2,-2),(2,1,2.5),2,.5);goggles=upload('SM_ProtectiveGoggles',goggles)
headset=Model()
for x in [-11.5,11.5]:
 headset.tube((x-1,0,-3),(x+1,0,-3),4.5,2,segments=24)
headset.tube((-12,0,-3),(-8,13,-8),.35,1,segments=12);headset.box((-8,13,-8),(2,3,1.5),2,.4)
headset=upload('SM_CommsHeadset',headset)

def carrier(light=False):
 m=Model();width=32 if light else 38;height=32 if light else 41
 m.box((0,0,0),(width,5,height),0,2)
 m.box((0,3,1),(width-5,1.3,height-7),1,1.5)
 for x in [-width/2+5,width/2-5]:m.box((x,-1,19),(5,6,17),0,.8)
 for z in [-12,-6,0,6,12]:
  for x in [-12,-6,0,6,12]:m.box((x,3.8,z),(4.8,.7,1.4),0,.2)
 for x in [-9,0,9]:
  m.box((x,7,-8),(7.8,8,15),0,1)
  m.box((x,11.3,-2),(6.5,.6,3),0,.3)
  m.box((x,11.5,-6),(1.4,.5,5),2,.2)
 m.box((0,4,12),(12,.8,4),2,.3)
 return m
vest=upload('SM_PlateCarrier',carrier());lightvest=upload('SM_LightCarrier',carrier(True))
pack=Model();pack.box((0,0,0),(29,17,42),0,3)
pack.box((0,-9,-8),(24,5,17),0,2)
for x in [-9,9]:pack.box((x,-9,3),(2,2,28),2,.3)
pack=upload('SM_FieldPack',pack)
pouch=Model();pouch.box((0,0,0),(11,8,18),0,1.7);pouch.box((0,4.5,5),(9,1,5),0,.6);pouch.box((0,5,1),(2,1,4),2,.25);pouch=upload('SM_BeltPouch',pouch)
knee=Model();knee.box((0,0,0),(11,4,15),0,2);knee.box((0,2.6,0),(9,2.2,12),1,2)
for z in [-4,0,4]:knee.box((0,4,z),(7,.6,1.2),2,.25)
knee=upload('SM_KneeArmor',knee)

# All colors are restrained linear values; camouflage uses position and UV fabric weave.
palettes=[('Woodland','Woodland Infantry',(.065,.095,.038),(.13,.105,.055),(.02,.03,.016),(.075,.085,.038)),('Desert','Desert Patrol',(.32,.235,.125),(.17,.12,.067),(.085,.06,.035),(.20,.145,.075)),('Urban','Urban Assault',(.105,.12,.135),(.035,.047,.06),(.018,.025,.03),(.065,.075,.085)),('Arctic','Arctic Ranger',(.65,.68,.64),(.28,.34,.34),(.10,.16,.17),(.43,.47,.44)),('Night','Night Operations',(.022,.032,.025),(.012,.018,.016),(.005,.009,.007),(.022,.028,.021)),('Recon','Olive Recon',(.12,.145,.055),(.065,.09,.028),(.035,.045,.016),(.10,.115,.043))]
def lit(name,code,colors):
 path=folder+'/Materials/'+name
 if a.does_asset_exist(path):return a.load_asset(path)
 m=tools.create_asset(name,folder+'/Materials',ue.Material,ue.MaterialFactoryNew())
 m.set_editor_property('used_with_skeletal_mesh',True)
 def node(cls):return edit.create_material_expression(m,cls)
 def link(a,b,pin):assert edit.connect_material_expressions(a,'',b,pin),pin
 def custom(code,inputs):
  n=node(ue.MaterialExpressionCustom);n.set_editor_property('code',code);n.set_editor_property('output_type',ue.CustomMaterialOutputType.CMOT_FLOAT3)
  params=[]
  for key in inputs:v=ue.CustomInput();v.set_editor_property('input_name',key);params.append(v)
  n.set_editor_property('inputs',params);return n
 pos=node(ue.MaterialExpressionPreSkinnedPosition);interp=node(ue.MaterialExpressionVertexInterpolator);link(pos,interp,'');uv=node(ue.MaterialExpressionTextureCoordinate)
 c=custom(code,['P','UV','A','B','C']);link(interp,c,'P');link(uv,c,'UV')
 for key,color in zip(['A','B','C'],colors):
  n=node(ue.MaterialExpressionConstant3Vector);n.constant=ue.LinearColor(*color,1);link(n,c,key)
 edit.connect_material_property(c,'',ue.MaterialProperty.MP_BASE_COLOR)
 norm=custom('return normalize(float3(sin(UV.x*1800)*.10,sin(UV.y*1700)*.10,1));',['UV']);link(uv,norm,'UV');edit.connect_material_property(norm,'',ue.MaterialProperty.MP_NORMAL)
 for prop,value in [(ue.MaterialProperty.MP_METALLIC,0),(ue.MaterialProperty.MP_ROUGHNESS,.88),(ue.MaterialProperty.MP_SPECULAR,.22)]:
  n=node(ue.MaterialExpressionConstant);n.r=value;edit.connect_material_property(n,'',prop)
 edit.recompile_material(m);assert a.save_loaded_asset(m,False);return m
uniform_code='float w=sin(P.x*.25+sin(P.z*.19)*1.7)*sin(P.y*.32+P.z*.13);float3 cloth=lerp(A,B,smoothstep(-.3,.25,w));cloth=lerp(cloth,C,smoothstep(.53,.78,sin(P.z*.27+P.x*.15)));float boot=1-smoothstep(18,24,P.z);float hand=smoothstep(43,50,abs(P.x));float head=smoothstep(153,157,P.z);float eyes=step(163,P.z)*step(P.z,169)*step(6,P.y)*step(abs(P.x),8);float3 mask=lerp(C*.65,float3(.006,.009,.01),eyes);return lerp(lerp(cloth,float3(.015,.019,.014),max(boot,hand)),mask,head)*(.95+.05*sin(UV.x*1800)*sin(UV.y*1700));'
fabric_code='float n=sin(P.x*.41+sin(P.z*.22))*sin(P.y*.4+P.z*.27);return lerp(A,B,smoothstep(-.1,.6,n))*(.95+.05*sin(UV.x*1300)*sin(UV.y*1500));'
armor_code='return A*(.94+.06*sin(UV.x*440)*sin(UV.y*420));'
bodypath=folder+'/SKM_PracticeSoldier'
body=a.load_asset(bodypath) if a.does_asset_exist(bodypath) else a.duplicate_asset('/Game/Characters/Mannequins/Meshes/SKM_Manny_Simple',bodypath)
assert body
catalog=a.load_asset(folder+'/DA_SoldierAppearances') if a.does_asset_exist(folder+'/DA_SoldierAppearances') else None
if not catalog:
 factory=ue.DataAssetFactory();factory.set_editor_property('data_asset_class',ue.CrosshairSoldierCatalog)
 catalog=tools.create_asset('DA_SoldierAppearances',folder,ue.CrosshairSoldierCatalog,factory)
assert catalog;catalog.set_editor_property('body_mesh',body)
bp=a.load_asset('/Game/Crosshair/Targets/BP_Target');cdo=ue.get_default_object(bp.generated_class());model=cdo.get_editor_property('model')
designs=[]
for index,(id,label,base,patch,dark,gearcolor) in enumerate(palettes):
 d=ue.CrosshairSoldierDesign();d.set_editor_property('id',id);d.set_editor_property('name',label)
 d.set_editor_property('uniform',lit('M_Uniform_'+id,uniform_code,[base,patch,dark]));d.set_editor_property('fabric',lit('M_Fabric_'+id,fabric_code,[gearcolor,patch,dark]));d.set_editor_property('armor',lit('M_Armor_'+id,armor_code,[gearcolor,patch,dark]))
 pieces=[]
 def add(mesh,bone,offset):
  ref=ue.CrosshairSoldierAppearance.get_reference_bone_transform(model,bone)
  ue.log('SOLDIER_REF '+str(bone)+' '+str(ref.translation))
  p=ue.CrosshairSoldierGear();p.set_editor_property('mesh',mesh);p.set_editor_property('bone',bone)
  tr=ue.Transform();tr.translation=ref.translation+ue.Vector(*offset);p.set_editor_property('reference_transform',tr);pieces.append(p)
 add(cap if index==1 else hat if index==5 else helmet,'head',(0,0,12 if index in [1,5] else 6))
 if index in [0,2,4]:add(goggles,'head',(0,12,7));add(headset,'head',(0,0,10))
 add(lightvest if index in [1,5] else vest,'spine_03',(0,12,20))
 if index in [0,3,5]:add(pack,'spine_03',(0,-15,15))
 for bone,x in [('thigh_l',-13),('thigh_r',13)]:add(pouch,'pelvis',(x,11,-5))
 for bone in ['calf_l','calf_r']:add(knee,bone,(0,6,-2))
 d.set_editor_property('equipment',pieces);designs.append(d)
catalog.set_editor_property('designs',designs);assert a.save_loaded_asset(catalog,False);assert a.save_loaded_asset(body,False)
cdo.get_editor_property('soldier_appearance').set_editor_property('catalog',catalog);assert a.save_loaded_asset(bp,False)
ue.log('CROSSHAIR_SOLDIER_ASSETS_OK designs=6')
