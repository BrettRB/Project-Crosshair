# Highrise Rooftops and weapon switching

## Highrise retired

Highrise was removed at the user's request on 2026-10-08. Its map, materials and generation scripts are removed, and it is no longer listed or cooked. The weapon-switching feature below remains available on Testing Map, Nuketown and local imports. Earlier Highrise results remain recorded in the interaction log.

## Weapon carry animation

All existing slot controls trigger an outgoing shoulder stow followed by an incoming draw. Hands follow the outgoing weapon root until it is stowed. The inactive class gun appears on the character's back for external views; owner visibility avoids clipping into the first-person camera. A configured world mannequin uses its separate unarmed body idle, preserving the first-person rifle animations.

StowSeconds and DrawSeconds are adjustable on each weapon definition; defaults are 0.18 and 0.22 seconds. Firing, aiming and reloading are blocked until the draw finishes. Switching cancels firing/reload on the old gun. Class changes, resets and replay state preserve the two-slot behavior. Camos remain on the carried weapon.

This is procedural motion using the existing rig and root transforms. Back carry currently follows the character capsule, not a spine socket; custom animated third-person locomotion and artist-authored sling/hand-off montages remain future polish. Scripts/setup_weapon_carry_body.py configures only BP_PracticeCharacter's world body and idle animation.

## Local verification

Editor Development build, nine Project.Crosshair automation tests, four gameplay regression runs, and the extended frontend test verify slot transitions, no firing during draw, back attachment, built-in/imported map travel, target hits and replay return. Rendered screenshots inspect shoulder stow, upright body/back carry and body pose. Packaged builds and physical controller hardware are not verified here.
