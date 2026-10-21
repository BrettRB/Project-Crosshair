# Grid map integration

Grid is a built-in practice map adapted from **Call of Duty Black Ops 1 Grid Inspired Map** by **PIPO (pipo_)**.

- [Original model / source](https://sketchfab.com/3d-models/call-of-duty-black-ops-1-grid-inspired-map-68c0b8b9ffe34a468876044b4c1da00a)
- [Creator profile](https://sketchfab.com/pipo_)
- [Creative Commons Attribution 4.0 International (CC BY 4.0)](https://creativecommons.org/licenses/by/4.0/)
- [Project third-party credits](third-party-credits.md)

Author/title/source/license are present in the original GLB metadata and were independently confirmed against Sketchfab's public model API on 2026-10-20. Source checksum and exact attribution are preserved in [ContentSource/Grid/source-manifest.json](../ContentSource/Grid/source-manifest.json).

## Import and gameplay adaptation

The user supplied `Downloads/call_of_duty_black_ops_1_grid_inspired_map.glb` (79,821,704 bytes), downloaded with the 2K texture option. Unreal Interchange imported its scene hierarchy, 590 static meshes, 38 materials and 30 texture assets into `/Game/Crosshair/Maps/Grid/call_of_duty_black_ops_1_grid_inspired_map`. Supplied textures retain their original dimensions; 2K is the selected maximum, not an upscaling of smaller source images.

The playable level is `/Game/Crosshair/Maps/L_Grid`. Imported meshes use static, blocking triangle collision with double-sided collision geometry, preserving open passageways rather than enclosing buildings in convex hulls. The scene retains its glTF units/orientation through Unreal's standard conversion. Grid distance-field resolution follows each mesh component's actual scene scale (capped at 0.25) to reduce first-build and Lumen representation cost for the many imported pieces; visible geometry and triangle collision are unchanged. `Scripts/tune_grid_map.py` applies this setting to an existing Grid import and audits texture/material counts. Added dynamic daylight, skylight/atmosphere, the existing practice GameMode and a PlayerStart. No unrelated map geometry/materials are replaced. The source GLB remains outside the repository; game runtime uses the saved Unreal assets.

`Scripts/import_grid_map.py` reproduces the import using Unreal APIs. Set `CROSSHAIR_GRID_SOURCE` to an absolute GLB path to override Downloads. The script refuses to overwrite an existing playable L_Grid map; partial imports before a successful map save can be retried in its dedicated Grid folder. Further designer edits belong in Unreal Editor.

## Playing and attribution

Open Home > Maps / Import, or Escape / Start > Maps. Cycle the Map row to **Grid**, then press Enter / A. Testing Map and Nuketown retain indexes 0 and 1; Grid uses 2; local imports start at 3. Selection remembers Grid on reopening the menu. Map travel uses existing class/settings/replay rules and clears local-import identity when entering a built-in map.

The Maps tab displays the full model title, author, CC BY 4.0 license and modification notice, with clickable Sketchfab and license links. Keep the attribution and links when distributing the map. No endorsement by the creator is implied.

L_Grid is explicitly included in MapsToCook. The existing startup system automatically discovers its assets and dependencies, preparing them along with other shipped content. First uncooked launch can spend several minutes creating shader, texture, Nanite and mesh distance-field derived data; later launches use the local cache. Normal world creation and streaming costs still apply. See [startup preparation](startup-preparation.md).

## Verification

- Local `Build.bat Project_CrosshairEditor Win64 Development -Project=<project> -WaitMutex -NoHotReloadFromIDE`: succeeded, including final capture timing change.
- Local `Build.bat Project_Crosshair Win64 Development -Project=<project> -WaitMutex -NoHotReloadFromIDE`: succeeded for production map/menu/credit integration.
- `UnrealEditor-Cmd <project> -run=pythonscript -script=<project>/Scripts/import_grid_map.py -unattended -NullRHI -nosound -abslog=<project>/Saved/Logs/GridImport.log`: CROSSHAIR_GRID_IMPORT_OK, 590 components/590 meshes, exit 0.
- Equivalent local commandlet with `Scripts/tune_grid_map.py`: CROSSHAIR_GRID_TUNING_OK, 590 meshes/30 textures/38 materials, exit 0. Texture audit confirms supplied sizes up to 2048 pixels. The source uses oversized local-space meshes under scaled scene nodes; scale-aware distance-field tuning avoids the original all-max-resolution workload.
- `UnrealEditor-Cmd <project> /Game/Crosshair/Maps/L_Practice -game -CrosshairSmoke -CrosshairFrontendSmoke -CrosshairFrontendVisual -RenderOffscreen -unattended -nosound -ResX=1280 -ResY=720 -windowed -ExecCmds="t.MaxFPS 60" -abslog=<project>/Saved/Logs/GridFrontendRendered.log`: CROSSHAIR_FRONTEND_OK, exit 0, no frontend/startup failures. Covers Grid selection, supported spawn, walking, firing, saved-position reset, sandbox target placement, class/cache preservation and return to practice, plus existing imported-map replay and soldier checks. Final warmed startup retained 4,983 assets and four map packages; preparation took 24.45 seconds on this machine.
- `UnrealEditor-Cmd <project> -unattended -NullRHI -nosound -ExecCmds="Automation RunTests Crosshair" -TestExit="Automation Test Queue Empty" -abslog=<project>/Saved/Logs/GridAutomation.log`: nine of nine tests succeeded, exit 0.
- `GridCredits.png`, settled `GridGameplay.png`, and `GridOverview.png` under Saved/Screenshots visually inspected. Python scripts parse, and task-file `git diff --check` passes.
- Packaged cook/runtime, physical controller and performance across other GPUs are unverified. Original GLB includes degenerate tangent warnings on some parts; the integration preserves its appearance rather than remodeling the source. Scene-scale distance fields affect indirect-lighting precision; triangle collision and visible geometry are unchanged.
