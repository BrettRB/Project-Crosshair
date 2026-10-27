# Offline matches (FFA / TDM) and LAN foundation

Added 2026-10-21. Offline Free-for-all and Team Deathmatch against bots on the COD maps (Nuketown and Grid; Space Station, a Nuketown copy, was added 2026-10-24), plus the first stage of same-network (LAN) multiplayer.

## Menus (2026-10-21 redesign)

The main menu separates the ways to play: **Trickshot practice** (the solo trickshot mode, unchanged), **Offline match vs bots** and **LAN match** as large cards, with Weapon classes, Maps / Import, Replays, Settings and Quit as option tiles. A strip shows your active class and next match setup. The pause menu uses the same style: sidebar tabs, label/value rows with `< value >` chips (click the arrows or use Left / Right), help text for the highlighted row and key hints along the bottom. Practice tabs are dimmed during a match.

**Offline match** has its own page: Mode, Match map, Bot difficulty, Bots (FFA) or Team size (TDM), Score limit and Time limit on the left, a live match summary card on the right, and a **START MATCH** button. **LAN** is a separate tab (host, find, join, network status).

## Playing

Main menu > **Offline match vs bots**, or Escape / Start > **Offline Match** tab. Rows:

| Row | Values | Default (COD) |
| --- | --- | --- |
| Mode | Free-for-all / Team Deathmatch | Free-for-all |
| Match map | Nuketown / Grid / Space Station / Random | Nuketown |
| Bot difficulty | Easy / Regular / Hardened / Veteran | Regular |
| Bot count (FFA) / Team size (TDM) | 1-11 bots / 1v1-6v6 | 7 bots (8 players) / 6v6 |
| Score limit | FFA kills 5-100 / TDM team kills 10-200 | 30 / 75 |
| Time limit | 1-30 minutes | 10 |
| Start offline match | travels to the selected map | |

Matches are only available on Nuketown, Grid and Space Station (which reuses Nuketown's 21 spawns). The Testing map and imported local maps stay practice-only: the match mode refuses any other map, spawns no bots and returns to practice.

During a match:

- Health is 100 and regenerates after 4 seconds without damage (40 per second). Red screen edges show damage.
- Weapons use their existing head/body damage and wallbang scaling; frags and tomahawks also damage players. TDM friendly fire is off.
- Death drops a ragdoll; you respawn after 3 seconds at a COD-style spawn (see below).
- Hold **Tab / Back (View)** for the scoreboard. The kill feed is top right; TDM teammates show blue names.
- Reaching the score limit or the time limit ends the match. The results screen shows the winner and standings; press **Enter / A** to leave, or it returns to Home after 20 seconds.
- Escape / Start pauses an offline match. The pause menu offers Resume, Classes, **Leave match** and Quit. Target editing, saved positions/reset, map changes and replay viewing are practice tools and are unavailable in matches.
- **Every kill you make is saved to Replays** (any weapon or lethal), without pausing the match. Each clip is labelled with what happened, e.g. `Mid-air 360 No-scope Headshot - Bolt-Action Sniper, 42 m`. Labels: Mid-air, 360 / 720 (yaw turned in the 1.5 s before the kill), No-scope (sniper not aimed) or Hip-fire (other guns not aimed), Wallbang, Headshot, Long shot (40 m+, or 15 m+ for lethals). Kills within one second share a clip (`Double kill`, `Triple kill`, `Multi kill`). Real tricks also flash a **TRICKSHOT** banner. Watch them from Main menu > Replays (or the pause Replays tab outside matches); playback follows your soldier in first person with the bots shown. Bot kills are not saved.
- Equipping a class mid-match applies on your next spawn, as in COD. Bots carry a random two-weapon class and lethal.

## Spawns

Spawns are `ACrosshairSpawnPoint` actors placed in `L_Nuketown` and `L_Grid` (not `APlayerStart`, so practice spawning is unchanged). Each has:

- `bFreeForAll` / `bTeamDeathmatch`: which modes may respawn there.
- `InitialTeam` (Alpha/Bravo): the TDM start area used for each team's first spawn.
- `Area`: a short name for diagnostics.

Respawn selection follows COD's approach: candidate points are scored away from and out of sight of living enemies, toward teammates in TDM, and away from points used in the last few seconds; occupied points are skipped. Rules live in `CrosshairMatchRules::ScoreSpawn` and are covered by `Crosshair.Match.Rules`.

Exact Call of Duty spawn coordinates are not publicly available, so placement approximates the published layouts and is editable in the Unreal Editor: move, add or retag spawn actors (World Outliner folder `Match/Spawns`) and save the level. `Scripts/setup_match_spawns.py` reproduces the placement (it refuses to duplicate existing spawns unless `CROSSHAIR_RESET_MATCH_SPAWNS=1`).

### Nuketown (21 spawns)

Black Ops Nuketown is two facing houses across a cul-de-sac; team modes start each side in its house's backyard. In this Source-engine port the houses sit at +X and -X with the street along Y.

| Spawns | Area | Use |
| --- | --- | --- |
| 6 | Backyard behind the +X house | Alpha TDM start; FFA and TDM respawns |
| 6 | Backyard behind the -X house | Bravo TDM start; FFA and TDM respawns |
| 9 | House side passages, street corners, Alpha front yard | FFA and TDM respawns |

### Grid (23 spawns)

Grid's built area sits inside a large snow plane; the playable space (fences/buildings, as measured by navigation) is about X -5700..4800, Y -5000..3000. Team starts are at opposite ends of the long X axis.

| Spawns | Area | Use |
| --- | --- | --- |
| 6 | North-east yard (+X end) | Alpha TDM start; respawns |
| 6 | South end beyond the trench (-X end) | Bravo TDM start; respawns |
| 11 | Lanes and yards around the buildings | FFA and TDM respawns |

The navigation bounds limit bots to the playable area; the player can still walk out onto Grid's open snow plane.

### Placement tooling

- `-CrosshairSmoke -CrosshairMatchLayoutProbe` logs a top-down collision height grid and large meshes for both maps (`MATCH_LAYOUT` lines).
- `-CrosshairSmoke -CrosshairMatchSmoke -CrosshairMatchSpawnAudit` reports every spawn that lacks ground, a clear capsule, navmesh or a full path to the map's central start, suggests the nearest valid spot, and prints a reachability map of the play space (`CROSSHAIR_MATCH_REACH`).
- The normal match smoke test fails if any spawn on either map is invalid.

## Bots

Bots carry their weapon in the soldier's right hand with the mannequin idle/walk/run blend (`ABP_Unarmed`); dedicated armed third-person animation is future art work. In TDM, Alpha wears the first three soldier designs and Bravo the last three.

Bots fire along their actual aim from eye height (the first-person camera only follows aim for whoever is viewing it, so world-view shooters use the controller rotation).

`ACrosshairBotController` uses the same character, weapons and damage as the player. It notices enemies in a 150 degree horizontal view cone at any height (or within 15 m in any direction), needs line of sight to the head or chest (no wall vision), waits its reaction time, then tracks with limited turn speed and a per-decision aim error before firing. Difficulty presets (`UCrosshairBotMatch`) are read live, so changing difficulty from the pause menu applies immediately. Bots roam using runtime navmesh inside each map's NavMeshBoundsVolume and fall back to direct movement if navigation is not ready.

## LAN multiplayer (foundation)

The Match tab also has **Host LAN match (same network)**, **Find LAN matches** and up to four **Join** rows.

- Uses Unreal's built-in `OnlineSubsystemNull`: LAN broadcast discovery on the local network only. No internet, account, cloud or paid service is involved.
- The host advertises a session and opens the selected map as a listen server with the selected mode. Match rules, scoring, spawns and bots run on the host and replicate to clients.
- Joining resolves the host's address and travels to it. Windows Firewall may ask to allow the game on private networks the first time.

Resolved on 2026-10-26 (see [LAN combat](#lan-combat-for-joined-players-2026-10-26)). Original limitation: gameplay code from practice is still host-authoritative without client RPCs, so a joined client can move and see the match, but client firing, lethals, reload/weapon switching and some HUD feedback are not yet sent to the host. Full LAN combat needs server RPCs for those actions, client-side hit feedback replication and multi-machine testing.

## Implementation map

| Piece | Where |
| --- | --- |
| Rules, spawn scoring, health, player/game state, FFA/TDM game mode | `Trickshot/CrosshairMatch.h/.cpp` (`CrosshairMatchRules`, `ACrosshairSpawnPoint`, `UCrosshairHealthComponent`, `ACrosshairMatchState`, `ACrosshairMatchGameMode`) |
| Bot AI | `Trickshot/CrosshairBot.h/.cpp` (`ACrosshairBotController`) |
| Offline start, LAN host/find/join, leave | `Trickshot/CrosshairMatchSession.h/.cpp` (`UCrosshairMatchSession`) |
| Saved options (mode, map, team size, limits) | `FCrosshairBotMatchOptions` in `CrosshairBotMatch.h` |
| Menus, scoreboard, kill feed, results | `CrosshairGame.cpp` (Match tab, `ACrosshairHUD::DrawMatch`) |
| Player/bot damage, ragdoll, world-view weapon carry | `CrosshairCharacter.cpp`, `CrosshairWeapon.cpp`, `CrosshairThrowable.cpp`, `CrosshairBallistics.cpp` |
| Spawn/navmesh placement | `Scripts/setup_match_spawns.py`; saved in `L_Nuketown`/`L_Grid` |

Engine-bundled additions: `NavigationSystem`, `OnlineSubsystem`, `OnlineSubsystemUtils` modules; `OnlineSubsystemNull` plugin; `DefaultPlatformService=Null` and runtime navmesh generation in `DefaultEngine.ini`.

## Local verification (2026-10-21)

- `Build.bat Project_CrosshairEditor Win64 Development` and `Build.bat Project_Crosshair Win64 Development`: succeeded.
- `UnrealEditor-Cmd <project> -unattended -NullRHI -nosound -ExecCmds="Automation RunTests Crosshair" -TestExit="Automation Test Queue Empty"`: 10 of 10 succeeded, including new `Crosshair.Match.Rules` (spawn scoring, friendly fire, limits, regeneration, team fill, option bounds, COD-map-only rule).
- `-game -CrosshairSmoke -CrosshairMatchSmoke -NullRHI`: `CROSSHAIR_MATCH_OK`. Covers Testing-map rejection, 8-player FFA on Nuketown at spawn points, no practice targets/replay recording, damage/regeneration/kill credit/kill feed/respawn, bots roaming and firing (bot kills observed), real weapon fire into bot hitboxes, kill limit and results, leaving, 6v6 TDM on Grid with team starts and friendly fire off, team scoring, scoreboard, offline pause, match menu, LAN host session as listen server and LAN search. Every spawn on both maps passes the ground/capsule/navmesh/reachability audit (21/21 and 23/23).
- Same command with `-CrosshairMatchVisual -RenderOffscreen -ResX=1280 -ResY=720`: `CROSSHAIR_MATCH_OK`; `Saved/Screenshots/Match*.png` inspected (HUD, kill feed, damage edges, soldier bots, results, TDM scoreboard).
- Two processes on one PC, `-CrosshairLanHost` then `-CrosshairLanClient`: client found "Free-for-all on Nuketown (1/8)", joined, received the replicated match (9 players) and spawned (`CROSSHAIR_LAN_CLIENT_OK`); host saw the second human (`CROSSHAIR_LAN_HOST_OK`).
- Regression: core `-CrosshairSmoke` (`CROSSHAIR_SMOKE_OK`), `-CrosshairFrontendSmoke`, `-CrosshairLethalSmoke`, `-CrosshairExpansionSmoke` all OK.

### Menus, difficulty and match replays (2026-10-21, second pass)

- The match smoke test now starts FFA through the menus (Main menu > Offline match vs bots > START MATCH) and additionally verifies: bots kill other bots and damage the player; a player sniper kill is saved to Replays with its label while the match continues; the saved clip plays from Replays after the match, following the player in first person with bots present.
- Difficulty duel on Grid's open snow (all other bots frozen; one bot with an assault rifle vs a durable passive target 12 m away, starting 60 degrees off-target; changing difficulty live): headless Easy first hit 2.70 s / 40 damage vs Veteran 0.37 s / 800 damage; rendered Easy 3.92 s / 160 vs Veteran 0.56 s / 680. The test also asserts the active bot's live tuning switches to the selected preset.
- The duel exposed and fixed a real bug: bots fired along their body facing with no pitch (their camera never received aim), so distant shots missed. They now fire along their aim.
- New automation test `Crosshair.Match.KillLabels`. Screenshots `MenuHome.png`, `MenuMatchSetup.png`, `MatchReplay.png` inspected.

Not verified: two separate PCs on a real LAN, Windows Firewall prompts, packaged/cooked builds, physical controller play, and match balance (bots and weapons are untuned; `Scripts/create_crosshair_assets.py` gives every weapon except the AR 100 body damage, so the SMG kills in one hit like the sniper; weapon tuning was not changed). Fixed on 2026-10-26 with per-weapon match damage; see [Weapon damage in matches](#weapon-damage-in-matches-2026-10-26).

## Soldier animation and LAN movement (2026-10-21)

Every third-person soldier body (bots, other LAN players, practice/match replays) uses `UCrosshairLocomotionAnimInstance`, a C++ animation instance with no Animation Blueprint. It blends the Unreal mannequin rifle set: rifle idle, 8-direction walk and jog cycles chosen by speed and travel direction relative to facing, a shared step phase so feet stay planted while blending, a faster cycle at sprint speed, a fall loop while airborne, and upper-body aim pitch. It reads only replicated movement (velocity, falling, aim pitch), so remote LAN players and replays animate exactly like local bots. The rifle animation folder is added to `DirectoriesToAlwaysCook` so packaged builds include it.

LAN movement: sprint and aim-down-sights change movement speed, and the host simulates every joined player, so the client now sends its sprint/aim intent to the host (`ServerSetMoveIntent`). The host moves joined players at the same speed they see locally instead of correcting them back to walk speed. Your own soldier body and its gear (helmet, pouches, pads) are hidden from your first-person view, live and in your replays, while other players still see them.

Verified: `Crosshair.Animation.LocomotionWeights` automation test; match smoke asserts moving bots play walk/jog cycles; two-process LAN test: host simulated the joined player's sprint at 680 cm/s, and the client saw the host's soldier at 681 cm/s with a full walk/jog pose (stepping weight 1.00). Crouch, reload, firing and equip are not yet animated on third-person bodies.

## LAN match setup (added 2026-10-25)

Escape / Start > **LAN** now opens a setup screen before Host / Find. Your choices are saved, and the offline match settings are separate.

| Row | What it does |
| --- | --- |
| Username | Your name in matches, offline and LAN. Press Enter / A to type it: letters, numbers, space and `- _ .`, up to 16 characters. Backspace deletes, and Enter or Escape saves. Left / Right cycles callsigns, which suits controller players. The first default is your Windows user name. |
| LAN mode | Free-for-all or Team Deathmatch. |
| LAN map | Nuketown, Grid or Space Station. |
| Bots | On fills the lobby with bots. Off makes it humans from your network only. |
| Bot difficulty | Easy / Regular / Hardened / Veteran. Shown only when bots are on. |
| Bot count / Team size | FFA: the number of bots. TDM: players per team, with bots filling empty slots. Shown only when bots are on. |
| Score limit, Time limit | As for offline matches. |
| Max players | Humans allowed, including the host (2–12). |
| Host LAN match / Find LAN matches | As before. |

How it works:

- **Hosting:** the host travels with a `?CrosshairLan` option, so `ACrosshairMatchGameMode` uses the LAN setup: mode, map, bots on/off, bot difficulty, counts, limits and max players. Without that option it uses the offline setup.
- **Player limit:** `PreLogin` refuses joins once the human limit is reached, with the message "This LAN match is full".
- **Listing:** the session advertises the host's username and the bots setting. Find shows, for example, "HostAce - Free-for-all on Space Station, no bots (1/4)".
- **Usernames:** every player's controller sends its own saved username to the server on arrival (`ServerSetPlayerName`), and the server cleans and applies it, so the scoreboard and kill feed show real names. Before this, LAN names were machine IDs from the Null subsystem.
- **Live difficulty:** bot difficulty is read live from the setup the running match uses, so changing it from the pause menu still applies mid-match.

Verified locally:

- **Two processes:** the host chose username HostAce, Free-for-all on Space Station, bots off and 4 players; the client was ClientBee.
  - The client's search listed "HostAce - Free-for-all on Space Station, no bots (1/4)".
  - The host ran Space Station with no bots and a 4-player limit.
  - Both machines saw both usernames.
  - The movement and animation checks passed.
- **Menu check** (in the graphics check): typing on the Username row with simulated keys produced the expected name, and the bot rows hid when Bots was turned off.

## Kill Confirmed and the LAN lobby (added 2026-10-26)

### Kill Confirmed

Kill Confirmed is a third match mode, next to Free-for-all and Team Deathmatch. It is available offline against bots and for LAN matches. On the Mode row, Left / Right cycles Free-for-all > Team Deathmatch > Kill Confirmed.

- **Teams and spawns:** Alpha vs Bravo, with the same team sizes, spawns and team start areas as Team Deathmatch. Friendly fire is off.
- **Dog tags:** when a player is killed by an enemy, they drop a dog tag where they die. If they die mid-air, the tag settles on the floor below. Suicides and falls drop no tag.
- **Confirm:** an enemy who walks over the tag confirms the kill, and their team scores 1. Kills alone never score.
- **Deny:** a teammate of the dead player who walks over the tag denies it, so nobody scores.
- **Collecting:** anyone alive walks within about 1 m of a tag to collect it. The host decides every pickup, and tags stay until someone collects them.
- **Seeing tags:** each player sees tags in their own colours. Enemy tags (to confirm) are gold and your team's tags (to deny) are blue. Tags spin and bob, and a diamond marker of the same colour shows tags in line of sight within 35 m.
- **Feedback:** "KILL CONFIRMED +1" or "KILL DENIED" flashes on screen.
- **Scoreboard:** shows Kills, Deaths, Conf (confirms) and Deny (denies). The top-left panel reads "First team to 65 confirmed kills".
- **Defaults:** COD defaults, 6v6 to 65 in 10 minutes. The score limit is saved separately from Team Deathmatch's and is adjustable from 10 to 200.
- **Bots:** when not fighting, a bot heads for the nearest tag within 25 m, whether it is an enemy tag to confirm or its own team's tag to deny. A bot that cannot reach a tag within 3 seconds gives up on it.

### LAN lobby

A hosted LAN match now opens in a lobby on the match map instead of starting at once. Offline matches still start straight away.

- **While waiting:** nobody has a soldier, no bots are added and the clock has not started. Each player's view shows their team's start area behind the lobby screen.
- **Lobby screen:** the left side shows the mode, map, score goal and setup, then these rows:
  - **Start match** (host only)
  - **Team** (Team Deathmatch and Kill Confirmed only)
  - **Weapon classes**
  - **Settings**
  - **Leave lobby**
- **Players:** the right side lists everyone who has joined, in Alpha and Bravo columns for team modes, with open slots and the host marked.
- **Escape:** pressing Escape on the class or settings pages returns to the lobby. The lobby cannot be closed until the match starts.
- **Teams:** new players are balanced onto the smaller team. The Team row (Left / Right, or Enter) switches teams. Each team holds up to half the human slots, rounded up (4 each for 8 players). Choosing a full team shows "Bravo is full."
- **Starting:** only the host's own machine can start the match. Starting adds bots for any open slots, spawns everyone in their team's start area, starts the clock and closes every player's lobby screen.
- **Leaving:** the host's Leave lobby closes the match for everyone.
- **Joining late:** players who join after the start spawn straight in, as before.

### Implementation map

| Piece | Where |
| --- | --- |
| Mode list, KC score limit, score goal text | `ECrosshairMatchMode`, `FCrosshairBotMatchOptions::ConfirmScoreLimit`, `UCrosshairBotMatch::IsTeamMode/NextMode/ScoreLimitFor/ScoreGoal` (`CrosshairBotMatch.h/.cpp`) |
| Tag, confirm/deny and team capacity rules | `CrosshairMatchRules::DropsTag/ConfirmsTag/TeamCapacity/CanJoinTeam` (`CrosshairMatch.h`) |
| Dog tag actor | `ACrosshairDogTag` (`CrosshairMatch.h/.cpp`) |
| Tag drop and pickup, lobby state, team requests, host start | `ACrosshairMatchGameMode::DropTag/CollectTag/RequestTeam/StartFromLobby/BeginMatch/PlayerCanRestart`; `ACrosshairMatchState::bInLobby/MaxHumans/LobbyInfo` |
| Bot tag seeking | `ACrosshairBotController::SeekTag` (`CrosshairBot.cpp`) |
| Lobby screen, team and start RPCs, notices | `ACrosshairPlayerController::ShowLobby/PlayerTick/ServerRequestTeam/ServerStartLobbyMatch/ClientNotify`, lobby branch of `ACrosshairHUD::DrawMenu` (`CrosshairGame.cpp`) |

### Local verification (2026-10-26)

- **Automation:** `Crosshair.Match.KillConfirmedAndLobby` (new) checks:
  - the mode set and cycling
  - the 65 default and its bounds
  - the `KC` travel option and score goal text
  - which deaths drop tags
  - confirm versus deny
  - team capacity
- **Match smoke** (`-CrosshairMatchSmoke`, headless and rendered), offline Kill Confirmed on Nuketown:
  - It is 6v6 to 65 with no lobby.
  - A kill credits the killer, scores nothing and drops the victim's tag.
  - The tag is coloured gold for the viewer.
  - Walking over the tag confirms it (Alpha +1, the player's Confirms 1, "KILL CONFIRMED").
  - Recovering a teammate's tag denies it (Denies 1, no score).
  - With bots unfrozen, bots collect tags, and each team's score equals its members' confirmed tags.
- **Match smoke**, hosted 2v2 Team Deathmatch LAN match:
  - It opens in the lobby: no soldiers, bots or clock.
  - The host's lobby offers Start and Team, and the Team row moves the host from Alpha to Bravo.
  - Start match adds 3 bots and spawns the host in the Bravo backyard with the clock running.
- **Two processes** (`-CrosshairLanHost` / `-CrosshairLanClient`), Kill Confirmed on Space Station, no bots, 4 players:
  - The client's search lists "HostAce - Kill Confirmed on Space Station, no bots (1/4)".
  - Both players wait in the lobby without soldiers.
  - The client lands on Bravo, its lobby offers Team but not Start, and it switches to Alpha. The host sees the switch.
  - The host starts the match. The client's lobby closes and its soldier spawns.
  - The movement and animation checks still pass (680 and 681 cm/s, stepping weight 1.00).
- **Screenshots inspected:** `MenuLobby.png`, `MatchDogTag.png`, `MatchKillConfirmed.png`, `MatchKillConfirmedScoreboard.png`, `MatchLobbyStarted.png`.

Not verified: two separate PCs, a full Kill Confirmed match to 65, and physical controller play. (When this section was written, only the host's shots could cause kills in a LAN match. Joined players can now fight too; see below.)

## LAN combat for joined players (2026-10-26)

Joined players now fight fully. The host still decides every hit, so the host and every joined player see the same result. Each action is sent to the host, which re-checks it against its own copy of the soldier (alive, not mid-throw, match running) before acting:

| Joined player's action | Sent to the host as | Comes back as |
| --- | --- | --- |
| Fire (press and release, automatic fire included) | `ServerFire` with the view the player had when they pulled the trigger | Shot sound and kick on everyone's screen; ammo; recoil on the shooter's own view; hit marker (`ClientTargetHit`) |
| Reload, switch weapon (1 / 2 / wheel / Y) | `ServerReload`, `ServerSelectWeaponSlot` | The replicated weapon and inventory |
| Lethal hold, release and change | `ServerLethal` | The thrown frag or tomahawk |
| Weapon class | `ServerApplyLoadout` at every spawn (`PawnClientRestart`) | The class they chose on their own machine, not the host's |
| Kill | (decided on the host) | Kill feed and scoreboard; the kill label and TRICKSHOT banner are worked out on the shooter's machine (`ClientMatchKill`) |

Fixes made along the way:

- **Sprint/aim intent:** the host no longer clears a joined player's sprint and aim intent when it resets their actions (for example when applying their class). Clearing it had left them simulated at walk speed.
- **Lethal changes:** changing lethal no longer writes the joined player's choice into the host's saved settings.

**Not covered:** kill replays are recorded only on the host's machine, so a joined player's kills are not saved to their Replays.

## Weapon damage in matches (2026-10-26)

The SMG and the sniper both had 100 body damage, so every SMG bullet killed. Matches now use a per-weapon match damage profile in the style of Black Ops core modes (100 health). The same values apply in Free-for-all, Team Deathmatch and Kill Confirmed, against players and bots alike.

Practice targets are unchanged: they still use the old per-weapon `BodyDamage` / `HeadDamage` values, so trickshot practice plays as before.

| Weapon | Damage up close → at range | Falloff | Head | Arms / legs | Shots to kill |
| --- | --- | --- | --- | --- | --- |
| Bolt-action sniper | 100 | none | x1.0 | x0.7 (70) | 1 to the head or torso at any range; 2 to an arm or leg |
| Assault rifle | 40 → 30 | 25 m → 50 m | x1.4 (56) | x1.0 | 3 close, 4 far; 2 headshots |
| SMG | 35 → 20 | 10 m → 25 m | x1.4 (49) | x1.0 | 3 close (fastest up close), 5 from 25 m; 3 headshots; never 1 |

- **Hit zones:** the `head` bone is a headshot. Upper arm, lower arm, hand, thigh, calf, foot and ball bones are limbs. Everything else, including the neck, is body.
- **Other scaling:** wallbangs still scale damage by the weapon's penetration factor. Frags (150 near the centre, down to 20% at the blast edge) and tomahawks (one-hit kill) are unchanged.
- **Where it lives:** `MatchMaxDamage`, `MatchMinDamage`, `MatchFalloffStart/End`, `MatchHeadMultiplier` and `MatchLimbMultiplier` on `UCrosshairWeaponDefinition`, applied by `CrosshairRules::MatchDamage` / `ZoneForBone` (`CrosshairData.h`).
- **Asset values:** set on `DA_Sniper`, `DA_AR` and `DA_SMG` by `Scripts/tune_match_damage.py`, and kept by `Scripts/create_crosshair_assets.py` on repair:

  ```
  UnrealEditor-Cmd <project> -run=pythonscript -script=Scripts/tune_match_damage.py -unattended -NullRHI -nosound
  ```

  The values are editable per weapon in the editor (category "Match damage").

These are Black Ops style approximations rather than exact Activision data, picked so each weapon has its COD role: the sniper for one-shot trickshots, the AR for range and the SMG for close range.

### Verification (2026-10-26)

- **`Crosshair.Weapon.MatchDamage` (automation):** loads the real weapon assets and checks hit zones, the sniper one-shot (and two to a limb), the AR's 3 / 4 shots and 2 headshots, that the SMG never one-shots at any range or zone (3 close, 5 at range), that the AR beats the SMG at 20 m, and that the SMG kills faster up close.
- **Match smoke, weapons against bots:** fires the player's real AR and SMG at a still bot 6 m away. The AR chest shot deals 40, the SMG chest shot 35 and the SMG headshot 49, and the bot survives each.
- **Two processes, LAN combat** (Kill Confirmed on Space Station, the host on Bravo and the joined player on Alpha):
  - The joined player's own SMG + AR class is applied on the host.
  - On an 8 m lane checked to be clear, the joined player fires with a simulated left mouse click. The host applies exactly 35 damage, credited to the joined player.
  - Ammo (30 → 29), the hit marker and recoil reach the joined player's machine.
  - The follow-up shot kills the host. The kill is credited to the joined player on both machines, and the host's dog tag drops.

## Bots look up, and Random map (2026-10-26)

### Bots check above and below them

- **The bug:** bots could not aim at anyone above or below them. `AAIController::UpdateControlRotation` runs every tick and resets an AI's pitch to level unless it is focused on a pawn ("don't pitch view unless looking at another pawn"). Our bots aim themselves without engine focus, so each frame their aim snapped back to level and the turn toward the target restarted from there. A bot could only tilt a degree or two per frame, so a player on a roof, a ledge or mid-jump overhead was almost impossible for it to hit.
- **The fix:** `ACrosshairBotController::UpdateControlRotation` keeps the bot's own aim while it is engaging and only turns the body to the aim's yaw. Roaming still looks level along the route.
- **Noticing players above:** the 150 degree view cone is now measured horizontally, so a player ahead of a bot is noticed at any height in line of sight. Before, a player more than 75 degrees above the bot's level view and more than 15 m away went unseen. The 15 m all-round awareness is unchanged, and bots still never see through walls.
- **What you see:** the soldier body still tilts its upper body by at most 50 degrees. Shots follow the full aim.

**Verified:** match smoke step 86 hovers the player 16 m from a bot at 75 degrees above it, under open sky, with the other bots moved out of sight. The bot noticed the player after 1.5 s, aimed up 76 degrees and hit for 40. With only the bot fix removed, the same step fails: the bot never targets the player.

### Random map

The Match map row (offline) and the LAN map row now include **Random**, after Space Station. Random is a saved choice and picks Nuketown, Grid or Space Station each time:

- **Offline:** rolled every time a match starts.
- **LAN:** rolled when the host advertises, so the LAN listing, the map everyone loads and the lobby all show the same real map.

The roll is `UCrosshairBotMatch::ResolveMap`, used in `UCrosshairMatchSession::HostLanMatch` and `TravelToMatch`.

**Verified:**
- `Crosshair.Match.RandomMap` (automation): checks cycling, saving, that Random always resolves to a real map, and that 200 rolls reach all three maps.
- Match smoke: the hosted LAN match uses Random. One run rolled Grid, loaded `L_Grid`, and the lobby named Grid.
