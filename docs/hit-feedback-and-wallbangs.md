# Hit feedback, wallbangs and UI

Implemented 2026-10-02.

## Hit markers

Actual skeletal `head` hits use a larger gold X with extra side marks, HEADSHOT text and a 0.4-second display. Other hit bones use the ordinary white X for 0.25 seconds. A hit through solid cover adds WALLBANG text. Feedback applies only when damage is accepted; already defeated targets do not create another hit marker. Headshot/wallbang flags accompany the replay-only view state so playback can display the matching marker.

## Penetration

The reusable CrosshairBallistics helper resolves camera aim without mutating glass, checks the actual barrel route, and traces beyond penetrable cover. Each weapon definition exposes:

| Property | Default | Meaning |
| --- | --- | --- |
| PenetrationDepth | 40cm | Combined entry-to-exit thickness budget |
| PenetrationLayers | 2 | Maximum solid cover layers (runtime capped at 4) |
| PenetrationDamageScale | 0.75 | Damage retained per solid layer |

Two layers retain 56.25% damage. Base head/body damage and target health remain unchanged. Glass shatters and passes the same bullet without consuming these budgets or reducing damage. Thick walls, horizontal floors/ceilings, missing/invalid exit geometry and the nearby camera-to-barrel obstruction gate stop penetration. Set either the actor or component tag `NoWallbang` on cover that must always stop shots; zero depth or zero layers disables wall penetration for a weapon.

An exit is found by reverse-tracing the hit component within the remaining thickness, requiring an outward-facing exit. The next trace starts beyond it without ignoring the cover actor or changing its collision. Other overlapping geometry inside the measured thickness remains blocking. One trigger shot still uses one round and one recoil event. Penetration is based on geometry; material-specific resistance values remain future tuning. Cooked/package and all individual imported wall shapes have not been verified.

## Asset repairs and UI

The three authored L_Practice targets had a 180-degree pitch from positional Python Rotator arguments. Their actor rotations are now pitch=0, yaw=180, roll=0, and create_crosshair_assets.py uses named arguments to prevent recurrence. The model's native orientation and placeable-target rotations are retained.

repair_feedback_assets.py also removes collision from the 82 Nuketown flower components, preserving separate beds/ground and original meshes. It reloads both maps and asserts the saved state. probe_feedback_assets.py reports target transforms and vegetation assets without changing them.

The HUD uses session/ammo/help panels, shorter contextual hints and larger readable fonts. Scoped views retain ammo/session information. Pause tabs and keyboard/controller/mouse navigation remain, with tab descriptions, distinct selected states, separated setting labels/values, and simpler camo names.

## Local verification

- Development Editor build succeeded.
- Editor repair command succeeded with zero script errors/warnings: upright=3 flowers=82; save/reload assertions passed.
- FeedbackSmoke.log: headless CROSSHAIR_EXPANSION_OK for head/body feedback, thin wall plus glass, thick cover, upright targets, all flowers and existing traversal/replay/camo checks.
- Final FeedbackVisual.log: rendered CROSSHAIR_EXPANSION_OK at 1280x720, including two-layer damage, third-layer blocking, combined thickness budget, NoWallbang tags and the near-barrel gate. Reviewed FeedbackHeadshot.png, ExpansionCamos.png and ExpansionArctic.png. Final UI type enlargement and scope panel ordering were included in this run.
- FeedbackCoreSmoke.log: CROSSHAIR_SMOKE_OK, including movement/input, weapons/obstruction, target placement, menus, recording/playback and restored layout/attempt start.
- A rendered test aiming race was corrected: the test now waits for camera rotation before firing. Rider captured a neck_01 hit with a still-horizontal shot direction; gameplay bone classification was preserved. Agent breakpoint/session were removed, leaving all eight user exception breakpoints unchanged.
