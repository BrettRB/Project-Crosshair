# Grid source and attribution

**Call of Duty Black Ops 1 Grid Inspired Map** by **PIPO (pipo_)**.

- Source: https://sketchfab.com/3d-models/call-of-duty-black-ops-1-grid-inspired-map-68c0b8b9ffe34a468876044b4c1da00a
- Creator: https://sketchfab.com/pipo_
- License: Creative Commons Attribution 4.0 International (CC BY 4.0), https://creativecommons.org/licenses/by/4.0/

Downloaded by the user as a GLB with the 2K texture option. Author/license/title/source verified against both embedded GLB asset.extras and the public Sketchfab model API on 2026-10-20. The source manifest records the original download's SHA-256 and attribution.

Crosshair adaptation: Unreal Interchange scene import preserving supplied geometry/materials/textures; static triangle collision; practice GameMode, spawn and daylight; map selection, startup warmup and packaging integration. No endorsement by the creator is implied.

The original 79,821,704-byte GLB remains in Downloads and is not duplicated into source control. Runtime assets are self-contained under Content/Crosshair/Maps/Grid and L_Grid.umap. To rebuild, provide the original GLB and run Scripts/import_grid_map.py with UnrealEditor-Cmd; CROSSHAIR_GRID_SOURCE can specify a different location. The script refuses to overwrite an existing playable map. See docs/grid-map.md for validation and integration notes.
