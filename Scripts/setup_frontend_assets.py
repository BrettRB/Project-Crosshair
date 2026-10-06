import unreal as ue
assets=ue.EditorAssetLibrary
path='/Game/Crosshair/Maps/L_UserMap'
if not assets.does_asset_exist(path):
 level=ue.get_editor_subsystem(ue.LevelEditorSubsystem)
 assert level.new_level(path)
 actors=ue.get_editor_subsystem(ue.EditorActorSubsystem)
 actors.spawn_actor_from_class(ue.PlayerStart,ue.Vector(0,0,200))
 sun=actors.spawn_actor_from_class(ue.DirectionalLight,ue.Vector(0,0,1000),ue.Rotator(-45,-35,0))
 sun.light_component.set_editor_property('mobility',ue.ComponentMobility.MOVABLE)
 sun.light_component.set_editor_property('intensity',5)
 sky=actors.spawn_actor_from_class(ue.SkyLight,ue.Vector(0,0,500))
 sky.light_component.set_editor_property('mobility',ue.ComponentMobility.MOVABLE)
 sky.light_component.set_editor_property('real_time_capture',True)
 actors.spawn_actor_from_class(ue.SkyAtmosphere,ue.Vector())
 world=ue.get_editor_subsystem(ue.UnrealEditorSubsystem).get_editor_world()
 world.get_world_settings().set_editor_property('default_game_mode',assets.load_asset('/Game/Crosshair/Player/BP_PracticeGameMode').generated_class())
 assert level.save_current_level()
path='/Game/Crosshair/Maps/M_ImportedMap'
if not assets.does_asset_exist(path):
 m=ue.AssetToolsHelpers.get_asset_tools().create_asset('M_ImportedMap','/Game/Crosshair/Maps',ue.Material,ue.MaterialFactoryNew())
 texture=ue.MaterialEditingLibrary.create_material_expression(m,ue.MaterialExpressionTextureSampleParameter2D,-250,0)
 texture.set_editor_property('parameter_name','Diffuse')
 texture.set_editor_property('texture',assets.load_asset('/Engine/EngineResources/DefaultTexture'))
 ue.MaterialEditingLibrary.connect_material_property(texture,'RGB',ue.MaterialProperty.MP_BASE_COLOR)
 m.set_editor_property('two_sided',True)
 ue.MaterialEditingLibrary.recompile_material(m)
 assert assets.save_loaded_asset(m,False)
ue.log('CROSSHAIR_FRONTEND_ASSETS_OK')
