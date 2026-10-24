# Realistic graphics

The graphics pass covers lighting, the soldier body, fitted gear, materials and the first-person arms. It is all done in code or by a regenerating script; no downloaded assets or external services are used.

## Lighting that follows every map

`UCrosshairEnvironmentSubsystem` (a world subsystem, `Trickshot/CrosshairEnvironment.h/.cpp`) runs when any game or PIE world begins play. It gives the Testing map, Nuketown, Grid and any imported user map the same physically based rig:

- **Sun.** The map's existing directional light is reused and keeps its direction; a sun is spawned if the map has none. It becomes the movable atmosphere sun at 5900 K, with at least 6.5 lux, the real sun's 0.53° angular size (soft shadow edges), contact shadows and cloud shadows. Any other directional lights stop driving the sky.
- **Sky.** A SkyAtmosphere is added, plus a real-time captured SkyLight, so ambient light always matches the sky and clouds.
- **Haze.** Exponential height fog with volumetric scattering, placed relative to the map's player start, so imported maps at any height get the same distance haze.
- **Clouds.** Volumetric clouds (engine `m_SimpleVolumetricCloud_Inst`) are added if the map has none.
- **Grade.** An unbound post-process volume at priority −10 sets:
  - histogram exposure with a −0.35 bias, so snow, sky and concrete don't blow out;
  - restrained bloom, ambient occlusion and higher Lumen quality, with hit-lit reflections;
  - slightly lowered saturation, more contrast, a 6300 K white balance, light vignette and grain, no chromatic fringe, and reduced motion blur for readable spins.

  A map's own post-process volumes have higher priority and still win.

Spawned actors are tagged `CrosshairEnvironment`. A map author who wants full control of a map's lighting can tag any actor in it `CrosshairCustomLighting`, and the subsystem leaves that map alone.

## Soldier physique

`CrosshairLocomotion::Physique()` lists bone-local thickness multipliers for the spine, neck, clavicles, upper arms, forearms, thighs and calves. For example, the upper arms are 1.24× thicker and the upper chest 1.14×. `ApplyPhysique` scales these in component space at the end of every pose evaluation:

- twist bones follow the bulk, so skinning stays smooth;
- hands, head, feet and the next limb segment keep their size and move with the wider frame.

The same physique is used by bots, LAN players, replays, practice targets and the first-person arms. Practice targets now use the C++ locomotion instance with the unarmed idle.

Hit detection uses the physics bodies on the posed bones, so hitboxes follow the bulk. The thicker neck slightly overlaps the base of the skull, so a shot exactly at the head bone pivot can register as a body hit; shots at the head itself are headshots.

## Fitted gear and materials

`Scripts/create_soldier_appearances.py` builds every piece of gear in mesh space from measured cross-sections of the body. The measurements come from the `-CrosshairSoldierProbe` run. Each piece attaches with an identity reference transform:

- an ellipsoid ballistic helmet with a rim, rails and an NVG mount;
- goggles strapped around the helmet, and headset cups at the ears;
- a cap and boonie with brims;
- plate and light carriers that wrap the torso, with webbing, mag pouches, admin and back pouches, and shoulder straps over the bulked shoulders;
- a field pack;
- a battle belt with angled pouches;
- curved knee armor on each calf.

The materials are the `M_Uniform_*_Real`, `M_Fabric_*_Real` and `M_Armor_*_Real` sets:

- procedural noise camouflage;
- fold and weave normals;
- matte fabric, semi-matte armor and glossier boots.

## First-person arms

`UCrosshairViewmodelAnimInstance` plays the arms' idle and reload, applies the physique, and places the left hand under the weapon's fore-end with two-bone IK. The IK is off during reloads, weapon switches, lethal throws and full scope. The grip comes from `UCrosshairWeaponDefinition::LeftHandGrip`, set in the weapon's model space. When it is zero, a default is used: under the fore-end, a third of the way up the model.

## Verification

Run the checks locally:

- `-game -CrosshairSmoke -CrosshairGraphicsCheck -RenderOffscreen -ResX=1280 -ResY=720 -windowed` → `CROSSHAIR_GRAPHICS_OK`. It visits the Testing map, Nuketown, Grid and the imported example user map. On each map it asserts exactly one atmosphere sun, a real-time sky light, volumetric fog, clouds and one grade volume. It saves `Saved/Screenshots/Gfx*.png` and logs the first-person arm joints and IK goal (`CROSSHAIR_ARMS`).
- `-game -CrosshairSmoke -CrosshairSoldierProbe` (editor builds) logs body cross-sections for regenerating gear.
- `-CrosshairFrontendSmoke -CrosshairFrontendVisual` captures all six soldier designs.
