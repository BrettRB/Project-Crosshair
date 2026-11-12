# Security Audit: Project Crosshair

| | |
|---|---|
| **Date** | 2026-11-12 |
| **Commit audited** | `b303a6a` (branch `master`) |
| **Release inspected** | `Project Crosshair v1.3.1.zip` (file listing only) |
| **Engine** | Unreal Engine 5.8 |
| **Method** | Manual static review of C++ source, config, scripts, plugins, git history and packaged-build contents. No code was changed. |

---

## 1. Executive Summary

Crosshair is a single-player and peer-to-peer multiplayer FPS. The listen-server host is authoritative. Players connect over LAN (Null subsystem, IP sockets) or online through Epic Online Services (EOS Game Services, P2P NetDriver). There is no game backend, database, web service or payment flow, so the attack surface is narrow. Most of the risk is cheating, session access, and how release builds are packaged.

The core combat path is sound. Damage, fire rate, ammo, hit traces, lobby start and team capacity are all decided on the host. The user-map importer is defensively written.

| # | Finding | Risk |
|---|---|---|
| F1 | EOS Client Secret ships as a plaintext loose file in the distributed build | **High** (pending policy check) |
| F2 | Online "friend codes" are not access control: sessions are publicly listed and the code is never checked on connect | **Medium** |
| F3 | `ServerApplyLoadout` lets a client refill ammo and lethals at any time | **Medium** |
| F4 | `ServerSetStance` trusts the client completely, so dive launches can be chained | **Medium** |
| F5 | Public release is a Development build with full PDB symbols | **Medium** |
| F6 | Default player name is the OS account username, broadcast to others | **Low** |
| F7 | LAN matches have no authentication, encryption or kick/ban | **Low** |
| F8 | Client-authoritative aim and a fully trusted host (P2P trust model) | **Low** |
| F9 | Imported-map files are only fully validated at import time | **Low** |
| F10 | Reliable server RPCs have no rate limiting | **Low** |
| F11 | EOS sanctions disabled on sessions | **Low** |
| F12 | AndroidFileServer security token committed in `DefaultEngine.ini` | **Informational** |
| F13 | Third-party and editor-only components | **Informational** |

No Critical issues were found. No secrets were found in git history: `.gitignore` covers `CrosshairOnline.local.ini`, and a history search (`git log -S ClientSecret`) shows only empty template values.

---

## 2. Scope and Methodology

**In scope**
- `Source/Project_Crosshair/**` (game module, with focus on `Trickshot/`, where all networked gameplay lives)
- `Config/*.ini`, `Project_Crosshair.uproject`, `*.Build.cs`, `*.Target.cs`
- `Content/Online/` (credential loading) and `.gitignore`
- `Scripts/` (editor Python, PowerShell, shell packaging)
- `Plugins/` (RiderLink, VisualStudioTools)
- Git history (secret search)
- The v1.3.1 release archive (file names only; nothing extracted or opened)

**Out of scope / limitations**
- Blueprint and `.uasset`/`.umap` logic (binary assets, not reviewable as text).
- Unreal Engine and EOS SDK source.
- Dynamic testing: no fuzzing, packet tampering or live multiplayer attack was run. Findings F3 and F4 are from code reading and have not been reproduced in a live match.
- The EOS Developer Portal client policy could not be checked from the repository (this matters for F1).
- GitHub repository visibility could not be confirmed (`gh` not installed). If the repo is public, F12 and the interaction log's local paths are publicly visible.

**Risk rating**

| Rating | Meaning |
|---|---|
| Critical | Remote compromise of player machines or developer accounts, likely and easy |
| High | Significant abuse of the product or accounts, or credential exposure with unclear blast radius |
| Medium | Practical cheating or unauthorized access affecting other players, or hardening gaps that materially help attackers |
| Low | Limited impact, needs local access or unusual conditions, or privacy hygiene |
| Informational | No direct vulnerability; noted for awareness |

---

## 3. Architecture and Trust Boundaries

```
 Player PC (client)                     Player PC (host = listen server)
 +----------------------+   UE net      +------------------------------+
 | ACrosshairCharacter  | -- Server --> | Authoritative game mode,     |
 |  (autonomous proxy)  |    RPCs       | weapons, health, scoring     |
 |                      | <-- Rep ----- |                              |
 +----------------------+               +------------------------------+
          |  EOS P2P (online)  /  IpNetDriver (LAN, plaintext)  |
          v                                                      v
   EOS Sessions + Connect (Device ID login)  <-- ClientId/Secret from loose ini
```

Trust boundaries:
1. **Client to host RPCs.** Any `UFUNCTION(Server, ...)` can be called with arbitrary arguments by a modified client.
2. **Host to client.** The host is fully trusted by every joiner (inherent to listen servers).
3. **Game to EOS.** Authenticated with product credentials shipped on every player's PC.
4. **Local files to game.** User maps (`UserMaps/<GUID>/`), save game, replays.

---

## 4. Findings

### F1. EOS Client Secret shipped as a plaintext loose file. **High** (pending verification)

**Location:** `Config/DefaultGame.ini` (`+DirectoriesToAlwaysStageAsNonUFS=(Path="Online")`), `Source/Project_Crosshair/Trickshot/CrosshairMatchSession.cpp` `IsOnlineConfigured()`, and in the release `Project_Crosshair/Content/Online/CrosshairOnline.local.ini`.

**Description.** The packaging rule stages `Content/Online/` as loose, non-pak files. So `CrosshairOnline.local.ini`, which holds ProductId, SandboxId, DeploymentId, ClientId, **ClientSecret** and EncryptionKey, ships in plaintext in every build. The v1.3.1 zip contains it. Anyone who downloads the game can read the secret with a text editor.

Epic expects game clients to carry client credentials. The impact therefore depends on the **client policy** attached to that ClientId in the Developer Portal:
- **Peer2Peer / GameClient policy (minimal permissions):** an attacker can create sessions, search the `Crosshair:Online` bucket and log in Device-ID users under your product. This is nuisance-level abuse (session spam, fake listings, using your product quota).
- **TrustedServer or a custom policy with extra permissions:** an attacker gains server-level rights, e.g. writing sanctions, stats, leaderboards or player data, and can act as your backend.

The policy has not been confirmed (Interactions 93–94 left it as a follow-up). That is why this is rated High.

The `EncryptionKey` in the same file protects only the local EOS cache on that PC. Shipping it everywhere gives no confidentiality, which is acceptable for its purpose. (The local key was confirmed not to be the all-zero placeholder.)

**Likelihood:** High (trivial to read). **Impact:** Low to High depending on the policy.

**Recommendations**
1. In the Developer Portal, confirm the client uses the **Peer2Peer** (or **GameClient**) policy and that no extra actions are granted. Once confirmed, this finding drops to **Low**.
2. If the client ever had a broader policy, **create a new client, revoke the old one**, update the local ini and repackage.
3. Treat the shipped values as public. Never reuse this ClientId for any backend or tool.
4. Don't ship `CrosshairOnline.example.ini` in builds (it is also staged). It is harmless, but it tells people where the secret lives.
5. Optionally load the ini from the pak (UFS) instead of a loose file. This only raises the effort slightly and is not a fix on its own.

---

### F2. Online "friend codes" do not restrict who can join. **Medium**

**Location:** `CrosshairMatchSession.cpp` `CreateOnlineSession()`, `FindOnlineHost()`; `CrosshairMatchSession.h` `CrosshairOnlineCode`; `CrosshairMatch.cpp` `PreLogin()`.

**Description.** The UI presents the 5-character code as what lets friends join a private match. In practice:
- The session is created with `EOS_OSPF_PublicAdvertised` and `bAllowJoinInProgress=TRUE`.
- The code (`CROSSHAIRCODE`) and the host's ProductUserId (`CROSSHAIRHOST`) are **advertised attributes**. Any EOS client holding the product credentials (see F1) can search the `Crosshair:Online` bucket **without a code filter**, list every live match and read each host's ID.
- Joining is just `ClientTravel("[EOS:<host id>]")`. The host's `PreLogin()` only checks capacity. It never checks that the joiner knew the code.
- Codes come from `FMath::RandRange`, which is not a cryptographic RNG, over a 32^5 (about 33.5 million) space. That matters less, since enumeration (above) bypasses guessing entirely.
- The host has no in-game kick or ban.

**Likelihood:** Medium (needs a custom EOS client or a modified game, both easy given F1). **Impact:** Medium. Strangers can enter "private" lobbies, grief, or harvest host IDs.

**Recommendations**
1. Make the code a real secret checked by the host. Have the client send it in the travel URL (`[EOS:<id>]?CrosshairCode=XXXXX`) and reject in `PreLogin()` when `UGameplayStatics::ParseOption(Options, "CrosshairCode")` does not match `OnlineCode`.
2. Stop advertising the plaintext code. Advertise a hash such as `SHA256(code)` for lookup, so listing the bucket doesn't reveal joinable codes. Alternatively, use `EOS_OSPF_JoinViaPresence`/invite-only with a separate lookup.
3. Generate codes from OS crypto randomness (or `FGuid::NewGuid()` bits) rather than `FMath::RandRange`. Consider 6–8 characters.
4. Add host-side **kick** (and optionally a session ban list) to the lobby UI.

---

### F3. `ServerApplyLoadout` refills ammo and lethals on demand. **Medium**

**Location:** `CrosshairCharacter.cpp:533` `ServerApplyLoadout_Implementation`; `CrosshairWeapon.cpp:437` `ApplyLoadout()` → `ResetWeapons()` / `Lethals->Reset()`.

**Description.** The only checks are `Inventory->Weapons.Num() >= 2` and `IsInMatch()`. The slot indices are sanitized correctly, but `ApplyLoadout` also:
- calls `ResetWeapons()`, which sets every weapon's `Ammo` to a full magazine and cancels reloads, and
- calls `Lethals->Reset()`, which restores `Remaining = Capacity` grenades/tomahawks.

A modified client can call this RPC mid-fight (alive, mid-reload, after throwing) for **infinite ammo, no reloads and unlimited lethals**. The fire-rate limit (`NextShotAt`) still applies.

**Likelihood:** Medium (needs a modified client). **Impact:** Medium (unfair kills in other players' matches).

**Recommendation.** On the host, accept loadout changes only while the player is dead, in the lobby, or within a short window after spawning. Otherwise store the request and apply it on the next respawn, as most shooters do. Also rate-limit the RPC.

---

### F4. `ServerSetStance` is entirely client-trusted. **Medium**

**Location:** `CrosshairStance.cpp:27` `ServerSetStance_Implementation`.

**Description.** The host applies whatever stance and direction it receives. None of the client-side checks are repeated (`CanAct()`, `BlocksMoveInput()`, mantling/placing, sprinting for dive, `HasRoom()` for standing up). Any transition into `Dive` calls `LaunchCharacter(Direction*DiveForwardSpeed + Up*DiveUpSpeed)`. Alternating `Dive`/`Normal` RPCs therefore produces repeated launches, including mid-air: **sustained flight and speed hacks**. Other effects include changing stance while dead and standing up inside geometry.

**Likelihood:** Medium. **Impact:** Medium (movement cheats ruin trickshot/match integrity).

**Recommendation.** Repeat the gating on the host: require `CanAct()`, grounded and sprinting for Dive, a cooldown between dives, `HasRoom()` before Normal/Crouch, and a valid enum value. Optionally use `WithValidation` to disconnect clients that send impossible transitions.

---

### F5. Public release is a Development build with PDB symbols. **Medium**

**Location:** v1.3.1 zip: `Project_Crosshair/Binaries/Win64/Project_Crosshair.exe` (no `-Shipping` suffix) and `Project_Crosshair.pdb` (387 MB). `Scripts/package_mac.sh` also defaults to `Development`.

**Description.** Development builds keep the in-game console, debug/stat commands, and `ECVF_Cheat` console variables, which are stripped only in Shipping. They also keep extra logging and debug-drawing paths. Shipping the full PDB gives anyone function names, types and line information. This makes it much easier to write cheats (F3/F4-style client mods, memory editing) or to find crashes worth exploiting. Rendering cvars can also give a client wallhack-like visibility.

**Likelihood:** High (it's the published build). **Impact:** Medium (lowers the bar for every other cheat and reverse-engineering effort).

**Recommendations**
1. Package public releases as **Shipping** (`-clientconfig=Shipping`), and make Shipping the default in `package_mac.sh` and the Windows packaging steps.
2. Exclude `.pdb` from the distributed archive. Keep it privately for crash symbolication.
3. Use Development builds only for testing.

---

### F6. Default player name is the OS account username. **Low**

**Location:** `CrosshairReplaySubsystem.cpp:36` (`Settings.PlayerName = CleanPlayerName(FPlatformProcess::UserName())`); `CrosshairGame.cpp:61` (`ServerSetPlayerName`); `CrosshairMatchSession.cpp` `HostLanMatch()` (`CROSSHAIR_HOST` advertised).

**Description.** A new player's name defaults to their Windows/macOS account name. These are often a real first or last name. The name is sent to every host, shown to all players, recorded in replays, and **broadcast to the whole local network** in LAN session advertisements. Players never see a prompt about this.

**Likelihood:** High (default behavior). **Impact:** Low (personal-information disclosure).

**Recommendation.** Default to a random entry from the existing `UCrosshairBotMatch::Callsigns()` list (plus digits) and let the player opt in to a custom name.

---

### F7. LAN matches have no authentication or encryption. **Low**

**Location:** `Config/DefaultEngine.ini` (LAN uses `IpNetDriver`), `CrosshairMatchSession.cpp` `HostLanMatch()`, `CrosshairMatch.cpp` `PreLogin()`.

**Description.** LAN sessions are discoverable by anyone on the subnet and accept any client up to `MaxPlayers`. Game traffic is unencrypted UE netcode. On shared networks (dorms, campus Wi-Fi), others can join uninvited, and in principle anyone on-path can read or tamper with packets.

**Likelihood:** Low to Medium. **Impact:** Low (a game session only, no credentials on the wire).

**Recommendations.** Add an optional lobby password checked in `PreLogin()`. Document that LAN mode is for trusted networks. If needed, UE's `AESGCMHandlerComponent` packet handler can encrypt traffic with a key exchanged at login.

---

### F8. Client-authoritative aim and a fully trusted host. **Low** (accepted design risk)

**Location:** `CrosshairCharacter.cpp:516` `ServerFire_Implementation` (sets control rotation from client-supplied `AimRotation`); listen-server architecture.

**Description.** The host performs the hit trace, but along the aim the client reports (pitch is clamped). This is standard for UE shooters and is needed for good feel, but it allows aimbots and "silent aim". Separately, the host machine runs all authoritative logic, so a malicious host can do anything to its own match: god mode, fake kills, kicking. There is no anti-cheat.

**Likelihood:** Medium. **Impact:** Low for a casual peer-to-peer game.

**Recommendations.** Accept this for now and document it. Lightweight host-side heuristics are possible: angular-velocity limits between `ServerFire` calls, or flagging implausible snap-to-head rates. For competitive or ranked play you would need dedicated servers and/or EOS Anti-Cheat.

---

### F9. Imported-map files only fully validated at import time. **Low**

**Location:** `CrosshairMapLibrary.cpp`: `Refresh()`, `ACrosshairImportedMap::OnRep_MapId()`, `MapLibraryRelativeFile()`.

**Description.** The importer is careful: GUID-only folder IDs, a relative-path check against `..`, `:` and absolute paths, a 64 KB JSON cap, a 32 MB OBJ cap, vertex/UV/triangle limits, NaN/range checks, and PNG/JPG with a 16 MB cap for textures. But:
- After import, `Refresh()` and `OnRep_MapId()` re-read `map.json` from disk and do **not** re-check the mesh file size or the texture's extension and size. A map folder edited or dropped in by hand (e.g. a "map pack" shared between players) is loaded with weaker checks. `FImageUtils::ImportFileAsTexture2D` then decodes any file type and size.
- `MapLibraryRelativeFile` compares path strings and does not resolve symbolic links or NTFS junctions. A link inside a map folder can point outside it.

The replicated `MapId` cannot reach outside the GUID folders, so a remote host cannot pick arbitrary files. Exposure is limited to files the local user placed on their own disk.

**Likelihood:** Low. **Impact:** Low to Medium (crash or memory exhaustion; image-decoder bugs are the worst case).

**Recommendation.** Factor the size/extension checks into one `ValidateMapFiles()` used by `Import`, `Refresh` and `OnRep_MapId`. Reject symlinks and reparse points before reading.

---

### F10. No rate limiting on reliable server RPCs. **Low**

**Location:** `ServerSetPlayerName` (calls `GameMode::ChangeName`, replicated to all), `ServerRequestTeam`, `ServerLethal`, `ServerSetStance`, `ServerSetMoveIntent` (all `Reliable`).

**Description.** A modified client can flood these. UE disconnects a client whose reliable buffer overflows, which limits the damage. Before that happens, name/team spam generates replicated churn for every player and extra host CPU.

**Likelihood:** Low. **Impact:** Low (temporary lag/annoyance).

**Recommendation.** Add simple per-controller cooldowns (e.g. name change once per few seconds) and make high-frequency, state-like RPCs (`ServerSetMoveIntent`) `Unreliable`.

---

### F11. EOS sanctions disabled on sessions. **Low**

**Location:** `CrosshairMatchSession.cpp` `CreateOnlineSession()` (`Create.bSanctionsEnabled = EOS_FALSE`).

**Description.** Even if you later ban players through EOS Sanctions, they could still join sessions. There is currently no ban mechanism of any kind.

**Recommendation.** Leave it as is until a moderation process exists, then enable `bSanctionsEnabled` along with Sanctions in the client policy (the minimum permissions still apply, see F1).

---

### F12. AndroidFileServer security token in `DefaultEngine.ini`. **Informational**

**Location:** `Config/DefaultEngine.ini` (`[/Script/AndroidFileServerEditor.AndroidFileServerRuntimeSettings] SecurityToken=...`), present since the first commit.

**Description.** This token was generated by the UE template. It authenticates the Android File Server used for deploying to Android devices during development. Android is not a target for this project, and the settings are `bIncludeInShipping=False` and `ConnectionType=USBOnly`, so there is no practical exposure. It is still a committed credential-like value.

**Recommendation.** If Android deployment is ever used, regenerate the token in Project Settings → Android File Server and keep it out of source control. Otherwise, no action is needed.

---

### F13. Third-party and editor-only components. **Informational**

- **RiderLink plugin (JetBrains, 2026.2.3):** module types are `EditorNoCommandlet`, so it never ships in packaged games. Its IPC socket binds to and connects on `127.0.0.1` only. It bundles spdlog 1.10.0 and {fmt} 8.1.1. Keep it updated through Rider rather than by hand.
- **VisualStudioTools plugin (Microsoft):** editor-only. It contains upstream `azure-pipelines/*.yml`, which are inert in this GitHub repo and not CI for this project. Do not enable them.
- **Engine plugins:** `OnlineServicesEOSGS`/EOS SDK ship with the engine (`EOSSDK-Win64-Shipping.dll`). Keep the engine patched (5.8.x hotfixes) for SDK fixes.
- **Editor Python scripts (`Scripts/*.py`):** run only inside the editor. `create_combat_character.py` and `create_soldier_appearances.py` call `exec()` on another repo-local script. That is acceptable for trusted repo content, but it means a malicious change to `Scripts/` executes in the editor, so review script diffs like code. No network calls, subprocess use or downloads were found.
- **`Scripts/package_mac.sh`:** quotes all variables. `UE_ROOT` is taken from the environment, which is acceptable for a developer tool.
- **Git hygiene:** `Binaries/`, `Intermediate/`, `Saved/`, `DerivedDataCache/` and the EOS local ini are ignored, and history contains no committed secrets. `docs/ai-interaction-log.md` contains local absolute paths, including the Windows account name. That only matters if the repo is public.

---

## 5. Positive Security Controls Observed

- **Host-authoritative combat.** Fire is gated by `HasAuthority()`, `CanAct()`, ammo and `NextShotAt` on the host. Hit traces start from the host's copy of the pawn. Damage (`UCrosshairHealthComponent::ApplyDamage`, `ACrosshairDummy::TakeDamage`) only runs with authority.
- **Lobby authority.** `StartFromLobby` requires `IsLocalController()` (host only). `RequestTeam` validates the team value, lobby state and capacity. `PreLogin` enforces the player cap.
- **Input sanitization.** `CleanPlayerName` allow-lists characters and caps names at 16 characters. Loadouts pass through `CrosshairLoadoutRules::Sanitize`. Weapon slot is clamped. Saved settings are re-validated on load (`Validate`, `Clamp`).
- **User-map importer.** Bounded file sizes, element counts and coordinate ranges. GUID-named storage. Relative-path enforcement. Replicated map IDs resolve only through the local library.
- **Replay storage.** Stream names are internally generated GUIDs, with no user-controlled paths.
- **Secret management in source control.** EOS credentials are kept out of git, an example template exists, and no secrets appear in history.
- **Minimal external surface.** No custom web requests, databases, or command execution from game code.

---

## 6. Prioritized Remediation Plan

| Priority | Action | Findings |
|---|---|---|
| 1 (now) | Confirm the EOS client policy is Peer2Peer/GameClient with no extra permissions. Rotate the client if it was ever broader. | F1 |
| 2 | Ship releases as Shipping builds without `.pdb` files. Change the packaging default. | F5 |
| 3 | Validate the join code on the host in `PreLogin`, stop advertising the plaintext code, and add host kick. | F2 |
| 4 | Restrict `ServerApplyLoadout` to dead/lobby/spawn windows, and add host-side stance validation and a dive cooldown. | F3, F4 |
| 5 | Default player names to random callsigns. | F6 |
| 6 | Hardening backlog: LAN password, RPC cooldowns, shared map-file validation and symlink rejection, sanctions once moderation exists. | F7, F9, F10, F11 |
| — | Document the accepted P2P trust model. | F8 |

---

## 7. Suggested Verification After Fixes

- **F2:** Automation test where a client travels to a host with a wrong or missing `CrosshairCode` and `PreLogin` rejects it.
- **F3:** Automation test calling `ServerApplyLoadout` on a live, mid-match character with partial ammo; ammo and `Lethals->Remaining` must be unchanged until respawn.
- **F4:** Test that `ServerSetStance(Dive)` from a non-sprinting or airborne character, or one still in dive cooldown, does not call `LaunchCharacter` (velocity unchanged).
- **F5:** Confirm the packaged exe is `*-Shipping.exe`, that the archive contains no `.pdb`, and that the `~` console does not open.
- **F9:** Test that a hand-placed map folder with an oversized OBJ or a non-PNG/JPG texture is skipped by `Refresh()`.
