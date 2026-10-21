"""Place COD-style match spawn points and navmesh bounds in L_Nuketown and L_Grid.

Run: UnrealEditor-Cmd <project> -run=pythonscript -script=<this file> -unattended -NullRHI -nosound
Positions were chosen from the in-game collision probe (-CrosshairSmoke -CrosshairMatchLayoutProbe),
approximating the Black Ops layouts: TDM teams start at opposite ends (Nuketown backyards,
Grid north/south ends) and FFA spawns ring the map edges, away from the central fights.
Exact Call of Duty spawn coordinates are not public; adjust the actors in the editor as needed.
The script refuses to duplicate existing spawns unless CROSSHAIR_RESET_MATCH_SPAWNS=1.
"""
import math
import os
import unreal as ue

CAPSULE_HALF = 97  # character capsule half-height plus 1 cm clearance

# name: (map, ground z, center for facing, navmesh bounds (min, max), spawns)
# spawn: (x, y, team: 'A' | 'B' | '', area)
MAPS = {
    'Nuketown': ('/Game/Crosshair/Maps/L_Nuketown', 60, (-600, 1300), ((-5400, -3600, -500), (4400, 6200, 1500)), [
        # Alpha: backyard behind the +X house.
        (3300, -200, 'A', 'Alpha backyard'), (3400, 400, 'A', 'Alpha backyard'), (3300, 1000, 'A', 'Alpha backyard'),
        (3300, 1600, 'A', 'Alpha backyard'), (3700, 100, 'A', 'Alpha backyard'), (3700, 1300, 'A', 'Alpha backyard'),
        # Bravo: backyard behind the -X house.
        (-4100, -200, 'B', 'Bravo backyard'), (-4100, 500, 'B', 'Bravo backyard'), (-4100, 1000, 'B', 'Bravo backyard'),
        (-4500, -400, 'B', 'Bravo backyard'), (-4500, 600, 'B', 'Bravo backyard'), (-4500, 1100, 'B', 'Bravo backyard'),
        # Edges inside the fences: house side passages, street corners and front yards.
        (1587, -950, '', 'Alpha house south side'), (1400, 2227, '', 'Alpha house north side'), (2700, -500, '', 'Alpha side yard'),
        (-2550, -913, '', 'Bravo house south side'), (-2600, 2000, '', 'Bravo house north side'),
        (-1300, -800, '', 'Street south corner'), (-1050, 2900, '', 'Street north corner'),
        (450, -300, '', 'Alpha front yard south'), (450, 2200, '', 'Alpha front yard north'),
    ]),
    'Grid': ('/Game/Crosshair/Maps/L_Grid', 0, (1000, -3000), ((-5600, -9000, -3500), (7400, 4000, 1500)), [
        # Alpha: north-east yard (the +X end of the play space).
        (4050, 0, 'A', 'North end'), (4050, 750, 'A', 'North end'), (4050, 1500, 'A', 'North end'),
        (4050, 2250, 'A', 'North end'), (3800, 500, 'A', 'North end'), (3550, 2500, 'A', 'North end'),
        # Bravo: south end beyond the trench (the -X end).
        (-5200, -3500, 'B', 'South end'), (-5200, -2500, 'B', 'South end'), (-5200, -1500, 'B', 'South end'),
        (-4700, -3000, 'B', 'South end'), (-4700, -500, 'B', 'South end'), (-4700, -1000, 'B', 'South end'),
        # Edges: lanes and yards around the buildings, away from the centre.
        (-2700, -4000, '', 'West lane'), (1800, -5000, '', 'West yard'), (550, -5000, '', 'South-west lane'),
        (3300, -3250, '', 'North-west yard'), (2550, -1000, '', 'Between tower and centre'), (2300, 2500, '', 'North-east yard'),
        (-2700, 2500, '', 'South-east yard'), (-1700, -500, '', 'Central courtyard'), (800, 0, '', 'East buildings'),
        (-200, -4500, '', 'South-west buildings'), (1550, 2750, '', 'East edge'),
    ]),
}

actors = ue.get_editor_subsystem(ue.EditorActorSubsystem)
reset = os.environ.get('CROSSHAIR_RESET_MATCH_SPAWNS') == '1'
for name, (path, ground, center, (lo, hi), spawns) in MAPS.items():
    world = ue.EditorLoadingAndSavingUtils.load_map(path)
    if not world:
        raise RuntimeError('Could not load ' + path)
    existing = [a for a in actors.get_all_level_actors() if isinstance(a, ue.CrosshairSpawnPoint)]
    if existing and not reset:
        raise RuntimeError('%s already has %d match spawns; set CROSSHAIR_RESET_MATCH_SPAWNS=1 to replace them' % (name, len(existing)))
    for actor in existing:
        actors.destroy_actor(actor)
    for actor in actors.get_all_level_actors():
        if isinstance(actor, ue.NavMeshBoundsVolume) and actor.get_actor_label().startswith('Crosshair match navigation'):
            actors.destroy_actor(actor)
    for x, y, team, area in spawns:
        yaw = math.degrees(math.atan2(center[1] - y, center[0] - x))
        point = actors.spawn_actor_from_class(ue.CrosshairSpawnPoint, ue.Vector(x, y, ground + CAPSULE_HALF), ue.Rotator(0, 0, yaw))
        point.set_editor_property('initial_team', ue.CrosshairSpawnTeam.ALPHA if team == 'A' else ue.CrosshairSpawnTeam.BRAVO if team == 'B' else ue.CrosshairSpawnTeam.NONE)
        point.set_editor_property('area', area)
        point.set_actor_label('Match spawn - ' + area)
        point.set_folder_path('Match/Spawns')
    middle = ue.Vector((lo[0] + hi[0]) / 2, (lo[1] + hi[1]) / 2, (lo[2] + hi[2]) / 2)
    volume = actors.spawn_actor_from_class(ue.NavMeshBoundsVolume, middle)
    origin, extent = volume.get_actor_bounds(False)
    if extent.x < 1:
        raise RuntimeError('NavMeshBoundsVolume spawned without a brush')
    volume.set_actor_scale3d(ue.Vector((hi[0] - lo[0]) / 2 / extent.x, (hi[1] - lo[1]) / 2 / extent.y, (hi[2] - lo[2]) / 2 / extent.z))
    volume.set_actor_label('Crosshair match navigation')
    volume.set_folder_path('Match')
    if not ue.EditorLoadingAndSavingUtils.save_map(world, path):
        raise RuntimeError('Could not save ' + path)
    ue.log('CROSSHAIR_MATCH_SPAWNS %s placed=%d nav_bounds=%s' % (name, len(spawns), str(volume.get_actor_bounds(False)[1])))
ue.log('CROSSHAIR_MATCH_SPAWNS_OK')
