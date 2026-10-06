# Home, classes, local maps and controller preset

The game opens a home menu on its first normal launch into practice. Play starts the active class; Weapon classes, Maps / Import, Settings and Quit are available with mouse, keyboard or controller. Escape / B returns from home configuration pages to Home. The pause menu has Classes and Maps tabs, plus a Home action in Practice. Existing opt-in smoke commands bypass automatic Home for controlled tests.

## Weapon classes and slots

Five local custom classes each store a primary, a distinct secondary and a frag or tomahawk. Both slots can use the current Sniper, AR or SMG catalog. Classes > Class selects the class to edit; Primary, Secondary and Lethal cycle choices. Equip this class activates it, cancels ongoing actions and restocks. Edits and active class persist in the existing local profile. Press Play to apply the active class before beginning an attempt.

During gameplay, number-row **1** selects primary and **2** selects secondary. Mouse wheel, Q and Y / Triangle toggle between those two slots. The firearm catalog remains loaded for class and camo previews; gameplay switching follows the chosen two-slot class. Camos remain saved per weapon. Applying a class does not change target layout or saved position.

## Controller and sandbox

RT fires, LT aims, RB prepares/releases the lethal, LB cycles lethal equipment, Y switches slots, A jumps/mantles, B toggles crouch, X reloads, and left-stick click latches sprint while moving. Stopping or crouching ends controller sprint; aiming/firing/reset/menu interrupts it. Keyboard Shift remains a held sprint button. Default yaw is 720 degrees/second and pitch 540; maximum adjustable values are 2160 and 1440. Full horizontal stick makes two turns per second without aiming. ADS keeps its separate sensitivity multiplier. Both inversion switches remain independent and Off by default; mouse/stick composition and hardware calibration are preserved. Existing profiles using the old 360/240 defaults are migrated once; custom speeds are retained.

Settings > Controls > Sandbox target editing (also in Targets) enables placement, rotation, removal and clearing. It ships Off. Enable it, resume/play, press P / D-pad Up to preview a dummy, RT / left mouse to place, X / R to rotate, and LT / right mouse to cancel. Ordinary movement, guns and lethals work while sandbox editing is enabled. Switching it Off cancels the preview and leaves placed targets available for shots. Wheel scrolling also navigates long menus.

## Local map import

Home > Maps / Import or pause > Maps > Import map from local file opens a path-entry window. Paste the full path to an OBJ file or a map.json package, then click Import. The Map row lists the new map alongside Testing Map and Nuketown; adjust it, then Enter / A loads the selection. Imports copy geometry and the optional texture into the game's persistent-download UserMaps directory; they do not depend on Downloads after import. Tests use a separate SmokeMaps directory. No code, scripts or arbitrary Unreal assets are executed from imported packages.

The first supported format is OBJ geometry with optional UVs and one PNG/JPG diffuse atlas. Export **X forward, Y right, Z up**; units are centimeters unless scale is provided. Use triangles or convex polygons (at most eight corners); triangulate concave polygons in your modeling tool. Positive and negative indices are accepted. Normals are computed per triangle; triangle order is converted to Unreal's collision winding. Mesh collision supports floors, cover and ledges, while target dummies remain independently placeable. Dynamic daylight is supplied by the dedicated L_UserMap level.

A bare OBJ uses scale 1 and places the player above the geometry bounding box. For a reliable map-specific spawn, use a package:

```json
{
  "name": "My trickshot map",
  "mesh": "arena.obj",
  "scale": 1,
  "spawn": [0, 0, 100],
  "yaw": 0,
  "texture": "atlas.png"
}
```

Spawn is the player capsule center in scaled world centimeters; allow roughly 100 cm above a supported floor and clear space around the capsule. Mesh and texture paths must be relative, inside the package folder. Texture is optional. Map metadata is bounded to 64 KB, OBJ to 32 MB / 200,000 triangles / 200,000 source vertices, texture to 16 MB. Invalid numbers, missing indices, escaping paths and unsupported file formats are rejected. Successful imports use unique folders and preserve earlier maps.

The original example under ContentSource/UserMapsExample provides a floor, backstop, configured spawn and tiny checker atlas. Imported maps recreate their geometry from the installed copy during demo replay. Saved replay entries remember the imported map ID and reject playback if that map is missing; returning restores the prior practice session.

This first importer does not ingest FBX, .umap/.uasset, archive files, scripts, multiple MTL materials, custom lighting actors, breakable-window metadata or arbitrary third-party Unreal dependencies. Export geometry and bake an atlas for this format. Those formats need separate import workflows. Physical controllers and packaged builds require further verification; current checks use local Editor Development game sessions and simulated device keys.

## Implementation and verification

CrosshairMapLibrary owns validation, local storage, map selection and the import window. ACrosshairImportedMap rebuilds procedural rendering/collision and records a stable map ID. Weapon inventory owns class slots; data/save structures own class and controller preferences. PlayerController coordinates input and menu navigation. ProceduralMeshComponent and Json are bundled Unreal modules; no external assets or services are required.

Local opt-in -CrosshairSmoke -CrosshairFrontendSmoke covers Home, class editing/equipping/saving, number keys, wheel, Y, sandbox gating/enabling, faster turning, malformed/path-escaping imports, local-copy registration, map travel, supported spawn, preserved class, recorded hit and imported geometry during playback/return, indoor light state and return to built-in practice. Add -CrosshairFrontendVisual -RenderOffscreen -UnattendedInput for screenshots. Crosshair.Map.OBJValidation exercises polygon conversion, negative indices, unit scaling, invalid input and floor winding. Generated logs/screenshots remain under Saved and are not source assets.
