# Bot free-for-all preparation

Home > Bot FFA setup and pause > Bot FFA expose the same saved options. Use mouse arrows, keyboard Left/Right or controller D-pad to select Easy, Regular, Hardened or Veteran, 1?11 bots, a 5?100 score limit and a 1?30 minute limit. Defaults are Regular, five bots, 30 points and ten minutes. Settings persist in the existing profile. Practice remains playable; this stage does not launch a bot match.

UCrosshairBotMatch is a reusable game-instance subsystem. SetDifficulty validates and saves the selected preset, then broadcasts OnDifficultyChanged for future active AI controllers. GetCurrentTuning reads the selected preset immediately, including when adjusted from pause. Replay playback/finalization blocks changes. Settings initialization and saving bound unsupported/corrupt values.

| Difficulty | Reaction seconds | Aim error degrees | Tracking degrees/second | Decision seconds | Movement cm/second |
|---|---:|---:|---:|---:|---:|
| Easy | 0.90 | 7.0 | 65 | 0.50 | 380 |
| Regular | 0.55 | 4.0 | 100 | 0.30 | 450 |
| Hardened | 0.30 | 2.0 | 160 | 0.20 | 500 |
| Veteran | 0.18 | 0.9 | 220 | 0.12 | 550 |

These are initial tuning values, awaiting combat playtesting. No combat AI is present yet. Presets retain finite reaction latency and aim error; difficulty is intended to tune behavior rather than grant wall vision.

Next implementation stage: add a dedicated FFA GameMode/GameState with score/time limits and a scoreboard; reusable health/death/respawn handling for player and bots; bot pawns/controllers using visible-enemy perception, cover-aware aiming and navigation; map spawn points and navigation validation; and a Start match action with return-to-home flow. Future active controllers should subscribe to OnDifficultyChanged and re-read tuning without restarting the match. Spawn count, health, lethals, friendly-free target selection and replay policy need integration tests before enabling match launch. Keep trickshot practice separate.

Crosshair.Bots.DifficultyPresets tests monotonic tuning and invalid configuration bounds. FrontendSmoke checks the home setup page, saved options, immediate tuning, and difficulty adjustments during gameplay. Local Editor Development verification does not establish packaged builds or physical controller behavior.
