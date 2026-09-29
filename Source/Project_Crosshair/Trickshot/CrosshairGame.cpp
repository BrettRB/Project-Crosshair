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
void ACrosshairPlayerController::FlushPressedKeys()
{
	if (auto* PracticePawn = Cast<ACrosshairCharacter>(GetPawn())) PracticePawn->StopActions();
	Super::FlushPressedKeys();
}
void ACrosshairPlayerController::ToggleMenu()
{
	FlushPressedKeys();
	bMenuOpen = !bMenuOpen;
	MenuSelection = 0;
	// Keep the world running: a successful replay must finish writing while the menu is open.
	SetInputMode(FInputModeGameOnly());
	bShowMouseCursor = false;
	if (!bMenuOpen) GetGameInstance()->GetSubsystem<UCrosshairReplaySubsystem>()->SaveSettings();
}
bool ACrosshairPlayerController::InputKey(const FInputKeyEventArgs& Params)
{
	const FKey Key = Params.Key;
	const bool Pressed = Params.Event == IE_Pressed;
	if (Pressed && (Key == EKeys::Escape || Key == EKeys::Gamepad_Special_Right)) { ToggleMenu(); return true; }
	if (!bMenuOpen) return Super::InputKey(Params);
	if (!Pressed) return true;
	const int32 Count = GetMenuRows().Num();
	if (Key == EKeys::Up || Key == EKeys::Gamepad_DPad_Up) MenuSelection = (MenuSelection + Count - 1) % Count;
	else if (Key == EKeys::Down || Key == EKeys::Gamepad_DPad_Down) MenuSelection = (MenuSelection + 1) % Count;
	else if (Key == EKeys::Left || Key == EKeys::Gamepad_DPad_Left) AdjustSelection(-1);
	else if (Key == EKeys::Right || Key == EKeys::Gamepad_DPad_Right) AdjustSelection(1);
	else if (Key == EKeys::Enter || Key == EKeys::Gamepad_FaceButton_Bottom) ActivateSelection();
	else if (Key == EKeys::Gamepad_FaceButton_Right) ToggleMenu();
	else if ((Key == EKeys::Delete || Key == EKeys::Gamepad_FaceButton_Left) && MenuSelection >= 13)
		GetGameInstance()->GetSubsystem<UCrosshairReplaySubsystem>()->DeleteSaved(MenuSelection - 13);
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
		TEXT("Save attempt start"), TEXT("Reset attempt"), TEXT("Remove aimed target"), TEXT("Clear all targets")
	};
	for (const auto& Entry : Replay->GetReplays()) Rows.Add(TEXT("Play: ") + Entry.RecordedAt);
	return Rows;
}
void ACrosshairPlayerController::AdjustSelection(int32 Direction)
{
	auto* Replay = GetGameInstance()->GetSubsystem<UCrosshairReplaySubsystem>();
	FCrosshairSettings& S = Replay->GetSettings();
	switch (MenuSelection)
	{
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
	if (Selected > 0 && Selected < 9) { AdjustSelection(1); return; }
	ToggleMenu();
	if (Selected == 0) { if (Replay->IsPlayback()) Replay->ReturnToPractice(); return; }
	if (Selected >= 13) { Replay->PlaySaved(Selected - 13); return; }
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
		DrawLine(X - 8, Y, X - 3, Y, White); DrawLine(X + 3, Y, X + 8, Y, White);
		DrawLine(X, Y - 8, X, Y - 3, White); DrawLine(X, Y + 3, X, Y + 8, White);
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
		const TArray<FString> Rows = PC->GetMenuRows();
		PC->MenuSelection = FMath::Clamp(PC->MenuSelection, 0, Rows.Num()-1);
		const int32 Visible = FMath::Max(1, FMath::FloorToInt((Canvas->ClipY - 220) / 26));
		const int32 First = FMath::Max(0, PC->MenuSelection - Visible + 1);
		DrawRect(FLinearColor(0.025f, .035f, .05f, .97f), 18, 92, FMath::Min(Canvas->ClipX-36, 740.f), Canvas->ClipY - 190);
		DrawText(TEXT("PRACTICE MENU | D-pad / arrows: select & adjust | A / Enter: activate"), Accent, 32, 104);
		DrawText(TEXT("B / Esc: close | X / Delete: delete selected replay"), White, 32, 128);
		for (int32 i = First; i < FMath::Min(Rows.Num(), First + Visible); ++i)
			DrawText((i == PC->MenuSelection ? TEXT("> ") : TEXT("  ")) + Rows[i], i == PC->MenuSelection ? Accent : White, 32, 157 + (i-First)*26);
	}
}
