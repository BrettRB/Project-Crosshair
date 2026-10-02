"""Author three original centimeter-scale rigid weapons and import through Unreal.
No downloaded or third-party artwork. Run with UnrealEditor-Cmd -run=pythonscript.
Generated OBJ sources are retained in ContentSource/Weapons for editing/reimport.
"""
from pathlib import Path
import math
import unreal as ue
ROOT = Path(ue.Paths.project_dir())
DEST = '/Game/Crosshair/Weapons/Models'


def sub(a,b): return tuple(x-y for x,y in zip(a,b))
def cross(a,b): return (a[1]*b[2]-a[2]*b[1],a[2]*b[0]-a[0]*b[2],a[0]*b[1]-a[1]*b[0])
def unit(a):
    length=math.sqrt(sum(x*x for x in a))
    return tuple(x/length for x in a) if length else (0,0,1)


class Model:
    def __init__(self): self.parts=[]
    def part(self, verts, faces, mat, smooth=False): self.parts.append((verts,faces,mat,smooth))
    def profile(self, outline, width, mat=0, bevel=.12, y=0):
        # Chamfer every outline edge rather than using a cube silhouette.
        cx=sum(p[0] for p in outline)/len(outline); cz=sum(p[1] for p in outline)/len(outline)
        radius=max(.5,max(math.hypot(x-cx,z-cz) for x,z in outline))
        verts=[]
        for yy,scale in [(-width/2,1-bevel/radius),(-width/2+bevel,1),(width/2-bevel,1),(width/2,1-bevel/radius)]:
            verts.extend((cx+(x-cx)*scale,yy+y,cz+(z-cz)*scale) for x,z in outline)
        n=len(outline); faces=[tuple(reversed(range(n))),tuple(range(3*n,4*n))]
        for k in range(3):
            for i in range(n): j=(i+1)%n; faces.append((k*n+i,k*n+j,(k+1)*n+j,(k+1)*n+i))
        self.part(verts,[tuple(reversed(f)) for f in faces],mat)
    def box(self, center, size, mat=1, bevel=.08):
        x,y,z=center; a,b,c=(v/2 for v in size)
        self.profile([(x-a,z-c+bevel),(x-a+bevel,z-c),(x+a-bevel,z-c),(x+a,z-c+bevel),(x+a,z+c-bevel),(x+a-bevel,z+c),(x-a+bevel,z+c),(x-a,z+c-bevel)],b*2,mat,bevel,y)
    def tube(self, a,b,r,mat=1,r2=None,segments=32):
        axis=unit(sub(b,a)); v=unit(cross(axis,(0,0,1) if abs(axis[2])<.9 else (0,1,0))); w=cross(axis,v)
        r2=r if r2 is None else r2
        verts=[]
        for p,radius in [(a,r),(b,r2)]:
            for i in range(segments):
                angle=2*math.pi*i/segments
                verts.append(tuple(p[k]+radius*(v[k]*math.cos(angle)+w[k]*math.sin(angle)) for k in range(3)))
        faces=[tuple(reversed(range(segments))),tuple(range(segments,segments*2))]
        faces.extend((i,(i+1)%segments,(i+1)%segments+segments,i+segments) for i in range(segments))
        self.part(verts,faces,mat,True)
    def loft(self, sections, mat=0, segments=32):
        # Elliptical cross sections create a rounded stock/receiver with tapered
        # proportions. End bevels prevent the hard cuboid ends of the prototype.
        first,last=sections[0],sections[-1]
        sections=[(first[0]-.12,first[1]*.94,first[2]*.94,first[3])]+sections+[(last[0]+.12,last[1]*.94,last[2]*.94,last[3])]
        verts=[]
        for x,ry,rz,z in sections:
            verts.extend((x,ry*math.cos(i*math.tau/segments),z+rz*math.sin(i*math.tau/segments)) for i in range(segments))
        faces=[tuple(reversed(range(segments))),tuple(range((len(sections)-1)*segments,len(sections)*segments))]
        for k in range(len(sections)-1):
            for i in range(segments):
                j=(i+1)%segments; faces.append((k*segments+i,k*segments+j,(k+1)*segments+j,(k+1)*segments+i))
        self.part(verts,faces,mat,True)
    def ring(self,x,y,z,r,thickness,depth,mat=1,segments=40):
        verts=[]
        for xx,rr in [(x-depth/2,r),(x+depth/2,r),(x-depth/2,r-thickness),(x+depth/2,r-thickness)]:
            verts.extend((xx,y+rr*math.cos(i*math.tau/segments),z+rr*math.sin(i*math.tau/segments)) for i in range(segments))
        faces=[]
        for i in range(segments):
            j=(i+1)%segments
            faces.extend([(i,j,segments+j,segments+i),(2*segments+j,2*segments+i,3*segments+i,3*segments+j),(j,i,2*segments+i,2*segments+j),(segments+i,segments+j,3*segments+j,3*segments+i)])
        self.part(verts,faces,mat,True)
    def rail(self,start,end,z,width=2):
        self.box(((start+end)/2,0,z),(end-start,width,.5),1)
        for i in range(int((end-start)/.8)):
            self.box((start+i*.8,0,z+.35),(.35,width+.3,.35),1,.035)
    def grip(self):
        self.profile([(-4,2),(-1,3),(3,1),(1,-9),(-2,-10),(-5,-8)],3.2,2,.2)
        for i in range(5): self.box((-2,-1.63,-2-i*1.2),(3.4,.18,.35),2,.06)
        # Open trigger guard and curved trigger, never a solid box.
        points=[(2,0,2),(8,0,2),(9,0,-2),(6,0,-4),(2,0,-3),(2,0,2)]
        for a,b in zip(points,points[1:]): self.tube(a,b,.28,1,segments=12)
        self.tube((5,0,2),(4.7,0,-1),.22,1,segments=12)
        self.tube((4.7,0,-1),(5.5,0,-1.7),.22,1,segments=12)
    def screws(self,x,z):
        for y in [-1,1]:
            self.tube((x,y*2.2,z),(x,y*2.45,z),.32,1,segments=16)
            self.box((x,y*2.48,z),(.38,.06,.07),2,.025)
    def write(self,path):
        lines=['# Original Crosshair weapon, centimeters, X forward / Z up','mtllib Crosshair.mtl']
        index=0; normal_index=0; uv_index=0
        for mat in range(4):
            lines.append('usemtl '+['Paint','Metal','Rubber','Glass'][mat])
            for verts,faces,slot,smooth in self.parts:
                if slot!=mat: continue
                lines.extend('v %.6f %.6f %.6f'%v for v in verts)
                side_normals={}
                if smooth:
                    for face in faces[2:]:
                        normal=unit(cross(sub(verts[face[1]],verts[face[0]]),sub(verts[face[2]],verts[face[0]])))
                        for i in face: side_normals[i]=tuple(a+b for a,b in zip(side_normals.get(i,(0,0,0)),normal))
                for fi,face in enumerate(faces):
                    normal=unit(cross(sub(verts[face[1]],verts[face[0]]),sub(verts[face[2]],verts[face[0]])))
                    refs=[]
                    # Project each face into its plane to prevent degenerate cap/screw UVs.
                    drop=max(range(3),key=lambda k:abs(normal[k])); axes=[k for k in range(3) if k!=drop]
                    for i in face:
                        lines.append('vt %.6f %.6f'%(verts[i][axes[0]]/12,verts[i][axes[1]]/12)); uv_index+=1
                        n=unit(side_normals[i]) if smooth and fi>=2 else normal
                        lines.append('vn %.6f %.6f %.6f'%n); normal_index+=1
                        refs.append('%d/%d/%d'%(index+i+1,uv_index,normal_index))
                    lines.append('f '+' '.join(refs))
                index+=len(verts)
        path.write_text('\n'.join(lines)+'\n')


def automatic(smg=False):
    m=Model(); m.grip()
    # Rounded upper receiver and tapered, chamfered lower receiver.
    if smg:
        m.tube((-6,0,7),(19,0,7),2.2,0)
        m.profile([(-7,3),(12,3),(18,5),(18,7),(-7,7)],3.6,0,.16)
        m.tube((18,0,7),(31,0,7),.75,1)
        m.tube((30,0,7),(34,0,7),1.3,1,1.1)
        m.profile([(15,4),(25,4.5),(27,6),(26,8.5),(15,9)],4.4,0,.25)
        for i in range(8): m.box((16+i*1.2,0,5),( .35,4.5,1.1),2,.08)
        # Compact stock with curved butt and two metal slide rails.
        m.tube((-25,-1.3,7),(-6,-1.3,7),.35,1)
        m.tube((-25,1.3,7),(-6,1.3,7),.35,1)
        m.profile([(-28,-2),(-25,-2),(-22,6),(-23,9),(-27,10),(-29,7)],4.3,2,.24)
        m.profile([(9,3),(13,3),(14,-4),(17,-14),(14,-15),(10,-6)],2.6,1,.16)
        for y in [-1.35,1.35]:
            for i in range(3): m.box((11+i*.55,y,-5),( .15,.1,10),2,.02)
        rear=-3; front=28
    else:
        m.loft([(-8,2.15,2.3,7.3),(13,2.15,2.3,7.3),(17,1.7,1.8,7.3)],0,24)
        m.profile([(-6,2),(5,2),(9,0),(14,0),(17,5),(-7,5)],3.8,0,.2)
        m.tube((16,0,8),(46,0,8),.8,1)
        m.tube((44,0,8),(48,0,8),1.15,1,1)
        m.profile([(17,5),(33,5.5),(35,7),(34,10),(17,10)],4.5,0,.22)
        m.rail(17,34,10.3)
        for y in [-2.3,2.3]:
            for i in range(7): m.box((19+i*1.85,y,7.7),(1.1,.15,.65),2,.12)
        m.tube((-26,0,7),(-8,0,7),1.1,1)
        m.loft([(-27,2,1.25,7.8),(-24,2.15,1.3,7.8),(-13,1.7,1.15,7.8)],0)
        m.profile([(-27,-2),(-25,-2),(-13,5.8),(-13,7.5),(-16,7.5),(-27,.5)],2.2,0,.2)
        m.profile([(-29,-2),(-27,-2),(-27,8),(-29,8)],4.6,2,.15)
        m.profile([(9,1),(15,1),(15,-7),(19,-13),(17,-15),(12,-14),(9,-7)],3.1,1,.18)
        for y in [-1.59,1.59]:
            for i in range(4): m.box((10+i*.8,y,-6),( .22,.12,9),2,.025)
        rear=-4; front=38
    # Ejection port, charging handle, safety selector, pins, sight aperture.
    m.box((8,2.27,7.5),(6,.13,1.7),2,.1)
    m.box((9,2.4,6.7),(5,.2,.18),1,.04)
    m.tube((-4,-2.2,7),(-4,-3.4,7),.35,1)
    m.tube((-4,-3.4,7),(-1,-3.4,7),.3,1)
    m.screws(-3,4); m.screws(13,4)
    m.rail(-6,13,10.2)
    m.box((rear,0,11),(1.8,2.3,1.3),1)
    m.ring(rear,0,12.8,.75,.16,.35)
    m.box((front,0,10),(1.2,2,3.8),1)
    m.box((front,0,12.65),(.5,.2,.7),1,.025)
    for y in [-.8,.8]: m.box((front,y,12.2),(.7,.15,1.3),1,.05)
    return m


def sniper():
    m=Model(); m.grip()
    m.loft([(-35,2.3,4.5,2),(-31,2.5,4.5,2),(-23,2.1,3.8,3),(-12,1.7,2.1,4.8),(-7,1.9,1.8,5)],0)
    m.loft([(-7,2,1.7,5),(20,2.15,1.7,5),(31,1.9,1.4,5),(35,1.5,1.1,5)],0)
    m.profile([(-36,-3),(-34,-3),(-34,7),(-36,7)],5.6,2,.18)
    m.loft([(-29,1.9,1.1,9),(-26,2.15,1.4,9),(-17,2.15,1.4,9),(-13,1.8,1.1,9)],0)
    m.tube((-8,0,8),(24,0,8),2,1,1.6)
    m.tube((24,0,8),(72,0,8),1.25,1,.8)
    m.ring(70,0,8,1.1,.4,2)
    m.profile([(8,3),(14,3),(14,-3),(8,-3)],3.5,1,.12)
    m.tube((0,2,8),(0,5,6),.4,1)
    m.tube((0,5,6),(0,5,4),.55,1,.75)
    m.screws(-5,5); m.screws(24,5)
    m.rail(-9,15,10)
    for x in [-4,10]:
        m.box((x,0,11.5),(2,3.5,2.5),1)
        m.ring(x,0,15,1.65,.25,1.4)
    m.tube((-14,0,15),(15,0,15),1.4,1)
    m.tube((15,0,15),(23,0,15),1.4,1,2.5)
    m.ring(23,0,15,2.65,.3,1.1)
    m.tube((23.1,0,15),(23.2,0,15),2.3,3)
    m.tube((-14,0,15),(-19,0,15),1.4,1,1.9)
    m.tube((-19.15,0,15),(-19.1,0,15),1.65,3)
    m.ring(-19,0,15,2,.35,1.2)
    for x in [-17,-15,18,20]: m.ring(x,0,15,1.85 if x<0 else 2.25,.12,.3,2)
    m.tube((2,0,16),(2,0,18),1.05,1)
    m.tube((2,1,15),(2,3,15),1,1)
    for i in range(10): m.box((26+i*.65,2.68,5),( .25,.12,.8),2,.025)
    return m

source=ROOT/'ContentSource'/'Weapons'; source.mkdir(parents=True,exist_ok=True)
(source/'Crosshair.mtl').write_text('\n'.join('newmtl '+n+'\nKd .1 .1 .1\n' for n in ['Paint','Metal','Rubber','Glass']))
tools=ue.AssetToolsHelpers.get_asset_tools(); assets=ue.EditorAssetLibrary
for name,model in [('AR',automatic()),('SMG',automatic(True)),('Sniper',sniper())]:
    obj=source/('SM_'+name+'.obj'); model.write(obj)
    options=ue.FbxImportUI(); options.set_editor_property('import_mesh',True)
    options.set_editor_property('import_materials',False); options.set_editor_property('import_textures',False)
    options.set_editor_property('automated_import_should_detect_type',False)
    options.set_editor_property('mesh_type_to_import',ue.FBXImportType.FBXIT_STATIC_MESH)
    data=options.get_editor_property('static_mesh_import_data'); data.set_editor_property('combine_meshes',True)
    data.set_editor_property('auto_generate_collision',False)
    data.set_editor_property('normal_import_method',ue.FBXNormalImportMethod.FBXNIM_IMPORT_NORMALS)
    task=ue.AssetImportTask(); task.set_editor_property('filename',str(obj)); task.set_editor_property('destination_path',DEST)
    task.set_editor_property('destination_name','SM_'+name); task.set_editor_property('automated',True)
    task.set_editor_property('replace_existing',True); task.set_editor_property('save',True); task.set_editor_property('options',options)
    tools.import_asset_tasks([task])
    mesh=assets.load_asset(DEST+'/SM_'+name)
    if not mesh: raise RuntimeError('Model import failed: '+name)
    slots=mesh.get_editor_property('static_materials')
    if len(slots)<3: raise RuntimeError('Expected paint/metal/rubber slots: '+str(slots))
    for i,slot in enumerate(slots): ue.log(name+' slot '+str(i)+' = '+str(slot.get_editor_property('material_slot_name')))
    definition=assets.load_asset('/Game/Crosshair/Weapons/DA_'+name)
    definition.set_editor_property('presentation_mesh',mesh); definition.set_editor_property('use_prototype_geometry',False)
    definition.set_editor_property('display_name',{'AR':'Assault Rifle','SMG':'Submachine Gun','Sniper':'Bolt-Action Sniper'}[name])
    definition.set_editor_property('hip_offset',ue.Vector(48,12,-19))
    definition.set_editor_property('aim_offset',ue.Vector(35,0,-15 if name=='Sniper' else -12.8))
    assets.save_loaded_asset(definition,False)
ue.log('CROSSHAIR_AUTHORED_MODELS_OK')
