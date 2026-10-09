# Highrise replacement source research

Status: Highrise cancelled by user on 2026-10-08. This document preserves the earlier investigation; no further acquisition is planned.

Research date: 2026-10-08. Request: replace the current simple visual blockout with an existing Highrise version that is easy to port to Unreal.

No complete, textured, downloadable editable Unreal project was verified in this search. Do not describe the following leads as ready to migrate. No map assets were downloaded or installed.

## Best engine-compatible lead: Chaostry's Pavlov Highrise

[Workshop listing](https://steamcommunity.com/sharedfiles/filedetails/?id=1522107260). [Collection preserving the description](https://steamcommunity.com/sharedfiles/filedetails/?id=1858115054).

The preserved description credits a Highrise port, McLovin's Hammer remake and engine version 4.21. The individual listing could not be fetched during research; the collection remains readable. An editable Unreal project or exported geometry/materials from the creator would be the best next source to investigate. Source availability, completeness, dependencies, permission to reuse and UE5.8 compatibility are unverified. No contact was sent.

A Workshop build is not evidence that editable source is included. [Epic's cooked-content documentation](https://dev.epicgames.com/documentation/en-us/unreal-engine/working-with-cooked-content-in-the-unreal-engine) describes read-only assets, configuration requirements and unsupported/untested types. A cooked game mod cannot be assumed to migrate as a normal editor map.

## Downloadable modeling lead: Modern Warfare 2 --- Highrise

[3D Warehouse model](https://embed-3dwarehouse-classic.sketchup.com/model/299997a62b9b68e6a005442d4981d053/Modern-Warfare-2-Highrise).

Listing shows 84,917 polygons, 17 materials and a 2 MB model. The author says the helicopter is missing and requests texturing. This is a geometry lead, not a finished visual replacement. Download format/access and full asset contents have not been inspected. Conversion/import feasibility needs verification after obtaining the source. Do not replace the current map with another untextured model without resolving its appearance.

## Rejected as easy replacement sources

- [SudoName Ravenfield Highrise](https://steamcommunity.com/sharedfiles/filedetails/?id=2216835006): creator explicitly says project files are no longer available; game-specific Workshop release.
- [Codefling Highrise](https://codefling.com/prefabs/highrise): Rust prefab listing, $19.99 at research time, not an Unreal project or standalone model package. No purchase made.
- [Zetta CS:GO Highrise](https://steamcommunity.com/sharedfiles/filedetails/?id=646308893): Source-engine Workshop map, not ready Unreal content; listing reports removed/incompatible. Source/texture exports would require a separate conversion workflow.
- Unrelated UE ShooterGame/UT maps also named Highrise are not MW2 Highrise.

## Acquisition and integration gate

For a straightforward import, obtain an uncooked Unreal level with all referenced meshes, textures and materials, or FBX/OBJ/glTF geometry plus textures and scene transforms. Verify actual files in a separate staging folder, preview in a temporary level, then migrate/import only environment assets. Strip other-game logic and configure Crosshair GameMode, supported spawn, collision, breakable windows, mantle routes and replay behavior. Preserve original source attribution and applicable terms. Highrise was subsequently removed at the user's request. No changes to gameplay, project settings or map assets were made by this research task.
