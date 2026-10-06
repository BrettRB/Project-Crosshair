# Traversal, windows and humanoid targets

First gameplay implementation: 2026-10-02. No damage, health, controller direction or sensitivity settings changed.

## Playing

Choose Red Tiger or Arctic in Camos. Both are immediately available on each weapon and save independently. Place targets as before; the placement preview now matches the humanoid target, with green/red clearance feedback. Head, torso, arms and legs use the mannequin physics bodies; the old target capsule only blocks movement, so shooting beside the head or between limbs does not score a capsule hit. A head-body hit uses the current weapon's head damage, all other hit bones use its existing body damage. The idle pose updates the physics bodies as it animates. Shot-only skeletal collision is enabled after the initial pose, so spawn clearance checks use the physical placement capsule rather than uninitialized hit bodies.

Shoot designated house glass to shatter it, then face the opening and press Space / controller A near it. The breaking shot continues through glass; thin walls can now be penetrated with reduced damage, while thick cover and NoWallbang-tagged frames stop the bullet. See [hit feedback and wallbangs](hit-feedback-and-wallbangs.md). Small decorative openings cannot fit the player. Use the same Jump button near a reachable ledge to mantle onto it. The path lifts, crosses and lowers the player with eased motion and swept capsule collision. A low opening automatically crouches the player; standing resumes only when there is clearance. Menu/focus loss/reset cancels traversal, and normal gravity is restored on both completion and cancellation.

Jump buffering holds a request for 120ms while pressed, allowing a jump just before landing. Coyote time allows the first jump within 100ms after walking off a lip. It does not grant a double jump. Extra movement mechanics such as sliding and dedicated mantle animations remain future work.

## Future-map setup

Place ACrosshairWindow (or a Blueprint derived from it), assign the Glass static mesh and glass material, and keep frames/walls in separate solid actors. The pane needs Visibility and Pawn blocking collision while intact. Pawn queries require appropriate simple collision or ComplexAsSimple collision on brush-style meshes. The actor hides and disables its pane collision when damaged, replicates that state into replays, and presents temporary collision-free shards. Attempt reset restores all ACrosshairWindow panes in the world.

For conventional axis-aligned pane meshes, opening bounds derive automatically from the mesh. For meshes whose rotation is baked into vertices, enable Configured Opening and supply mesh-local Opening Center, Opening Normal (horizontal plane normal), Half Width and Half Height. The Nuketown conversion computes these from exported vertices and retains the original transform; exports under Saved/WindowGeometry are diagnostic output, not runtime dependencies. No source-map or frame assets are regenerated. Scripts/setup_expansion_assets.py regenerates the finishes, converts remaining house panes and compiles the relevant Blueprints through the editor.

The character owns UCrosshairTraversalComponent. Designers can tune its exposed values:

| Setting | Default |
| --- | --- |
| Reach | 120cm |
| Maximum mantle height | 160cm |
| Minimum generic ledge height | 45cm |
| Duration | 0.55s |
| Coyote time | 0.10s |
| Landing jump buffer | 0.12s |

Detection checks vertical wall/top normals, landing support, capsule overlap and the entire lift/across/drop sweep. Actual movement sweeps the same waypoints even when a long frame crosses a path corner. Traversal temporarily disables gravity while retaining Unreal's falling mode, because flying mode forces automatic uncrouching. It restores the prior gravity scale afterwards. It does not support dedicated network multiplayer movement prediction, moving platforms or bespoke mantle animation yet; first-person replay is supported.

## Local verification

- Editor Development build passed.
- setup_expansion_assets.py succeeded with no script errors/warnings; 48 panes were converted, and a second pass reported converted=0, total=48.
- -game -CrosshairSmoke -CrosshairExpansionSmoke -NullRHI passed CROSSHAIR_EXPANSION_OK. This uses the isolated CrosshairSmoke_v1 save rather than the normal player profile.
- Coverage: four camos on all three weapons; paint-only material changes; direct Red Tiger/Arctic menu selection; reset/map persistence; real skeletal head/torso/leg traces from front and side; capsule-only near-head miss; one bullet through glass; opaque cover; glass reset; keyboard/controller ledge mantles; saved ledge start; reset cancellation; low-ceiling rejection; ordinary jump, coyote jump and buffered landing jump; actual Nuketown window crossing; replayed broken glass and mantle movement; glass state after replay return and next reset.
- All seven Crosshair automation tests passed (input frame rate/dead zone, hardware calibration, independent inversion, mouse displacement, damage thresholds, placement support and firing gates).
- Existing -CrosshairFollowupSmoke passed CROSSHAIR_FOLLOWUP_SMOKE_OK, including physical-key mouse/controller injection, calibration, menu/camo persistence and raised-grass/map regressions.
- Rendered -CrosshairExpansionSmoke -CrosshairExpansionVisual -UnattendedInput -RenderOffscreen run passed CROSSHAIR_EXPANSION_OK at 1280x720. Reviewed Saved/Screenshots/ExpansionRedTigerTarget.png, ExpansionArctic.png and ExpansionCamos.png; the target and both coating patterns render correctly. An initial visual-harness timeout while its menu was open was corrected before the final run.
- The existing full -CrosshairSmoke passed CROSSHAIR_SMOKE_OK, including placement, weapon handling, successful-shot recording, replay camera/weapon restoration, target layout and attempt-start restoration. -CrosshairSmokeSaved passed CROSSHAIR_SMOKE_SAVED_OK in a separate process, verifying saved playback and deletion. The final expansion suite also passed after these corrections. Initial placement and practice-map replay startup failures were corrected before the final runs.
- Short replay startup seeks to 0.1s rather than zero. Debugger state showed frame zero with buffered packets, a pending connection and a frozen 0.0167s playback clock; engine code waits for more stream data before processing that first buffered packet. The nonzero seek processes the initial frame. This omits the first 100ms from short replay viewing; longer attempts retain the existing eight-second lead-in.
- Runtime debugging at CrosshairTraversal.cpp captured bAutoCrouched=true while the character's actual bIsCrouched was false, with a blocking window-frame hit. Unreal's flying mode caused the unintended uncrouch; the fix preserves crouch and sweeps corners. Agent breakpoints/session were removed/stopped afterward; user breakpoints were preserved.

Physical wireless-controller hardware, packaged/cooked builds, every individual Nuketown opening and bespoke animation/art polish remain unverified. The native input route is unchanged; controller activation tests inject actual A-button input into the controller path. Existing imported missing texture/material warnings remain documented in nuketown-import.md.
