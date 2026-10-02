"""Relocate only the playable Nuketown PlayerStart; preserve level designer edits.
Run with UnrealEditor-Cmd -run=pythonscript -script=<this file>.
The central street position was verified with a standing player capsule at runtime.
"""
import unreal as ue
MAP = '/Game/Crosshair/Maps/L_Nuketown'
world = ue.EditorLoadingAndSavingUtils.load_map(MAP)
if not world:
    raise RuntimeError('Could not load playable Nuketown')
actors = ue.get_editor_subsystem(ue.EditorActorSubsystem)
starts = [a for a in actors.get_all_level_actors() if isinstance(a, ue.PlayerStart)]
if len(starts) != 1:
    raise RuntimeError('Expected one PlayerStart; refusing ambiguous level edits')
start = starts[0]
start.set_actor_location_and_rotation(ue.Vector(-600, 1300, 180), ue.Rotator(0, 0, 0), False, True)
start.set_actor_label('Crosshair Nuketown start')
if not ue.EditorLoadingAndSavingUtils.save_map(world, MAP):
    raise RuntimeError('Could not save Nuketown start')
ue.log('NUKETOWN_SPAWN_REPAIR_OK location=' + str(start.get_actor_location()))
