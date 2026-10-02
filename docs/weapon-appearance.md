# Weapon models and finishes

The sniper, AR and SMG now have three separate original models. The AR has a rounded machined upper receiver, vented fore-end, curved magazine and an open adjustable stock. The compact SMG has a cylindrical receiver, ribbed handguard, curved magazine and sliding stock rails. The bolt-action sniper has a rounded, tapered stock and fore-end, bolt handle, scope mounts, turrets, rings and dark lenses. All models include separate metal, painted and rubber surfaces; geometry uses centimeter dimensions, chamfered edges and smooth cylindrical/tapered sections.

Open the pause menu and choose **Camos**. Select the weapon, then click Original, Woodland or Desert, or press Enter / A on its row. Each weapon remembers its own choice across resets, switching, travel and restarts. The active choice is marked Equipped. These three finishes are available immediately; unlock progression remains future work.

Woodland and Desert use four subdued coating colors, layered irregular patches, sparse coating chips, fine grain, subtle tangent-normal detail and varying roughness. Camo applies only to the Paint slot. Barrels, magazines, sights and scopes retain dark metallic finishes; grips/butt pads stay matte rubber and scope lenses keep their separate optical finish. Original restores every authored material slot.

## Source and regeneration

All geometry and shaders were authored locally for this project. No assets were downloaded, purchased or copied from COD. The editable OBJ sources and placeholder MTL are in ContentSource/Weapons. Scripts/create_weapon_models.py generates these sources and imports only /Game/Crosshair/Weapons/Models/SM_AR, SM_SMG and SM_Sniper through Unreal's asset importer. Scripts/create_weapon_finishes.py rebuilds only the project-owned finish graphs and applies the three cosmetic catalogs.

Run the model script first, followed by the finish script, using UnrealEditor-Cmd with -run=pythonscript -script=<absolute script path> -unattended -NullRHI -nosound. Reimport resets slots, so always follow it with the finish script. The scripts configure the three definitions' model, display name and presentation offsets without changing damage, ammunition, timing or firing behavior. Re-running the generator overwrites its own generated models/source files; export artist changes elsewhere before regeneration.

## Rendering

UCrosshairWeaponDefinition::PresentationMesh optionally supplies a complete rigid weapon in X-forward / Z-up camera axes with its grip at the origin. ACrosshairWeapon renders it through a collision-free UStaticMeshComponent under the existing weapon root, counter-rotated into the template grip basis. Hands, recoil, ADS and reload continue to share that root transform. The skeletal mesh and primitive details remain a fallback for definitions without PresentationMesh. Sniper scope view hides the rigid model and hands as before.

FCrosshairWeaponSkin uses stable IDs and optional per-slot overrides; a null entry preserves the corresponding authored material. Cosmetic selection replicates with the weapon and saves per definition name. It does not affect ammunition or damage. Full replay playback and packaged rendering are not reverified in this change.

## Local verification

The Editor Development build and all seven Crosshair automation tests passed. The expanded rendered follow-up test checks distinct models, hidden prototype pieces, scoped-model suppression, actual material assignment, preserved metal, direct camo selection and persistence across switches, resets and map travel. Final hip-fire and ADS screenshots for both automatic weapons, sniper hip view, Camos and Controls were reviewed. Additional polish checks cover movement, ADS, aligned grip and saved-position resets.

These are original detailed game meshes rather than scanned production art. Further artist work can replace the rigid meshes through the same definition field without changing gameplay. Finger pose/left-hand contact and bespoke per-weapon reload animations can still be refined independently.
