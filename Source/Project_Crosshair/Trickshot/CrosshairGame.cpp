#include "CrosshairGame.h"
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
#include "Engine/Canvas.h"
#include "Engine/GameInstance.h"
#include "InputKeyEventArgs.h"
#include "Camera/PlayerCameraManager.h"
#include "Kismet/GameplayStatics.h"
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
	if (IsLocalController()) ApplyGameplayInputMode();
 auto* Library=GetGameInstance()->GetSubsystem<UCrosshairMapLibrary>();
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
void ACrosshairPlayerController::ShowHome()
{
 if (!bMenuOpen) ToggleMenu();
 bHomeScreen=bFromHome=true; MenuSelection=0;
}
void ACrosshairPlayerController::StartPlaying()
{
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
	if (bMenuOpen)
		if (auto* PracticePawn = Cast<ACrosshairCharacter>(GetPawn())) PracticePawn->GetCharacterMovement()->StopMovementImmediately();
	if (bMenuOpen)
 { MapChoice=GetWorld()->GetOutermost()->GetName().Contains(TEXT("L_Nuketown")) ? 1 : 0;
   const auto* Library=GetGameInstance()->GetSubsystem<UCrosshairMapLibrary>();
   for (int32 i=0;i<Library->Maps.Num();++i) if (Library->Maps[i].Id==Library->ActiveMapId) MapChoice=i+2;
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
}
bool ACrosshairPlayerController::InputKey(const FInputKeyEventArgs& Params)
{
	const FKey Key = Params.Key;
	const bool Pressed = Params.Event == IE_Pressed;
	if (Pressed && (Key == EKeys::Escape || Key == EKeys::Gamepad_Special_Right)) { ToggleMenu(); return true; }
	if (bMenuOpen && ControllerCalibrationStep && Params.Event == IE_Axis && (Key == EKeys::Gamepad_RightX || Key == EKeys::Gamepad_RightY))
	{
		CalibrateAxis(Key, Params.AmountDepressed);
		return true;
	}
	if (!bMenuOpen)
	{
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
int32 ACrosshairPlayerController::GetFirstReplayRow() const { return GetImportRow()+1; }
TArray<FString> ACrosshairPlayerController::GetMenuRows() const
{
	if (bHomeScreen) return {TEXT("Play"),TEXT("Weapon classes"),TEXT("Maps / Import"),TEXT("Settings"),TEXT("Quit to Desktop")};
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
 Rows.Add(TEXT("Home")); Rows.Add(TEXT("Import map from local file"));
 for (const auto& Entry : Replay->GetReplays()) Rows.Add(TEXT("Play: ") + Entry.RecordedAt);
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
	if (bHomeScreen) return {0,1,2,3,4};
 switch (MenuTab)
 {
	case 1: return {1, 2, InvertHorizontalRow, InvertVerticalRow, CalibrationRow, 3, 4, 5, 6, GetSandboxRow()};
	case 2: return {7};
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
 auto* ClassReplay=GetGameInstance()->GetSubsystem<UCrosshairReplaySubsystem>(); auto& Settings=ClassReplay->GetSettings();
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
 if (bHomeScreen)
 { if (Selected==0) StartPlaying(); else if (Selected==1) SetMenuTab(6); else if (Selected==2) SetMenuTab(7); else if (Selected==3) SetMenuTab(1); else { bFromHome=false; MenuSelection=QuitRow; ActivateSelection(); } return; }
 if (Selected==GetHomeRow()) { if (!Replay->IsFinishing() && !Replay->IsPlayback()) ShowHome(); return; }
 if (Selected==GetImportRow()) { GetGameInstance()->GetSubsystem<UCrosshairMapLibrary>()->ShowImportDialog(this); return; }
 if (Selected==GetSandboxRow()) { AdjustSelection(1); return; }
 const int32 ClassOffset=Selected-GetClassRow();
 if (ClassOffset>=0 && ClassOffset<5)
 {
  if (Replay->IsPlayback() || Replay->IsFinishing()) return;
  if (ClassOffset<4) { if (MenuTab!=6) SetMenuTab(6); else AdjustSelection(1); return; }
  Replay->GetSettings().ActiveClass=EditingClass;
  if (auto* P=Cast<ACrosshairCharacter>(GetPawn())) P->Inventory->ApplyClass();
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
	if (!PC || !PC->bMenuOpen)
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
	if (!PC || !PC->bMenuOpen)
	{
		const float Bottom=Canvas->ClipY/HUDScale-102;
		Panel(24,Bottom,470,78);
		Label(TEXT("P target   /   K save position   /   T reset"),White,40,Bottom+9,.7f);
		Label(TEXT("G / RB hold + release throw   /   F / LB lethal"),Accent,40,Bottom+31,.65f);
		Label(TEXT("Space / A jump + mantle   /   Esc / Start menu"),Muted,40,Bottom+53,.65f);
	}

	if (PC && PC->bMenuOpen)
	{
		const float Scale = FMath::Min(Canvas->ClipX / 1280.f, Canvas->ClipY / 720.f);
		const float Left = (Canvas->ClipX - 1000.f * Scale) * .5f;
		const float Top = (Canvas->ClipY - 560.f * Scale) * .5f;
		const FLinearColor MenuPanel(.022f,.03f,.045f,.99f);
		auto Rect = [&](FLinearColor Color, float RX, float RY, float W, float H)
		{ DrawRect(Color, Left + RX * Scale, Top + RY * Scale, W * Scale, H * Scale); };
		auto Text = [&](const FString& Label, FLinearColor Color, float TX, float TY, float Size = 1.f)
		{ DrawText(Label, Color, Left + TX * Scale, Top + TY * Scale, GEngine->GetMediumFont(), Size * 1.1f * Scale); };
		auto HitBox = [&](FName Name, float HX, float HY, float W, float H)
		{ AddHitBox(FVector2D(Left + HX * Scale, Top + HY * Scale), FVector2D(W * Scale, H * Scale), Name, true); };
		DrawRect(FLinearColor(0, 0, 0, .72f), 0, 0, Canvas->ClipX, Canvas->ClipY);
		Rect(MenuPanel, 0, 0, 1000, 560);
		Rect(Accent, 0, 0, 1000, 3);
		Text(TEXT("CROSSHAIR"), White, 28, 18, 1.7f);
		Text(Replay->IsPlayback() ? TEXT("REPLAY SESSION") : TEXT("PRACTICE SESSION"), Muted, 28, 52,.85f);
		Text(PC->bHomeScreen ? TEXT("HOME") : PC->bFromHome ? TEXT("Esc / B to home") : TEXT("Esc / Start to resume"), Muted, 755, 34);
		Rect(FLinearColor(.07f, .09f, .12f), 0, 88, 1000, 1);
		if (PC->bHomeScreen)
        {
         Text(TEXT("WELCOME TO CROSSHAIR"),White,230,115,1.6f);
         Text(TEXT("Build your class. Pick a map. Land your next trickshot."),Muted,230,156,.95f);
         auto HomeRows=PC->GetMenuRows();
         for (int32 i=0;i<HomeRows.Num();++i) { const float HomeY=205+i*48; Rect(PC->MenuSelection==i ? FLinearColor(.085f,.16f,.21f) : FLinearColor(.04f,.055f,.075f),230,HomeY,540,40); Text(HomeRows[i],PC->MenuSelection==i ? Accent : White,250,HomeY+9,1.1f); HitBox(FName(*FString::Printf(TEXT("Row_%d"),i)),230,HomeY,540,40); }
         Text(TEXT("Arrows / D-pad select   |   Enter / A play or open"),Muted,230,490,.85f); return;
        }
 const TCHAR* Tabs[] = {TEXT("Practice"), TEXT("Controls"), TEXT("Display"), TEXT("Targets"), TEXT("Replays"), TEXT("Camos"),TEXT("Classes"),TEXT("Maps")};
		for (int32 Tab = 0; Tab < ACrosshairPlayerController::MenuTabCount; ++Tab)
		{
			const float TY = 108 + Tab * 46;
			const bool Selected = PC->MenuTab == Tab;
			if (Selected) { Rect(FLinearColor(.07f, .16f, .21f), 16, TY, 178, 40); Rect(Accent, 16, TY, 3, 40); }
			Text(Tabs[Tab], Selected ? Accent : Muted, 34, TY + 12, 1.15f);
			HitBox(FName(*FString::Printf(TEXT("Tab_%d"), Tab)), 16, TY, 178, 40);
		}
		const TArray<FString> Rows = PC->GetMenuRows();
		const TArray<int32> Visible = PC->GetVisibleMenuRows();
		int32 Index = Visible.IndexOfByKey(PC->MenuSelection);
		if (Index == INDEX_NONE) { PC->MenuSelection = Visible[0]; Index = 0; }
		const int32 PageSize = PC->MenuTab == 1 ? 9 : 7;
		const float RowPitch = PC->MenuTab == 1 ? 34.f : 44.f;
		const float RowHeight = RowPitch - 4.f;
		const int32 First = FMath::Max(0, Index - PageSize + 1);
		Text(Tabs[PC->MenuTab], White, 224, 102, 1.4f);
		const TCHAR* Descriptions[]={TEXT("Set up your next attempt"),TEXT("Tune your mouse and controller"),TEXT("Adjust your field of view"),TEXT("Manage your practice targets"),TEXT("Watch your saved attempts"),TEXT("Choose a finish for your weapon"),TEXT("Build and equip your weapon class"),TEXT("Play and import local maps")};
		Text(Descriptions[PC->MenuTab],Muted,450,109,.85f);
		if (PC->MenuTab == 4 && Visible.Num() == 1) Text(TEXT("No saved replays yet. Land a shot to record an attempt."), Muted, 224, 176);
		for (int32 i = First; i < FMath::Min(Visible.Num(), First + PageSize); ++i)
		{
			const int32 Row = Visible[i];
			const float RY = 146 + (i - First) * RowPitch;
			const bool Selected = Row == PC->MenuSelection;
			Rect(Selected ? FLinearColor(.085f, .16f, .21f) : FLinearColor(.04f, .055f, .075f), 216, RY, 756, RowHeight);
			FString RowLabel=Rows[Row],Value;
			if (Rows[Row].Split(TEXT(": "),&RowLabel,&Value) && Row!=PC->CalibrationRow)
			{
				if (PC->MenuTab==5 && Row!=PC->WeaponRow) { RowLabel=Value; Value=TEXT(""); }
				Text(RowLabel,Selected ? Accent : White,232,RY+10,1.f);
				Text(Value,White,620,RY+10,.9f);
			}
			else Text(Rows[Row],Selected ? Accent : White,232,RY+10,1.f);
			HitBox(FName(*FString::Printf(TEXT("Row_%d"), Row)), 216, RY, 650, RowHeight);
			if ((Row >= 1 && Row <= 8) || Row == PC->WeaponRow || Row == PC->GetLethalRow() || (Row>=PC->GetClassRow() && Row<PC->GetClassRow()+4) || Row==PC->GetSandboxRow() || Row == PC->MapRow || Row == PC->InvertHorizontalRow || Row == PC->InvertVerticalRow)
			{
				Text(TEXT("<"), Muted, 895, RY + 10, 1.2f);
				Text(TEXT(">"), Muted, 941, RY + 10, 1.2f);
				HitBox(FName(*FString::Printf(TEXT("Less_%d"), Row)), 876, RY, 42, RowHeight);
				HitBox(FName(*FString::Printf(TEXT("More_%d"), Row)), 924, RY, 48, RowHeight);
	}
}
		if (PC->MenuTab == 0) Text(TEXT("Save your spot on a balcony or platform, then reset for each attempt."), Muted, 224, 468);
		if (PC->MenuTab == 1) Text(TEXT("Left / Right adjusts settings. Aim sensitivity applies while aiming."), Muted, 224, 468);
		if (PC->MenuTab == 5) Text(TEXT("Enter / A or click a camo to equip it. Saved separately for each weapon."), Muted, 224, 468);
		if (PC->MenuTab == 3) Text(TEXT("Enable sandbox editing, then P / D-pad Up places targets."), Muted, 224, 468);
		if (PC->MenuTab==6) Text(TEXT("Choose two weapons and a lethal. Equip applies the class and restocks."),Muted,224,468,.85f);
        if (PC->MenuTab==7) Text(TEXT("Local OBJ geometry / map.json packages. Import to add a playable map."),Muted,224,468,.85f);
        if (PC->MenuTab == 4) Text(TEXT("X / Delete removes the selected replay."), Muted, 224, 468);
		Rect(FLinearColor(.07f, .09f, .12f), 20, 504, 960, 1);
		Text(TEXT("Q / E, Tab or LB / RB: tabs    |    Arrows / D-pad: select    |    Enter / A: activate"), Muted, 28, 526);
	}
}
void ACrosshairHUD::NotifyHitBoxClick(FName BoxName)
{
	Super::NotifyHitBoxClick(BoxName);
	auto* PC = Cast<ACrosshairPlayerController>(PlayerOwner);
	if (!PC || !PC->bMenuOpen) return;
	const FString Name = BoxName.ToString();
	FString Kind, Value;
	if (!Name.Split(TEXT("_"), &Kind, &Value)) return;
	const int32 Index = FCString::Atoi(*Value);
	if (Kind == TEXT("Tab")) PC->SetMenuTab(Index);
	else if (Kind == TEXT("Row")) PC->ActivateMenuRow(Index);
	else if (Kind == TEXT("Less")) PC->AdjustMenuRow(Index, -1);
	else if (Kind == TEXT("More")) PC->AdjustMenuRow(Index, 1);
}
