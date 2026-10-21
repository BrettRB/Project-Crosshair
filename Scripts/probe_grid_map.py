"""Read-only local Grid bounds/build audit for import diagnostics."""
import unreal as ue
root='/Game/Crosshair/Maps/Grid/call_of_duty_black_ops_1_grid_inspired_map/StaticMeshes/'
editor=ue.get_editor_subsystem(ue.StaticMeshEditorSubsystem) or ue.get_default_object(ue.StaticMeshEditorSubsystem)
for name in ['Object_0','Object_100','Object_200','Object_300','Object_500','Object_589']:
 mesh=ue.EditorAssetLibrary.load_asset(root+name)
 if not mesh: continue
 box=mesh.get_bounding_box()
 settings=editor.get_lod_build_settings(mesh,0)
 ue.log('GRID_LOCAL_BOUNDS %s min=%s max=%s df_scale=%s'%(name,box.min,box.max,settings.distance_field_resolution_scale))
ue.log('CROSSHAIR_GRID_PROBE_OK')
