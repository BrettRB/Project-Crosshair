# Practice controls and presentation

Open the menu with Escape / Start. Click a category or use Q / E, Tab, or LB / RB to switch between Practice, Controls, Display, Targets, Replays, and Camos. Arrows / D-pad select rows; Left / Right adjust values; Enter / A activates. Click the arrow buttons to adjust a setting with the mouse. Escape / B closes the menu.

## Saved attempt position

Stand upright on supported ground, a platform, or a balcony. Use **Save position** in the Practice tab, K, or D-pad Right. The current location and view direction become the reset point for this map session. Use T / D-pad Down or **Reset to saved position** to repeat attempts with weapons and targets reset. Saving in the air, while crouched, during replay playback, or while a successful replay is finishing is rejected. Map travel starts a fresh session.

## Controller input

Windows builds use the bundled GameInputWindows plugin, including connection/disconnection handling. Windows per-platform GameInput settings enable standard gamepad readings; XInputDevice is disabled to avoid duplicate gamepad events. Keyboard and mouse continue through the normal Windows input path. Controls remain Left stick move, Right stick look, A jump, L3 sprint, B crouch, RT fire, LT aim, X reload, Y switch weapon, and D-pad for practice actions.

Local validation confirmed GameInput 3.5.270.0 initialized and registered gamepad connection callbacks. Simulated physical-key input passed movement, look, ADS, saved-position reset, and menu navigation. No physical controller was connected during verification. The user's controller model/connection is still unspecified; generic HID devices that Steam translates into gamepad input may need a device-specific mapping after identification. A Steam-game result alone does not verify native GameInput support for that device.

## Controller look direction

Controls includes **Invert controller horizontal** and **Invert controller vertical**. Both default to Off, save independently in the local profile, and apply only to controller look. With both Off, right stick right turns right and pushing it up looks up. Each toggle reverses only its named axis, including while aiming. Mouse look and movement are unchanged.

The player controller reads MouseX/MouseY displacement and Gamepad_RightX/Gamepad_RightY held axes before legacy/Enhanced Input modifiers. Character ApplyLookInput combines both once per UpdateRotation; controller dead zone, sensitivity and independent inversion are applied there. Mouse displacement is consumed once, while a held stick integrates over time. WASD and left-stick movement remain Enhanced Input actions.

GameOnly mode with permanent capture including the first mouse-down is applied at startup and after closing the pause menu. Cursor/click flags are cleared before restoring capture. Opening the menu or losing focus flushes pending mouse and held stick state, so input does not accumulate into a surprise turn when play resumes.

## Camos

The Camos tab selects a weapon; click Original, Woodland, Desert, Red Tiger, or Arctic, or select a row with arrows/D-pad and press Enter/A. The active camo is marked Equipped. Each weapon remembers its own finish across resets, switches, map travel, and restarts. See docs/weapon-appearance.md for the model-replacement foundation and remaining art/progression work.

## Weapon hands

The animated HandGrip_R position is aligned with the weapon each frame after pose evaluation. The arms share weapon ADS, recoil, and reload transforms. Reload keeps the idle alignment reference so free-hand animation is retained. Dedicated masked material copies in /Game/Crosshair/Player/Arms hide the full mannequin's torso/head/legs using reference-pose vertex positions; original mannequin assets/materials are unchanged. The material passes reference positions through a vertex interpolator before its pixel-stage opacity mask.

Scripts/create_first_person_arm_materials.py creates/configures these dedicated materials through Unreal APIs. Scripts/repair_nuketown_spawn.py moves only the playable Nuketown PlayerStart; setup_nuketown.py uses the same position for a fresh import.

## Local verification (2026-10-01)

- Project_CrosshairEditor Win64 Development: build succeeded.
- Crosshair automation suite: all five tests passed.
- -game -CrosshairSmoke -CrosshairPolishSmoke -NullRHI: CROSSHAIR_POLISH_SMOKE_OK. Verified controller stick movement/look, Start/bumper/D-pad menus, sensitivity adjustment, mouse category/save-button callbacks, three elevated resets preserving view direction, D-pad reset, trigger ADS, and aligned animated grip.
- -game -CrosshairSmoke -CrosshairQuitSmoke -NullRHI: normal menu quit, exit 0, after WASD/damage/weapon-menu regression checks.
- -game -CrosshairSmoke -CrosshairMapSmoke -CrosshairMapVisual -RenderOffscreen: CROSSHAIR_MAP_SMOKE_OK. Verified central spawn, ground, walking, firing, target placement, reset, settings persistence, and map/layout isolation.
- -game -CrosshairSmoke -CrosshairVisual -UnattendedInput -RenderOffscreen: CROSSHAIR_VISUAL_OK. Reviewed final hip/scope/iron-sight/crouch/menu screenshots. Visual tests wait for shaders before capture.
- Rendered tests used 1280x720 and t.MaxFPS 60. Logs/screenshots are generated under Saved. No packaged build or physical wireless hot-plug test was performed.

## Follow-up verification (2026-10-01)

Local Editor Development build, all six Crosshair automation tests, -CrosshairFollowupSmoke, -CrosshairPolishSmoke, and the final -CrosshairFollowupVisual rendered run passed. The grass repair persisted across save/reload and all four types were nonblocking at runtime with solid ground. Both controller axes passed positive/negative physical-key injection under all four inversion combinations. Finish material, disk persistence, reset/switch/map travel, original restoration, and authored-model presentation were verified. Final screenshots show both starter camos, Weapons tab, and all eight Controls rows. Physical device, unlock progression, realistic replacement artwork, packaging, and full replay playback remain unverified or deferred as described above.

## Mouse/controller repair verification (2026-10-01)

After the reported mouse/controller regression, look handling moved to independent native axis input in the player controller. Local build, six automation tests, expanded follow-up headless/rendered integration, polish regression, and keyboard/weapon quit regression passed. Tests cover stale legacy inversion, mouse/stick composition, menu clearing, and capture restoration. Final Camos tab screenshot shows direct choices and Equipped status. Physical controller direction and real-device capture still require confirmation with the user.

## Controller direction calibration and model update (2026-10-01)

If your controller still looks in the opposite direction with inversion Off, open Controls and activate **Calibrate controller direction**. Push the right stick RIGHT, release it to center, push it UP, then release it again. The row shows the current instruction. Completing both directions saves the hardware correction and sets both optional inversion toggles Off. Closing the menu, changing tabs, losing focus, or activating the calibration row again cancels an incomplete calibration without changing the saved correction. Recalibrate when switching to a device/driver with a different axis convention.

Calibration corrects each device axis before the radial dead zone, sensitivity and optional user inversion. Mouse displacement is independent. Defaults remain standard right/up with both inversion switches Off. The calibration is local-profile-wide, not a per-device-ID catalog.

The actual user's saved horizontal and vertical flags were read as Off during validation, and Windows' targeted device inventory reported no matching connected controller. Simulated standard and reversed hardware axes passed the actual player-controller/camera path, calibration persistence/cancellation and independent mouse checks. Physical-controller confirmation remains outstanding; no device-specific root cause is claimed.

The Camos tab now applies realistic layered coatings to three distinct authored models; see docs/weapon-appearance.md for sources, regeneration and material-slot details.

## Mantling and jump forgiveness (2026-10-02)

Press Space / A while facing a reachable ledge or a broken window. Traversal checks the full capsule path and landing clearance, automatically crouches through sufficiently large low openings, and stands up afterwards when space permits. A blocked path cancels safely. Reset, opening the menu and input focus loss cancel active mantling. Glass shatters when shot; that bullet continues through the pane. Thin cover supports reduced-damage wallbangs; thick cover still stops shots. Resetting an attempt restores the glass.

Ordinary jump, sprint and crouch remain available. Jump has 100ms of coyote time after walking off an edge and a 120ms landing buffer while the button stays pressed. See [traversal and target setup](traversal-and-targets.md) for reusable map setup, tuning and local verification. Additional movement mechanics and cosmetic unlock progression remain in [the backlog](backlog.md).

## Hit feedback and wallbangs (2026-10-02)

Head hits show a larger gold marker and HEADSHOT text; body/limb hits show the ordinary white marker. Penetrated hits add WALLBANG text. The HUD now uses compact session/ammo/help panels, larger type and clearer menu rows; ammo remains visible when scoped. Three authored practice targets are upright and face the player. All 82 decorative Nuketown flower meshes have no collision, preserving the bed/ground and source assets.

Wallbangs allow up to two vertical thin cover surfaces and 40cm total thickness, retaining 75% damage per layer. Glass consumes neither budget nor damage. See [hit feedback and wallbang tuning](hit-feedback-and-wallbangs.md).

## Lethal throwables (2026-10-02)

Hold G / RB to prepare the selected lethal; release to throw. Hold a frag to cook its three-second fuse. F / LB cycles Frag grenade and Tomahawk / battle axe during play. Choose **Lethal** in the Practice tab to save your selection. Bumpers still change tabs while the menu is open. The HUD shows the selected type, supply, and remaining fuse while cooking.

Each attempt has two shared lethal charges, independent of firearm ammo. Reset restores supply and removes thrown items. Frags bounce and deal distance-based blast damage; solid cover blocks the blast. Tomahawks spin and damage targets on direct impact, then stick. Both break shootable glass. Successful lethal target hits use the existing trickshot recording and replay flow. See [lethal setup and tuning](lethal-throwables.md).

## Home, classes, map imports and sandbox (2026-10-05)

Home now offers Play, Weapon classes, Maps / Import and Settings. Five saved classes choose a primary, a distinct secondary, and a lethal. Classes is also available in the pause menu. Equip this class applies the selection; Play uses the active class. Number-row 1/2 selects a slot; wheel, Q and Y / Triangle switches between the two chosen guns.

Controller default yaw/pitch are 720/540 degrees per second, with higher adjustable limits of 2160/1440. Old default speeds migrate once while custom speeds remain. Left-stick click latches sprint while moving. Settings > Controls > Sandbox target editing enables dummy placement/removal/clear controls; it ships Off and is also accessible in Targets. Mouse wheel navigates long menus.

Maps / Import accepts local OBJ geometry or a map.json package with scale/spawn and one optional PNG/JPG atlas. Imports keep a local copy, create collision and participate in replay playback. See [home, classes and map import](home-classes-and-map-import.md) for steps, supported formats, limits and implementation.
