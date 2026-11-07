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

`CrosshairLocomotion::Physique()` lists bone-local thickness multipliers for the spine, neck, clavicles, upper arms, forearms, thighs and calves. For example, the upper arms are 1.18× thicker and the upper chest 1.14×. `ApplyPhysique` scales these in component space at the end of every pose evaluation:

- twist bones follow the bulk, so skinning stays smooth;
- hands, head, feet and the next limb segment keep their size and move with the wider frame.

The same physique is used by bots, LAN players, replays, practice targets and the first-person arms. Practice targets now use the C++ locomotion instance with the unarmed idle.

Hit detection uses the physics bodies on the posed bones, so hitboxes follow the bulk. The thicker neck slightly overlaps the base of the skull, so a shot exactly at the head bone pivot can register as a body hit; shots at the head itself are headshots.

## Fitted gear and materials

`Scripts/create_soldier_appearances.py` builds every piece of gear in mesh space from measured cross-sections of the body. The measurements come from the `-CrosshairSoldierProbe` run. Each piece attaches with an identity reference transform:

- an ellipsoid ballistic helmet with a rim, rails and an NVG mount;
- goggles strapped around the helmet, and headset cups at the ears;
- a cap and boonie with brims;
- plate and light carriers that wrap the torso and stop at the lower chest (front top z 134, or 132 for the light carrier, well below the collarbones and chin), with webbing, mag pouches, admin and back pouches, and shoulder straps laid on the shoulders rather than beside the neck;
- a field pack;
- a battle belt with angled pouches;
- curved knee armor on each calf.

The materials are the `M_Uniform_*_Real`, `M_Fabric_*_Real` and `M_Armor_*_Real` sets:

- procedural noise camouflage;
- fold and weave normals;
- matte fabric, semi-matte armor and glossier boots.

## First-person arms

`UCrosshairViewmodelAnimInstance` plays the arms' idle and reload, applies the physique, and places the left hand under the weapon's fore-end with two-bone IK. The IK is off during reloads, weapon switches, lethal throws and full scope. The grip comes from `UCrosshairWeaponDefinition::LeftHandGrip`, set in the weapon's model space. When it is zero, a default is used: under the fore-end, a third of the way up the model.

## Map skies

The lighting rig gives each built-in map its own sky (`UCrosshairEnvironmentSubsystem::SkyProfileFor`). Each map keeps its sun's compass direction, so shadows fall where the level expects; the profile sets the sun's height and colour, the atmosphere's blue and haze, the fog tint and the cloud weather:

| Map | Sky | Sun | Atmosphere and clouds |
| --- | --- | --- | --- |
| Testing Map | Clear midday | 55° high, 5800 K | Clean blue, light cumulus |
| Nuketown | Desert late afternoon | 20° high, warm 4700 K | Dusty Mie haze, warm fog, a few high clouds |
| Grid | Cool morning, broken cloud | 30° high, 6400 K | Deeper blue, cool fog, more cloud cover |
| Imported maps | Clear day | Map's own direction, 5900 K | Engine-default atmosphere and clouds |

Clouds come from a dynamic instance of the engine's volumetric cloud material: `Cloud_GlobalCoverage`, `Cloud_GlobalDensity` and `Cloud_AlbedoColor`, plus the cloud layer's altitude and thickness. A map that brings its own cloud material, or is tagged `CrosshairCustomLighting`, is left alone. Space Station uses its own vacuum sky; see [Space Station](space-station.md).

The graphics check logs each map's profile (`sky=...`) and captures it.

## Soldier faces

The face area of the uniform shader is now visible skin, framed by the head covering, with a darker band at the eyes. Before, it was a uniformly dark balaclava, which read as armor over the face. The plate and light carriers stop at the lower chest: front top z 134 (light carrier 132), sides 120, bottom 107. The graphics check now captures a front close-up of a practice soldier (`GfxSoldierFront.png`).

## Graphics settings and ray tracing

Open Escape / Start > **Graphics**. It works in practice and in matches, and changes apply at once without a restart. The rows come in this order:

| Row | What it does |
| --- | --- |
| Graphics quality | Left/Right or Enter cycles the four laptop presets below. Shows "Custom, based on ..." once any individual option differs from the preset. |
| Detect best settings for this PC | Runs the engine benchmark and picks a preset. Integrated graphics are capped at Balanced. |
| Resolution scale | 50–100% in 5% steps. Lower is faster. |
| Ray tracing | Turns hardware ray tracing on or off. On a GPU without DirectX 12 ray tracing it shows "not supported". |
| Field of view | Unchanged. |
| Anti-aliasing | Off, FXAA, TAA or TSR (`r.AntiAliasingMethod`). TSR upscales best from a lower resolution scale. |
| Frame rate limit | Unlimited, 30, 60, 75, 90, 120, 144, 165 or 240 FPS (`t.MaxFPS`). A cap saves battery and heat. |
| VSync | On or off (`r.VSync`). |
| View distance, Shadows, Global illumination, Reflections, Textures, Effects, Post-processing, Foliage, Shading | Low, Medium, High or Epic: the engine's own scalability groups, each set on its own. View distance also sets landscape quality. Global illumination Low turns Lumen off. |
| Volumetric fog | Sun-lit haze and light shafts (`r.VolumetricFog`). |
| Volumetric clouds | Shows or hides the 3D cloud layer. Hidden clouds also stop darkening the sky light. |
| Motion blur | Off, Low or High. |
| Film grain and vignette | Film grain, a light vignette and a faint sun lens flare. |
| Brightness | -5 to +5, in tenths of a stop of exposure. |
| Sharpening | Off, Low, Medium or High (`r.Tonemapper.Sharpen`). Useful at a lower resolution scale. |

Individual options use chips: Left/Right (or the < and > arrows) change them, and quality levels and lists wrap around. The preset and ray tracing rows have long values, so they show the value on the right instead.

| Preset | Engine quality | Resolution | Ray tracing | Other defaults | Meant for |
| --- | --- | --- | --- | --- | --- |
| Battery saver (integrated graphics) | Low | 67% | Off | TAA, 60 FPS cap, no volumetric fog, clouds or motion blur | Integrated graphics, battery power |
| Balanced laptop | Medium | 80% | Off | TSR, unlimited FPS | Thin laptops, Radeon 780M / Intel Arc class graphics |
| Gaming laptop | High | 100% | On | TSR, unlimited FPS | Laptops with a dedicated GPU |
| High-end laptop / desktop | Epic | 100% | On | TSR, unlimited FPS | High-end laptops and desktops |

All presets start with VSync off, low motion blur (except Battery saver), film effects on, default brightness and low sharpening.

Choosing a preset sets its resolution, ray tracing and every individual option; you can change any of them afterwards. The choice is saved with the other Crosshair settings (`FCrosshairSettings::GraphicsOptions`) and applied at every launch. Saves made before the individual options existed get their preset's options on load.

**First launch:** if no graphics choice has been saved yet, Crosshair runs the same benchmark as "Detect best settings" once at startup and saves the preset it picks, so a laptop with integrated graphics starts on Battery saver or Balanced instead of Epic. Automated test runs (any `-...Smoke` flag) and headless runs skip this and keep the engine's defaults.

The lighting rig follows the options:

- **Lumen quality:** Global illumination Epic raises the rig's Lumen scene lighting, final-gather and scene detail. Reflections Epic raises reflection quality, allows two reflection bounces and traces rougher surfaces.
- **Reflections:** hit-lit ray-traced reflections are used only when ray tracing is on and Reflections is High or Epic.
- **Clouds:** cloud ray-march sample counts follow Effects (50% on Low up to 100% on Epic).
- **Ambient occlusion:** lighter when Global illumination is Low.
- Motion blur, film effects and brightness are set on the rig's grade volume. Space Station keeps its own vacuum lighting, so there only motion blur (through `r.MotionBlurQuality`) and the engine options apply.

**How ray tracing switches:** the project already enables ray tracing (`r.RayTracing=True` on DirectX 12), and UE 5.8 lets it be toggled at runtime (`r.RayTracing.EnableOnDemand`). The toggle sets two console variables, `r.RayTracing.Enable` and `r.Lumen.HardwareRayTracing`:

- **On:** Lumen global illumination and reflections are hardware ray-traced.
- **Off:** Lumen falls back to software tracing.

No project settings were changed. The code is in `Trickshot/CrosshairGraphics.h/.cpp`.

The graphics check verifies each preset's engine quality, resolution and options; that individual options override the preset, show Custom and reach the renderer and lighting rig; that re-choosing a preset restores them; old-save upgrade; the menu rows; and ray tracing turning off and back on. It also saves `GfxNuketownRayTracingOff.png` and `GfxNuketownRayTracingOn.png` for comparison.

## More realistic lighting (2026-11-06)

The shared lighting rig gained these, on every map it manages:

- **Sun:** cloud shadows at 85% strength (ground under cloud dims without going black), shadows cast onto the cloud layer and into the atmosphere (light shafts through gaps), and slightly stronger volumetric scattering for sun shafts in the fog.
- **Sky light:** cloud ambient occlusion, so the sky's ambient light is also softer under heavy cloud.
- **Height fog:** a thin, denser second fog layer near the ground (real air is hazier low down), and a soft glow in the haze toward the sun, tinted by the sun's colour temperature.
- **Grade:** a filmic response with slightly cooler shadows and warmer, less saturated highlights; a little more local-exposure detail; a small amount of sky light leaking so interiors are dark but readable; and longer Lumen traces for distant bounce light.

## Verification

Run the checks locally:

- `-game -CrosshairSmoke -CrosshairGraphicsCheck -RenderOffscreen -ResX=1280 -ResY=720 -windowed` → `CROSSHAIR_GRAPHICS_OK`. It visits the Testing map, Nuketown, Grid and the imported example user map. On each map it asserts exactly one atmosphere sun, a real-time sky light, volumetric fog, clouds and one grade volume. It also checks the Space Station map keeps its own vacuum lighting and 21 spawns. It saves `Saved/Screenshots/Gfx*.png` and logs the first-person arm joints and IK goal (`CROSSHAIR_ARMS`).
- `-game -CrosshairSmoke -CrosshairSoldierProbe` (editor builds) logs body cross-sections for regenerating gear.
- `-CrosshairFrontendSmoke -CrosshairFrontendVisual` captures all six soldier designs.
