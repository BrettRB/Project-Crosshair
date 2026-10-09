# COD map candidates after retiring Highrise

Research: 2026-10-08. No new map has been bought, downloaded or installed. Listings below are source leads, not locally tested assets. Current built-ins are Testing Map and Nuketown; weapon stow/draw/back carry remains intact.

| Candidate | Supplied assets / listing | Assessment |
| --- | --- | --- |
| [MW2 Rust by Blenderworks](https://www.cgtrader.com/3d-models/exterior/industrial-exterior/call-of-duty-rust-map-1fa6f5c2-824f-4313-a222-af6007fc61a1) | $49 at research time; Blender, FBX, OBJ, glTF and other formats; author explicitly includes all textures and a separate texture folder. FBX listed as 23.1 MB. | Strongest documented import candidate. My recommendation based on portable formats, textures and compact map scope. Needs asset inspection, materials, scale, collision, lighting and gameplay setup; no guarantee of UE5.8 compatibility or visual quality until previewed. |
| [BO2 Raid UE5 realism remake by dist.fx](https://payhip.com/b/6P0bH) | Advertised as Raid made in UE5; 4 GB RAR. Opened page showed pay-what-you-want $0, while search snippets showed $9.99. | Best native-engine lead, contingent on the archive including an editable Unreal project, not just a playable build. Project version, dependencies, usage terms and checkout price need confirmation. No purchase or creator contact sent. |
| [Shipment Arena Map by Polygrade](https://www.fab.com/listings/bbe19a57-c86a-40f5-9016-25bd084f2cc2) | FBX and converted glTF/GLB; 21 props, vehicles, containers and crates with 4K albedo/normal/roughness/metalness textures. Price was not exposed in the fetched page. | Portable assets for a Shipment-style arena. Stylized art, not a verified exact COD map recreation; likely less suited to the requested realistic look. Scene assembly and game collision would still need inspection. |

## Lower-priority free geometry

[Terminal MW2 on Free3D](https://free3d.com/3d-model/terminal-mw2-59921.html) is free under the listing's Personal Use License, but C4D-only with no textures, materials or UV mapping reported. It requires format conversion and substantial art work, so it is not recommended as a quick polished replacement. The older [TurboSquid Rust](https://www.turbosquid.com/3d-models/modern-2-rust-3ds/725440) likewise explicitly requires texturing.

## Next import workflow

Rust is the best-supported technical lead, not an already working game map. Inspect obtained files in a separate staging location; preview geometry/textures in a temporary Unreal level before replacing any current map. Import/migrate environment assets only, preserve author attribution/terms, and configure Crosshair GameMode, supported spawn, floor/cover collision, targets, reset/replay and movement routes. Standalone FBX/glTF packages should be integrated through Unreal Editor; the in-game OBJ importer supports one texture atlas and bounded file sizes, not arbitrary multi-material scenes.

Highrise's level/materials and generator/repair scripts have been removed at the user's request. Its cook entry and menu selection are removed; imported maps again start at index two. Earlier source research and course interaction history are preserved as historical records.
