"""Run inside UnrealEditor-Cmd with -run=pythonscript -script=<this file>.
Creates missing Crosshair assets only. Existing assets are validated, never reset.
Blueprint defaults and weapon definitions remain editable in Unreal Editor.
"""
import unreal as ue

ROOT = "/Game/Crosshair"
tools = ue.AssetToolsHelpers.get_asset_tools()
assets = ue.EditorAssetLibrary
created = []
required = []

def asset(path, cls, factory):
    required.append((path, cls))
    if assets.does_asset_exist(path):
        obj = assets.load_asset(path)
        if not isinstance(obj, cls):
            raise RuntimeError("Wrong asset type at " + path)
        return obj, False
    folder, name = path.rsplit("/", 1)
    assets.make_directory(folder)
    obj = tools.create_asset(name, folder, cls, factory)
    if not obj:
        raise RuntimeError("Asset creation failed: " + path)
    created.append(path)
    return obj, True

def data(path, cls):
    factory = ue.DataAssetFactory()
    factory.set_editor_property("data_asset_class", cls)
    return asset(path, cls, factory)

def blueprint(path, parent):
    factory = ue.BlueprintFactory()
    factory.set_editor_property("parent_class", parent)
    return asset(path, ue.Blueprint, factory)

def save(obj):
    if not assets.save_loaded_asset(obj, only_if_is_dirty=False):
        raise RuntimeError("Unable to save " + obj.get_path_name())

def load(path):
    obj = assets.load_asset(path)
    if obj is None:
        raise RuntimeError("Required template asset missing: " + path)
    return obj

names = ["Move", "MouseLook", "StickLook", "Jump", "Sprint", "Crouch", "Fire", "Aim", "Reload", "SwitchWeapon", "Placement", "RotateTarget", "RemoveTarget", "ClearTargets", "SaveStart", "Reset", "Menu"]
actions = {}
for name in names:
    obj, new = asset(ROOT + "/Input/IA_" + name, ue.InputAction, ue.InputAction_Factory())
    if new:
        obj.set_editor_property("value_type", ue.InputActionValueType.AXIS2D if name in names[:3] else ue.InputActionValueType.BOOLEAN)
        save(obj)
    actions[name] = obj
context, new = asset(ROOT + "/Input/IMC_Practice", ue.InputMappingContext, ue.InputMappingContext_Factory())
if new:
    mappings = []
    def bind(name, key, swizzle=False, negate=False):
        key_value = ue.Key()
        key_value.set_editor_property("key_name", key)
        mapping = context.map_key(actions[name], key_value)
        modifiers = []
        if negate:
            mod = ue.InputModifierNegate(context)
            modifiers.append(mod)
        if swizzle:
            mod = ue.InputModifierSwizzleAxis(context)
            mod.set_editor_property("order", ue.InputAxisSwizzle.YXZ)
            modifiers.append(mod)
        mapping.set_editor_property("modifiers", modifiers)
        mappings.append(mapping)
    bind("Move", "W", True)
    bind("Move", "S", True, True)
    bind("Move", "D")
    bind("Move", "A", False, True)
    bind("Move", "Gamepad_Left2D")
    bind("MouseLook", "Mouse2D")
    bind("StickLook", "Gamepad_Right2D")
    keys = {
        "Jump": ("SpaceBar", "Gamepad_FaceButton_Bottom"),
        "Sprint": ("LeftShift", "Gamepad_LeftThumbstick"),
        "Crouch": ("C", "Gamepad_FaceButton_Right"),
        "Fire": ("LeftMouseButton", "Gamepad_RightTrigger"),
        "Aim": ("RightMouseButton", "Gamepad_LeftTrigger"),
        "Reload": ("R", "Gamepad_FaceButton_Left"),
        "SwitchWeapon": ("Q", "Gamepad_FaceButton_Top"),
        "Placement": ("P", "Gamepad_DPad_Up"),
        "RotateTarget": ("E", "Gamepad_DPad_Left"),
        "RemoveTarget": ("Delete",), "ClearTargets": ("BackSpace",),
        "SaveStart": ("K", "Gamepad_DPad_Right"), "Reset": ("T", "Gamepad_DPad_Down"),
        "Menu": ("Escape", "Gamepad_Special_Right")
    }
    for name, key_list in keys.items():
        for key in key_list:
            bind(name, key)
    context.set_editor_property("mappings", mappings)
    save(context)
config, new = data(ROOT + "/Input/DA_Input", ue.CrosshairInputConfig)
if new:
    config.set_editor_property("mapping", context)
    for name, action in actions.items():
        import re
        prop = re.sub(r"(?<!^)(?=[A-Z])", "_", name).lower()
        config.set_editor_property(prop, action)
    save(config)

mesh = load("/Game/Weapons/Rifle/Meshes/SKM_Rifle")
fire_sound = load("/Game/Weapons/GrenadeLauncher/Audio/FirstPersonTemplateWeaponFire02")
weapons = []
for name, automatic, magazine, interval, reload_time, ads, spread, recoil in [
        ("Sniper", False, 5, .85, 2.0, .25, 1.2, 1.5),
        ("AR", True, 30, .10, 1.8, .20, 1.5, .45),
        ("SMG", True, 30, .075, 1.6, .16, 2.0, .35)]:
    definition, new = data(ROOT + "/Weapons/DA_" + name, ue.CrosshairWeaponDefinition)
    if new:
        values = dict(display_name=name + " (prototype)", automatic=automatic, magazine_size=magazine,
                      shot_interval=interval, reload_seconds=reload_time, aim_seconds=ads,
                      hip_spread_degrees=spread, recoil_degrees=recoil, aim_fov=35.0 if name == "Sniper" else 65.0,
                      aim_spread_degrees=0.0 if name == "Sniper" else .15, mesh=mesh, fire_sound=fire_sound)
        for key, value in values.items():
            definition.set_editor_property(key, value)
        save(definition)
    weapons.append(definition)

material, new = asset(ROOT + "/Targets/M_Target", ue.Material, ue.MaterialFactoryNew())
if new:
    color = ue.MaterialEditingLibrary.create_material_expression(material, ue.MaterialExpressionVectorParameter, -250, 0)
    color.set_editor_property("parameter_name", "Color")
    color.set_editor_property("default_value", ue.LinearColor(.05, .7, .85, 1))
    ue.MaterialEditingLibrary.connect_material_property(color, "", ue.MaterialProperty.MP_BASE_COLOR)
    ue.MaterialEditingLibrary.recompile_material(material)
    save(material)
dummy, new = blueprint(ROOT + "/Targets/BP_Target", ue.CrosshairDummy)
if new:
    ue.get_default_object(dummy.generated_class()).set_editor_property("target_material", material)
    save(dummy)
character, new = blueprint(ROOT + "/Player/BP_PracticeCharacter", ue.CrosshairCharacter)
if new:
    cdo = ue.get_default_object(character.generated_class())
    cdo.set_editor_property("inputs", config)
    cdo.set_editor_property("default_loadout", weapons)
    cdo.set_editor_property("target_class", dummy.generated_class())
    cdo.set_editor_property("idle_animation", load("/Game/Characters/Mannequins/Anims/Rifle/MF_Rifle_Idle_ADS"))
    cdo.set_editor_property("reload_animation", load("/Game/Characters/Mannequins/Anims/Rifle/MM_Rifle_Reload"))
    cdo.get_editor_property("first_person_mesh").set_skeletal_mesh(load("/Game/Characters/Mannequins/Meshes/SKM_Manny_Simple"))
    save(character)
game, new = blueprint(ROOT + "/Player/BP_PracticeGameMode", ue.CrosshairGameMode)
if new:
    ue.get_default_object(game.generated_class()).set_editor_property("default_pawn_class", character.generated_class())
    save(game)

map_path = ROOT + "/Maps/L_Practice"
if not assets.does_asset_exist(map_path):
    level = ue.get_editor_subsystem(ue.LevelEditorSubsystem)
    if not level.new_level(map_path):
        raise RuntimeError("Cannot create practice level")
    actors = ue.get_editor_subsystem(ue.EditorActorSubsystem)
    cube = load("/Engine/BasicShapes/Cube")
    def block(label, location, scale):
        actor = actors.spawn_actor_from_class(ue.StaticMeshActor, ue.Vector(*location))
        actor.set_actor_label(label)
        actor.static_mesh_component.set_static_mesh(cube)
        actor.set_actor_scale3d(ue.Vector(*scale))
    block("Practice floor", (0, 0, -50), (100, 100, 1))
    block("Low jump platform", (-800, 0, 100), (5, 8, 2))
    block("High jump platform", (-1600, 0, 300), (5, 8, 6))
    block("Backstop", (3500, 0, 300), (1, 50, 6))
    block("Cover for obstruction checks", (800, -500, 100), (1, 8, 2))
    actors.spawn_actor_from_class(ue.PlayerStart, ue.Vector(-750, 0, 310))
    for y in (-500, 0, 500):
        actors.spawn_actor_from_class(dummy.generated_class(), ue.Vector(1800, y, 94), ue.Rotator(0, 180, 0))
    sun = actors.spawn_actor_from_class(ue.DirectionalLight, ue.Vector(0, 0, 1000), ue.Rotator(-45, -30, 0))
    sun.light_component.set_editor_property("intensity", 3.0)
    actors.spawn_actor_from_class(ue.SkyLight, ue.Vector(0, 0, 500))
    actors.spawn_actor_from_class(ue.SkyAtmosphere, ue.Vector())
    world = ue.get_editor_subsystem(ue.UnrealEditorSubsystem).get_editor_world()
    world.get_world_settings().set_editor_property("default_game_mode", game.generated_class())
    if not level.save_current_level():
        raise RuntimeError("Unable to save practice level")
    created.append(map_path)

# Validate required connections without changing existing designer values.
for path, cls in required:
    if not isinstance(assets.load_asset(path), cls):
        raise RuntimeError("Validation failed: " + path)
for name in names:
    import re
    prop = re.sub(r"(?<!^)(?=[A-Z])", "_", name).lower()
    if config.get_editor_property(prop) is None:
        raise RuntimeError("Input configuration is missing " + prop)
cdo = ue.get_default_object(character.generated_class())
if not cdo.get_editor_property("inputs") or len(cdo.get_editor_property("default_loadout")) != 3:
    raise RuntimeError("Practice character input/loadout is incomplete")
ue.log("CROSSHAIR_ASSETS_OK created=" + str(len(created)) + " validated=" + str(len(required)))
