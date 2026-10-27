"""Set each weapon's match damage profile (players and bots in Free-for-all, Team Deathmatch and Kill Confirmed).

Run: UnrealEditor-Cmd <project> -run=pythonscript -script=<this file> -unattended -NullRHI -nosound
Black Ops (core modes, 100 health) style: full damage out to falloff_start, easing to min_damage at falloff_end.
  Sniper: 100 anywhere above the waist -> one shot to the head or torso; arms and legs take 70 (two shots).
  AR:     40 to 25 m, 30 from 50 m -> 3 shots close, 4 far; 56 to the head (2 headshots).
  SMG:    35 to 10 m, 20 from 25 m -> 3 shots close, 5 at range; 49 to the head (3 headshots).
Practice targets keep body_damage / head_damage (trickshot practice is unchanged).
"""
import unreal as ue

PROFILES = {
    # name: (max, min, falloff start cm, falloff end cm, head multiplier, limb multiplier)
    'DA_Sniper': (100.0, 100.0, 100000.0, 100000.0, 1.0, 0.7),
    'DA_AR': (40.0, 30.0, 2500.0, 5000.0, 1.4, 1.0),
    'DA_SMG': (35.0, 20.0, 1000.0, 2500.0, 1.4, 1.0),
}
KEYS = ('match_max_damage', 'match_min_damage', 'match_falloff_start', 'match_falloff_end',
        'match_head_multiplier', 'match_limb_multiplier')

for name, values in PROFILES.items():
    path = '/Game/Crosshair/Weapons/' + name
    weapon = ue.EditorAssetLibrary.load_asset(path)
    if not weapon:
        raise RuntimeError('Missing ' + path)
    for key, value in zip(KEYS, values):
        weapon.set_editor_property(key, value)
    if not ue.EditorAssetLibrary.save_loaded_asset(weapon, False):
        raise RuntimeError('Could not save ' + path)
    saved = [round(weapon.get_editor_property(key), 2) for key in KEYS]
    ue.log('CROSSHAIR_MATCH_DAMAGE %s %s' % (name, saved))
ue.log('CROSSHAIR_MATCH_DAMAGE_OK')
