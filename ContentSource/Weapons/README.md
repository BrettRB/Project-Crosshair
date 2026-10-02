# Original weapon source geometry

These three meshes are original project artwork authored by Scripts/create_weapon_models.py. No third-party source models were used. Dimensions are centimeters, X forward and Z up; the right-hand grip is at the origin. Paint, Metal, Rubber and Glass are semantic material slots. The placeholder Crosshair.mtl is used only for importing slot names; Unreal finishes are assigned separately.

The OBJ files can be edited in a DCC tool and reimported through Unreal. Running the generator overwrites these source files and the three generated meshes, so preserve artist edits elsewhere first. Run Scripts/create_weapon_finishes.py after a reimport to restore project material assignments. See docs/weapon-appearance.md for the rendering and regeneration workflow.
