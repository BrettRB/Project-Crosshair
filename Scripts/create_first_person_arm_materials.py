"""Create dedicated arms-only material copies; leave mannequin/template materials untouched.
The reference-pose mask excludes the torso/head/legs while preserving animated arms.
Run in UnrealEditor-Cmd with -run=pythonscript -script=<this file>.
"""
import unreal as ue
assets=ue.EditorAssetLibrary
lib=ue.MaterialEditingLibrary
folder='/Game/Crosshair/Player/Arms'
assets.make_directory(folder)
source='/Game/Characters/Mannequins/Materials/M_Mannequin'
path=folder+'/M_FirstPersonArms'
if not assets.does_asset_exist(path):
    mat=assets.duplicate_asset(source,path)
    if not mat:
        raise RuntimeError('Could not duplicate mannequin material')
    if mat.get_editor_property('use_material_attributes'):
        raise RuntimeError('Material attributes require an explicit opacity-mask hookup')
    mat.set_editor_property('blend_mode',ue.BlendMode.BLEND_MASKED)
    mat.set_editor_property('used_with_skeletal_mesh',True)
    def node(cls,x,y): return lib.create_material_expression(mat,cls,x,y)
    def link(a,out,b,pin):
        if not lib.connect_material_expressions(a,out,b,pin):
            raise RuntimeError('Failed material link: '+pin)
    pos=node(ue.MaterialExpressionPreSkinnedPosition,-1300,1000)
    interp=node(ue.MaterialExpressionVertexInterpolator,-1100,1000)
    link(pos,'',interp,'')
    x=node(ue.MaterialExpressionComponentMask,-900,1000)
    x.set_editor_property('r',True); x.set_editor_property('g',False);x.set_editor_property('b',False);x.set_editor_property('a',False)
    link(interp,'',x,'')
    ax=node(ue.MaterialExpressionAbs,-700,1000);link(x,'',ax,'')
    z=node(ue.MaterialExpressionComponentMask,-900,1200)
    z.set_editor_property('r',False);z.set_editor_property('g',False);z.set_editor_property('b',True);z.set_editor_property('a',False)
    link(interp,'',z,'')
    one=node(ue.MaterialExpressionConstant,-700,1400);one.set_editor_property('r',1.)
    zero=node(ue.MaterialExpressionConstant,-700,1500);zero.set_editor_property('r',0.)
    def greater(value,threshold,y):
        expr=node(ue.MaterialExpressionIf,-450,y);expr.set_editor_property('const_b',threshold)
        link(value,'',expr,'A')
        link(one,'',expr,'A > B');link(zero,'',expr,'A == B');link(zero,'',expr,'A < B')
        return expr
    arms=greater(ax,23.,1000)
    upper=greater(z,95.,1200)
    mask=node(ue.MaterialExpressionMultiply,-200,1000)
    link(arms,'',mask,'A');link(upper,'',mask,'B')
    if not lib.connect_material_property(mask,'',ue.MaterialProperty.MP_OPACITY_MASK):
        raise RuntimeError('Could not connect arms mask')
    lib.recompile_material(mat)
    if not assets.save_loaded_asset(mat,False): raise RuntimeError('Could not save arms material')
else:
    mat=assets.load_asset(path)
    if mat.get_editor_property('blend_mode') != ue.BlendMode.BLEND_MASKED:
        raise RuntimeError('Existing arms material has no mask')
# Upgrade existing dedicated material graphs to carry reference position from vertex to pixel shader.
mask=lib.get_material_property_input_node(mat,ue.MaterialProperty.MP_OPACITY_MASK)
for condition in lib.get_inputs_for_material_expression(mat,mask):
    value=lib.get_inputs_for_material_expression(mat,condition)[0]
    if isinstance(value,ue.MaterialExpressionAbs):
        value=lib.get_inputs_for_material_expression(mat,value)[0]
    source_node=lib.get_inputs_for_material_expression(mat,value)[0]
    if isinstance(source_node,ue.MaterialExpressionPreSkinnedPosition):
        interp=lib.create_material_expression(mat,ue.MaterialExpressionVertexInterpolator,-1100,1800)
        if not lib.connect_material_expressions(source_node,'',interp,'') or not lib.connect_material_expressions(interp,'',value,''):
            raise RuntimeError('Could not interpolate reference-pose position')
lib.recompile_material(mat)
if not assets.save_loaded_asset(mat,False): raise RuntimeError('Could not save arms shader')
materials=[]
for i in [1,2]:
    source='/Game/Characters/Mannequins/Materials/Manny/MI_Manny_0%d_New'%i
    path=folder+'/MI_FirstPersonArms_0%d'%i
    mi=assets.load_asset(path) if assets.does_asset_exist(path) else assets.duplicate_asset(source,path)
    lib.set_material_instance_parent(mi,mat)
    if not assets.save_loaded_asset(mi,False): raise RuntimeError('Could not save arms instance')
    materials.append(mi)
bp=assets.load_asset('/Game/Crosshair/Player/BP_PracticeCharacter')
ue.get_default_object(bp.generated_class()).set_editor_property('first_person_arm_materials',materials)
if not assets.save_loaded_asset(bp,False): raise RuntimeError('Could not save first-person arm configuration')
ue.log('CROSSHAIR_ARMS_MATERIALS_OK')

