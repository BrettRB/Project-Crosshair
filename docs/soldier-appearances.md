# Soldier target appearances

Six project-owned designs replace the bare mannequin presentation for placed and map-provided practice targets. Enable sandbox target editing and place normally; no new controls are required.

| Design | Uniform and equipment |
| --- | --- |
| Woodland Infantry | Green/brown camouflage, ballistic helmet, goggles, comms headset, full carrier and field pack |
| Desert Patrol | Tan camouflage, field cap and lighter carrier |
| Urban Assault | Gray camouflage, ballistic helmet, goggles, headset and full carrier |
| Arctic Ranger | Snow camouflage, helmet, full carrier and field pack |
| Night Operations | Dark camouflage, helmet, goggles, headset and full carrier |
| Olive Recon | Olive camouflage, boonie hat, lighter carrier and field pack |

All designs include utility pouches and knee armor. Fabric shading, matte armor and dark face coverings replace the metallic mannequin look. The body is a project-owned copy of the existing mannequin rig, with original curved headgear and modeled tactical equipment attached to its animated bones. This is an improvement using the existing proportions and animations, not a newly sculpted or scanned human body. Equipment is cosmetic: it adds no armor bonus, bullet collision or navigation obstacle. Existing posed head/torso/limb physics bodies and damage rules remain authoritative.

## Assignment and replay

UCrosshairSoldierSelection owns a shuffled deck in each world. Every design is drawn once per complete cycle, and adjacent cycles avoid repeating the last design. Existing level targets participate in that deck too. Resetting an attempt or target keeps its appearance. CaptureTargets/RestoreTargets store each transform and design together for return from replay. The component replicates DesignIndex for recorded playback; playback does not choose random designs. Old recordings without that field receive a fixed Woodland visual fallback until recorded state is available.

## Shared bot integration

UCrosshairSoldierAppearance is a Blueprint-spawnable actor component with no dependency on ACrosshairDummy. Give a future bot pawn this component, then call InitializeAppearance(WorldBodyMesh) after assigning its world skeletal mesh. Authority selects an outfit, or pass a specific design index to initialize it explicitly. SetDesign changes it on authority and GetDesignName exposes its name. The actor must support replication for replicated appearance/recorded playback. Runtime gear is recreated locally from the replicated design and attaches to the shared skeleton.

The shared DA_SoldierAppearances catalog contains the body, six uniforms, gear fabrics/armor and equipment definitions. Keep existing design order stable for recorded indices; append future designs. ReferenceTransform is in mesh reference-pose space and is converted to the named bone's local space at runtime, keeping equipment aligned during animation. Custom skeletons need appropriate bone names and reference placements.

This prepares the same skins for the bot FFA mode. Combat AI and FFA match launch remain a separate implementation stage; no playable bot match is added by this appearance change.

## Source and authoring

Scripts/create_soldier_appearances.py generates only /Game/Crosshair/Soldiers, ContentSource/Character/Soldiers OBJ sources and BP_Target's appearance catalog configuration through Unreal Editor APIs. It duplicates the template mesh without editing template skeletons/physics assets. Existing material graphs are preserved on rerun; gear meshes and catalog definitions are regenerated, so preserve artist edits elsewhere before rerunning. OBJ sources explicitly compensate for Unreal's Y-axis conversion.

Run locally with UnrealEditor-Cmd <project> -run=pythonscript -script=<absolute path to generator> -unattended -NullRHI -nosound. No downloaded character assets, paid APIs or external services are used.

## Verification

The extended local frontend smoke verifies six-design shuffle cycles, automatically assigned target appearances, body/material/equipment configuration, noncolliding bone attachments, reset stability, snapshot restoration and recorded playback/return. -CrosshairFrontendVisual captures all six designs for inspection. Existing expansion coverage checks head/torso/leg traces, headshots, wallbangs and replay; core and lethal smoke checks cover normal practice and throwable damage/replay. Editor Development checks do not establish packaged-build compatibility, physical controller behavior or network multiplayer correctness.
