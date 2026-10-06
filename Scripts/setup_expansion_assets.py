import runpy
from pathlib import Path
import unreal as ue
root = Path(ue.Paths.project_dir())
runpy.run_path(str(root/'Scripts/create_weapon_finishes.py'))
runpy.run_path(str(root/'Scripts/setup_breakable_windows.py'))
for path in ['/Game/Crosshair/Player/BP_PracticeCharacter','/Game/Crosshair/Targets/BP_Target']:
    bp=ue.load_asset(path)
    assert bp, path
    if bp:
        ue.BlueprintEditorLibrary.compile_blueprint(bp)
        ue.EditorAssetLibrary.save_loaded_asset(bp,False)
ue.log('CROSSHAIR_EXPANSION_ASSETS_OK')
