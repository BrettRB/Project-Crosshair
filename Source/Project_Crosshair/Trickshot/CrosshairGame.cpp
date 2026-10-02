#include "CrosshairGame.h"
#include "CrosshairCharacter.h"
#include "CrosshairData.h"
#include "CrosshairPractice.h"
#include "CrosshairWeapon.h"
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
ACrosshairPlayerController::ACrosshairPlayerController()
{
	bShouldPerformFullTickWhenPaused = true;
}
void ACrosshairPlayerController::BeginPlay()
{
	Super::BeginPlay();
	if (IsLocalController()) ApplyGameplayInputMode();
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
void ACrosshairPlayerController::ToggleMenu()
{
	FlushPressedKeys();
	bMenuOpen = !bMenuOpen;
	MenuSelection = 0;
	MenuTab = 0;
	if (bMenuOpen)
		if (auto* PracticePawn = Cast<ACrosshairCharacter>(GetPawn())) PracticePawn->GetCharacterMovement()->StopMovementImmediately();
	if (bMenuOpen) MapChoice = GetWorld()->GetOutermost()->GetName().Contains(TEXT("L_Nuketown")) ? 1 : 0;
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
	else if (Key == EKeys::Up || Key == EKeys::Gamepad_DPad_Up) MenuSelection = Visible[(Index + Count - 1) % Count];
	else if (Key == EKeys::Down || Key == EKeys::Gamepad_DPad_Down) MenuSelection = Visible[(Index + 1) % Count];
	else if (Key == EKeys::Left || Key == EKeys::Gamepad_DPad_Left) AdjustSelection(-1);
	else if (Key == EKeys::Right || Key == EKeys::Gamepad_DPad_Right) AdjustSelection(1);
	else if (Key == EKeys::Enter || Key == EKeys::Gamepad_FaceButton_Bottom) ActivateSelection();
	else if (Key == EKeys::Gamepad_FaceButton_Right) ToggleMenu();
	else if ((Key == EKeys::Delete || Key == EKeys::Gamepad_FaceButton_Left) && MenuSelection >= FirstReplayRow)
		GetGameInstance()->GetSubsystem<UCrosshairReplaySubsystem>()->DeleteSaved(MenuSelection - FirstReplayRow);
	return true;
}
TArray<FString> ACrosshairPlayerController::GetMenuRows() const
{
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
		FString::Printf(TEXT("Map: %s (Enter / A to load)"), MapChoice == 0 ? TEXT("Testing Map") : TEXT("Nuketown")));
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
	switch (MenuTab)
	{
	case 1: return {1, 2, InvertHorizontalRow, InvertVerticalRow, CalibrationRow, 3, 4, 5, 6};
	case 2: return {7};
	case 3: return {11, 12};
	case 5: return {WeaponRow, SkinRow, WoodlandRow, DesertRow};
	case 4:
	{
		TArray<int32> Rows = {0};
		for (int32 i = FirstReplayRow; i < GetMenuRows().Num(); ++i) Rows.Add(i);
		return Rows;
	}
	default: return {0, 9, 10, 8, WeaponRow, MapRow, QuitRow};
	}
}
void ACrosshairPlayerController::SetMenuTab(int32 Tab)
{
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
	auto* Replay = GetGameInstance()->GetSubsystem<UCrosshairReplaySubsystem>();
	FCrosshairSettings& S = Replay->GetSettings();
	switch (MenuSelection)
	{
	case MapRow:
		if (!Replay->IsPlayback() && !Replay->IsFinishing()) MapChoice = (MapChoice + Direction + 2) % 2;
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
		Replay->ChangePracticeMap(FName(MapChoice == 0 ? TEXT("/Game/Crosshair/Maps/L_Practice") : TEXT("/Game/Crosshair/Maps/L_Nuketown")));
		return;
	}
	if (Selected >= SkinRow && Selected <= DesertRow)
	{
		if (!Replay->IsPlayback() && !Replay->IsFinishing())
			if (auto* PracticePawn = Cast<ACrosshairCharacter>(GetPawn()))
				if (auto* Current = PracticePawn->Inventory->GetCurrent())
				{
					const int32 Index = Selected - SkinRow - 1;
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
	if (Selected == WeaponRow || Selected == InvertHorizontalRow || Selected == InvertVerticalRow) { AdjustSelection(1); return; }
	if (Selected > 0 && Selected < 9) { AdjustSelection(1); return; }
	ToggleMenu();
	if (Selected == 0) { if (Replay->IsPlayback()) Replay->ReturnToPractice(); return; }
	if (Selected >= FirstReplayRow) { Replay->PlaySaved(Selected - FirstReplayRow); return; }
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
	const FLinearColor White(.9f, .94f, 1.f), Accent(.2f, .85f, 1.f);
	DrawText(TEXT("CROSSHAIR | Classic practice"), Accent, 24, 18, nullptr, 1.3f);
	DrawText(Replay->IsPlayback() ? TEXT("FIRST-PERSON REPLAY | Menu: Esc / Start") : TEXT("Menu: Esc / Start   |   Reset: T / D-pad Down"), White, 24, 46);
	DrawText(Replay->Status, White, 24, 68);
	if (PracticePawn)
	{
		if (auto* W = PracticePawn->Inventory->GetCurrent())
		{
			const FString Name = W->Definition ? W->Definition->DisplayName.ToString() : TEXT("Weapon");
			DrawText(FString::Printf(TEXT("%s   %d%s"), *Name, W->Ammo, W->bReloading ? TEXT("   RELOADING") : TEXT("")), White, 24, Canvas->ClipY - 85, nullptr, 1.4f);
		}
		if (PracticePawn->Placement->bPlacing)
		{
			const bool Valid = PracticePawn->Placement->bValidPlacement;
			DrawText(Valid ? TEXT("CLEAR: Fire to place | Reload / D-pad Left: rotate | Aim: cancel") : TEXT("BLOCKED: aim at clear ground"), Valid ? FLinearColor::Green : FLinearColor::Red, 24, 100);
		}
		if (GetWorld()->GetTimeSeconds() < PracticePawn->NoticeUntil) DrawText(PracticePawn->Notice, Accent, 24, 124);
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
			DrawLine(X-14,Y-14,X-7,Y-7,Accent,2); DrawLine(X+7,Y+7,X+14,Y+14,Accent,2);
			DrawLine(X-14,Y+14,X-7,Y+7,Accent,2); DrawLine(X+7,Y-7,X+14,Y-14,Accent,2);
		}
	}
	DrawText(TEXT("WASD / Left stick: move | Mouse / Right stick: look | Space / A: jump | Shift / L3: sprint | C / B: crouch"), White, 24, Canvas->ClipY - 55);
	DrawText(TEXT("LMB / RT: fire | RMB / LT: aim | R / X: reload | Q / Y: switch | P / D-pad Up: target | K / D-pad Right: save start"), White, 24, Canvas->ClipY - 32);

	if (PC && PC->bMenuOpen)
	{
		const float Scale = FMath::Min(Canvas->ClipX / 1280.f, Canvas->ClipY / 720.f);
		const float Left = (Canvas->ClipX - 1000.f * Scale) * .5f;
		const float Top = (Canvas->ClipY - 560.f * Scale) * .5f;
		const FLinearColor Panel(.022f, .03f, .045f, .99f), Muted(.48f, .55f, .65f);
		auto Rect = [&](FLinearColor Color, float RX, float RY, float W, float H)
		{ DrawRect(Color, Left + RX * Scale, Top + RY * Scale, W * Scale, H * Scale); };
		auto Text = [&](const FString& Label, FLinearColor Color, float TX, float TY, float Size = 1.f)
		{ DrawText(Label, Color, Left + TX * Scale, Top + TY * Scale, nullptr, Size * Scale); };
		auto HitBox = [&](FName Name, float HX, float HY, float W, float H)
		{ AddHitBox(FVector2D(Left + HX * Scale, Top + HY * Scale), FVector2D(W * Scale, H * Scale), Name, true); };
		DrawRect(FLinearColor(0, 0, 0, .72f), 0, 0, Canvas->ClipX, Canvas->ClipY);
		Rect(Panel, 0, 0, 1000, 560);
		Rect(Accent, 0, 0, 1000, 3);
		Text(TEXT("CROSSHAIR"), Accent, 28, 22, 1.7f);
		Text(Replay->IsPlayback() ? TEXT("Replay menu") : TEXT("Practice menu"), White, 28, 52, 1.1f);
		Text(TEXT("Esc / Start to resume"), Muted, 755, 34);
		Rect(FLinearColor(.07f, .09f, .12f), 0, 88, 1000, 1);
		const TCHAR* Tabs[] = {TEXT("Practice"), TEXT("Controls"), TEXT("Display"), TEXT("Targets"), TEXT("Replays"), TEXT("Camos")};
		for (int32 Tab = 0; Tab < ACrosshairPlayerController::MenuTabCount; ++Tab)
		{
			const float TY = 108 + Tab * 54;
			const bool Selected = PC->MenuTab == Tab;
			if (Selected) { Rect(FLinearColor(.07f, .16f, .21f), 16, TY, 178, 44); Rect(Accent, 16, TY, 3, 44); }
			Text(Tabs[Tab], Selected ? Accent : Muted, 34, TY + 12, 1.15f);
			HitBox(FName(*FString::Printf(TEXT("Tab_%d"), Tab)), 16, TY, 178, 44);
		}
		const TArray<FString> Rows = PC->GetMenuRows();
		const TArray<int32> Visible = PC->GetVisibleMenuRows();
		int32 Index = Visible.IndexOfByKey(PC->MenuSelection);
		if (Index == INDEX_NONE) { PC->MenuSelection = Visible[0]; Index = 0; }
		const int32 PageSize = PC->MenuTab == 1 ? 9 : 7;
		const float RowPitch = PC->MenuTab == 1 ? 34.f : 44.f;
		const float RowHeight = RowPitch - 4.f;
		const int32 First = FMath::Max(0, Index - PageSize + 1);
		Text(Tabs[PC->MenuTab], White, 224, 104, 1.4f);
		if (PC->MenuTab == 4 && Visible.Num() == 1) Text(TEXT("No saved replays yet. Land a shot to record an attempt."), Muted, 224, 176);
		for (int32 i = First; i < FMath::Min(Visible.Num(), First + PageSize); ++i)
		{
			const int32 Row = Visible[i];
			const float RY = 146 + (i - First) * RowPitch;
			const bool Selected = Row == PC->MenuSelection;
			Rect(Selected ? FLinearColor(.085f, .16f, .21f) : FLinearColor(.04f, .055f, .075f), 216, RY, 756, RowHeight);
			Text(Rows[Row], Selected ? Accent : White, 232, RY + 11, 1.05f);
			HitBox(FName(*FString::Printf(TEXT("Row_%d"), Row)), 216, RY, 650, RowHeight);
			if ((Row >= 1 && Row <= 8) || Row == PC->WeaponRow || Row == PC->MapRow || Row == PC->InvertHorizontalRow || Row == PC->InvertVerticalRow)
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
		if (PC->MenuTab == 3) Text(TEXT("Place targets in game with P / D-pad Up, then Fire to confirm."), Muted, 224, 468);
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
