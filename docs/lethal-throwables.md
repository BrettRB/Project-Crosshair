# Lethal throwables

The initial equipment choices are Frag grenade and Tomahawk / battle axe. Grenade/frag names describe one explosive type; axe/tomahawk names describe one throwing weapon. No outside assets or services are required.

## Play

- Hold G / right bumper to prepare; release to throw along the camera direction, with gravity and a quarter of player velocity added.
- Hold a frag to cook its three-second fuse. Holding through the fuse detonates it and spends one charge.
- F / left bumper cycles equipment during play. Practice > Lethal also cycles and saves the selected type. Menu bumpers continue to select tabs.
- Two shared charges per attempt, independent of gun ammunition. Reset replenishes both charges and destroys held, flying, and stuck throwables.
- Opening the menu or losing focus cancels preparation without spending a charge. Fire, aim, reload, switching weapons, and starting a mantle are blocked while preparing or during the short release cooldown.

## Gameplay and tuning

UCrosshairLethalComponent owns preparation, cooking, selection, supply, cooldown, and reset cleanup. ACrosshairThrowable owns replicated presentation, swept collision, flight, impacts, and explosive damage. These are separate from the firearm class. The character's Lethals component exposes both FCrosshairLethalSpec configurations and capacity to Blueprint.

Default frag: speed 1400 cm/s, gravity multiplier 1, bounce .45, fuse 3 seconds, damage 150 inside 120 cm, linear falloff to 30 at 350 cm. Visibility traces block blast damage through solid cover. Nearby exposed breakable panes shatter. Default tomahawk: speed 2200 cm/s, gravity multiplier .5, direct damage 250, no explosive radius or fuse. It sticks on impact and expires after eight seconds; misses expire after twelve seconds. A release sweep prevents launching beyond nearby walls. Projectile substeps and per-frame swept target traces prevent tunneling at ordinary frame rates. UCrosshairLethalMovement skips wall deflection for a pane just broken by impact.

State and movement are recorded by the existing demo replay system. Playback presents recorded movement, spin, glass and detonation state without applying damage again. Returning to practice resets the supply and clears stale projectiles. The selected equipment is stored in FCrosshairSettings with Frag as the backward-compatible default.

## Assets

Original curved grenade and axe meshes are in /Game/Crosshair/Weapons/Throwables. OBJ sources and material slots are in ContentSource/Throwables. Scripts/create_throwable_models.py regenerates only these two meshes using Unreal's Python import APIs and the existing weapon finish materials. Run it through UnrealEditor-Cmd with -run=pythonscript -script=Scripts/create_throwable_models.py. It overwrites those two generated meshes; do not run it over later hand-authored replacements.

## Local checks and current limits

The development build and dedicated -CrosshairSmoke -CrosshairLethalSmoke integration test cover keyboard/controller key paths, cooking, cancellation, menu selection/profile persistence, supply, swept direct hits, floor bounce, explosive falloff, blast obstruction, near-wall release, reset cleanup, glass penetration and recorded axe flight/impact with replay return. The rendered variant adds -CrosshairLethalVisual -RenderOffscreen -UnattendedInput and captures the equipment menu and both held models.

The practice player remains invulnerable; overcooking consumes a charge but does not introduce a new player health system. Throw-specific hand animation, audio, particle polish, alternate grenade types, axe pickup, equipment unlocks and online multiplayer authority/RPC support remain future work. These controls are native fixed bindings. Tests use the isolated CrosshairSmoke_v1 profile. No packaged/cooked build or physical controller test is claimed.
