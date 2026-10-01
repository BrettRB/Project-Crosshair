# Nuketown environment integration

Source listing: https://www.artstation.com/marketplace/p/l0MN8/nuketown-from-black-ops-unreal-engine-5-remake

Environment author: Alexey Shishnin. The supplied instructions credit ValenQpr's CS:GO remake as the source. This is the Black Ops 1 version from that listing, rather than the Black Ops 2 Nuketown 2025 variant.

The user downloaded Nuketown.txt. Its links supplied Nuketown.rar (656 MB) and Surfaces_Mats.rar (about 5 MB). The former contains a UE 5.3 project and `/Game/BlackOps_NukeTown_V2`. The latter contains surface material instances, not the full set of required texture assets. Original staging files stay outside this repository in `C:/Users/BrettRB/Downloads/CrosshairMapImport`.

## Asset layout

The new ArchViz, BlackOPSIK, Colorama, EasyFog, MDL, MSPresets, and Megascans Content folders preserve the author's package paths so internal references resolve. Incoming Config, project files, caches, developer files, and collections are not imported. Existing Crosshair/template assets are not replaced.

`Scripts/setup_nuketown.py` creates `/Game/Crosshair/Maps/L_Nuketown` from the source level through Unreal APIs. It converts inch-based scene transforms to centimeters, rebases the scene above the game's fall-reset limit, adds accurate complex collision to imported environment meshes, configures the practice GameMode and player start, removes promotional cameras/sequences, and adds native daylight. It refuses to overwrite an existing playable copy. Designer adjustments should be made to L_Nuketown in Unreal.

## Missing dependencies and appearance

Ultra Dynamic Sky/Weather and some Megascans plants/textures are absent from the provided files. No purchases were made. Built-in directional light, skylight, and atmosphere replace the sky system. Explicit local fallback material instances replace imported Megascans surface slots whose textures were not supplied. Original bundled textures elsewhere remain in use. Missing fern/boxwood variants are absent; this playable version does not reproduce the listing's complete lighting, foliage, and surface detail.

The source level and unused imported assets can still report missing dependency warnings. The playable level is saved separately with resolved available actors and explicit fallback surface overrides. Supply the author's listed Megascans dependencies and Ultra Dynamic Sky to pursue an exact visual reconstruction; do not blindly overwrite this project's settings.

## Map selection

In game, press Escape/Start. Select the Map row, use Left/Right or D-pad to choose Testing Map or Nuketown, and press Enter/A to load. Travel starts a fresh attempt and map target layout, preserves settings and saved replays, and discards the active unsaved recording. Changing maps is unavailable during replay playback or successful-replay saving. Both playable levels are listed in local packaging configuration.

## Local validation

Build Project_CrosshairEditor Win64 Development with the project's existing Build.bat command. Launch L_Practice with `-game -CrosshairSmoke -CrosshairMapSmoke -NullRHI -unattended -nosound -ExecCmds="t.MaxFPS 60"` for the opt-in round-trip map test. Add `-CrosshairMapVisual -RenderOffscreen` and omit NullRHI for a Nuketown screenshot. Read `CROSSHAIR_MAP_SMOKE_OK` in the log; a wrapper exit code alone does not establish success.

### Verified results (2026-09-30)

- Editor Win64 Development build succeeded.
- Headless map round-trip test passed. After visual inspection, the final rendered round-trip also passed with the final spawn at (2000, -2000, 190), yaw 180.
- Tests verified missing-map rejection, keyboard selection without premature travel, practice pawn/input/three-weapon initialization, supported floor collision, walking, weapon firing, target placement, resetting, controller return selection, settings persistence, and isolated map target layouts.
- Five existing Crosshair automation tests passed.
- Inspected the final 1280x720 screenshot at Saved/Screenshots/CrosshairNuketown.png. Map-specific fallback materials support Nanite; the existing target material is unchanged.
- Two author-supplied furniture/lattice materials still fail SM6 compilation and use Unreal's default material. Full Megascans foliage/surface detail and Ultra Dynamic Sky are still absent. No packaged-build/cook verification was performed.
- The separately reported replay playback stall was not addressed or re-tested by this map-focused test.
