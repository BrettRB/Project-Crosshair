# Gameplay backlog

Recorded 2026-10-01. Originally deferred at the user's request. Implementation began on 2026-10-02 when the user asked to start these features. The original request remains below; no unattended work is scheduled.

## Requested features

| Item | Requested outcome | Checks to use when implementing |
| --- | --- | --- |
| Red tiger camo | A realistic red-and-dark tiger stripe finish inspired by the user's favorite camo. This is the user's strongest named cosmetic preference. | Available through Camos for sniper, AR and SMG; painted surfaces receive the pattern while metal, rubber and optics retain their materials; selection persists across resets, weapon switches and restarts. |
| Arctic and additional basic camos | Add an arctic/snow finish and expand the basic camo choices for other players. | Visually distinct finishes with the same material-slot and persistence behavior as existing camos. The menu displays catalog entries as the catalog grows. Other specific patterns remain to be selected. |
| Target models and hitboxes | Make the targets' appearance and shot detection feel closer to the classic COD-style shooters that inspired this project. | Target shape and hittable regions visually agree; head/body and any agreed limb regions report hits consistently from different angles and while moving; avoid invisible protruding hitboxes; preserve placeable targets, shot feedback and quick attempt resets. |
| Shootable, traversable windows | Shoot out window glass and climb through the resulting openings on Nuketown; support the same mechanic in future maps. | Shots break designated glass, broken glass stops obstructing the player, and the opening can be traversed when it fits the player. Window frames and surrounding walls retain collision. Glass state restores appropriately when repeating attempts; future maps can use reusable window setup. |
| Mantling and further movement polish | Add mantling to reach suitable ledges and help window traversal feel closer to COD movement. | Contextual ledge detection and traversal, clear landing/capsule space, smooth movement, and reliable keyboard/mouse and controller activation. Block traversal into solid geometry; preserve jumping, sprinting, crouching and saved-position resets. Additional movement mechanics are not specified yet. |

## First implementation (2026-10-02)

- Red Tiger and Arctic are immediately selectable on sniper, AR and SMG, alongside Original/Woodland/Desert. Camos uses the weapon catalog for additional entries. Existing material-slot behavior and saved IDs are retained.
- Targets and placement previews use the existing humanoid mannequin in an idle pose. Visibility shots hit the animated physics bodies and report their bone; head damage requires the head body, all other bodies retain existing body damage. Health/damage values are unchanged.
- All 48 designated house glass panes in playable Nuketown are reusable breakable-window actors. The breaking shot can continue through glass; solid frames/walls remain collision geometry. Attempt reset restores the panes, and replay return preserves the captured broken state until the next reset.
- Contextual Jump mantles onto collision-tested ledges or through sufficiently large broken windows. Automatic crouching handles low openings. Coyote time and landing jump buffering are included. See [setup and verification](traversal-and-targets.md).

Further work: extra basic patterns, optional unlock progression, custom target art/animations, per-limb damage tuning if requested, bespoke mantle/hand animations and any additional specifically requested movement mechanics. These are not implemented by this first pass.

## Suggested implementation sequence

Start with red tiger and arctic camos, then target presentation/hit regions, followed by reusable breakable windows and mantling together. This is a suggested order, not a user-mandated priority. Review any remaining physical-controller direction issue when a controller is available.

## Original implementation questions

- Pick the target model/pose and specific reference feel; confirm which hit regions and damage rules are desired before changing current weapon damage or target health.
- Determine the new camo names and whether they are immediately available or unlocked. Existing finishes are immediate; the earlier unlock concept remains undecided.
- Confirm whether a shot should continue through glass on the breaking shot, how glass responds to repeat shots, and when broken windows reset. Inspect how Nuketown's window glass/frame meshes and collision are authored before choosing the integration.
- Tune mantle height, reach, timing, activation and animation using representative Nuketown windows/ledges. Keep the system reusable for future map configuration.
- Verify recording/playback captures target, glass and traversal state, alongside local gameplay tests, when those features are implemented.

## Current foundations

- [Weapon models and finishes](weapon-appearance.md): existing Original/Woodland/Desert catalogs and saved cosmetic selection.
- [Gameplay controls](gameplay-controls.md): input, controller calibration, menus, saved positions and hand transforms.
- [Nuketown integration](nuketown-import.md): imported asset paths, playable map, collision repairs and download provenance.

## Original request

I would also like to add more camos to the game at a later date. My favorite camo of all time is tiger red so adding something similar to that would be awesome. Adding more basic stuff like arctic and stuff would also be important to other possible users. I would also like the target models and hit boxes to look and behave more like COD game hit boxes. I also need the ability to shoot out and climb through the windows in nuketown but also in other future maps. Adding more movement stuff like mantling would also make it feel more like COD. But that can all be done later just listing things down now so you can start working on them next time I boot this up to work on it[@CrosshairCharacter.cpp](file:///C:/Users/BrettRB/Project_Crosshair/Source/Project_Crosshair/Trickshot/CrosshairCharacter.cpp)
