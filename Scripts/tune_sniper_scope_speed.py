"""Make the sniper scope in/out a little faster for quick scopes.

Run: UnrealEditor-Cmd <project> -run=pythonscript -script=<this file> -unattended -NullRHI -nosound
AimSeconds is the time to go fully into (or out of) the scope; 0.20 s is 20% faster than the original 0.25 s.
"""
import unreal as ue

PATH = '/Game/Crosshair/Weapons/DA_Sniper'
TARGET = 0.20
sniper = ue.EditorAssetLibrary.load_asset(PATH)
if not sniper:
    raise RuntimeError('Missing ' + PATH)
before = sniper.get_editor_property('aim_seconds')
sniper.set_editor_property('aim_seconds', TARGET)
if not ue.EditorAssetLibrary.save_loaded_asset(sniper, False):
    raise RuntimeError('Could not save ' + PATH)
ue.log('CROSSHAIR_SCOPE_SPEED_OK aim_seconds %.3f -> %.3f' % (before, sniper.get_editor_property('aim_seconds')))
