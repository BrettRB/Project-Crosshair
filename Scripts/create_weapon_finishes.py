"""Build original layered camouflage and physically based weapon finishes.
Only project-owned finish graphs are rebuilt. Painted surfaces use slot Paint;
barrels, sights, magazines, grips and lenses retain their own surface materials.
"""
import unreal as ue
assets=ue.EditorAssetLibrary; editing=ue.MaterialEditingLibrary; tools=ue.AssetToolsHelpers.get_asset_tools()
ROOT='/Game/Crosshair/Weapons/Finishes'

def build(name, colors, metallic, roughness, camo=False):
    path=ROOT+'/'+name
    m=assets.load_asset(path) if assets.does_asset_exist(path) else tools.create_asset(name,ROOT,ue.Material,ue.MaterialFactoryNew())
    editing.delete_all_material_expressions(m)
    def node(cls,x=0,y=0): return editing.create_material_expression(m,cls,x,y)
    def link(a,b,key):
        if not editing.connect_material_expressions(a,'',b,key): raise RuntimeError('Cannot connect '+key)
    def scalar(value):
        n=node(ue.MaterialExpressionConstant); n.set_editor_property('r',value); return n
    def color(value):
        n=node(ue.MaterialExpressionConstant3Vector); n.set_editor_property('constant',ue.LinearColor(*value,1)); return n
    def noise(scale,offset):
        uv=node(ue.MaterialExpressionTextureCoordinate,-1100,offset)
        xyz=node(ue.MaterialExpressionAppendVector,-900,offset); link(uv,xyz,'A'); link(scalar(offset*.017),xyz,'B')
        n=node(ue.MaterialExpressionNoise,-700,offset); n.set_editor_property('scale',scale)
        n.set_editor_property('levels',3); n.set_editor_property('quality',2)
        n.set_editor_property('output_min',0); n.set_editor_property('output_max',1); link(xyz,n,'World Position'); return n
    def mask(n,threshold):
        mult=node(ue.MaterialExpressionMultiply); mult.set_editor_property('const_b',18); link(n,mult,'A')
        bias=node(ue.MaterialExpressionAdd); bias.set_editor_property('const_b',-threshold*18); link(mult,bias,'A')
        sat=node(ue.MaterialExpressionSaturate); link(bias,sat,''); return sat
    def blend(a,b,alpha):
        n=node(ue.MaterialExpressionLinearInterpolate); link(a,n,'A'); link(b,n,'B'); link(alpha,n,'Alpha'); return n
    surface=color(colors[0])
    if camo:
        for i,(scale,threshold) in enumerate([(3.8,.43),(6.1,.54),(10.5,.64)]):
            surface=blend(surface,color(colors[i+1]),mask(noise(scale,160+i*160),threshold))
    if camo or name == 'M_PaintedWeapon':
        # Sparse coating chips reveal a subdued gray layer rather than bright
        # clean plastic. The pattern remains dominant at normal viewing distance.
        wear=mask(noise(180,760),.73)
        strength=node(ue.MaterialExpressionMultiply); strength.set_editor_property('const_b',.32); link(wear,strength,'A')
        surface=blend(surface,color((.12,.13,.115)),strength)
    fine=noise(450,900)
    # Small variation in coating color, roughness, and tangent normals gives a
    # matte painted finish rather than a uniform plastic shader.
    tint=node(ue.MaterialExpressionMultiply); link(surface,tint,'A')
    grain=node(ue.MaterialExpressionMultiply); grain.set_editor_property('const_b',.07); link(fine,grain,'A')
    bias=node(ue.MaterialExpressionAdd); bias.set_editor_property('const_b',.93); link(grain,bias,'A'); link(bias,tint,'B')
    editing.connect_material_property(tint,'',ue.MaterialProperty.MP_BASE_COLOR)
    r=node(ue.MaterialExpressionMultiply); r.set_editor_property('const_b',.12); link(fine,r,'A')
    rb=node(ue.MaterialExpressionAdd); rb.set_editor_property('const_b',roughness-.06); link(r,rb,'A')
    editing.connect_material_property(rb,'',ue.MaterialProperty.MP_ROUGHNESS)
    editing.connect_material_property(scalar(metallic),'',ue.MaterialProperty.MP_METALLIC)
    bump=node(ue.MaterialExpressionCustom)
    bump.set_editor_property('code','return normalize(float3(sin(UV.x*1800.0)*0.018, sin(UV.y*1700.0)*0.018, 1.0));')
    bump.set_editor_property('output_type',ue.CustomMaterialOutputType.CMOT_FLOAT3)
    inp=ue.CustomInput(); inp.set_editor_property('input_name','UV'); bump.set_editor_property('inputs',[inp])
    link(node(ue.MaterialExpressionTextureCoordinate),bump,'UV')
    editing.connect_material_property(bump,'',ue.MaterialProperty.MP_NORMAL)
    m.set_editor_property('used_with_skeletal_mesh',True)
    editing.recompile_material(m); assets.save_loaded_asset(m,False); return m

wood=build('M_Woodland',[(.075,.093,.046),(.15,.135,.075),(.04,.052,.025),(.016,.019,.012)],.05,.64,True)
desert=build('M_Desert',[(.29,.225,.145),(.42,.34,.23),(.16,.125,.08),(.065,.052,.035)],.05,.67,True)
original=build('M_PaintedWeapon',[(.032,.039,.032)],.08,.57)
metal=build('M_WeaponMetal',[(.035,.04,.045)],.88,.34)
rubber=build('M_WeaponRubber',[(.013,.016,.018)],0,.8)
glass=build('M_OpticGlass',[(.006,.018,.026)],.35,.09)
# Keep existing stable instance asset paths and skin IDs for saved profiles.
instances=[]
for name,parent in [('Woodland',wood),('Desert',desert)]:
    path=ROOT+'/MI_'+name
    instance=assets.load_asset(path) if assets.does_asset_exist(path) else tools.create_asset('MI_'+name,ROOT,ue.MaterialInstanceConstant,ue.MaterialInstanceConstantFactoryNew())
    editing.set_material_instance_parent(instance,parent); assets.save_loaded_asset(instance,False); instances.append(instance)
for name in ['Sniper','AR','SMG']:
    definition=assets.load_asset('/Game/Crosshair/Weapons/DA_'+name); mesh=definition.get_editor_property('presentation_mesh')
    slots=[]
    for previous in mesh.get_editor_property('static_materials'):
        key=str(previous.get_editor_property('material_slot_name'))
        slot=previous.copy()
        slot.set_editor_property('material_slot_name',key)
        slot.set_editor_property('material_interface',{'Paint':original,'Metal':metal,'Rubber':rubber,'Glass':glass}[key])
        slots.append(slot)
    mesh.set_editor_property('static_materials',slots); assets.save_loaded_asset(mesh,False)
    for slot in mesh.get_editor_property('static_materials'):
        if not slot.get_editor_property('material_interface'): raise RuntimeError('Missing model surface material')
        ue.log(name+' material '+str(slot.get_editor_property('material_slot_name'))+' = '+slot.get_editor_property('material_interface').get_path_name())
    skins=[]
    for skin_name,instance in zip(['Woodland','Desert'],instances):
        skin=ue.CrosshairWeaponSkin(); skin.set_editor_property('id',skin_name); skin.set_editor_property('display_name',skin_name)
        skin.set_editor_property('materials',[instance if str(slot.get_editor_property('material_slot_name'))=='Paint' else None for slot in slots])
        skins.append(skin)
    definition.set_editor_property('skins',skins); assets.save_loaded_asset(definition,False)
ue.log('CROSSHAIR_REALISTIC_FINISHES_OK')
