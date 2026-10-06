ï»¿# Nuketown environment integration

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

In game, press Escape/Start. In the Practice tab, select the Map row, use Left/Right or D-pad to choose Testing Map or Nuketown, and press Enter/A to load. Travel starts a fresh attempt and map target layout, preserves settings and saved replays, and discards the active unsaved recording. Changing maps is unavailable during replay playback or successful-replay saving. Both playable levels are listed in local packaging configuration.

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

### Spawn correction (2026-10-01)

The playable PlayerStart is now at (-600,1300,180), yaw 0 in the central street between the school bus and moving truck. Scripts/repair_nuketown_spawn.py relocates only this start through Unreal APIs; setup_nuketown.py uses the same position for a fresh import. The final shader-ready rendered round-trip test passed CROSSHAIR_MAP_SMOKE_OK, including central-position, floor, movement, firing, target placement/reset, settings, and map/layout isolation checks. See docs/gameplay-controls.md for the updated category menu and saved-position controls.


### Raised grass collision correction (2026-10-01)

The four instanced raised-grass meshes under /Game/ArchViz/Grass/Grass_grass_ now use NoCollision in the playable map. Ground surface meshes/materials retain collision. Scripts/repair_nuketown_grass.py makes this scoped change and checks it after a save/reload; fresh imports use the same exclusion in setup_nuketown.py. Original source map, grass mesh assets, and other vegetation are preserved.

### Downloads cleanup audit (2026-10-01)

All 4,316 Unreal asset files listed in Downloads/CrosshairMapImport/Nuketown.rar and all 35 asset files in Surfaces_Mats.rar are already present under this project's Content folder at their original package paths. The extracted source Content contains the same 4,351 files; no project counterpart is missing. A SHA-256 comparison found 1,236 identical files and 3,115 differing project versions; imported assets have been saved/updated for the existing integration, so original copies must not overwrite the current versions.

The original Downloads/Nuketown.txt instructions are preserved byte-for-byte at docs/source-assets/Nuketown.txt. CrosshairMapImport is temporary source/staging data, not a runtime mount. Its archives, extracted original project, diagnostic helpers and caches are unnecessary for playing or building this integrated map. They can be removed from Downloads after this audit. Keep the Project_Crosshair Content folder, including the original BlackOps_NukeTown_V2 map and imported dependency folders, as well as the playable Crosshair/Maps/L_Nuketown map.

Downloads/nuketown_v10.zip contains a separate 2D map pack (gfx sprites/tiles and .lua/.map files). It is not an input to this Unreal project and can also be removed without affecting it. Unrelated Downloads files, including Project Crosshair.zip and Windows, were not assessed for deletion.

Removing the original archives means restoring pristine author-supplied copies later would require another download or a separate backup. Missing third-party texture/sky dependencies already documented above are not supplied by these archives and are unrelated to deleting staging files.


### Breakable house windows (2026-10-02)

The 48 separate `func_breakable_*` house panes with only glass/nodraw material slots in playable L_Nuketown are now ACrosshairWindow actors. Original pane meshes, materials and transforms are preserved. Frames, walls, vehicle glass and the imported source map are unchanged. Scripts/setup_breakable_windows.py measures the pane plane from editor-exported vertices, including house rotations baked into mesh geometry, then saves the map through Unreal APIs. Re-running the conversion finds the existing 48 windows and creates no duplicates. A fresh setup_nuketown.py import should be followed by setup_breakable_windows.py.

Shots shatter glass and continue through the opening. Jump can mantle through openings with sufficient capsule clearance; decorative small panes remain shootable but cannot fit the player. Collision checks preserve solid frames and walls. See [traversal and target setup](traversal-and-targets.md) for future-map configuration and verification.

## Flower-bed collision repair (2026-10-02)

Scripts/repair_feedback_assets.py removes collision only from the 82 playable-map components using `/Game/BlackOPSIK/2/models_bo1_nuketown_outside_mc_t5_foliage_flowers01` through `flowers05`. It preserves mesh assets, source map, flower appearance, bed geometry and ground. Save/reload verification and runtime inspection confirm every matching component uses NoCollision. This complements the earlier four raised-grass repairs; other vegetation and planters remain separate assets.

## Indoor lighting (2026-10-05)

The eleven existing point/rect lights in playable L_Nuketown now use movable dynamic lighting, lumen units, soft sources, 5000 K color and an 850 cm local attenuation radius. Point lights use inverse-square falloff and 500 lumens; rect lights use 650 lumens. Indirect contribution is 1.15. Scripts/repair_indoor_lighting.py updates only these lights and asserts their state after save/reload. Exterior daylight, geometry, collision, source map and materials remain intact. Rendered checks compare indoor views and tune highlights without globally increasing skylight/exposure.
