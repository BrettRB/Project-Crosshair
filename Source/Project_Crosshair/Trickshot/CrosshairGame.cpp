#include "CrosshairGame.h"
#include "CrosshairStartup.h"
#include "CrosshairMapLibrary.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "TimerManager.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerStart.h"
#include "Engine/DemoNetDriver.h"
#include "CrosshairCharacter.h"
#include "CrosshairData.h"
#include "CrosshairPractice.h"
#include "CrosshairWeapon.h"
#include "CrosshairThrowable.h"
#include "CrosshairReplaySubsystem.h"
#include "CrosshairMatch.h"
#include "CrosshairMatchSession.h"
#include "Engine/Canvas.h"
#include "Engine/GameInstance.h"
#include "InputKeyEventArgs.h"
#include "Camera/PlayerCameraManager.h"
#include "Kismet/GameplayStatics.h"
#include "CrosshairGraphics.h"
#include "Scalability.h"
#include "Kismet/KismetSystemLibrary.h"
#include "GameFramework/CharacterMovementComponent.h"

ACrosshairGameMode::ACrosshairGameMode()
{
	DefaultPawnClass = ACrosshairCharacter::StaticClass();
	PlayerControllerClass = ACrosshairPlayerController::StaticClass();
	ReplaySpectatorPlayerControllerClass = ACrosshairPlayerController::StaticClass();
	HUDClass = ACrosshairHUD::StaticClass();
}
void ACrosshairGameMode::InitGame(const FString& MapName,const FString& Options,FString& ErrorMessage)
{
 Super::InitGame(MapName,Options,ErrorMessage);
 auto* Library=GetGameInstance()->GetSubsystem<UCrosshairMapLibrary>();
 if (!MapName.Contains(TEXT("L_UserMap"))) return;
 const auto* Map=Library->Find(Library->ActiveMapId); if (!Map) return;
 auto* Actor=GetWorld()->SpawnActor<ACrosshairImportedMap>(); Actor->MapId=Map->Id; Actor->OnRep_MapId();
 for (TActorIterator<APlayerStart> It(GetWorld());It;++It) It->SetActorLocationAndRotation(Map->Spawn,FRotator(0,Map->Yaw,0));
}
ACrosshairPlayerController::ACrosshairPlayerController()
{
	bShouldPerformFullTickWhenPaused = true;
}
void ACrosshairPlayerController::BeginPlay()
{
	Super::BeginPlay();
	if (IsLocalController()) { ApplyGameplayInputMode(); GetGameInstance()->GetSubsystem<UCrosshairStartup>()->BeginPreparation(); }
 auto* Library=GetGameInstance()->GetSubsystem<UCrosshairMapLibrary>();
 // Matches show each player's own username (LAN clients send theirs to the host).
 if (IsLocalController() && !GetWorld()->GetDemoNetDriver()) ServerSetPlayerName(GetGameInstance()->GetSubsystem<UCrosshairReplaySubsystem>()->GetSettings().PlayerName);
 if (IsLocalController() && !Library->bHomeVisited && !FParse::Param(FCommandLine::Get(),TEXT("CrosshairSmoke")) && !GetWorld()->GetDemoNetDriver())
 { Library->bHomeVisited=true; GetWorldTimerManager().SetTimerForNextTick(FTimerDelegate::CreateWeakLambda(this,[this](){ ShowHome(); })); }
}
void ACrosshairPlayerController::ApplyGameplayInputMode()
{
	bShowMouseCursor = false;
	bEnableClickEvents = false;
	bEnableMouseOverEvents = false;
	FInputModeGameOnly Mode;
	Mode.SetConsumeCaptureMouseDown(false);
	SetInputMode(Mode);
}
void ACrosshairPlayerController::UpdateRotation(float DeltaTime)
{
	if (GetGameInstance()->GetSubsystem<UCrosshairStartup>()->IsPreparing()) { PendingMouse=HeldStick=FVector2D::ZeroVector; return; }
	Super::UpdateRotation(DeltaTime);
	if (auto* PracticePawn = Cast<ACrosshairCharacter>(GetPawn()))
		PracticePawn->ApplyLookInput(PendingMouse, HeldStick, DeltaTime);
	PendingMouse = FVector2D::ZeroVector;
}
void ACrosshairPlayerController::FlushPressedKeys()
{
	if (auto* PracticePawn = Cast<ACrosshairCharacter>(GetPawn())) PracticePawn->StopActions();
	PendingMouse = HeldStick = FVector2D::ZeroVector;
	ControllerCalibrationStep = 0;
	CalibrationAxis = FVector2D::ZeroVector;
	Super::FlushPressedKeys();
}
// Watching a match replay is a practice-side activity, not a live match.
bool ACrosshairPlayerController::IsInMatch() const
{ return ACrosshairMatchState::Get(this)!=nullptr && !GetGameInstance()->GetSubsystem<UCrosshairReplaySubsystem>()->IsPlayback(); }
FString ACrosshairPlayerController::GetRowHelp(int32 Row) const
{
 if (bHomeScreen)
  switch (Row)
  {
  case 0: return TEXT("Place targets, save your spot and record every hit.");
  case 1: return TEXT("Build two-weapon classes with a lethal.");
  case 2: return TEXT("Choose a practice map or import your own.");
  case 3: return TEXT("Controls, sensitivity and display.");
  case 4: return TEXT("Free-for-all or Team Deathmatch against bots.");
  case 5: return TEXT("Save settings and close Crosshair.");
  case 6: return TEXT("Host or join a match on your local network.");
  case 7: return TEXT("Watch saved trickshots and match kills.");
  default: return FString();
  }
 const int32 B=GetBotMatchRow();
 const auto& O=GetGameInstance()->GetSubsystem<UCrosshairReplaySubsystem>()->GetSettings().BotMatch;
 const int32 G=GetGraphicsRow();
 if (Row==G) return CrosshairGraphics::PresetHelp(CrosshairGraphics::CurrentPreset(GetGameInstance()->GetSubsystem<UCrosshairReplaySubsystem>()->GetSettings()));
 if (Row==G+1) return CrosshairGraphics::IsRayTracingSupported() ? TEXT("Hardware ray-traced lighting and reflections (Lumen). Costly on integrated graphics.") : TEXT("Needs a DirectX 12 GPU with ray tracing support.");
 if (Row==G+2) return TEXT("Render below native resolution for more frames per second; 100% is sharpest.");
 if (Row==G+3) return TEXT("Benchmark this PC and pick the preset that suits it.");
 const int32 Lan=GetLanRow();
 if (Row==Lan) return bEditingName ? TEXT("Type your username (letters, numbers, space, - _ .). Backspace deletes; Enter saves.") : TEXT("Your name in matches. Enter to type it; Left / Right picks a callsign.");
 if (Row==Lan+1) return TEXT("Free-for-all or Team Deathmatch for the match you host.");
 if (Row==Lan+2) return TEXT("Nuketown, Grid or Space Station.");
 if (Row==Lan+3) return TEXT("On: bots fill the lobby. Off: only players on your network.");
 if (Row==Lan+4) return TEXT("How well the bots react and aim.");
 if (Row==Lan+5) return TEXT("Free-for-all: bots added. Team Deathmatch: players per team; bots fill empty slots.");
 if (Row==Lan+6) return TEXT("Kills (or team kills) needed to win.");
 if (Row==Lan+7) return TEXT("The highest score wins when time runs out.");
 if (Row==Lan+8) return TEXT("Players from your network who can join, including you.");
 const bool bTeams=O.Mode==ECrosshairMatchMode::TeamDeathmatch;
 if (Row==B)
 {
  switch (O.Difficulty)
  {
  case ECrosshairBotDifficulty::Easy: return TEXT("Easy: slow reactions and wide aim error.");
  case ECrosshairBotDifficulty::Hardened: return TEXT("Hardened: quick reactions and tight aim.");
  case ECrosshairBotDifficulty::Veteran: return TEXT("Veteran: elite reactions and punishing aim.");
  default: return TEXT("Regular: balanced reactions and aim.");
  }
 }
 if (Row==B+1) return bTeams ? TEXT("Players on each team, including you. Bots fill the rest.") : TEXT("Bot opponents in the free-for-all.");
 if (Row==B+2) return bTeams ? TEXT("Team kills needed to win.") : TEXT("Kills needed to win.");
 if (Row==B+3) return TEXT("The highest score wins when time runs out.");
 if (Row==B+4) return TEXT("Free-for-all: everyone for themselves. Team Deathmatch: Alpha vs Bravo.");
 if (Row==B+5) return TEXT("Matches run on the match maps only: Nuketown, Grid and Space Station.");
 if (Row==B+6) return TEXT("Load the map and start. Every kill you land is saved to Replays.");
 if (Row==B+7) return TEXT("Advertise a match to players on this network. Windows may ask for firewall access.");
 if (Row==B+8) return TEXT("Search this network for hosted matches.");
 if (Row>B+8 && Row<B+MatchRowCount) return TEXT("Join this match.");
 if (Row>=GetFirstReplayRow()) return TEXT("Watch this replay. X / Delete removes it.");
 if (Row>=GetClassRow() && Row<GetClassRow()+4) return TEXT("Left / Right changes the selection for this class.");
 if (Row==GetClassRow()+4) return IsInMatch() ? TEXT("Equip this class on your next spawn.") : TEXT("Equip this class and restock.");
 if (Row==GetSandboxRow()) return TEXT("Allow placing, rotating and removing practice targets.");
 if (Row==GetHomeRow()) return IsInMatch() ? TEXT("Leave the match and return to the home screen.") : TEXT("Return to the home screen.");
 if (Row==GetImportRow()) return TEXT("Import an OBJ or map.json package from your computer.");
 if (Row==GetLethalRow()) return TEXT("Frag grenade or tomahawk.");
 if ((Row>=SkinRow && Row<=DesertRow) || (Row>=FirstReplayRow && Row<GetLethalRow())) return TEXT("Apply this finish to the current weapon.");
 switch (Row)
 {
 case 0: return TEXT("Close the menu and continue.");
 case 1: return TEXT("Right-stick horizontal turn speed.");
 case 2: return TEXT("Right-stick vertical turn speed.");
 case 3: return TEXT("Ignore small stick movement near the centre.");
 case 4: return TEXT("Higher values give finer control of small stick movements.");
 case 5: return TEXT("Mouse look speed.");
 case 6: return TEXT("Look speed multiplier while aiming down sights.");
 case 7: return TEXT("Horizontal field of view.");
 case 8: return TEXT("Off saves a replay of every hit. On keeps practising without replays.");
 case 9: return TEXT("Store your current position and view as the reset point.");
 case 10: return TEXT("Return to your saved position and reset the targets.");
 case 11: return TEXT("Remove the target under your crosshair.");
 case 12: return TEXT("Remove every placed target.");
 case WeaponRow: return TEXT("Cycle the equipped weapon.");
 case MapRow: return TEXT("Choose a practice map, then press Enter / A to load it.");
 case QuitRow: return TEXT("Save settings and close the game.");
 case InvertHorizontalRow: return TEXT("Reverse right-stick left/right look.");
 case InvertVerticalRow: return TEXT("Reverse right-stick up/down look.");
 case CalibrationRow: return TEXT("Run this if your controller looks the wrong way.");
 default: return FString();
 }
}
void ACrosshairPlayerController::ShowHome()
{
 if (!bMenuOpen) ToggleMenu();
 bHomeScreen=bFromHome=true; MenuSelection=0;
}
void ACrosshairPlayerController::StartPlaying()
{
 if (GetGameInstance()->GetSubsystem<UCrosshairStartup>()->IsPreparing()) return;
 bHomeScreen=bFromHome=false; if (bMenuOpen) ToggleMenu();
 if (auto* PracticePawn=Cast<ACrosshairCharacter>(GetPawn())) { PracticePawn->Inventory->ApplyClass(); PracticePawn->Attempt->ResetAttempt(); }
}
void ACrosshairPlayerController::ToggleMenu()
{
	FlushPressedKeys();
	if (bMenuOpen && bFromHome) { ShowHome(); return; }
 bHomeScreen=false;
 bMenuOpen = !bMenuOpen;
	MenuSelection = 0;
	MenuTab = 0;
	bEditingName = false;
	if (bMenuOpen)
		if (auto* PracticePawn = Cast<ACrosshairCharacter>(GetPawn())) PracticePawn->GetCharacterMovement()->StopMovementImmediately();
	if (bMenuOpen)
 { MapChoice=GetWorld()->GetOutermost()->GetName().Contains(TEXT("L_SpaceStation")) ? 3 : GetWorld()->GetOutermost()->GetName().Contains(TEXT("L_Nuketown")) ? 1 : GetWorld()->GetOutermost()->GetName().Contains(TEXT("L_Grid")) ? 2 : 0;
   const auto* Library=GetGameInstance()->GetSubsystem<UCrosshairMapLibrary>();
   for (int32 i=0;i<Library->Maps.Num();++i) if (Library->Maps[i].Id==Library->ActiveMapId) MapChoice=i+UCrosshairMapLibrary::BuiltinMapCount;
   EditingClass=GetGameInstance()->GetSubsystem<UCrosshairReplaySubsystem>()->GetSettings().ActiveClass;
 }
	// Keep the world running: a successful replay must finish writing while the menu is open.
	if (bMenuOpen)
	{
		FInputModeGameAndUI Mode;
		Mode.SetHideCursorDuringCapture(false);
		Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
		SetInputMode(Mode);
	}
	else ApplyGameplayInputMode();
	if (bMenuOpen)
	{
		bShowMouseCursor = true;
		bEnableClickEvents = true;
		bEnableMouseOverEvents = true;
	}
	if (!bMenuOpen) GetGameInstance()->GetSubsystem<UCrosshairReplaySubsystem>()->SaveSettings();
	// Offline matches pause behind the menu; a LAN host keeps the match running for others.
	if (IsInMatch() && GetNetMode()==NM_Standalone) SetPause(bMenuOpen);
}
bool ACrosshairPlayerController::InputKey(const FInputKeyEventArgs& Params)
{
	if (GetGameInstance()->GetSubsystem<UCrosshairStartup>()->IsPreparing()) return true;
	const FKey Key = Params.Key;
	const bool Pressed = Params.Event == IE_Pressed;
	// Typing a username: every key goes to the name until Enter / Escape / A / B finishes.
	if (bEditingName && bMenuOpen) { if (Pressed || Params.Event == IE_Repeat) EditNameKey(Key); return true; }
	if (Pressed && (Key == EKeys::Escape || Key == EKeys::Gamepad_Special_Right)) { ToggleMenu(); return true; }
	if (bMenuOpen && ControllerCalibrationStep && Params.Event == IE_Axis && (Key == EKeys::Gamepad_RightX || Key == EKeys::Gamepad_RightY))
	{
		CalibrateAxis(Key, Params.AmountDepressed);
		return true;
	}
	if (!bMenuOpen)
	{
		if (const auto* Match=ACrosshairMatchState::Get(this))
		{
			if (Key==EKeys::Tab || Key==EKeys::Gamepad_Special_Left)
			{ if (Pressed) bScoreboardHeld=true; else if (Params.Event==IE_Released) bScoreboardHeld=false; return true; }
			if (Match->bMatchOver && Pressed && (Key==EKeys::Enter || Key==EKeys::Gamepad_FaceButton_Bottom))
			{ GetGameInstance()->GetSubsystem<UCrosshairMatchSession>()->LeaveMatch(); return true; }
		}
		if (auto* PracticePawn=Cast<ACrosshairCharacter>(GetPawn()))
		{
			if (Pressed && (Key==EKeys::One || Key==EKeys::Two || Key==EKeys::MouseScrollUp || Key==EKeys::MouseScrollDown))
            { if (PracticePawn->CanAct() && !PracticePawn->Lethals->IsBusy()) { if (Key==EKeys::One || Key==EKeys::Two) PracticePawn->SelectWeaponSlot(Key==EKeys::One ? 0 : 1); else PracticePawn->SelectWeaponSlot(-1); } return true; }
            if (Key==EKeys::Gamepad_LeftThumbstick) { if (Pressed) PracticePawn->ControllerSprintPressed(); return true; }
            if (Key==EKeys::G || Key==EKeys::Gamepad_RightShoulder)
			{
				if (Pressed) PracticePawn->Lethals->Prepare();
				else if (Params.Event==IE_Released) PracticePawn->Lethals->Throw();
				return true;
			}
			if ((Key==EKeys::F || Key==EKeys::Gamepad_LeftShoulder) && Pressed)
			{ if (PracticePawn->CanAct()) PracticePawn->Lethals->Cycle(); return true; }
		}
		// Read the physical axis before Enhanced Input/legacy modifiers, keeping both
		// input devices active. Mouse is a displacement; the stick is a held rate.
		if (Params.Event == IE_Axis && FMath::IsFinite(Params.AmountDepressed))
		{
			if (Key == EKeys::MouseX) { PendingMouse.X += Params.AmountDepressed; return true; }
			if (Key == EKeys::MouseY) { PendingMouse.Y += Params.AmountDepressed; return true; }
			if (Key == EKeys::Gamepad_RightX) { HeldStick.X = FMath::Clamp(Params.AmountDepressed, -1.f, 1.f); return true; }
			if (Key == EKeys::Gamepad_RightY) { HeldStick.Y = FMath::Clamp(Params.AmountDepressed, -1.f, 1.f); return true; }
		}
		return Super::InputKey(Params);
	}
	if (Key == EKeys::LeftMouseButton) return Super::InputKey(Params);
	if (!Pressed) return true;
	const TArray<int32> Visible = GetVisibleMenuRows();
	const int32 Count = Visible.Num();
	const int32 Index = FMath::Max(0, Visible.IndexOfByKey(MenuSelection));
	if (Key == EKeys::Tab || Key == EKeys::Gamepad_RightShoulder || Key == EKeys::E) SetMenuTab(MenuTab + 1);
	else if (Key == EKeys::Gamepad_LeftShoulder || Key == EKeys::Q) SetMenuTab(MenuTab - 1);
	else if (Key == EKeys::Up || Key == EKeys::Gamepad_DPad_Up || Key==EKeys::MouseScrollUp) MenuSelection = Visible[(Index + Count - 1) % Count];
	else if (Key == EKeys::Down || Key == EKeys::Gamepad_DPad_Down || Key==EKeys::MouseScrollDown) MenuSelection = Visible[(Index + 1) % Count];
	else if (Key == EKeys::Left || Key == EKeys::Gamepad_DPad_Left) AdjustSelection(-1);
	else if (Key == EKeys::Right || Key == EKeys::Gamepad_DPad_Right) AdjustSelection(1);
	else if (Key == EKeys::Enter || Key == EKeys::Gamepad_FaceButton_Bottom) ActivateSelection();
	else if (Key == EKeys::Gamepad_FaceButton_Right) ToggleMenu();
	else if ((Key == EKeys::Delete || Key == EKeys::Gamepad_FaceButton_Left) && MenuSelection >= GetFirstReplayRow())
		GetGameInstance()->GetSubsystem<UCrosshairReplaySubsystem>()->DeleteSaved(MenuSelection - GetFirstReplayRow());
	return true;
}
int32 ACrosshairPlayerController::GetSkinCount() const
{
	const auto* PracticePawn = Cast<ACrosshairCharacter>(GetPawn());
	const auto* Weapon = PracticePawn ? PracticePawn->Inventory->GetCurrent() : nullptr;
	return Weapon && Weapon->Definition ? Weapon->Definition->Skins.Num() : 0;
}
int32 ACrosshairPlayerController::GetLethalRow() const { return FirstReplayRow + FMath::Max(0, GetSkinCount()-2); }
int32 ACrosshairPlayerController::GetFirstReplayRow() const { return GetLanRow()+LanRowCount; }
TArray<FString> ACrosshairPlayerController::GetMenuRows() const
{
	if (bHomeScreen) return {TEXT("Trickshot practice"),TEXT("Weapon classes"),TEXT("Maps / Import"),TEXT("Settings"),TEXT("Offline match vs bots"),TEXT("Quit to Desktop"),TEXT("LAN match"),TEXT("Replays")};
 auto* Replay = GetGameInstance()->GetSubsystem<UCrosshairReplaySubsystem>();
 const FCrosshairSettings& S = Replay->GetSettings();
	TArray<FString> Rows = {
		Replay->IsPlayback() ? TEXT("Return to practice") : TEXT("Resume"),
		FString::Printf(TEXT("Controller yaw: %.0f deg/s"), S.StickYawSpeed),
		FString::Printf(TEXT("Controller pitch: %.0f deg/s"), S.StickPitchSpeed),
		FString::Printf(TEXT("Stick dead zone: %.2f"), S.StickDeadZone),
		FString::Printf(TEXT("Stick curve: %.2f"), S.StickExponent),
		FString::Printf(TEXT("Mouse sensitivity: %.2f"), S.MouseSensitivity),
		FString::Printf(TEXT("Aim sensitivity: %.2f"), S.AimSensitivity),
		FString::Printf(TEXT("Field of view: %.0f"), S.FieldOfView),
		FString::Printf(TEXT("Continuous practice: %s"), S.bContinuousPractice ? TEXT("On (no auto replay)") : TEXT("Off (save hits)")),
		TEXT("Save position   [K / D-pad Right]"), TEXT("Reset to saved position   [T / D-pad Down]"), TEXT("Remove aimed target"), TEXT("Clear all targets")
	};
	const auto* PracticePawn = Cast<ACrosshairCharacter>(GetPawn());
	const auto* Current = PracticePawn ? PracticePawn->Inventory->GetCurrent() : nullptr;
	Rows.Add(Replay->IsPlayback() ? TEXT("Weapon: unavailable during replay") :
		TEXT("Weapon: ") + (Current && Current->Definition ? Current->Definition->DisplayName.ToString() : TEXT("Unavailable")));
	Rows.Add(Replay->IsPlayback() || Replay->IsFinishing() ? TEXT("Map: unavailable during replay/save") :
		TEXT("Map: ")+GetGameInstance()->GetSubsystem<UCrosshairMapLibrary>()->MapName(MapChoice)+TEXT(" (Enter / A to load)"));
	Rows.Add(TEXT("Quit to Desktop"));
	Rows.Add(FString::Printf(TEXT("Invert controller horizontal: %s"), S.bInvertControllerHorizontal ? TEXT("On") : TEXT("Off")));
	Rows.Add(FString::Printf(TEXT("Invert controller vertical: %s"), S.bInvertControllerVertical ? TEXT("On") : TEXT("Off")));
		for (int32 Index=-1; Index<2; ++Index)
	{
		const bool Available = Current && Current->Definition && (Index < 0 || Current->Definition->Skins.IsValidIndex(Index));
		const FName Id = Available && Index >= 0 ? Current->Definition->Skins[Index].Id : NAME_None;
		const FString Name = Index < 0 ? TEXT("Original") : (Available ? Current->Definition->Skins[Index].DisplayName.ToString() : TEXT("Unavailable"));
		Rows.Add(TEXT("Camo: ") + Name + (Available && Current->SkinId == Id ? TEXT("  [Equipped]") : TEXT("")));
	}
	static const TCHAR* CalibrationLabels[] = {TEXT("Calibrate controller direction (Enter / A)"), TEXT("Push RIGHT stick RIGHT"), TEXT("Release right stick to center"), TEXT("Push RIGHT stick UP"), TEXT("Release right stick to save")};
	Rows.Add(CalibrationLabels[ControllerCalibrationStep]);
	for (int32 Index=2; Current && Current->Definition && Index<Current->Definition->Skins.Num(); ++Index)
	{
		const auto& Skin = Current->Definition->Skins[Index];
		Rows.Add(TEXT("Camo: ")+Skin.DisplayName.ToString()+(Current->SkinId == Skin.Id ? TEXT("  [Equipped]") : TEXT("")));
	}
	Rows.Add(TEXT("Lethal: ")+(PracticePawn ? PracticePawn->Lethals->GetName() : FString(TEXT("Unavailable during replay"))));
	const auto& C=S.Classes[FMath::Clamp(EditingClass,0,S.Classes.Num()-1)];
 auto WeaponName=[&](int32 Index) { const auto* W=PracticePawn && PracticePawn->Inventory->Weapons.IsValidIndex(Index) ? PracticePawn->Inventory->Weapons[Index].Get() : nullptr; return W && W->Definition ? W->Definition->DisplayName.ToString() : FString(Index==0 ? TEXT("Sniper") : Index==1 ? TEXT("AR") : TEXT("SMG")); };
 Rows.Add(TEXT("Class: ")+C.Name+(S.ActiveClass==EditingClass ? TEXT(" [Active]") : TEXT("")));
 Rows.Add(TEXT("Primary: ")+WeaponName(C.Primary));
 Rows.Add(TEXT("Secondary: ")+WeaponName(C.Secondary));
 Rows.Add(TEXT("Lethal: ")+FString(C.Lethal==ECrosshairLethalType::Frag ? TEXT("Frag grenade") : TEXT("Tomahawk")));
 Rows.Add(TEXT("Equip this class"));
 Rows.Add(FString::Printf(TEXT("Sandbox target editing: %s"),S.bSandboxTargets ? TEXT("On") : TEXT("Off")));
 Rows.Add(IsInMatch() ? TEXT("Leave match") : TEXT("Home")); Rows.Add(TEXT("Import map from local file"));
 const auto& O=S.BotMatch; const bool bTeams=O.Mode==ECrosshairMatchMode::TeamDeathmatch;
 const auto* Session=GetGameInstance()->GetSubsystem<UCrosshairMatchSession>();
 Rows.Add(TEXT("Bot difficulty: ")+UCrosshairBotMatch::DifficultyName(O.Difficulty));
 Rows.Add(bTeams ? FString::Printf(TEXT("Team size: %dv%d"),O.TeamSize,O.TeamSize) : FString::Printf(TEXT("Bot count: %d"),O.BotCount));
 Rows.Add(FString::Printf(TEXT("Score limit: %d"),bTeams ? O.TeamScoreLimit : O.ScoreLimit));
 Rows.Add(FString::Printf(TEXT("Time limit: %d minutes"),O.TimeLimitMinutes));
 Rows.Add(TEXT("Mode: ")+UCrosshairBotMatch::ModeName(O.Mode));
 Rows.Add(TEXT("Match map: ")+UCrosshairBotMatch::MapName(O.Map));
 Rows.Add(IsInMatch() ? TEXT("Restart with these settings") : TEXT("Start match"));
 Rows.Add(Session->HasLanSession() ? TEXT("Hosting LAN match") : TEXT("Host LAN match (same network)"));
 Rows.Add(Session->bSearching ? TEXT("Searching this network...") : TEXT("Find LAN matches"));
 for (int32 i=0;i<UCrosshairMatchSession::MaxLanResults;++i) Rows.Add(Session->LanResults.IsValidIndex(i) ? TEXT("Join: ")+Session->LanResults[i] : FString(TEXT("No LAN match")));
 const int32 Preset=CrosshairGraphics::CurrentPreset(S);
 Rows.Add(TEXT("Graphics quality: ")+FString(CrosshairGraphics::PresetName(Preset))+(S.bGraphicsConfigured ? TEXT("") : TEXT(" (default)")));
 Rows.Add(!CrosshairGraphics::IsRayTracingSupported() ? FString(TEXT("Ray tracing: not supported on this GPU")) : FString::Printf(TEXT("Ray tracing: %s"),CrosshairGraphics::IsRayTracingActive() ? TEXT("On") : TEXT("Off")));
 Rows.Add(FString::Printf(TEXT("Resolution scale: %d%%"),S.bGraphicsConfigured ? S.ResolutionScale : FMath::RoundToInt(Scalability::GetQualityLevels().ResolutionQuality)));
 Rows.Add(TEXT("Detect best settings for this PC"));
 {
  const auto& L=S.Lan; const auto& M=L.Match; const bool bLanTeams=M.Mode==ECrosshairMatchMode::TeamDeathmatch;
  Rows.Add(bEditingName ? TEXT("Username: ")+S.PlayerName+TEXT("_   (type, Enter to finish)") : TEXT("Username: ")+UCrosshairBotMatch::CleanPlayerName(S.PlayerName));
  Rows.Add(TEXT("LAN mode: ")+UCrosshairBotMatch::ModeName(M.Mode));
  Rows.Add(TEXT("LAN map: ")+UCrosshairBotMatch::MapName(M.Map));
  Rows.Add(FString::Printf(TEXT("Bots: %s"),M.bBots ? TEXT("On") : TEXT("Off")));
  Rows.Add(TEXT("Bot difficulty: ")+UCrosshairBotMatch::DifficultyName(M.Difficulty));
  Rows.Add(bLanTeams ? FString::Printf(TEXT("Team size: %dv%d (bots fill open slots)"),M.TeamSize,M.TeamSize) : FString::Printf(TEXT("Bot count: %d"),M.BotCount));
  Rows.Add(FString::Printf(TEXT("Score limit: %d"),bLanTeams ? M.TeamScoreLimit : M.ScoreLimit));
  Rows.Add(FString::Printf(TEXT("Time limit: %d minutes"),M.TimeLimitMinutes));
  Rows.Add(FString::Printf(TEXT("Max players: %d"),L.MaxPlayers));
 }
 for (const auto& Entry : Replay->GetReplays())
 {
  const FString MapLabel=Entry.Map.Contains(TEXT("L_SpaceStation")) ? TEXT("Space Station") : Entry.Map.Contains(TEXT("L_Nuketown")) ? TEXT("Nuketown") : Entry.Map.Contains(TEXT("L_Grid")) ? TEXT("Grid") : Entry.Map.Contains(TEXT("L_UserMap")) ? TEXT("Imported map") : TEXT("Testing Map");
  Rows.Add(FString::Printf(TEXT("%s  |  %s  |  %s"),Entry.Label.IsEmpty() ? TEXT("Practice hit") : *Entry.Label,*MapLabel,*Entry.RecordedAt));
 }
	return Rows;
}
void ACrosshairPlayerController::CalibrateAxis(FKey Key, float Value)
{
	if (!FMath::IsFinite(Value)) return;
	if (Key == EKeys::Gamepad_RightX) CalibrationAxis.X = Value;
	else CalibrationAxis.Y = Value;
	if (ControllerCalibrationStep == 1 && FMath::Abs(CalibrationAxis.X) > .65f && FMath::Abs(CalibrationAxis.Y) < .3f)
	{
		CalibrationDirection.X = FMath::Sign(CalibrationAxis.X);
		ControllerCalibrationStep = 2;
	}
	else if (ControllerCalibrationStep == 2 && CalibrationAxis.Size() < .2f) ControllerCalibrationStep = 3;
	else if (ControllerCalibrationStep == 3 && FMath::Abs(CalibrationAxis.Y) > .65f && FMath::Abs(CalibrationAxis.X) < .3f)
	{
		CalibrationDirection.Y = FMath::Sign(CalibrationAxis.Y);
		ControllerCalibrationStep = 4;
	}
	else if (ControllerCalibrationStep == 4 && CalibrationAxis.Size() < .2f)
	{
		auto* Replay = GetGameInstance()->GetSubsystem<UCrosshairReplaySubsystem>();
		auto& Settings = Replay->GetSettings();
		Settings.ControllerAxisDirection = CalibrationDirection;
		Settings.bInvertControllerHorizontal = Settings.bInvertControllerVertical = false;
		Replay->SaveSettings();
		ControllerCalibrationStep = 0;
		if (auto* PracticePawn = Cast<ACrosshairCharacter>(GetPawn())) PracticePawn->Notify(TEXT("Controller direction saved: right turns right, up looks up."));
	}
}
TArray<int32> ACrosshairPlayerController::GetVisibleMenuRows() const
{
	// Home order: play modes first, then loadout/options. Row indices stay stable for input and tests.
	if (bHomeScreen) return {0,4,6,1,2,7,3,5};
	const int32 B=GetBotMatchRow();
	if (MenuTab==8) return {B+4,B+5,B+0,B+1,B+2,B+3,B+6};
	if (MenuTab==9)
	{
		// Setup first (bot rows only when bots are on), then Host / Find and any joinable matches.
		const int32 L=GetLanRow();
		TArray<int32> Rows={L,L+1,L+2,L+3};
		if (GetGameInstance()->GetSubsystem<UCrosshairReplaySubsystem>()->GetSettings().Lan.Match.bBots) Rows.Append({L+4,L+5});
		Rows.Append({L+6,L+7,L+8,B+7,B+8});
		const auto* Session=GetGameInstance()->GetSubsystem<UCrosshairMatchSession>();
		for (int32 i=0;i<Session->LanResults.Num() && i<UCrosshairMatchSession::MaxLanResults;++i) Rows.Add(B+9+i);
		return Rows;
	}
	// Practice tools (targets, replays, map travel, save/reset) are unavailable in a match.
	if (IsTabLockedInMatch(MenuTab)) return {0,GetHomeRow()};
	if (IsInMatch() && MenuTab==0) return {0,GetClassRow(),GetHomeRow(),QuitRow};
 switch (MenuTab)
 {
	case 1: return {1, 2, InvertHorizontalRow, InvertVerticalRow, CalibrationRow, 3, 4, 5, 6, GetSandboxRow()};
	case 2: { const int32 G=GetGraphicsRow(); return {G,G+1,G+2,G+3,7}; }
	case 3: return {GetSandboxRow(),11,12};
 case 6: return {GetClassRow(),GetClassRow()+1,GetClassRow()+2,GetClassRow()+3,GetClassRow()+4};
 case 7: return {MapRow,GetImportRow(),GetHomeRow()};
	case 5:
	{
		TArray<int32> Rows = {WeaponRow, SkinRow, WoodlandRow, DesertRow};
		for (int32 i=FirstReplayRow; i<GetLethalRow(); ++i) Rows.Add(i);
		return Rows;
	}
	case 4:
	{
		TArray<int32> Rows = {0};
		for (int32 i = GetFirstReplayRow(); i < GetMenuRows().Num(); ++i) Rows.Add(i);
		return Rows;
	}
	default: return {0, 9, 10, 8, GetClassRow(), GetLethalRow(), MapRow,GetHomeRow(), QuitRow};
	}
}
void ACrosshairPlayerController::SetMenuTab(int32 Tab)
{
	bHomeScreen=false;
 ControllerCalibrationStep = 0;
	MenuTab = (Tab + MenuTabCount) % MenuTabCount;
	MenuSelection = GetVisibleMenuRows()[0];
}
void ACrosshairPlayerController::ActivateMenuRow(int32 Row)
{
 if (GetGameInstance()->GetSubsystem<UCrosshairStartup>()->IsPreparing()) return;
	if (!bMenuOpen || !GetVisibleMenuRows().Contains(Row)) return;
	MenuSelection = Row;
	ActivateSelection();
}
void ACrosshairPlayerController::AdjustMenuRow(int32 Row, int32 Direction)
{
	if (!bMenuOpen || !GetVisibleMenuRows().Contains(Row)) return;
	MenuSelection = Row;
	AdjustSelection(Direction);
}
void ACrosshairPlayerController::AdjustSelection(int32 Direction)
{
 if (bHomeScreen) return;
 if (MenuSelection>=GetGraphicsRow() && MenuSelection<GetGraphicsRow()+GraphicsRowCount) { AdjustGraphics(MenuSelection-GetGraphicsRow(),Direction); return; }
 if (MenuSelection>=GetLanRow() && MenuSelection<GetLanRow()+LanRowCount) { AdjustLan(MenuSelection-GetLanRow(),Direction); return; }
 auto* ClassReplay=GetGameInstance()->GetSubsystem<UCrosshairReplaySubsystem>(); auto& Settings=ClassReplay->GetSettings();
 const int32 BotOffset=MenuSelection-GetBotMatchRow();
 if (BotOffset>=0 && BotOffset<MatchRowCount)
 {
  if (ClassReplay->IsPlayback() || ClassReplay->IsFinishing() || BotOffset>5) return;
  auto& O=Settings.BotMatch; const bool bTeams=O.Mode==ECrosshairMatchMode::TeamDeathmatch;
  if (BotOffset==0) { GetGameInstance()->GetSubsystem<UCrosshairBotMatch>()->SetDifficulty(ECrosshairBotDifficulty((int32(O.Difficulty)+Direction+4)%4)); return; }
  if (BotOffset==1) (bTeams ? O.TeamSize : O.BotCount)+=Direction;
  if (BotOffset==2) (bTeams ? O.TeamScoreLimit : O.ScoreLimit)+=Direction*5;
  if (BotOffset==3) O.TimeLimitMinutes+=Direction;
  if (BotOffset==4) O.Mode=bTeams ? ECrosshairMatchMode::FreeForAll : ECrosshairMatchMode::TeamDeathmatch;
  if (BotOffset==5) O.Map=ECrosshairMatchMap((uint8(O.Map)+(Direction<0 ? 2 : 1))%3);
  UCrosshairBotMatch::Validate(O); ClassReplay->SaveSettings();
  return;
 }
 const int32 Offset=MenuSelection-GetClassRow();
 if (Offset>=0 && Offset<4)
 {
  if (ClassReplay->IsPlayback() || ClassReplay->IsFinishing()) return;
  if (Offset==0) EditingClass=(EditingClass+Direction+Settings.Classes.Num())%Settings.Classes.Num();
  else { auto& C=Settings.Classes[EditingClass];
   if (Offset==1) { C.Primary=(C.Primary+Direction+3)%3; if (C.Primary==C.Secondary) C.Secondary=(C.Primary+1)%3; }
   if (Offset==2) { do { C.Secondary=(C.Secondary+Direction+3)%3; } while(C.Secondary==C.Primary); }
   if (Offset==3) C.Lethal=C.Lethal==ECrosshairLethalType::Frag ? ECrosshairLethalType::Tomahawk : ECrosshairLethalType::Frag;
  }
  ClassReplay->SaveSettings(); return;
 }
 if (MenuSelection==GetSandboxRow())
 { Settings.bSandboxTargets=!Settings.bSandboxTargets; if (auto* P=Cast<ACrosshairCharacter>(GetPawn())) { P->Placement->bPlacing=false; P->StopActions(); } ClassReplay->SaveSettings(); return; }

	if (MenuSelection==GetLethalRow()) { if (auto* PracticePawn=Cast<ACrosshairCharacter>(GetPawn())) PracticePawn->Lethals->Cycle(); return; }
	auto* Replay = GetGameInstance()->GetSubsystem<UCrosshairReplaySubsystem>();
	FCrosshairSettings& S = Replay->GetSettings();
	switch (MenuSelection)
	{
	case MapRow:
		if (!Replay->IsPlayback() && !Replay->IsFinishing()) MapChoice = (MapChoice+Direction+GetGameInstance()->GetSubsystem<UCrosshairMapLibrary>()->MapCount())%GetGameInstance()->GetSubsystem<UCrosshairMapLibrary>()->MapCount();
		return;
	case WeaponRow:
		if (!Replay->IsPlayback() && !Replay->IsFinishing())
			if (auto* PracticePawn = Cast<ACrosshairCharacter>(GetPawn()))
			{
				FlushPressedKeys();
				const int32 Count = PracticePawn->Inventory->Weapons.Num();
				if (Count > 0) PracticePawn->Inventory->Equip((PracticePawn->Inventory->ActiveIndex + Direction + Count) % Count);
			}
		return;
	case InvertHorizontalRow: S.bInvertControllerHorizontal = !S.bInvertControllerHorizontal; break;
	case InvertVerticalRow: S.bInvertControllerVertical = !S.bInvertControllerVertical; break;
	case 1: S.StickYawSpeed += Direction * 30; break;
	case 2: S.StickPitchSpeed += Direction * 30; break;
	case 3: S.StickDeadZone += Direction * .01f; break;
	case 4: S.StickExponent += Direction * .1f; break;
	case 5: S.MouseSensitivity += Direction * .01f; break;
	case 6: S.AimSensitivity += Direction * .05f; break;
	case 7: S.FieldOfView += Direction * 5; break;
	case 8:
		if (!Replay->IsFinishing() && !Replay->IsPlayback()) { S.bContinuousPractice = !S.bContinuousPractice; Replay->BeginAttempt(); }
		break;
	default: return;
	}
	Replay->SaveSettings();
}
void ACrosshairPlayerController::ActivateSelection()
{
	auto* Replay = GetGameInstance()->GetSubsystem<UCrosshairReplaySubsystem>();
	const int32 Selected = MenuSelection;
 if (!bHomeScreen && Selected>=GetGraphicsRow() && Selected<GetGraphicsRow()+GraphicsRowCount) { AdjustGraphics(Selected-GetGraphicsRow(),1); return; }
 if (!bHomeScreen && Selected==GetLanRow()) { bEditingName=true; return; }   // keys now type the name (EditNameKey)
 if (!bHomeScreen && Selected>GetLanRow() && Selected<GetLanRow()+LanRowCount) { AdjustLan(Selected-GetLanRow(),1); return; }
 if (bHomeScreen)
 {
  if (Selected==0) StartPlaying(); else if (Selected==1) SetMenuTab(6); else if (Selected==2) SetMenuTab(7); else if (Selected==3) SetMenuTab(1);
  else if (Selected==4) SetMenuTab(8); else if (Selected==6) SetMenuTab(9); else if (Selected==7) SetMenuTab(4);
  else { bFromHome=false; bHomeScreen=false; MenuSelection=QuitRow; ActivateSelection(); }
  return;
 }
 if (Selected==GetHomeRow())
 {
  if (IsInMatch()) { FlushPressedKeys(); SetPause(false); GetGameInstance()->GetSubsystem<UCrosshairMatchSession>()->LeaveMatch(); }
  else if (!Replay->IsFinishing() && !Replay->IsPlayback()) ShowHome();
  return;
 }
 if (Selected==GetImportRow()) { GetGameInstance()->GetSubsystem<UCrosshairMapLibrary>()->ShowImportDialog(this); return; }
 if (Selected==GetSandboxRow()) { AdjustSelection(1); return; }
 if (Selected>=GetBotMatchRow() && Selected<GetFirstReplayRow())
 {
  const int32 Offset=Selected-GetBotMatchRow();
  auto* Session=GetGameInstance()->GetSubsystem<UCrosshairMatchSession>();
  if (Offset<6) { AdjustSelection(1); return; }
  FlushPressedKeys();
  if (Offset==6 || Offset==7 || Offset>=9) SetPause(false);
  if (Offset==6) Session->StartOfflineMatch();
  else if (Offset==7) Session->HostLanMatch();
  else if (Offset==8) Session->FindLanMatches();
  else Session->JoinLanMatch(Offset-9);
  return;
 }
 const int32 ClassOffset=Selected-GetClassRow();
 if (ClassOffset>=0 && ClassOffset<5)
 {
  if (Replay->IsPlayback() || Replay->IsFinishing()) return;
  if (ClassOffset<4) { if (MenuTab!=6) SetMenuTab(6); else AdjustSelection(1); return; }
  Replay->GetSettings().ActiveClass=EditingClass;
  // As in COD, a class chosen mid-match takes effect on the next spawn.
  if (auto* P=Cast<ACrosshairCharacter>(GetPawn())) { if (IsInMatch()) P->Notify(TEXT("Class applies on your next spawn.")); else P->Inventory->ApplyClass(); }
  Replay->SaveSettings(); return;
 }

	if (Selected == QuitRow)
	{
		FlushPressedKeys();
		Replay->SaveSettings();
		UKismetSystemLibrary::QuitGame(this, this, EQuitPreference::Quit, false);
		return;
	}
	if (Selected == MapRow)
	{
		FlushPressedKeys();
		GetGameInstance()->GetSubsystem<UCrosshairMapLibrary>()->PlayMap(MapChoice);
		return;
	}
	if ((Selected >= SkinRow && Selected <= DesertRow) || (Selected >= FirstReplayRow && Selected < GetLethalRow()))
	{
		if (!Replay->IsPlayback() && !Replay->IsFinishing())
			if (auto* PracticePawn = Cast<ACrosshairCharacter>(GetPawn()))
				if (auto* Current = PracticePawn->Inventory->GetCurrent())
				{
					const int32 Index = Selected <= DesertRow ? Selected - SkinRow - 1 : Selected - FirstReplayRow + 2;
					if (Index < 0) Current->SetSkin(NAME_None);
					else if (Current->Definition && Current->Definition->Skins.IsValidIndex(Index)) Current->SetSkin(Current->Definition->Skins[Index].Id);
				}
		return;
	}
	if (Selected == CalibrationRow)
	{
		const bool Cancel = ControllerCalibrationStep != 0;
		FlushPressedKeys();
		if (!Cancel) ControllerCalibrationStep = 1;
		return;
	}
	if (Selected == GetLethalRow()) { AdjustSelection(1); return; }
	if (Selected == WeaponRow || Selected == InvertHorizontalRow || Selected == InvertVerticalRow) { AdjustSelection(1); return; }
	if (Selected > 0 && Selected < 9) { AdjustSelection(1); return; }
	ToggleMenu();
	if (Selected == 0) { if (Replay->IsPlayback()) Replay->ReturnToPractice(); return; }
	if (Selected >= GetFirstReplayRow()) { Replay->PlaySaved(Selected - GetFirstReplayRow()); return; }
	if (auto* PracticePawn = Cast<ACrosshairCharacter>(GetPawn()))
	{
		switch (Selected)
		{
		case 9: PracticePawn->Attempt->SaveStart(); break;
		case 10: PracticePawn->Attempt->ResetAttempt(); break;
		case 11: PracticePawn->Placement->RemoveAimedTarget(); break;
		case 12: PracticePawn->Placement->ClearTargets(); break;
		default: break;
		}
	}
}
void ACrosshairHUD::DrawHUD()
{
	Super::DrawHUD();
	if (!Canvas || !PlayerOwner) return;
	auto* Replay = GetGameInstance()->GetSubsystem<UCrosshairReplaySubsystem>();
	auto* PC = Cast<ACrosshairPlayerController>(PlayerOwner);
	auto* PracticePawn = Cast<ACrosshairCharacter>(PlayerOwner->GetViewTarget());
	const float X = Canvas->ClipX * .5f, Y = Canvas->ClipY * .5f;
	const FLinearColor White(.92f,.95f,.98f), Accent(.32f,.85f,.78f), Muted(.58f,.66f,.72f);
	const float HUDScale=FMath::Min(Canvas->ClipX/1280.f,Canvas->ClipY/720.f);
	auto Label=[&](const FString& Value,FLinearColor Color,float LX,float LY,float Size=1.f)
	{ DrawText(Value,Color,LX*HUDScale,LY*HUDScale,GEngine->GetMediumFont(),Size*1.4f*HUDScale); };
	auto Panel=[&](float PX,float PY,float W,float H)
	{ DrawRect(FLinearColor(.018f,.026f,.036f,.86f),PX*HUDScale,PY*HUDScale,W*HUDScale,H*HUDScale); };

 if (const auto* Startup=GetGameInstance()->GetSubsystem<UCrosshairStartup>(); Startup->IsPreparing())
 {
  DrawRect(FLinearColor(.018f,.026f,.036f,1),0,0,Canvas->ClipX,Canvas->ClipY);
  Label(TEXT("PREPARING GAME"),White,390,285,1.6f);
  Label(Startup->GetStatus(),Accent,240,355,.8f);
  Label(TEXT("Please wait. Preparing maps, equipment and shaders."),Muted,240,400,.7f);
  return;
 }
	if (PracticePawn)
	{

		const auto* Current = PracticePawn->Inventory->GetCurrent();
		const bool ScopeView = Current && Current->Definition && Current->Definition->AimStyle == ECrosshairAimStyle::Scope && PracticePawn->GetAimAlpha() >= .95f;
		if (ScopeView)
		{
			const float Radius = FMath::Min(X, Y) * .9f;
			const FLinearColor Black = FLinearColor::Black;
			DrawRect(Black, 0, 0, Canvas->ClipX, Y - Radius);
			DrawRect(Black, 0, Y + Radius, Canvas->ClipX, Y - Radius);
			const float Band = 2.f;
			for (float Row = -Radius; Row < Radius; Row += Band)
			{
				const float Edge = FMath::Max(FMath::Abs(Row), FMath::Abs(FMath::Min(Row + Band, Radius)));
				const float HalfWidth = FMath::Sqrt(FMath::Max(0.f, Radius * Radius - Edge * Edge));
				DrawRect(Black, 0, Y + Row, X - HalfWidth, FMath::Min(Band, Radius - Row));
				DrawRect(Black, X + HalfWidth, Y + Row, X - HalfWidth, FMath::Min(Band, Radius - Row));
			}
			DrawLine(X - Radius, Y, X + Radius, Y, Black, 1.5f);
			DrawLine(X, Y - Radius, X, Y + Radius, Black, 1.5f);
		}
		else if (PracticePawn->GetAimAlpha() < .95f)
		{
			DrawLine(X - 8, Y, X - 3, Y, White); DrawLine(X + 3, Y, X + 8, Y, White);
			DrawLine(X, Y - 8, X, Y - 3, White); DrawLine(X, Y + 3, X, Y + 8, White);
		}
		if (GetWorld()->GetTimeSeconds() < PracticePawn->HitMarkerUntil)
		{
			const bool Head=PracticePawn->bLastHitHeadshot;
			const FLinearColor HitColor=Head ? FLinearColor(1,.68f,.2f) : White;
			const float Outer=(Head ? 18 : 14)*HUDScale, Inner=7*HUDScale;
			for (float SignX : {-1.f,1.f}) for (float SignY : {-1.f,1.f})
			{
				DrawLine(X+SignX*Inner,Y+SignY*Inner,X+SignX*Outer,Y+SignY*Outer,HitColor,Head ? 3 : 2);
				if (Head) DrawLine(X+SignX*(Outer+4*HUDScale),Y+SignY*Inner,X+SignX*(Outer+4*HUDScale),Y+SignY*Outer,HitColor,2);
			}
			const FString Feedback=(Head ? TEXT("HEADSHOT") : TEXT("")) + FString(PracticePawn->bLastHitWallbang ? (Head ? TEXT("  /  WALLBANG") : TEXT("WALLBANG")) : TEXT(""));
			float TW,TH; GetTextSize(Feedback,TW,TH,GEngine->GetMediumFont(),HUDScale);
			DrawText(Feedback,HitColor,X-TW*.5f,Y+30*HUDScale,GEngine->GetMediumFont(),HUDScale);
		}
	}
	// Match replays use the practice replay HUD, not the live match overlay.
	ACrosshairMatchState* Match=Replay->IsPlayback() ? nullptr : ACrosshairMatchState::Get(this);
	if (Match) DrawMatch(Match,HUDScale);
	if (!Match && (!PC || !PC->bMenuOpen))
	{
		Panel(24,24,310,74);
		Label(TEXT("CROSSHAIR"),White,40,34,1.15f);
		Label(Replay->IsPlayback() ? TEXT("REPLAY  /  Esc to open menu") : TEXT("PRACTICE  /  Esc menu  /  T reset"),Muted,40,65,.65f);
		if (!Replay->Status.IsEmpty()) Label(Replay->Status,Accent,40,110,.7f);
	}
	if (PracticePawn && (!PC || !PC->bMenuOpen))
	{
		if (auto* W = PracticePawn->Inventory->GetCurrent())
		{
			const FString Name = W->Definition ? W->Definition->DisplayName.ToString() : TEXT("Weapon");
			if (!PC || !PC->bMenuOpen)
			{
				const float Right=Canvas->ClipX/HUDScale-284;
				const float Bottom=Canvas->ClipY/HUDScale-152;
				Panel(Right,Bottom,260,128);
				Label(Name.ToUpper(),Muted,Right+18,Bottom+12,.7f);
				Label(FString::Printf(TEXT("%02d"),W->Ammo),White,Right+18,Bottom+32,1.8f);
				Label(W->bReloading ? TEXT("RELOADING") : FString::Printf(TEXT("/ %d   R reload"),W->Definition ? W->Definition->MagazineSize : 0),W->bReloading ? Accent : Muted,Right+86,Bottom+52,.7f);
				const auto* Lethal=PracticePawn->Lethals.Get();
				Label(FString::Printf(TEXT("%s  x%d"),Lethal->Selected==ECrosshairLethalType::Frag ? TEXT("FRAG") : TEXT("TOMAHAWK"),Lethal->Remaining),Accent,Right+18,Bottom+94,.7f);
				if (Lethal->IsHolding()) Label(Lethal->Selected==ECrosshairLethalType::Frag ? FString::Printf(TEXT("Fuse %.1fs / Release to throw"),FMath::Max(0.f,Lethal->Frag.Fuse-Lethal->GetCookedSeconds())) : TEXT("Release to throw tomahawk"),Accent,40,204,.85f);
			}
		}
		if (PracticePawn->Placement->bPlacing)
		{
			const bool Valid = PracticePawn->Placement->bValidPlacement;
			Label(Valid ? TEXT("PLACE TARGET  /  Fire confirm  /  Reload rotate  /  Aim cancel") : TEXT("PLACEMENT BLOCKED  /  Aim at clear ground"),Valid ? Accent : FLinearColor(1,.4f,.3f),40,144,.8f);
		}
		if (GetWorld()->GetTimeSeconds() < PracticePawn->NoticeUntil) Label(PracticePawn->Notice, Accent, 40, 172,.8f);
	}
	if (Match && (!PC || !PC->bMenuOpen))
	{
		const float Bottom=Canvas->ClipY/HUDScale-80;
		Panel(24,Bottom,470,56);
		Label(TEXT("Tab / Back scoreboard   /   G / RB lethal"),White,40,Bottom+9,.7f);
		Label(TEXT("Space / A jump + mantle   /   Esc / Start menu"),Muted,40,Bottom+31,.65f);
	}
	else if (!PC || !PC->bMenuOpen)
	{
		const float Bottom=Canvas->ClipY/HUDScale-102;
		Panel(24,Bottom,470,78);
		Label(TEXT("P target   /   K save position   /   T reset"),White,40,Bottom+9,.7f);
		Label(TEXT("G / RB hold + release throw   /   F / LB lethal"),Accent,40,Bottom+31,.65f);
		Label(TEXT("Space / A jump + mantle   /   Esc / Start menu"),Muted,40,Bottom+53,.65f);
	}

	if (PC && PC->bMenuOpen) DrawMenu(PC,Match);
}
void ACrosshairHUD::DrawMatch(ACrosshairMatchState* Match,float Scale)
{
	auto* PC=Cast<ACrosshairPlayerController>(PlayerOwner);
	if (PC && PC->bMenuOpen) return;
	const FLinearColor White(.92f,.95f,.98f), Accent(.32f,.85f,.78f), Muted(.58f,.66f,.72f), Friendly(.35f,.65f,1.f), Enemy(1.f,.38f,.32f), Gold(1.f,.78f,.3f);
	UFont* Font=GEngine->GetMediumFont();
	auto Label=[&](const FString& Value,FLinearColor Color,float LX,float LY,float Size=1.f)
	{ DrawText(Value,Color,LX*Scale,LY*Scale,Font,Size*1.4f*Scale); };
	auto Centered=[&](const FString& Value,FLinearColor Color,float LY,float Size=1.f)
	{ float W,H; GetTextSize(Value,W,H,Font,Size*1.4f*Scale); DrawText(Value,Color,(Canvas->ClipX-W)*.5f,LY*Scale,Font,Size*1.4f*Scale); };
	auto Panel=[&](float PX,float PY,float W,float H,float Alpha=.86f)
	{ DrawRect(FLinearColor(.018f,.026f,.036f,Alpha),PX*Scale,PY*Scale,W*Scale,H*Scale); };
	const auto* Me=PC ? PC->GetPlayerState<ACrosshairMatchPlayerState>() : nullptr;
	const uint8 MyTeam=Me ? Me->Team : CrosshairMatchRules::NoTeam;
	const auto* MyPawn=Cast<ACrosshairCharacter>(PlayerOwner->GetPawn());
	const float Width=Canvas->ClipX/Scale;

	// Damage: red screen edges that fade as health regenerates.
	if (MyPawn && !MyPawn->Health->bDead && MyPawn->Health->Health<MyPawn->Health->MaxHealth)
	{
		const float Alpha=(1.f-MyPawn->Health->Health/MyPawn->Health->MaxHealth)*.55f;
		const float Edge=90.f*Scale;
		for (int32 Ring=0;Ring<3;++Ring)
		{
			const float E=Edge*(1.f-Ring*.3f); const FLinearColor Red(.55f,0,0,Alpha*(.35f+Ring*.2f));
			DrawRect(Red,0,0,Canvas->ClipX,E); DrawRect(Red,0,Canvas->ClipY-E,Canvas->ClipX,E);
			DrawRect(Red,0,E,E,Canvas->ClipY-2*E); DrawRect(Red,Canvas->ClipX-E,E,E,Canvas->ClipY-2*E);
		}
	}

	// Score and clock.
	Panel(24,24,360,92);
	const int32 Seconds=FMath::CeilToInt(Match->GetRemainingSeconds());
	Label(FString::Printf(TEXT("%s  /  %s"),Match->IsTeamMode() ? TEXT("TEAM DEATHMATCH") : TEXT("FREE-FOR-ALL"),*Match->MapName.ToUpper()),Muted,40,32,.6f);
	Label(FString::Printf(TEXT("%d:%02d"),Seconds/60,Seconds%60),White,312,32,.75f);
	if (Match->IsTeamMode())
	{
		const uint8 Us=MyTeam==CrosshairMatchRules::Bravo ? CrosshairMatchRules::Bravo : CrosshairMatchRules::Alpha;
		const uint8 Them=Us==CrosshairMatchRules::Alpha ? CrosshairMatchRules::Bravo : CrosshairMatchRules::Alpha;
		Label(FString::Printf(TEXT("%s  %d"),*ACrosshairMatchState::TeamName(Us).ToUpper(),Match->GetTeamScore(Us)),Friendly,40,56,1.f);
		Label(FString::Printf(TEXT("%s  %d"),*ACrosshairMatchState::TeamName(Them).ToUpper(),Match->GetTeamScore(Them)),Enemy,210,56,1.f);
	}
	else
	{
		const auto Standings=Match->GetStandings();
		Label(FString::Printf(TEXT("YOU  %d"),Me ? Me->Kills : 0),Accent,40,56,1.f);
		Label(FString::Printf(TEXT("LEADER  %d"),Standings.IsEmpty() ? 0 : Standings[0]->Kills),White,210,56,1.f);
	}
	Label(FString::Printf(TEXT("First to %d"),Match->ScoreLimit),Muted,40,92,.55f);

	// Kill feed, newest first, top right.
	const float Now=Match->GetServerWorldTimeSeconds();
	float FeedY=24.f;
	for (int32 i=Match->KillFeed.Num()-1;i>=0;--i)
	{
		const FCrosshairKillEntry& Entry=Match->KillFeed[i];
		if (Now-Entry.ServerTime>6.f) continue;
		const FString Line=Entry.Killer.IsEmpty() ? FString::Printf(TEXT("%s  [%s]"),*Entry.Victim,*Entry.Weapon)
			: FString::Printf(TEXT("%s  [%s%s]  %s"),*Entry.Killer,*Entry.Weapon,Entry.bHeadshot ? TEXT(" / HEADSHOT") : TEXT(""),*Entry.Victim);
		const bool Mine=Me && (Entry.Killer==Me->GetPlayerName() || Entry.Victim==Me->GetPlayerName());
		const FLinearColor Color=Match->IsTeamMode() ? (Entry.KillerTeam==MyTeam ? Friendly : Enemy) : (Mine ? Accent : White);
		float W,H; GetTextSize(Line,W,H,Font,.65f*1.4f*Scale);
		Panel(Width-W/Scale-48,FeedY,W/Scale+24,30,.7f);
		Label(Line,Color,Width-W/Scale-36,FeedY+5,.65f);
		FeedY+=34.f;
	}

	if (MyPawn && GetWorld()->GetTimeSeconds()<MyPawn->TrickshotUntil)
	{
		Centered(TEXT("TRICKSHOT"),Gold,150,1.6f);
		Centered(MyPawn->TrickshotBanner,White,198,.8f);
	}
	// Teammate names (TDM): COD-style friendly identification.
	if (Match->IsTeamMode())
		for (TActorIterator<ACrosshairCharacter> It(GetWorld());It;++It)
		{
			const auto* Other=It->GetPlayerState<ACrosshairMatchPlayerState>();
			if (*It==MyPawn || It->Health->bDead || !Other || Other->Team!=MyTeam) continue;
			const FVector Point=Project(It->GetActorLocation()+FVector(0,0,115));
			if (Point.Z<=0) continue;
			float W,H; GetTextSize(Other->GetPlayerName(),W,H,Font,.9f*Scale);
			DrawText(Other->GetPlayerName(),Friendly,Point.X-W*.5f,Point.Y-H,Font,.9f*Scale);
		}

	if (MyPawn && MyPawn->Health->bDead && !Match->bMatchOver)
	{
		DrawRect(FLinearColor(0,0,0,.35f),0,0,Canvas->ClipX,Canvas->ClipY);
		Centered(MyPawn->Health->KilledBy.IsEmpty() || MyPawn->Health->KilledBy==TEXT("Fell") ? FString(TEXT("YOU DIED")) : TEXT("KILLED BY ")+MyPawn->Health->KilledBy.ToUpper(),Enemy,280,1.4f);
		Centered(TEXT("Respawning..."),Muted,330,.8f);
	}

	// Scoreboard while held, and the results screen when the match ends.
	if ((PC && PC->bScoreboardHeld) || Match->bMatchOver)
	{
		const auto Standings=Match->GetStandings();
		const int32 Rows=FMath::Min(Standings.Num(),12);
		const float Left=(Width-640.f)*.5f, Top=Match->bMatchOver ? 150.f : 110.f;
		if (Match->bMatchOver)
		{
			Panel(Left,Top-96,640,84,.94f);
			Centered(TEXT("MATCH OVER  -  ")+Match->Result.ToUpper(),Gold,Top-86,1.3f);
			Centered(TEXT("Enter / A: leave match"),Muted,Top-46,.75f);
		}
		Panel(Left,Top,640,48+Rows*30.f,.94f);
		Label(TEXT("PLAYER"),Muted,Left+20,Top+12,.65f);
		Label(TEXT("KILLS"),Muted,Left+430,Top+12,.65f);
		Label(TEXT("DEATHS"),Muted,Left+530,Top+12,.65f);
		for (int32 i=0;i<Rows;++i)
		{
			const auto* Player=Standings[i];
			const float RY=Top+44+i*30.f;
			const FLinearColor Color=Player==Me ? Accent : Match->IsTeamMode() ? (Player->Team==MyTeam ? Friendly : Enemy) : White;
			const FString Name=Match->IsTeamMode() ? FString::Printf(TEXT("[%s] %s"),*ACrosshairMatchState::TeamName(Player->Team),*Player->GetPlayerName()) : Player->GetPlayerName();
			Label(Name,Color,Left+20,RY,.7f);
			Label(FString::FromInt(Player->Kills),Color,Left+440,RY,.7f);
			Label(FString::FromInt(Player->Deaths),Color,Left+545,RY,.7f);
		}
	}
}
void ACrosshairHUD::DrawMenu(ACrosshairPlayerController* PC,ACrosshairMatchState* Match)
{
	auto* Replay=GetGameInstance()->GetSubsystem<UCrosshairReplaySubsystem>();
	const auto* Session=GetGameInstance()->GetSubsystem<UCrosshairMatchSession>();
	const FCrosshairSettings& Settings=Replay->GetSettings();
	UFont* Font=GEngine->GetMediumFont();
	// Layout is authored on a 1120x640 canvas and scaled to the viewport.
	const float S=FMath::Min(Canvas->ClipX/1280.f,Canvas->ClipY/720.f);
	const float W=1120.f,H=640.f;
	const float Left=(Canvas->ClipX-W*S)*.5f,Top=(Canvas->ClipY-H*S)*.5f;
	const FLinearColor Ink(.93f,.95f,.97f),Muted(.50f,.57f,.64f),Dim(.26f,.30f,.35f);
	const FLinearColor Amber(.98f,.58f,.13f),AmberDeep(.42f,.22f,.04f),OnAmber(.03f,.025f,.02f);
	const FLinearColor Base(.005f,.007f,.010f,.97f),PanelColor(.010f,.013f,.018f,1),Raised(.019f,.024f,.031f,1),Hover(.060f,.040f,.016f,1),Line(.045f,.054f,.066f,1);
	auto Rect=[&](FLinearColor C,float X,float Y,float RW,float RH){ DrawRect(C,Left+X*S,Top+Y*S,RW*S,RH*S); };
	auto Frame=[&](FLinearColor C,float X,float Y,float RW,float RH){ Rect(C,X,Y,RW,1); Rect(C,X,Y+RH-1,RW,1); Rect(C,X,Y,1,RH); Rect(C,X+RW-1,Y,1,RH); };
	auto Measure=[&](const FString& T,float Size){ float TW,TH; GetTextSize(T,TW,TH,Font,Size*1.1f*S); return TW/S; };
	auto Text=[&](const FString& T,FLinearColor C,float X,float Y,float Size=1.f){ DrawText(T,C,Left+X*S,Top+Y*S,Font,Size*1.1f*S); };
	auto TextRight=[&](const FString& T,FLinearColor C,float RX,float Y,float Size=1.f){ Text(T,C,RX-Measure(T,Size),Y,Size); };
	auto TextCenter=[&](const FString& T,FLinearColor C,float CX,float Y,float Size=1.f){ Text(T,C,CX-Measure(T,Size)*.5f,Y,Size); };
	auto HitBox=[&](const FString& Name,float X,float Y,float RW,float RH){ AddHitBox(FVector2D(Left+X*S,Top+Y*S),FVector2D(RW*S,RH*S),FName(*Name),true); };
	auto Chip=[&](const FString& Key,const FString& Action,float X,float Y)
	{
		const float KeyWidth=Measure(Key,.7f)+18;
		Rect(Raised,X,Y,KeyWidth,24); Frame(Line,X,Y,KeyWidth,24);
		Text(Key,Ink,X+9,Y+4,.7f); Text(Action,Muted,X+KeyWidth+8,Y+4,.7f);
		return X+KeyWidth+8+Measure(Action,.7f)+22;
	};
	const TArray<FString> Rows=PC->GetMenuRows();
	const int32 B=PC->GetBotMatchRow();
	const auto& Options=Settings.BotMatch;
	const bool bTeams=Options.Mode==ECrosshairMatchMode::TeamDeathmatch;

	DrawRect(FLinearColor(0,0,0,PC->bHomeScreen ? .9f : .74f),0,0,Canvas->ClipX,Canvas->ClipY);
	Rect(Base,0,0,W,H); Frame(Line,0,0,W,H); Rect(Amber,0,0,W,3);
	// Header: wordmark, session and where you are.
	Rect(Amber,32,28,5,32);
	Text(TEXT("CROSSHAIR"),Ink,48,22,1.7f);
	const FString SessionName=Match ? FString::Printf(TEXT("%s  /  %s"),Match->IsTeamMode() ? TEXT("TEAM DEATHMATCH") : TEXT("FREE-FOR-ALL"),*Match->MapName.ToUpper())
		: Replay->IsPlayback() ? FString(TEXT("REPLAY")) : FString(TEXT("TRICKSHOT PRACTICE"));
	Text(SessionName,Muted,50,60,.72f);
	const TCHAR* Tabs[]={TEXT("Practice"),TEXT("Controls"),TEXT("Graphics"),TEXT("Targets"),TEXT("Replays"),TEXT("Camos"),TEXT("Classes"),TEXT("Maps"),TEXT("Offline Match"),TEXT("LAN")};
	TextRight(PC->bHomeScreen ? FString(TEXT("MAIN MENU")) : FString(Tabs[PC->MenuTab]).ToUpper(),Ink,W-32,30,.95f);
	TextRight(PC->bHomeScreen ? FString(TEXT("Choose how you want to play")) : PC->bFromHome ? FString(TEXT("Esc / B returns to the main menu")) : FString(TEXT("Esc / Start resumes")),Muted,W-32,58,.68f);
	Rect(Line,0,92,W,1);
	Rect(Line,0,H-56,W,1);

	if (PC->bHomeScreen)
	{
		Text(TEXT("PLAY"),Amber,32,110,.7f);
		const int32 Cards[]={0,4,6};
		for (int32 i=0;i<3;++i)
		{
			const int32 Row=Cards[i]; const float Y=134+i*104; const bool Sel=PC->MenuSelection==Row;
			Rect(Sel ? Hover : Raised,32,Y,604,92); Frame(Sel ? AmberDeep : Line,32,Y,604,92);
			if (Sel) Rect(Amber,32,Y,4,92);
			Text(Rows[Row].ToUpper(),Sel ? Amber : Ink,58,Y+17,1.3f);
			Text(PC->GetRowHelp(Row),Muted,58,Y+55,.78f);
			TextRight(Sel ? TEXT("ENTER  >") : TEXT(">"),Sel ? Amber : Dim,612,Y+33,.95f);
			HitBox(FString::Printf(TEXT("Row_%d"),Row),32,Y,604,92);
		}
		Text(TEXT("OPTIONS"),Amber,664,110,.7f);
		const int32 Tiles[]={1,2,7,3,5};
		for (int32 i=0;i<5;++i)
		{
			const int32 Row=Tiles[i]; const float Y=134+i*61; const bool Sel=PC->MenuSelection==Row;
			Rect(Sel ? Hover : Raised,664,Y,424,53); Frame(Sel ? AmberDeep : Line,664,Y,424,53);
			if (Sel) Rect(Amber,664,Y,3,53);
			Text(Rows[Row],Sel ? Amber : Ink,686,Y+7,.98f);
			Text(PC->GetRowHelp(Row),Muted,686,Y+31,.64f);
			HitBox(FString::Printf(TEXT("Row_%d"),Row),664,Y,424,53);
		}
		// Loadout and match setup at a glance.
		const auto& Class=Settings.Classes[FMath::Clamp(Settings.ActiveClass,0,Settings.Classes.Num()-1)];
		static const TCHAR* WeaponNames[]={TEXT("Sniper"),TEXT("Assault rifle"),TEXT("SMG")};
		Rect(PanelColor,32,456,1056,48); Frame(Line,32,456,1056,48);
		Text(FString::Printf(TEXT("ACTIVE CLASS   %s  /  %s + %s  /  %s"),*Class.Name.ToUpper(),WeaponNames[FMath::Clamp(Class.Primary,0,2)],WeaponNames[FMath::Clamp(Class.Secondary,0,2)],
			Class.Lethal==ECrosshairLethalType::Frag ? TEXT("Frag") : TEXT("Tomahawk")),Muted,52,470,.7f);
		TextRight(FString::Printf(TEXT("NEXT MATCH   %s on %s  /  %s bots"),*UCrosshairBotMatch::ModeName(Options.Mode),*UCrosshairBotMatch::MapName(Options.Map),*UCrosshairBotMatch::DifficultyName(Options.Difficulty)),Muted,1068,470,.7f);
		float ChipX=32;
		ChipX=Chip(TEXT("Arrows / D-pad"),TEXT("Select"),ChipX,H-40);
		Chip(TEXT("Enter / A"),TEXT("Open"),ChipX,H-40);
		return;
	}

	// Sidebar tabs.
	Rect(PanelColor,16,104,220,H-176); Frame(Line,16,104,220,H-176);
	for (int32 Tab=0;Tab<ACrosshairPlayerController::MenuTabCount;++Tab)
	{
		const float TY=112+Tab*45; const bool Sel=PC->MenuTab==Tab; const bool Locked=PC->IsTabLockedInMatch(Tab);
		if (Sel) { Rect(Hover,20,TY,212,40); Rect(Amber,20,TY,3,40); }
		Text(Tabs[Tab],Sel ? Amber : Locked ? Dim : Ink,38,TY+10,.92f);
		HitBox(FString::Printf(TEXT("Tab_%d"),Tab),20,TY,212,40);
	}
	const TCHAR* Descriptions[]={TEXT("Set up your next trickshot attempt."),TEXT("Tune your mouse and controller."),TEXT("Quality presets for your laptop or PC, ray tracing and field of view."),TEXT("Manage your practice targets."),
		TEXT("Watch saved trickshots and match kills."),TEXT("Choose a finish for your weapon."),TEXT("Build and equip your weapon class."),TEXT("Play and import practice maps."),
		TEXT("Free-for-all or Team Deathmatch against bots on Nuketown, Grid and Space Station."),TEXT("Play with others on the same network.")};
	const float ContentX=260,ContentRight=W-32;
	Text(FString(Tabs[PC->MenuTab]).ToUpper(),Ink,ContentX,104,1.4f);
	Text(PC->IsTabLockedInMatch(PC->MenuTab) ? FString(TEXT("Unavailable during a match. Leave the match to use practice tools.")) : FString(Descriptions[PC->MenuTab]),Muted,ContentX,140,.78f);

	const TArray<int32> Visible=PC->GetVisibleMenuRows();
	int32 Index=Visible.IndexOfByKey(PC->MenuSelection);
	if (Index==INDEX_NONE) { PC->MenuSelection=Visible[0]; Index=0; }
	const float RowPitch=PC->MenuTab==1 ? 38.f : 46.f, RowHeight=RowPitch-6.f, RowsTop=174.f;
	const int32 PageSize=PC->MenuTab==1 ? 9 : 8;
	const int32 First=FMath::Max(0,Index-PageSize+1);
	if (Visible.Num()>PageSize) TextRight(FString::Printf(TEXT("%d-%d of %d"),First+1,FMath::Min(Visible.Num(),First+PageSize),Visible.Num()),Dim,ContentRight,144,.68f);
	const bool bSummary=PC->MenuTab==8;
	const float RowRight=bSummary ? 760.f : ContentRight;
	if (PC->MenuTab==4 && Visible.Num()==1) Text(TEXT("No saved replays yet. Land a practice hit or a match kill to record one."),Muted,ContentX,RowsTop+RowPitch+8,.8f);
	for (int32 i=First;i<FMath::Min(Visible.Num(),First+PageSize);++i)
	{
		const int32 Row=Visible[i];
		const float RY=RowsTop+(i-First)*RowPitch; const float RW=RowRight-ContentX;
		const bool Sel=Row==PC->MenuSelection;
		const FString RowName=FString::Printf(TEXT("Row_%d"),Row);
		if (Row==B+6)
		{
			// Primary call to action.
			Rect(Sel ? Amber : AmberDeep,ContentX,RY,RW,RowHeight);
			TextCenter(Rows[Row].ToUpper()+TEXT("  >"),Sel ? OnAmber : Ink,ContentX+RW*.5f,RY+(RowHeight-24)*.5f,1.05f);
			HitBox(RowName,ContentX,RY,RW,RowHeight);
			continue;
		}
		Rect(Sel ? Hover : Raised,ContentX,RY,RW,RowHeight);
		if (Sel) Rect(Amber,ContentX,RY,3,RowHeight);
		FString RowLabel=Rows[Row],Value;
		const bool bSplit=Row!=PC->CalibrationRow && Rows[Row].Split(TEXT(": "),&RowLabel,&Value);
		if (bSplit && PC->MenuTab==5 && Row!=PC->WeaponRow) { RowLabel=Value; Value.Reset(); }
		const float TextY=RY+(RowHeight-22)*.5f;
		Text(RowLabel,Sel ? Ink : FLinearColor(.80f,.84f,.88f),ContentX+20,TextY,.92f);
		const bool bAdjustable=(Row>=1 && Row<=8) || Row==PC->WeaponRow || Row==PC->GetLethalRow() || (Row>=PC->GetClassRow() && Row<PC->GetClassRow()+4) || Row==PC->GetSandboxRow()
			|| (Row>=B && Row<B+6) || Row==PC->MapRow || Row==PC->InvertHorizontalRow || Row==PC->InvertVerticalRow;
		if (bAdjustable)
		{
			const float ChipWidth=FMath::Min(250.f,RW*.46f), ChipX=RowRight-ChipWidth-8;
			Rect(PanelColor,ChipX,RY+5,ChipWidth,RowHeight-10);
			Text(TEXT("<"),Sel ? Amber : Dim,ChipX+12,TextY,.95f);
			TextRight(TEXT(">"),Sel ? Amber : Dim,ChipX+ChipWidth-12,TextY,.95f);
			TextCenter(Value.IsEmpty() ? RowLabel : Value,Sel ? Amber : Ink,ChipX+ChipWidth*.5f,TextY+1,.85f);
			HitBox(RowName,ContentX,RY,RW-ChipWidth-8,RowHeight);
			HitBox(FString::Printf(TEXT("Less_%d"),Row),ChipX,RY,ChipWidth*.35f,RowHeight);
			HitBox(FString::Printf(TEXT("More_%d"),Row),ChipX+ChipWidth*.65f,RY,ChipWidth*.35f,RowHeight);
		}
		else
		{
			if (!Value.IsEmpty()) TextRight(Value,Muted,RowRight-36,TextY+1,.85f);
			TextRight(TEXT(">"),Sel ? Amber : Dim,RowRight-14,TextY,.9f);
			HitBox(RowName,ContentX,RY,RW,RowHeight);
		}
	}
	if (bSummary)
	{
		// Match summary card beside the settings.
		const float CX=780,CW=ContentRight-CX,CY=RowsTop,CH=7*46.f-6;
		Rect(PanelColor,CX,CY,CW,CH); Frame(Line,CX,CY,CW,CH); Rect(Amber,CX,CY,CW,3);
		Text(TEXT("MATCH SUMMARY"),Amber,CX+20,CY+18,.68f);
		Text(UCrosshairBotMatch::ModeName(Options.Mode).ToUpper(),Ink,CX+20,CY+40,1.2f);
		Text(UCrosshairBotMatch::MapName(Options.Map).ToUpper(),Muted,CX+20,CY+74,.9f);
		const FString Lines[]={
			bTeams ? FString::Printf(TEXT("%dv%d  /  you + %d bots"),Options.TeamSize,Options.TeamSize,Options.TeamSize*2-1) : FString::Printf(TEXT("%d players  /  you + %d bots"),Options.BotCount+1,Options.BotCount),
			bTeams ? FString::Printf(TEXT("First team to %d kills"),Options.TeamScoreLimit) : FString::Printf(TEXT("First to %d kills"),Options.ScoreLimit),
			FString::Printf(TEXT("%d minute time limit"),Options.TimeLimitMinutes),
			UCrosshairBotMatch::DifficultyName(Options.Difficulty)+TEXT(" bots")};
		for (int32 i=0;i<4;++i) { Rect(Line,CX+20,CY+112+i*36,CW-40,1); Text(Lines[i],Ink,CX+20,CY+121+i*36,.8f); }
		Text(PC->GetRowHelp(B),Muted,CX+20,CY+CH-62,.62f);
		Text(Match ? TEXT("Restarting applies these settings.") : TEXT("Every kill you land is saved to Replays."),Muted,CX+20,CY+CH-36,.62f);
	}
	if (PC->MenuTab==9)
	{
		Rect(PanelColor,ContentX,470,ContentRight-ContentX,64); Frame(Line,ContentX,470,ContentRight-ContentX,64);
		Text(TEXT("NETWORK"),Amber,ContentX+20,479,.66f);
		Text(Session->LanStatus.IsEmpty() ? FString(TEXT("Local network only. Joined players can move and watch; LAN combat is a preview.")) : Session->LanStatus,Ink,ContentX+20,502,.78f);
	}
	if (PC->MenuTab==7 && !PC->IsTabLockedInMatch(7))
	{
		Rect(PanelColor,ContentX,340,ContentRight-ContentX,150); Frame(Line,ContentX,340,ContentRight-ContentX,150);
		Text(TEXT("CREDITS"),Amber,ContentX+20,352,.66f);
		Text(TEXT("Call of Duty Black Ops 1 Grid Inspired Map"),Ink,ContentX+20,374,.9f);
		Text(TEXT("by PIPO (pipo_)  /  CC BY 4.0  /  Adapted for Crosshair: import, collision, lighting and spawns."),Muted,ContentX+20,402,.68f);
		Text(TEXT("View source / creator on Sketchfab"),Amber,ContentX+20,436,.8f);
		HitBox(TEXT("GridSource"),ContentX+12,430,360,32);
		Text(TEXT("CC BY 4.0 license"),Amber,ContentX+420,436,.8f);
		HitBox(TEXT("GridLicense"),ContentX+412,430,260,32);
	}
	// Help for the highlighted row.
	const FString Help=PC->GetRowHelp(PC->MenuSelection);
	if (!Help.IsEmpty()) Text(Help,Muted,ContentX,H-82,.74f);
	float ChipX=32;
	ChipX=Chip(TEXT("Q / E"),TEXT("Tabs"),ChipX,H-40);
	ChipX=Chip(TEXT("Arrows"),TEXT("Select"),ChipX,H-40);
	ChipX=Chip(TEXT("Left / Right"),TEXT("Adjust"),ChipX,H-40);
	ChipX=Chip(TEXT("Enter / A"),TEXT("Confirm"),ChipX,H-40);
	if (PC->MenuTab==4) ChipX=Chip(TEXT("X / Delete"),TEXT("Delete replay"),ChipX,H-40);
	Chip(TEXT("Esc / B"),PC->bFromHome ? TEXT("Main menu") : TEXT("Resume"),ChipX,H-40);
}
void ACrosshairHUD::NotifyHitBoxClick(FName BoxName)
{
	Super::NotifyHitBoxClick(BoxName);
	auto* PC = Cast<ACrosshairPlayerController>(PlayerOwner);
	if (!PC || !PC->bMenuOpen) return;
	const FString Name = BoxName.ToString();
 if (PC->MenuTab==7 && (Name==TEXT("GridSource") || Name==TEXT("GridLicense")))
 {
  FPlatformProcess::LaunchURL(Name==TEXT("GridSource") ? TEXT("https://sketchfab.com/3d-models/call-of-duty-black-ops-1-grid-inspired-map-68c0b8b9ffe34a468876044b4c1da00a") : TEXT("https://creativecommons.org/licenses/by/4.0/"),nullptr,nullptr);
  return;
 }
	FString Kind, Value;
	if (!Name.Split(TEXT("_"), &Kind, &Value)) return;
	const int32 Index = FCString::Atoi(*Value);
	if (Kind == TEXT("Tab")) PC->SetMenuTab(Index);
	else if (Kind == TEXT("Row")) PC->ActivateMenuRow(Index);
	else if (Kind == TEXT("Less")) PC->AdjustMenuRow(Index, -1);
	else if (Kind == TEXT("More")) PC->AdjustMenuRow(Index, 1);
}
void ACrosshairPlayerController::AdjustGraphics(int32 Offset,int32 Direction)
{
 auto* Replay=GetGameInstance()->GetSubsystem<UCrosshairReplaySubsystem>();
 FCrosshairSettings& S=Replay->GetSettings();
 auto* PracticePawn=Cast<ACrosshairCharacter>(GetPawn());
 // The first change adopts the current engine level so nothing else jumps.
 if (!S.bGraphicsConfigured) CrosshairGraphics::SelectPreset(S,CrosshairGraphics::CurrentPreset(S));
 switch (Offset)
 {
 case 0: CrosshairGraphics::SelectPreset(S,(S.GraphicsPreset+Direction+CrosshairGraphics::PresetCount)%CrosshairGraphics::PresetCount); break;
 case 1:
  if (!CrosshairGraphics::IsRayTracingSupported()) { if (PracticePawn) PracticePawn->Notify(TEXT("This GPU or driver does not support hardware ray tracing.")); return; }
  S.bRayTracing=!S.bRayTracing; break;
 case 2: S.ResolutionScale=FMath::Clamp(S.ResolutionScale+Direction*5,50,100); break;
 case 3:
 {
  const int32 Preset=CrosshairGraphics::Detect(S);
  if (PracticePawn) PracticePawn->Notify(FString(TEXT("Detected: "))+CrosshairGraphics::PresetName(Preset));
  break;
 }
 default: return;
 }
 CrosshairGraphics::Apply(S);
 Replay->SaveSettings();
}
void ACrosshairPlayerController::ServerSetPlayerName_Implementation(const FString& Name)
{
 if (AGameModeBase* Mode=GetWorld()->GetAuthGameMode()) Mode->ChangeName(this,UCrosshairBotMatch::CleanPlayerName(Name),false);
}
void ACrosshairPlayerController::EditNameKey(const FKey& Key)
{
 auto* Replay=GetGameInstance()->GetSubsystem<UCrosshairReplaySubsystem>();
 FString& Name=Replay->GetSettings().PlayerName;
 if (Key==EKeys::Enter || Key==EKeys::Escape || Key==EKeys::Gamepad_FaceButton_Bottom || Key==EKeys::Gamepad_FaceButton_Right || Key==EKeys::Gamepad_Special_Right)
 {
  bEditingName=false; FlushPressedKeys();
  Name=UCrosshairBotMatch::CleanPlayerName(Name); Replay->SaveSettings();
  ServerSetPlayerName(Name);
  return;
 }
 if (Key==EKeys::BackSpace) { Name.LeftChopInline(1); return; }
 const bool bShift=IsInputKeyDown(EKeys::LeftShift) || IsInputKeyDown(EKeys::RightShift);
 const FString Code=Key.GetFName().ToString();
 static const TCHAR* Digits[]={TEXT("Zero"),TEXT("One"),TEXT("Two"),TEXT("Three"),TEXT("Four"),TEXT("Five"),TEXT("Six"),TEXT("Seven"),TEXT("Eight"),TEXT("Nine")};
 TCHAR Char=0;
 if (Code.Len()==1 && FChar::IsAlpha(Code[0])) Char=bShift ? FChar::ToUpper(Code[0]) : FChar::ToLower(Code[0]);
 for (int32 i=0;i<10;++i) if (Code==Digits[i] || Code==FString(TEXT("NumPad"))+Digits[i]) Char=TCHAR('0'+i);
 if (Key==EKeys::SpaceBar) Char=TCHAR(' ');
 else if (Key==EKeys::Hyphen) Char=bShift ? TCHAR('_') : TCHAR('-');
 else if (Key==EKeys::Period) Char=TCHAR('.');
 if (Char && Name.Len()<16) Name.AppendChar(Char);
}
void ACrosshairPlayerController::AdjustLan(int32 Offset,int32 Direction)
{
 auto* Replay=GetGameInstance()->GetSubsystem<UCrosshairReplaySubsystem>();
 FCrosshairSettings& S=Replay->GetSettings();
 FCrosshairLanOptions& L=S.Lan; FCrosshairBotMatchOptions& M=L.Match;
 const bool bTeams=M.Mode==ECrosshairMatchMode::TeamDeathmatch;
 switch (Offset)
 {
 case 0:
 {
  // Controller players cycle callsigns; keyboard players can also type (Enter).
  const TArray<FString>& Names=UCrosshairBotMatch::Callsigns();
  const int32 Current=Names.IndexOfByKey(UCrosshairBotMatch::CleanPlayerName(S.PlayerName));
  S.PlayerName=Names[(Current==INDEX_NONE ? 0 : Current+Direction+Names.Num())%Names.Num()];
  ServerSetPlayerName(S.PlayerName);
  break;
 }
 case 1: M.Mode=bTeams ? ECrosshairMatchMode::FreeForAll : ECrosshairMatchMode::TeamDeathmatch; break;
 case 2: M.Map=ECrosshairMatchMap((uint8(M.Map)+(Direction<0 ? 2 : 1))%3); break;
 case 3: M.bBots=!M.bBots; break;
 case 4: M.Difficulty=ECrosshairBotDifficulty((int32(M.Difficulty)+Direction+4)%4); break;
 case 5: (bTeams ? M.TeamSize : M.BotCount)+=Direction; break;
 case 6: (bTeams ? M.TeamScoreLimit : M.ScoreLimit)+=Direction*5; break;
 case 7: M.TimeLimitMinutes+=Direction; break;
 case 8: L.MaxPlayers+=Direction; break;
 default: return;
 }
 UCrosshairBotMatch::Validate(L);
 Replay->SaveSettings();
}
