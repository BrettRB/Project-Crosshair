"""Build /Game/Crosshair/Maps/L_SpaceStation: a copy of the playable Nuketown re-themed as an orbital station.

The layout, houses, cover, breakable windows, spawn points and navigation bounds are kept exactly, so the map
plays like Nuketown. The copy then gets:
  * foliage removed (no plants in orbit),
  * world-aligned procedural station materials in place of the suburban surfaces (deck plating with guide
    lights, white hull panels, solar-cell roofs, gunmetal fences and fittings, asteroid rock for the cliffs),
  * space lighting: no atmosphere, a hard white sun, a starfield sky sphere and a lit planet with an
    atmosphere rim, cool station light colours, and its own exposure/bloom grade,
  * solar arrays with warning beacons around the perimeter.
The map is tagged CrosshairCustomLighting so the shared Earth-sky rig leaves it alone.

Rerunning regenerates the map from L_Nuketown (designer edits to L_SpaceStation are replaced).
Run: UnrealEditor-Cmd <project> -run=pythonscript -script=<this file> -unattended -NullRHI -nosound
"""
import math
import unreal as ue

SOURCE = '/Game/Crosshair/Maps/L_Nuketown'
MAP = '/Game/Crosshair/Maps/L_SpaceStation'
FOLDER = '/Game/Crosshair/Maps/SpaceStationMaterials'
CENTER = ue.Vector(-600, 1300, 60)      # Nuketown play-space centre and street height (setup_match_spawns.py)
a = ue.EditorAssetLibrary
tools = ue.AssetToolsHelpers.get_asset_tools()
edit = ue.MaterialEditingLibrary

# Unload the target before replacing it.
ue.EditorLoadingAndSavingUtils.load_map('/Game/Crosshair/Maps/L_Practice')
if a.does_asset_exist(MAP):
    assert a.delete_asset(MAP), 'Could not delete the previous space station map'
    ue.SystemLibrary.collect_garbage()

# ---- Materials (world-aligned, so they need no UVs on the imported brushes) ----
NOISE = ('struct N{float h(float3 p){return frac(sin(dot(p,float3(127.1,311.7,74.7)))*43758.5453);}'
         'float n(float3 p){float3 i=floor(p);float3 f=frac(p);f=f*f*(3-2*f);return lerp(lerp(lerp(h(i),h(i+float3(1,0,0)),f.x),lerp(h(i+float3(0,1,0)),h(i+float3(1,1,0)),f.x),f.y),lerp(lerp(h(i+float3(0,0,1)),h(i+float3(1,0,1)),f.x),lerp(h(i+float3(0,1,1)),h(i+float3(1,1,1)),f.x),f.y),f.z);}'
         'float fbm(float3 p){return n(p)*.55+n(p*2.03+7.1)*.3+n(p*4.11+3.3)*.15;}'
         'float2 plane(float3 P,float3 Nn){float3 a=abs(Nn);return a.z>max(a.x,a.y)?P.xy:(a.x>a.y?P.yz:P.xz);}'
         'float seam(float2 uv,float s,float w){float2 f=frac(uv/s);float d=min(min(f.x,1-f.x),min(f.y,1-f.y))*s;return 1-smoothstep(0,w,d);}}; N g;')
HULL = NOISE + ('float2 uv=g.plane(P,Nn);float2 cell=floor(uv/160);float tint=.9+.1*g.h(float3(cell,3));'
                'float3 c=float3(.52,.54,.56)*tint;c=lerp(c,float3(.12,.13,.14),g.seam(uv,160,2.2));'
                'c=lerp(c,float3(.55,.2,.06),step(.93,g.h(float3(cell,9)))*.6);return c*(.94+.06*g.fbm(P*.02));')
DECK = NOISE + ('float2 uv=g.plane(P,Nn);float plate=g.seam(uv,200,2.5);float grip=g.n(P*.35)*.08;'
                'float3 c=float3(.11,.115,.12)*(.92+grip+.08*g.h(float3(floor(uv/200),1)));return lerp(c,float3(.03,.032,.035),plate);')
DECK_GLOW = NOISE + ('float2 uv=g.plane(P,Nn);float2 f=frac(uv/800);float d=min(min(f.x,1-f.x),min(f.y,1-f.y))*800;'
                     'return float3(.15,.65,1.0)*(1-smoothstep(1.5,4,d))*step(.6,abs(Nn.z))*4;')
SOLAR = NOISE + ('float2 uv=g.plane(P,Nn);float grid=g.seam(uv,45,1.2);float3 c=float3(.015,.03,.075)*(.85+.3*g.n(P*.05));'
                 'return lerp(c,float3(.35,.37,.4),grid);')
METAL = NOISE + 'return float3(.07,.075,.08)*(.85+.15*g.fbm(float3(P.x*.4,P.y*.4,P.z*.02)));'
ROCK = NOISE + 'float a=g.fbm(P*.0025);float b=g.fbm(P*.02+4);return lerp(float3(.07,.065,.06),float3(.16,.15,.14),a)*(.8+.4*b);'
# Sky: stars, a faint galactic band, the sun disc and a planet, all drawn analytically per pixel so the
# planet limb is perfectly round. S is the direction to the sun (the light below: pitch -32, yaw -58);
# the planet sits 22 degrees wide at yaw -20, elevation 14, roughly opposite the sun so it shows mostly lit (gibbous).
SKY = NOISE + ('float3 D=normalize(P-C);float3 S=normalize(float3(-.449,.719,.530));float3 PD=normalize(float3(.912,-.332,.242));'
               'float sr=sin(radians(11.0));float b=dot(D,PD);float disc=b*b-(1-sr*sr);bool hit=disc>0&&b>0;'
               'float3 cell=floor(D*420);float h=g.h(cell);float3 f=frac(D*420)-.5;'
               'float star=step(.9975,h)*(1-smoothstep(.05,.32,length(f)))*(2+30*g.h(cell+5));'
               'float band=exp(-pow(dot(D,normalize(float3(.35,.8,.48)))*6,2));float neb=g.fbm(D*9)*band;'
               'float3 col=(float3(1,.97,.92)*star+float3(.05,.07,.16)*neb*1.6+float3(.002,.0025,.004))*(hit?0:1);'
               'float sd=dot(D,S);col+=float3(1,.96,.88)*(smoothstep(.99985,.99993,sd)*60+pow(saturate(sd),900)*1.5)*(hit?0:1);'
               'if(hit){float3 H=D*(b-sqrt(disc));float3 Q=normalize(H-PD);float lit=saturate(dot(Q,S));'
               'float land=smoothstep(.5,.56,g.fbm(Q*3.2+1.7));float ice=smoothstep(.82,.9,abs(Q.z));'
               'float3 c=lerp(float3(.02,.085,.24),lerp(float3(.08,.13,.04),float3(.25,.2,.12),g.fbm(Q*8)),land);'
               'c=lerp(c,float3(.8,.82,.85),ice);c=lerp(c,float3(.85,.87,.9),smoothstep(.52,.68,g.fbm(Q*5+9))*.85);'
               'float rim=pow(1-saturate(dot(Q,-D)),3);col=c*lit*3.6+float3(.15,.35,1)*rim*saturate(dot(Q,S)+.3)*1.2+c*.004;}'
               'else{float off=acos(clamp(b,-1,1))-radians(11.0);col+=float3(.12,.3,.95)*exp(-max(off,0)*90)*saturate(dot(PD,S)+.6)*.8;}'
               'return col;')


def make_material(name, color_code, roughness, metallic, emissive_code=None, unlit=False, two_sided=False,
                  inputs=('P', 'Nn'), emissive_inputs=None):
    path = FOLDER + '/' + name
    if a.does_asset_exist(path):
        assert a.delete_asset(path), path
    m = tools.create_asset(name, FOLDER, ue.Material, ue.MaterialFactoryNew())
    m.set_editor_property('used_with_instanced_static_meshes', True)
    try:
        m.set_editor_property('used_with_nanite', True)
    except Exception:
        pass
    if unlit:
        m.set_editor_property('shading_model', ue.MaterialShadingModel.MSM_UNLIT)
    if two_sided:
        m.set_editor_property('two_sided', True)

    def node(cls):
        return edit.create_material_expression(m, cls)

    def custom(src, keys):
        n = node(ue.MaterialExpressionCustom)
        n.set_editor_property('code', src)
        n.set_editor_property('output_type', ue.CustomMaterialOutputType.CMOT_FLOAT3)
        params = []
        for key in keys:
            v = ue.CustomInput()
            v.set_editor_property('input_name', key)
            params.append(v)
        n.set_editor_property('inputs', params)
        sources = {'P': ue.MaterialExpressionWorldPosition, 'Nn': ue.MaterialExpressionVertexNormalWS,
                   'C': ue.MaterialExpressionCameraPositionWS, 'O': ue.MaterialExpressionObjectPositionWS}
        for key in keys:
            assert edit.connect_material_expressions(node(sources[key]), '', n, key), key
        return n

    color = custom(color_code, inputs)
    edit.connect_material_property(color, '', ue.MaterialProperty.MP_EMISSIVE_COLOR if unlit else ue.MaterialProperty.MP_BASE_COLOR)
    if not unlit:
        for prop, value in [(ue.MaterialProperty.MP_ROUGHNESS, roughness), (ue.MaterialProperty.MP_METALLIC, metallic)]:
            n = node(ue.MaterialExpressionConstant)
            n.r = value
            edit.connect_material_property(n, '', prop)
        if emissive_code:
            glow = custom(emissive_code, emissive_inputs if emissive_inputs is not None else inputs)
            edit.connect_material_property(glow, '', ue.MaterialProperty.MP_EMISSIVE_COLOR)
    edit.recompile_material(m)
    assert a.save_loaded_asset(m, False), name
    return m


hull = make_material('M_Station_Hull', HULL, .42, .25)
deck = make_material('M_Station_Deck', DECK, .5, .85, DECK_GLOW)
solar = make_material('M_Station_Solar', SOLAR, .18, .6)
metal = make_material('M_Station_Gunmetal', METAL, .34, 1.0)
rock = make_material('M_Station_Asteroid', ROCK, .92, 0.0)
stars = make_material('M_Station_Starfield', SKY, 1, 0, unlit=True, two_sided=True, inputs=('P', 'C'))
if a.does_asset_exist(FOLDER + '/M_Station_Planet'):
    a.delete_asset(FOLDER + '/M_Station_Planet')   # the planet is now drawn by the sky shader
beacon = make_material('M_Station_Beacon', 'return float3(.05,0,0);', .5, 0.0, 'return float3(30,1.5,.5);', inputs=(), emissive_inputs=())

# ---- Copy Nuketown ----
if not a.duplicate_asset(SOURCE, MAP):
    # A just-deleted map can linger in memory; save Nuketown under the new name instead.
    source = ue.EditorLoadingAndSavingUtils.load_map(SOURCE)
    assert source and ue.EditorLoadingAndSavingUtils.save_map(source, MAP), 'Could not copy Nuketown'
world = ue.EditorLoadingAndSavingUtils.load_map(MAP)
assert world, 'Space station copy could not be loaded'
actors = ue.get_editor_subsystem(ue.EditorActorSubsystem)

PLANTS = ['foliage', 'weed', 'flower', '_tree', 'trees', 'vine', '_fern', 'palm', 'pot_clay', 'plant', 'branches', 'grass_grass', 'hedge', 'bush']
KEEP = ['toolsnodraw', 'toolstrigger', 'glass', 'worldgrid', 'dev_', 'illum', 'curtain', 'book', 'flag', 'door_handle', 'cardboard']
FAMILIES = [
    (deck, ['mi_sand', 'mi_grass', 'mi_stone', 'curb', 'concrete_tile', 'tile_stone', 'floora', 'asphalt', 'road', 'dirt', 'sand']),
    (rock, ['rock', 'cliff']),
    (solar, ['mi_roof', 'roof', 'shingle']),
    (metal, ['metal', 'ibeam', 'iron', 'fence', 'gutter', 'pipe', 'joist', 'pole', 'rubber', 'garage', 'support', 'gate', 'black', 'wire', 'cable', 'electric', 'antenna', 'bolt']),
    (hull, ['wood', 'siding', 'plaster', 'wall', 'brick', 'concrete', 'paint', 'pared', 'marble', 'navajo', 'winningred', 'forest', 'carpet', 'tile', 'season', 'shacks', 'santorini', 'castle', 'stone']),
]


def family(material):
    path = material.get_path_name().lower()
    if any(k in path for k in KEEP):
        return None
    for target, keys in FAMILIES:
        if any(k in path for k in keys):
            return target
    return None


removed = restyled = 0
for actor in actors.get_all_level_actors():
    if isinstance(actor, (ue.InstancedFoliageActor, ue.SkyAtmosphere, ue.ExponentialHeightFog)):
        actors.destroy_actor(actor)
        removed += 1
        continue
    components = actor.get_components_by_class(ue.StaticMeshComponent)
    if isinstance(actor, ue.StaticMeshActor) and components and components[0].static_mesh:
        mesh = components[0].static_mesh.get_path_name().lower()
        if any(k in mesh for k in PLANTS):
            actors.destroy_actor(actor)
            removed += 1
            continue
    for c in components:
        for i in range(c.get_num_materials()):
            current = c.get_material(i)
            target = family(current) if current else None
            if target:
                c.set_material(i, target)
                restyled += 1

# ---- Lighting: vacuum sun, starfield, planet, station lights ----
sun = None
for actor in actors.get_all_level_actors():
    for light in actor.get_components_by_class(ue.LightComponent):
        if isinstance(light, ue.DirectionalLightComponent):
            sun = actor
            light.set_editor_property('atmosphere_sun_light', False)
            light.set_editor_property('use_temperature', False)
            light.set_light_color(ue.LinearColor(1.0, .98, .95, 1))
            light.set_intensity(9.0)
            light.set_editor_property('light_source_angle', .25)
        else:
            light.set_light_color(ue.LinearColor(.72, .88, 1.0, 1))
            light.set_intensity(light.intensity * 1.6)
    for sky in actor.get_components_by_class(ue.SkyLightComponent):
        sky.set_editor_property('source_type', ue.SkyLightSourceType.SLS_CAPTURED_SCENE)
        sky.set_editor_property('real_time_capture', False)
        sky.set_editor_property('sky_distance_threshold', 100000.0)
        sky.set_editor_property('lower_hemisphere_is_black', False)
        sky.set_editor_property('lower_hemisphere_color', ue.LinearColor(.06, .065, .075, 1))
        sky.set_intensity(4.0)   # fill so shaded sides stay readable in play
assert sun, 'Nuketown copy has no directional light'
sun.set_actor_rotation(ue.Rotator(roll=0, pitch=-32, yaw=-58), False)
# The light component can carry its own rotation; set it in world space so the sky shader's sun matches.
for light in sun.get_components_by_class(ue.DirectionalLightComponent):
    light.set_world_rotation(ue.Rotator(roll=0, pitch=-32, yaw=-58), False, False)
    ue.log('CROSSHAIR_SPACE_SUN forward=%s' % str(light.get_forward_vector()))
sun.tags = list(sun.tags) + [ue.Name('CrosshairCustomLighting')]

SPHERE = ue.load_asset('/Engine/BasicShapes/Sphere')
# Sky sphere mesh, sized from its own bounds.
SMOOTH = ue.load_asset('/Engine/EngineMeshes/Sphere') or SPHERE


def radius_scale(mesh, radius):
    r = radius / mesh.get_bounds().sphere_radius
    return ue.Vector(r, r, r)

CUBE = ue.load_asset('/Engine/BasicShapes/Cube')


def prop(mesh, material, location, scale, rotation=ue.Rotator(), label=None, shadows=True):
    actor = actors.spawn_actor_from_class(ue.StaticMeshActor, location, rotation)
    c = actor.static_mesh_component
    c.set_static_mesh(mesh)
    c.set_material(0, material)
    actor.set_actor_scale3d(scale)
    c.set_cast_shadow(shadows)
    c.set_collision_profile_name('NoCollision')
    c.set_collision_enabled(ue.CollisionEnabled.NO_COLLISION)
    if label:
        actor.set_actor_label(label)
    return actor


# The sky sphere is 8 km wide; its shader draws the stars, sun and planet.
prop(SMOOTH, stars, CENTER, radius_scale(SMOOTH, 400000.0), label='Space Starfield', shadows=False)

# Solar arrays on masts around the perimeter, outside the playable fences, with red beacons.
for k in range(10):
    t = k * math.tau / 10
    base = CENTER + ue.Vector(math.cos(t) * 9500, math.sin(t) * 9000, 0)
    yaw = math.degrees(t) + 90
    prop(CUBE, metal, base + ue.Vector(0, 0, 1200), ue.Vector(.8, .8, 24), label='Station Mast')
    for side in [-1, 1]:
        offset = ue.Vector(math.cos(t + math.pi / 2), math.sin(t + math.pi / 2), 0) * (side * 1900.0)
        prop(CUBE, solar, base + offset + ue.Vector(0, 0, 2100), ue.Vector(34, 9, .15), ue.Rotator(roll=18, pitch=0, yaw=yaw), 'Station Solar Wing')
        prop(CUBE, metal, base + offset * .5 + ue.Vector(0, 0, 2100), ue.Vector(19, .3, .3), ue.Rotator(roll=0, pitch=0, yaw=yaw), 'Station Truss')
    prop(SPHERE, beacon, base + ue.Vector(0, 0, 2450), ue.Vector(.9, .9, .9), label='Station Beacon', shadows=False)

# Exposure is held to the sunlit range (EV100 0.3-2) so shaded views do not lift the vacuum sky to grey.
# Grade: deep blacks, bright highlights and some bloom on the beacons and deck guide lights.
grade = actors.spawn_actor_from_class(ue.PostProcessVolume, CENTER)
grade.set_actor_label('Space Station Grade')
grade.set_editor_property('unbound', True)
s = grade.settings
for key, value in [('auto_exposure_method', ue.AutoExposureMethod.AEM_HISTOGRAM), ('auto_exposure_bias', 0.0),
                   ('auto_exposure_min_brightness', .3), ('auto_exposure_max_brightness', 2.0),
                   ('bloom_intensity', .65), ('ambient_occlusion_intensity', .7), ('color_contrast', ue.Vector4(1.1, 1.1, 1.1, 1)),
                   ('color_saturation', ue.Vector4(.95, .97, 1.0, 1)), ('white_temp', 6800.0), ('vignette_intensity', .35),
                   ('film_grain_intensity', .05), ('scene_fringe_intensity', .15), ('motion_blur_amount', .2)]:
    s.set_editor_property('override_' + key, True)
    s.set_editor_property(key, value)
grade.set_editor_property('settings', s)

assert ue.EditorLoadingAndSavingUtils.save_map(world, MAP), 'Space station map could not be saved'
spawns = len([x for x in actors.get_all_level_actors() if x.get_class().get_name() == 'CrosshairSpawnPoint'])
ue.log('CROSSHAIR_SPACE_STATION_OK removed=%d restyled=%d spawns=%d' % (removed, restyled, spawns))
