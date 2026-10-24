"""Original soldier outfits/gear on a project-owned copy of the template rig.
Only creates/updates /Game/Crosshair/Soldiers and BP_Target's appearance configuration.

Gear is authored in the body's reference-pose (mesh) space from measured body cross-sections
(run -CrosshairSmoke -CrosshairSoldierProbe to re-measure), so helmets, carriers, belts and pads
sit on the surface with a small clearance instead of floating or clipping. Each piece is attached
to the bone it should follow; chest gear thickens with the runtime soldier physique
(CrosshairLocomotion::Physique) because it inherits the chest bone's scale.
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
FABRIC,HARD,DARK,GLASS=0,1,2,3  # OBJ slots -> soldier fabric, armor, rubber/strap, lens

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

# ---- Measured body (mesh space: X sideways, Y forward, Z up; cm) ----
# Torso slices (z, half width, back y, front y) and belly slices, from the soldier probe.
TORSO=[(104,14.0,-7.0,14.0),(108,14.1,-7.0,14.2),(112,13.8,-7.0,14.6),(116,14.5,-7.3,14.9),(120,15.5,-9.3,15.6),(124,17.2,-11.6,15.9),(128,18.1,-13.1,16.4),(132,18.5,-14.2,16.2),(136,17.9,-14.4,15.6),(140,16.6,-13.5,14.4),(144,15.4,-12.8,12.5),(148,14.2,-11.3,9.1),(152,11.8,-9.7,6.6),(156,9.5,-7.4,3.3)]
BELLY=[(88,15.0,-10.3,12.6),(92,16.9,-11.1,12.9),(96,16.5,-11.1,13.4),(100,16.3,-9.8,13.7),(104,15.6,-8.2,13.9)]
HEAD_CENTER=(0,1.8,166.0)   # skull: x +-8.8, y -8.6..14 (face front), z 154..180.5
KNEE=(12.6,2.6,48.5)        # left knee centre; calf front surface y 7.8

def slice_at(table,z):
 if z<=table[0][0]:return table[0][1:]
 for (z0,*v0),(z1,*v1) in zip(table,table[1:]):
  if z<=z1:
   t=(z-z0)/(z1-z0);return tuple(p+(q-p)*t for p,q in zip(v0,v1))
 return table[-1][1:]
def superellipse(rx,ry,cy,t,n=2.6):
 c,s=math.cos(t),math.sin(t)
 return (rx*math.copysign(abs(c)**(2/n),c),cy+ry*math.copysign(abs(s)**(2/n),s))

def shell(m,rows_outer,rows_inner,mat,cap_top=False):
 """Closed shell from matching outer/inner rows (each a list of points, top row first)."""
 seg=len(rows_outer[0]);R=len(rows_outer);verts=[p for row in rows_outer for p in row]+[p for row in rows_inner for p in row];I=R*seg;faces=[]
 for k in range(R-1):
  for i in range(seg):
   j=(i+1)%seg
   faces.append((k*seg+i,k*seg+j,(k+1)*seg+j,(k+1)*seg+i))
   faces.append(tuple(reversed((I+k*seg+i,I+k*seg+j,I+(k+1)*seg+j,I+(k+1)*seg+i))))
 last=(R-1)*seg
 for i in range(seg):
  j=(i+1)%seg
  faces.append((last+i,last+j,I+last+j,I+last+i))
  if not cap_top:faces.append((I+i,I+j,j,i))
 if cap_top:faces+= [tuple(reversed(range(seg))),tuple(range(I,I+seg))]
 m.part(verts,faces,mat,True)

def rotate_last(m,center,yaw):
 verts,faces,mat,smooth=m.parts[-1];c,s=math.cos(yaw),math.sin(yaw)
 m.parts[-1]=([(center[0]+(x-center[0])*c-(y-center[1])*s,center[1]+(x-center[0])*s+(y-center[1])*c,z) for x,y,z in verts],faces,mat,smooth)

# ---- Headgear ----
def helmet_shell(m,brow=170.5,back=164.5):
 cx,cy,cz=HEAD_CENTER;seg=56;R=16;outer=[];inner=[]
 for k in range(R):
  ro=[];ri=[]
  for i in range(seg):
   t=i*math.tau/seg
   rim=back+(brow-back)*max(0.0,math.sin(t))**1.6
   phi_rim=math.acos(max(-1,min(1,(rim-cz)/16.4)));phi=.06+(phi_rim-.06)*k/(R-1)
   for rx,ry,rz,row in [(11.4,14.2,16.4,ro),(10.2,13.0,15.2,ri)]:
    row.append((cx+rx*math.sin(phi)*math.cos(t),cy+ry*math.sin(phi)*math.sin(t),cz+rz*math.cos(phi)))
  outer.append(ro);inner.append(ri)
 shell(m,outer,inner,HARD,cap_top=True)
def helmet_front(z):
 phi=math.acos((z-HEAD_CENTER[2])/16.4);return HEAD_CENTER[1]+14.2*math.sin(phi)
helmet=Model();helmet_shell(helmet)
for x in [-11.7,11.7]:helmet.box((x,1.0,168.6),(1.1,11,1.8),DARK,.3)
helmet.box((0,helmet_front(172.5)+.55,172.5),(4.6,1.2,3.6),DARK,.3)
helmet=upload('SM_BallisticHelmet',helmet)

goggles=Model()
z=172.2;phi=math.acos((z-HEAD_CENTER[2])/16.4);rx=11.4*math.sin(phi)+.25;ry=14.2*math.sin(phi)+.25
band=[];seg=48
for zz in [z+1.1,z-1.1]:
 band.append([(rx*math.cos(i*math.tau/seg),HEAD_CENTER[1]+ry*math.sin(i*math.tau/seg),zz) for i in range(seg)])
shell(goggles,band,[[(x*.97,HEAD_CENTER[1]+(y-HEAD_CENTER[1])*.97,zz) for x,y,zz in row] for row in band],DARK)
front=HEAD_CENTER[1]+ry
goggles.box((0,front+.6,z+.4),(18,1.3,5.2),DARK,.5)
for x in [-4.6,4.6]:goggles.box((x,front+1.3,z+.4),(7.6,.5,4.0),GLASS,.6)
goggles=upload('SM_ProtectiveGoggles',goggles)

headset=Model()
for x in [-12.2,12.2]:headset.tube((x-1.4*math.copysign(1,x),.5,165.2),(x+1.4*math.copysign(1,x),.5,165.2),3.8,DARK,segments=28)
headset.tube((-12.6,3.0,163.0),(-6.5,14.8,161.0),.35,DARK,segments=12);headset.box((-5.8,15.4,161.0),(2,1.4,1.4),DARK,.4)
headset=upload('SM_CommsHeadset',headset)

def soft_crown(m,rim,radii,brim=None):
 cx,cy,cz=HEAD_CENTER[0],HEAD_CENTER[1],rim-1.0;seg=48;R=12;rows=[]
 rx,ry,rz=radii
 for k in range(R):
  phi=.06+(math.pi*.5-.06)*k/(R-1)
  rows.append([(cx+rx*math.sin(phi)*math.cos(i*math.tau/seg),cy+ry*math.sin(phi)*math.sin(i*math.tau/seg),cz+rz*math.cos(phi)) for i in range(seg)])
 shell(m,rows,[[(x*.92,cy+(y-cy)*.92,cz+(zz-cz)*.92) for x,y,zz in row] for row in rows],FABRIC,cap_top=True)
 if brim:
  start,end,width,droop=brim;verts=[];faces=[];n=40
  for i in range(n+1):
   t=start+(end-start)*i/n;c,s=math.cos(t),math.sin(t)
   verts+=[(rx*c,cy+ry*s,cz),((rx+width)*c,cy+(ry+width)*s,cz-droop),((rx+width)*c,cy+(ry+width)*s,cz-droop-.6),(rx*c,cy+ry*s,cz-.6)]
  for i in range(n):
   b=i*4;q=(i+1)*4
   faces+=[(b,q,q+1,b+1),(b+3,b+2,q+2,q+3),(b+1,q+1,q+2,b+2),(b,b+3,q+3,q)]
  m.part(verts,faces,FABRIC,True)
cap=Model();soft_crown(cap,169.2,(10.0,11.9,12.6),(math.radians(25),math.radians(155),7.0,1.2));cap=upload('SM_FieldCap',cap)
hat=Model();soft_crown(hat,169.6,(10.3,12.1,11.6),(0,math.tau,6.5,1.4));hat=upload('SM_ReconBoonie',hat)

# ---- Plate carriers (follow spine_03, so they thicken with the chest) ----
def carrier(light=False):
 m=Model();seg=64;R=12;bottom=111.0;front_top=145.0 if light else 149.0;side_top=131.0;clear=1.0
 def thickness(t):return 1.4+(.8 if light else 1.6)*min(1,max(0,(abs(math.sin(t))-.45)/.3))
 outer=[];inner=[]
 for k in range(R):
  ro=[];ri=[]
  for i in range(seg):
   t=i*math.tau/seg;top=side_top+(front_top-side_top)*abs(math.sin(t))**2.5
   z=top-(top-bottom)*k/(R-1);hw,back,frontY=slice_at(TORSO,z);cy=(back+frontY)/2;ry=(frontY-back)/2
   x,y=superellipse(hw+clear,ry+clear,cy,t);ri.append((x,y,z))
   x,y=superellipse(hw+clear+thickness(t),ry+clear+thickness(t),cy,t);ro.append((x,y,z))
  outer.append(ro);inner.append(ri)
 shell(m,outer,inner,FABRIC)
 def surface(z,side):
  hw,back,frontY=slice_at(TORSO,z);th=1.4+(.8 if light else 1.6)
  return (frontY+clear+th) if side>0 else (back-clear-th)
 for z in range(118,int(front_top)-3,5):m.box((0,surface(z,1)+.35,z),(20 if light else 24,.7,1.3),FABRIC,.15)
 pouches=[-6,6] if light else [-9,0,9]
 for x in pouches:
  y=surface(118,1);m.box((x,y+2.6,118),(7.2,5.2,12),FABRIC,1.0);m.box((x,y+5.3,123),(6.6,.7,3.2),FABRIC,.3)
 if not light:
  m.box((0,surface(140,1)+1.8,140.5),(13,3.4,7),FABRIC,.8)
  m.box((7,surface(128,-1)-2.5,128),(7.5,5,15),FABRIC,.8)
 # Shoulder straps lie on top of the trapezius (shoulder surface ~z155 at x+-8.6), not arched above it.
 for x in [-8.6,8.6]:
  path=[(x,surface(front_top-1,1)-.8,front_top-1),(x,5.6,153.6),(x*1.02,1.0,156.8),(x*1.02,-4.0,156.4),(x,-9.0,153.0),(x,surface(front_top-1,-1)+.8,front_top-1)]
  for s,e in zip(path,path[1:]):m.tube(s,e,1.25,FABRIC,segments=14)
 return m
vest=upload('SM_PlateCarrier',carrier());lightvest=upload('SM_LightCarrier',carrier(True))

pack=Model();hw,back,frontY=slice_at(TORSO,131);pack_front=back-1.0-3.0-.6
pack.box((0,pack_front-8.6,131),(28,17,40),FABRIC,3.2);pack.box((0,pack_front-17.8,124),(22,4.5,16),FABRIC,1.6)
for x in [-15.2,15.2]:pack.box((x,pack_front-8,126),(3.6,10,18),FABRIC,1.2)
for x in [-9,9]:pack.tube((x,pack_front-.5,149),(x,pack_front-.5,114),.9,DARK,segments=12)
pack=upload('SM_FieldPack',pack)

# ---- Battle belt with angled pouches (follows pelvis) ----
belt=Model();seg=60;outerrows=[];innerrows=[]
for z in [99.0,93.0]:
 hw,back,frontY=slice_at(BELLY,z);cy=(back+frontY)/2;ry=(frontY-back)/2
 innerrows.append([superellipse(hw+.8,ry+.8,cy,i*math.tau/seg)+(z,) for i in range(seg)])
 outerrows.append([superellipse(hw+2.6,ry+2.6,cy,i*math.tau/seg)+(z,) for i in range(seg)])
shell(belt,outerrows,innerrows,FABRIC)
hw,back,frontY=slice_at(BELLY,96);cy=(back+frontY)/2;ry=(frontY-back)/2
belt.box((0,superellipse(hw+2.6,ry+2.6,cy,math.pi/2)[1]+.6,96),(6,1.2,4.5),DARK,.4)
for deg in [38,142,215,325]:
 t=math.radians(deg);x,y=superellipse(hw+2.6,ry+2.6,cy,t);n=(math.cos(t)/(hw+2.6),math.sin(t)/(ry+2.6));l=math.hypot(*n);n=(n[0]/l,n[1]/l)
 c=(x+n[0]*4.2,y+n[1]*4.2,93.5);belt.box(c,(10,8,15),FABRIC,1.4);rotate_last(belt,c,math.atan2(n[1],n[0])-math.pi/2)
 f=(x+n[0]*8.4,y+n[1]*8.4,98.5);belt.box(f,(9,.8,4.5),FABRIC,.3);rotate_last(belt,f,math.atan2(n[1],n[0])-math.pi/2)
belt=upload('SM_BattleBelt',belt)

# ---- Knee pads (follow each calf) ----
def kneepad(side):
 m=Model();kx,ky,kz=KNEE;kx*=side;seg=24;R=6;outer=[];inner=[]
 for k in range(R):
  z=56.0-15.0*k/(R-1);ro=[];ri=[]
  for i in range(seg):
   t=math.radians(38+104*i/(seg-1))
   for r,row in [(8.8,ro),(7.2,ri)]:row.append((kx+r*math.cos(t)*.85,ky+r*math.sin(t),z))
  outer.append(ro);inner.append(ri)
 # open arc: build faces manually (no wrap-around)
 verts=[p for row in outer for p in row]+[p for row in inner for p in row];I=R*seg;faces=[]
 for k in range(R-1):
  for i in range(seg-1):
   faces.append((k*seg+i,k*seg+i+1,(k+1)*seg+i+1,(k+1)*seg+i));faces.append(tuple(reversed((I+k*seg+i,I+k*seg+i+1,I+(k+1)*seg+i+1,I+(k+1)*seg+i))))
 for i in range(seg-1):
  last=(R-1)*seg;faces+= [(last+i,last+i+1,I+last+i+1,I+last+i),(I+i,I+i+1,i+1,i)]
 for k in range(R-1):
  faces+= [(k*seg,(k+1)*seg,I+(k+1)*seg,I+k*seg),(I+k*seg+seg-1,I+(k+1)*seg+seg-1,(k+1)*seg+seg-1,k*seg+seg-1)]
 m.part(verts,faces,HARD,True)
 for z in [44.0,52.0]:m.box((kx,ky+8.9,z),(6.5,.6,1.2),DARK,.2)
 return m
knee_l=upload('SM_KneeArmor_L',kneepad(1));knee_r=upload('SM_KneeArmor_R',kneepad(-1))

# ---- Materials: noise camouflage, woven fabric, matte armor ----
NOISE='struct N{float h(float3 p){return frac(sin(dot(p,float3(127.1,311.7,74.7)))*43758.5453);} float n(float3 p){float3 i=floor(p);float3 f=frac(p);f=f*f*(3-2*f);return lerp(lerp(lerp(h(i),h(i+float3(1,0,0)),f.x),lerp(h(i+float3(0,1,0)),h(i+float3(1,1,0)),f.x),f.y),lerp(lerp(h(i+float3(0,0,1)),h(i+float3(1,0,1)),f.x),lerp(h(i+float3(0,1,1)),h(i+float3(1,1,1)),f.x),f.y),f.z);} float fbm(float3 p){return n(p)*.55+n(p*2.03+7.1)*.3+n(p*4.11+3.3)*.15;}}; N g;'
uniform_code=NOISE+'float a=g.fbm(P*.085);float b=g.fbm(P*.14+11.0);float3 cloth=lerp(A,B,smoothstep(.47,.53,a));cloth=lerp(cloth,C,smoothstep(.57,.61,b));cloth*=.93+.07*g.n(float3(UV*900,0));float boot=1-smoothstep(18,24,P.z);float hand=smoothstep(43,50,abs(P.x));float head=smoothstep(153,157,P.z);float eyes=step(163,P.z)*step(P.z,169)*step(6,P.y)*step(abs(P.x),8);float3 mask=lerp(C*.55,float3(.006,.009,.01),eyes);return lerp(lerp(cloth,float3(.014,.016,.013),max(boot,hand)),mask,head);'
fabric_code=NOISE+'float a=g.fbm(P*.12);float b=g.fbm(P*.19+5.0);float3 c=lerp(A,B,smoothstep(.47,.53,a));c=lerp(c,C,smoothstep(.58,.62,b));return c*(.92+.08*g.n(float3(UV*1300,1)));'
armor_code=NOISE+'float w=g.fbm(P*.35);return A*(.86+.14*w);'
normal_code=NOISE+'float2 weave=float2(sin(UV.x*1800),sin(UV.y*1700))*.07;float2 fold=float2(g.n(P*.06)-.5,g.n(P*.06+9.0)-.5)*.5;return normalize(float3(weave+fold,1));'
palettes=[('Woodland','Woodland Infantry',(.065,.095,.038),(.13,.105,.055),(.02,.03,.016),(.075,.085,.038)),('Desert','Desert Patrol',(.32,.235,.125),(.17,.12,.067),(.085,.06,.035),(.20,.145,.075)),('Urban','Urban Assault',(.105,.12,.135),(.035,.047,.06),(.018,.025,.03),(.065,.075,.085)),('Arctic','Arctic Ranger',(.65,.68,.64),(.28,.34,.34),(.10,.16,.17),(.43,.47,.44)),('Night','Night Operations',(.022,.032,.025),(.012,.018,.016),(.005,.009,.007),(.022,.028,.021)),('Recon','Olive Recon',(.12,.145,.055),(.065,.09,.028),(.035,.045,.016),(.10,.115,.043))]
def lit(name,code,colors,roughness,with_folds=True,boot_roughness=None):
 # Fresh "_Real" materials (clearing an existing graph in a commandlet trips an engine assertion);
 # the superseded originals are removed once the catalog points at these.
 name=name+'_Real';path=folder+'/Materials/'+name
 if a.does_asset_exist(path):assert a.delete_asset(path),path
 m=tools.create_asset(name,folder+'/Materials',ue.Material,ue.MaterialFactoryNew())
 m.set_editor_property('used_with_skeletal_mesh',True);m.set_editor_property('used_with_static_lighting',False)
 def node(cls):return edit.create_material_expression(m,cls)
 def link(x,y,pin):assert edit.connect_material_expressions(x,'',y,pin),pin
 def custom(src,inputs,output=ue.CustomMaterialOutputType.CMOT_FLOAT3):
  n=node(ue.MaterialExpressionCustom);n.set_editor_property('code',src);n.set_editor_property('output_type',output)
  params=[]
  for key in inputs:v=ue.CustomInput();v.set_editor_property('input_name',key);params.append(v)
  n.set_editor_property('inputs',params);return n
 pos=node(ue.MaterialExpressionPreSkinnedPosition);interp=node(ue.MaterialExpressionVertexInterpolator);link(pos,interp,'');uv=node(ue.MaterialExpressionTextureCoordinate)
 c=custom(code,['P','UV','A','B','C']);link(interp,c,'P');link(uv,c,'UV')
 for key,color in zip(['A','B','C'],colors):
  n=node(ue.MaterialExpressionConstant3Vector);n.constant=ue.LinearColor(*color,1);link(n,c,key)
 edit.connect_material_property(c,'',ue.MaterialProperty.MP_BASE_COLOR)
 if with_folds:
  norm=custom(normal_code,['P','UV']);link(interp,norm,'P');link(uv,norm,'UV');edit.connect_material_property(norm,'',ue.MaterialProperty.MP_NORMAL)
 rough=custom('return lerp(%.3f,%.3f,1-smoothstep(18,24,P.z));'%(roughness,boot_roughness if boot_roughness is not None else roughness),['P'],ue.CustomMaterialOutputType.CMOT_FLOAT1);link(interp,rough,'P')
 edit.connect_material_property(rough,'',ue.MaterialProperty.MP_ROUGHNESS)
 for prop,value in [(ue.MaterialProperty.MP_METALLIC,0),(ue.MaterialProperty.MP_SPECULAR,.3)]:
  n=node(ue.MaterialExpressionConstant);n.r=value;edit.connect_material_property(n,'',prop)
 edit.recompile_material(m);assert a.save_loaded_asset(m,False);return m

bodypath=folder+'/SKM_PracticeSoldier'
body=a.load_asset(bodypath) if a.does_asset_exist(bodypath) else a.duplicate_asset('/Game/Characters/Mannequins/Meshes/SKM_Manny_Simple',bodypath)
assert body
catalog=a.load_asset(folder+'/DA_SoldierAppearances') if a.does_asset_exist(folder+'/DA_SoldierAppearances') else None
if not catalog:
 factory=ue.DataAssetFactory();factory.set_editor_property('data_asset_class',ue.CrosshairSoldierCatalog)
 catalog=tools.create_asset('DA_SoldierAppearances',folder,ue.CrosshairSoldierCatalog,factory)
assert catalog;catalog.set_editor_property('body_mesh',body)
bp=a.load_asset('/Game/Crosshair/Targets/BP_Target');cdo=ue.get_default_object(bp.generated_class())
designs=[]
for index,(id,label,base,patch,dark,gearcolor) in enumerate(palettes):
 d=ue.CrosshairSoldierDesign();d.set_editor_property('id',id);d.set_editor_property('name',label)
 d.set_editor_property('uniform',lit('M_Uniform_'+id,uniform_code,[base,patch,dark],.9,True,.55))
 d.set_editor_property('fabric',lit('M_Fabric_'+id,fabric_code,[gearcolor,patch,dark],.86))
 d.set_editor_property('armor',lit('M_Armor_'+id,armor_code,[gearcolor,patch,dark],.62,False))
 pieces=[]
 def add(mesh,bone):
  # Gear is authored in mesh space: identity reference transform, attached to the bone it follows.
  p=ue.CrosshairSoldierGear();p.set_editor_property('mesh',mesh);p.set_editor_property('bone',bone);p.set_editor_property('reference_transform',ue.Transform());pieces.append(p)
 add(cap if index==1 else hat if index==5 else helmet,'head')
 if index in [0,2,4]:add(goggles,'head');add(headset,'head')
 add(lightvest if index in [1,5] else vest,'spine_03')
 if index in [0,3,5]:add(pack,'spine_03')
 add(belt,'pelvis');add(knee_l,'calf_l');add(knee_r,'calf_r')
 d.set_editor_property('equipment',pieces);designs.append(d)
catalog.set_editor_property('designs',designs);assert a.save_loaded_asset(catalog,False);assert a.save_loaded_asset(body,False)
cdo.get_editor_property('soldier_appearance').set_editor_property('catalog',catalog);assert a.save_loaded_asset(bp,False)
# Remove superseded materials and gear meshes that no design references any more.
for id,*_ in palettes:
 for kind in ['Uniform','Fabric','Armor']:
  old=folder+'/Materials/M_%s_%s'%(kind,id)
  if a.does_asset_exist(old):a.delete_asset(old)
for old in ['SM_BeltPouch','SM_KneeArmor']:
 if a.does_asset_exist(folder+'/Gear/'+old):a.delete_asset(folder+'/Gear/'+old)
ue.log('CROSSHAIR_SOLDIER_ASSETS_OK designs=6')
