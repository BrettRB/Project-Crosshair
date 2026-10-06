"""Author only original frag/tomahawk models; do not regenerate firearm assets."""
from pathlib import Path
import math
import unreal as ue
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



root=Path(ue.Paths.project_dir()); source=root/'ContentSource'/'Throwables'; source.mkdir(parents=True,exist_ok=True)
(source/'Crosshair.mtl').write_text('\n'.join('newmtl '+n+'\nKd .1 .1 .1\n' for n in ['Paint','Metal','Rubber','Glass']))
frag=Model()
frag.loft([(-4,.8,1.2,0),(-3,2.8,3.8,0),(-1,4,5,0),(1,4,5,0),(3,2.8,3.8,0),(4,.8,1.2,0)],0,48)
frag.tube((0,0,4.5),(0,0,6.3),1.4,1,segments=32)
frag.box((2.6,0,2.5),(.7,1.1,7.8),1,.15)
frag.tube((0,-1.8,5.5),(0,1.8,5.5),.22,1,segments=16)
for i in range(24):
 a=i*math.tau/24; b=(i+1)*math.tau/24
 frag.tube((1.3*math.cos(a),-2,6.4+1.3*math.sin(a)),(1.3*math.cos(b),-2,6.4+1.3*math.sin(b)),.13,1,segments=8)
axe=Model()
axe.tube((0,0,-18),(0,0,11),1.05,2,r2=.85,segments=32)
axe.profile([(-1,9),(-1,12),(3,13),(6,15),(9,17),(11,17),(12,15),(11,11),(9,8),(7,7),(3,9)],1.2,1,.25)
axe.profile([(7,7),(9,8),(11,11),(12,15),(11.6,15.3),(10.5,11.2),(8.6,8.5)],.55,1,.1)
axe.tube((0,0,-18.5),(0,0,-17.5),1.15,1,segments=32)
for z in range(-15,3,2): axe.tube((0,0,z),(0,0,z+.28),1.1,1,segments=24)
assets=ue.EditorAssetLibrary; tools=ue.AssetToolsHelpers.get_asset_tools()
for name,model in [('Frag',frag),('Tomahawk',axe)]:
 obj=source/('SM_'+name+'.obj'); model.write(obj)
 options=ue.FbxImportUI(); options.set_editor_property('import_mesh',True)
 options.set_editor_property('import_materials',False); options.set_editor_property('import_textures',False)
 options.set_editor_property('automated_import_should_detect_type',False)
 options.set_editor_property('mesh_type_to_import',ue.FBXImportType.FBXIT_STATIC_MESH)
 data=options.get_editor_property('static_mesh_import_data'); data.set_editor_property('combine_meshes',True)
 data.set_editor_property('auto_generate_collision',False)
 data.set_editor_property('normal_import_method',ue.FBXNormalImportMethod.FBXNIM_IMPORT_NORMALS)
 task=ue.AssetImportTask(); task.filename=str(obj); task.destination_path='/Game/Crosshair/Weapons/Throwables'
 task.destination_name='SM_'+name; task.automated=True; task.replace_existing=True; task.save=True; task.options=options
 tools.import_asset_tasks([task]); mesh=assets.load_asset(task.destination_path+'/'+task.destination_name)
 assert mesh,name
 for i,slot in enumerate(mesh.get_editor_property('static_materials')):
  label=str(slot.get_editor_property('material_slot_name'))
  finish={'Paint':'M_Woodland','Metal':'M_WeaponMetal','Rubber':'M_WeaponRubber','Glass':'M_OpticGlass'}.get(label,'M_WeaponMetal')
  mesh.set_material(i,assets.load_asset('/Game/Crosshair/Weapons/Finishes/'+finish))
 assert assets.save_loaded_asset(mesh,False)
ue.log('CROSSHAIR_LETHAL_MODELS_OK')
