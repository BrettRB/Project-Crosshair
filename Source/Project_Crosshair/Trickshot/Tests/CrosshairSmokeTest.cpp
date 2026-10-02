#include "CrosshairSmokeTest.h"
#include "../CrosshairCharacter.h"
#include "../CrosshairData.h"
#include "../CrosshairDummy.h"
#include "../CrosshairGame.h"
#include "../CrosshairPractice.h"
#include "../CrosshairReplaySubsystem.h"
#include "../CrosshairWeapon.h"
#include "Engine/GameInstance.h"
#include "Engine/StaticMesh.h"
#include "Engine/DemoNetDriver.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Camera/CameraComponent.h"
#include "InputKeyEventArgs.h"
#include "Engine/GameViewportClient.h"
#include "GameFramework/PlayerInput.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "HAL/PlatformMisc.h"
#include "EngineUtils.h"
#include "UnrealClient.h"
#include "EnhancedPlayerInput.h"
#include "Engine/DamageEvents.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/OverlapResult.h"
#include "GameFramework/PlayerStart.h"
#include "Misc/Paths.h"
#if WITH_EDITOR
#include "ShaderCompiler.h"
#endif
#include "Components/SkeletalMeshComponent.h"
#include "Components/CapsuleComponent.h"

bool UCrosshairSmokeTest::ShouldCreateSubsystem(UObject* Outer) const
{
#if UE_BUILD_SHIPPING
	return false;
#else
	return FParse::Param(FCommandLine::Get(), TEXT("CrosshairSmoke")) || FParse::Param(FCommandLine::Get(), TEXT("CrosshairSmokeSaved"));
#endif
}
void UCrosshairSmokeTest::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	Collection.InitializeDependency<UCrosshairReplaySubsystem>();
	Started = StepStarted = FPlatformTime::Seconds();
}
bool UCrosshairSmokeTest::Check(bool Condition, const TCHAR* Message)
{
	if (!Condition)
	{
		UE_LOG(LogTemp, Error, TEXT("CROSSHAIR_SMOKE_FAIL step=%d %s"), Step, Message);
		bFinished = true;
		FPlatformMisc::RequestExitWithStatus(false, 1);
	}
	else UE_LOG(LogTemp, Display, TEXT("CROSSHAIR_SMOKE_PASS %s"), Message);
	return Condition;
}
void UCrosshairSmokeTest::Next(int32 NewStep) { Step = NewStep; StepStarted = FPlatformTime::Seconds(); }
void UCrosshairSmokeTest::Tick(float DeltaSeconds)
{
	UWorld* World = GetWorld();
	if (!World || !World->IsGameWorld() || bFinished) return;
	const double Now = FPlatformTime::Seconds(), Elapsed = Now - StepStarted;
	if (Now - Started > ((FParse::Param(FCommandLine::Get(), TEXT("CrosshairMapVisual")) || FParse::Param(FCommandLine::Get(), TEXT("CrosshairVisual")) || FParse::Param(FCommandLine::Get(), TEXT("CrosshairFollowupVisual"))) ? 300 : 100)) { Check(false, TEXT("Runtime test timed out")); return; }
#if WITH_EDITOR
	// Screenshots must show finished shaders, rather than temporary default materials.
	if ((FParse::Param(FCommandLine::Get(), TEXT("CrosshairMapVisual")) || FParse::Param(FCommandLine::Get(), TEXT("CrosshairVisual")) || FParse::Param(FCommandLine::Get(), TEXT("CrosshairFollowupVisual")))
		&& (Step == 0 || Step == 61 || (Step >= 86 && Step <= 97) || (Step >= 30 && Step <= 37))
		&& GShaderCompilingManager && GShaderCompilingManager->IsCompiling()) return;
#endif
	auto* Replay = GetGameInstance()->GetSubsystem<UCrosshairReplaySubsystem>();
	auto* PC = Cast<ACrosshairPlayerController>(World->GetFirstPlayerController());
	auto* Pawn = PC ? Cast<ACrosshairCharacter>(PC->GetPawn()) : nullptr;
	auto Key = [PC](FKey K, EInputEvent Event) { if (PC) PC->InputKey(FInputKeyEventArgs::CreateSimulated(K, Event, Event == IE_Released ? 0.f : 1.f)); };
	if (Step >= 50 && Step < 60)
	{
		if (World->GetDemoNetDriver() && World->GetDemoNetDriver()->IsPlaying())
		{
			if (auto* View = PC ? Cast<ACrosshairCharacter>(PC->GetViewTarget()) : nullptr)
			{
				if (!bSawPlayback)
				{
					if (!Check(View->Inventory->GetCurrent() != nullptr, TEXT("Replay restores equipped weapon"))) return;
					if (!Check(View->GetFirstPersonCameraComponent()->GetComponentLocation().Size() > 1, TEXT("Replay restores camera"))) return;
					bSawPlayback = true;
					if (FParse::Param(FCommandLine::Get(), TEXT("CrosshairCapture"))) FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir() / TEXT("Screenshots/CrosshairReplay.png"), false, false);
				}
			}
			return;
		}
		if (!bSawPlayback || !Pawn || !Pawn->CanAct() || Replay->IsPlayback()) return;
		if (Step == 50)
		{
			if (!Check(Replay->GetReplays().Num() > 0, TEXT("Successful recording cataloged"))) return;
			if (!Check(Pawn->Placement->GetLayout().Num() == TargetCount, TEXT("Target layout restored after replay"))) return;
			if (!Check(FVector::Dist(Pawn->GetActorLocation(), Pawn->Attempt->StartTransform.GetLocation()) < 150, TEXT("Attempt start restored"))) return;
			Replay->SaveSettings();
			UE_LOG(LogTemp, Display, TEXT("CROSSHAIR_SMOKE_OK recording retained for restart test"));
			bFinished = true; FPlatformMisc::RequestExitWithStatus(false, 0);
		}
		else if (Step == 51)
		{
			Replay->DeleteSaved(0); Next(52);
		}
		else if (Step == 52 && Replay->GetReplays().IsEmpty())
		{
			UE_LOG(LogTemp, Display, TEXT("CROSSHAIR_SMOKE_SAVED_OK replay survived process restart and deletion succeeded"));
			bFinished = true; FPlatformMisc::RequestExitWithStatus(false, 0);
		}
		return;
	}
	if (!Pawn || !PC || !Pawn->Inventory->GetCurrent()) return;
	auto* Weapon = Pawn->Inventory->GetCurrent();
	switch (Step)
	{

	case 76:
	{
		const float FrameTime = 1.f/60;
		auto Axis = [PC](FKey InputKey, float Value)
		{
			PC->InputKey(FInputKeyEventArgs::CreateSimulated(InputKey, IE_Axis, Value, 1));
		};
		auto View = [PC]() { const FRotator R=PC->GetControlRotation(); return FVector2D(FRotator::NormalizeAxis(R.Yaw), FRotator::NormalizeAxis(R.Pitch)); };
		// Reproduce stale/inverted legacy properties: the native look path must ignore them.
		FInputAxisProperties Legacy;
		Legacy.bInvert=true; Legacy.Sensitivity=.001f; Legacy.DeadZone=.9f;
		for (FKey K : {EKeys::MouseX,EKeys::MouseY,EKeys::Gamepad_RightX,EKeys::Gamepad_RightY}) PC->PlayerInput->SetAxisProperties(K,Legacy);
		PC->SetControlRotation(FRotator::ZeroRotator);
		Axis(EKeys::MouseX,100); Axis(EKeys::MouseY,50); PC->UpdateRotation(FrameTime);
		if (!Check(View().Equals(FVector2D(12,6),.01),TEXT("Mouse alone turns both camera axes despite legacy input properties"))) return;
		PC->UpdateRotation(FrameTime);
		if (!Check(View().Equals(FVector2D(12,6),.01),TEXT("Mouse displacement is consumed once"))) return;
		PC->SetControlRotation(FRotator::ZeroRotator);
		Axis(EKeys::Gamepad_RightX,.5f); Axis(EKeys::Gamepad_RightY,.5f);
		Axis(EKeys::MouseX,100); Axis(EKeys::MouseY,50);
		PC->UpdateRotation(FrameTime);
		const FVector2D Stick = CrosshairRules::StickDelta(FVector2D(.5,.5),Replay->GetSettings(),0,FrameTime);
		if (!Check(View().Equals(FVector2D(12,6)+Stick,.01),TEXT("Mouse and controller look combine in the same frame"))) return;
		Replay->GetSettings().bInvertControllerHorizontal=true;
		Replay->GetSettings().bInvertControllerVertical=true;
		PC->SetControlRotation(FRotator::ZeroRotator);
		Axis(EKeys::MouseX,100); Axis(EKeys::MouseY,50); PC->UpdateRotation(FrameTime);
		if (!Check(View().Equals(FVector2D(12,6)-Stick,.01),TEXT("Controller inversion leaves mouse direction unchanged"))) return;
		Replay->GetSettings().bInvertControllerHorizontal=false;
		Replay->GetSettings().bInvertControllerVertical=false;
		PC->FlushPressedKeys(); PC->SetControlRotation(FRotator::ZeroRotator);
		Key(EKeys::Escape,IE_Pressed);
		Axis(EKeys::MouseX,100); Axis(EKeys::Gamepad_RightX,1); PC->UpdateRotation(FrameTime);
		if (!Check(View().IsNearlyZero(),TEXT("Menu blocks both look devices without buffering a turn"))) return;
		Key(EKeys::Escape,IE_Pressed);
		if (!Check(!PC->bShowMouseCursor && GetGameInstance()->GetGameViewportClient()->GetMouseCaptureMode() == EMouseCaptureMode::CapturePermanently_IncludingInitialMouseDown,TEXT("Closing menu restores permanent mouse capture"))) return;
		Axis(EKeys::MouseX,100); PC->UpdateRotation(FrameTime);
		if (!Check(View().Equals(FVector2D(12,0),.01),TEXT("Mouse look works after closing the pause menu"))) return;
		PC->FlushPressedKeys(); PC->SetControlRotation(FRotator::ZeroRotator);
		Next(80); return;
	}
	case 80:
	{
		Replay->GetSettings().bInvertControllerHorizontal = (TargetCount & 1) != 0;
		Replay->GetSettings().bInvertControllerVertical = (TargetCount & 2) != 0;
		PC->SetControlRotation(FRotator::ZeroRotator);
		PC->InputKey(FInputKeyEventArgs::CreateSimulated(EKeys::Gamepad_RightX, IE_Axis, .5f, 1));
		PC->InputKey(FInputKeyEventArgs::CreateSimulated(EKeys::Gamepad_RightY, IE_Axis, .5f, 1));
		Next(81); return;
	}
	case 81:
	case 83:
	{
		if (Elapsed < .15) return;
		const float Direction = Step == 81 ? 1.f : -1.f;
		const float YawSign = (TargetCount & 1) ? -Direction : Direction;
		const float PitchSign = (TargetCount & 2) ? -Direction : Direction;
		const FRotator View = PC->GetControlRotation();
		UE_LOG(LogTemp, Display, TEXT("CONTROLLER_DIRECTION flags=%d direction=%.0f yaw=%.2f pitch=%.2f"), TargetCount, Direction, FRotator::NormalizeAxis(View.Yaw), FRotator::NormalizeAxis(View.Pitch));
		if (!Check(FRotator::NormalizeAxis(View.Yaw) * YawSign > 1 && FRotator::NormalizeAxis(View.Pitch) * PitchSign > 1, TEXT("Physical right-stick keys respect independent inversion on both axes"))) return;
		PC->InputKey(FInputKeyEventArgs::CreateSimulated(EKeys::Gamepad_RightX, IE_Axis, 0.f, 1));
		PC->InputKey(FInputKeyEventArgs::CreateSimulated(EKeys::Gamepad_RightY, IE_Axis, 0.f, 1));
		if (Step == 81) Next(82);
		else { ++TargetCount; Next(TargetCount < 4 ? 80 : 84); }
		return;
	}
	case 82:
		PC->SetControlRotation(FRotator::ZeroRotator);
		PC->InputKey(FInputKeyEventArgs::CreateSimulated(EKeys::Gamepad_RightX, IE_Axis, -.5f, 1));
		PC->InputKey(FInputKeyEventArgs::CreateSimulated(EKeys::Gamepad_RightY, IE_Axis, -.5f, 1));
		Next(83); return;
	case 84:
	{
		// Mouse and controller activation use the same category rows.
		Key(EKeys::Escape, IE_Pressed); PC->SetMenuTab(1);
		PC->ActivateMenuRow(PC->InvertHorizontalRow);
		if (!Check(!Replay->GetSettings().bInvertControllerHorizontal && Replay->GetSettings().bInvertControllerVertical, TEXT("Horizontal menu toggle leaves vertical unchanged"))) return;
		PC->AdjustMenuRow(PC->InvertVerticalRow, -1);
		if (!Check(!Replay->GetSettings().bInvertControllerHorizontal && !Replay->GetSettings().bInvertControllerVertical, TEXT("Both inversion settings can return to off"))) return;
		auto* Disk = Cast<UCrosshairSaveGame>(UGameplayStatics::LoadGameFromSlot(TEXT("CrosshairSmoke_v1"), 0));
		if (!Check(Disk && !Disk->Settings.bInvertControllerHorizontal && !Disk->Settings.bInvertControllerVertical, TEXT("Inversion settings persist to the local profile"))) return;
		PC->SetMenuTab(1); PC->ActivateMenuRow(PC->CalibrationRow);
		const auto AxisCalibration = [PC](FKey K, float V) { PC->InputKey(FInputKeyEventArgs::CreateSimulated(K, IE_Axis, V, 1)); };
		AxisCalibration(EKeys::Gamepad_RightX,-.8f);
		if (!Check(PC->ControllerCalibrationStep == 2, TEXT("Calibration captures a reversed hardware horizontal direction"))) return;
		AxisCalibration(EKeys::Gamepad_RightX,0);
		AxisCalibration(EKeys::Gamepad_RightY,-.8f);
		AxisCalibration(EKeys::Gamepad_RightY,0);
		if (!Check(!PC->ControllerCalibrationStep && Replay->GetSettings().ControllerAxisDirection.Equals(FVector2D(-1,-1)), TEXT("Calibration learns and saves both reversed device axes"))) return;
		const FVector2D Calibrated = CrosshairRules::StickDelta(FVector2D(-.8,-.8),Replay->GetSettings(),0,.016f);
		if (!Check(Calibrated.X > 0 && Calibrated.Y > 0, TEXT("Calibrated physical right/up produce right/up camera direction"))) return;
		Disk = Cast<UCrosshairSaveGame>(UGameplayStatics::LoadGameFromSlot(TEXT("CrosshairSmoke_v1"),0));
		if (!Check(Disk && Disk->Settings.ControllerAxisDirection.Equals(FVector2D(-1,-1)), TEXT("Controller hardware calibration persists to disk"))) return;
		Key(EKeys::Escape,IE_Pressed); PC->SetControlRotation(FRotator::ZeroRotator);
		AxisCalibration(EKeys::Gamepad_RightX,-.8f); AxisCalibration(EKeys::Gamepad_RightY,-.8f); PC->UpdateRotation(1.f/60.f);
		if (!Check(PC->GetControlRotation().Yaw > 0 && PC->GetControlRotation().Pitch > 0, TEXT("Calibrated reversed axes turn the actual camera right and up"))) return;
		PC->FlushPressedKeys(); const float BeforeMouse = PC->GetControlRotation().Yaw;
		AxisCalibration(EKeys::MouseX,100); PC->UpdateRotation(1.f/60.f);
		if (!Check(FMath::IsNearlyEqual(PC->GetControlRotation().Yaw-BeforeMouse,12.f,.01f), TEXT("Mouse direction remains independent of device calibration"))) return;
		Key(EKeys::Escape,IE_Pressed); PC->SetMenuTab(1); PC->ActivateMenuRow(PC->CalibrationRow);
		AxisCalibration(EKeys::Gamepad_RightX,.8f); AxisCalibration(EKeys::Gamepad_RightX,0);
		AxisCalibration(EKeys::Gamepad_RightY,.8f); AxisCalibration(EKeys::Gamepad_RightY,0);
		if (!Check(Replay->GetSettings().ControllerAxisDirection.Equals(FVector2D(1,1)), TEXT("Standard controller can restore normal hardware calibration"))) return;
		PC->ActivateMenuRow(PC->CalibrationRow); AxisCalibration(EKeys::Gamepad_RightX,-.8f); PC->SetMenuTab(5);
		if (!Check(!PC->ControllerCalibrationStep && Replay->GetSettings().ControllerAxisDirection.Equals(FVector2D(1,1)), TEXT("Incomplete calibration is canceled without changing saved direction"))) return;
		PC->SetMenuTab(5);
		if (!Check(PC->GetVisibleMenuRows() == TArray<int32>({PC->WeaponRow, PC->SkinRow, PC->WoodlandRow, PC->DesertRow}), TEXT("Camos tab contains weapon and direct camo choices"))) return;
		if (auto* Profile = Cast<UCrosshairSaveGame>(UGameplayStatics::LoadGameFromSlot(TEXT("CrosshairProfile_v1"),0)))
			UE_LOG(LogTemp, Display, TEXT("USER_CONTROLLER_SETTINGS horizontal=%d vertical=%d"), Profile->Settings.bInvertControllerHorizontal, Profile->Settings.bInvertControllerVertical);
		const int32 Ammo = Weapon->Ammo;
		if (!Check(Weapon->Definition->Skins.Num() == 2 && !Weapon->SetSkin(TEXT("MissingSkin")), TEXT("Starter finish catalog rejects unknown skins"))) return;
		PC->ActivateMenuRow(PC->WoodlandRow);
		if (!Check(Weapon->SkinId == TEXT("Woodland") && Weapon->Mesh->GetMaterial(0) == Weapon->Definition->Skins[0].Materials[0], TEXT("Finish selector applies Woodland material"))) return;
		if (!Check(Weapon->PresentationMesh->GetMaterial(0) == Weapon->Definition->Skins[0].Materials[0], TEXT("Authored stock receives Woodland paint"))) return;
		if (!Check(Weapon->PresentationMesh->GetMaterial(1) == Weapon->Definition->PresentationMesh->GetStaticMaterials()[1].MaterialInterface, TEXT("Camo preserves the separate metal surface"))) return;
		if (!Check(Weapon->PresentationMesh->GetMaterial(1) && Weapon->PresentationMesh->GetMaterial(1)->GetPathName().Contains(TEXT("M_WeaponMetal")), TEXT("Authored metal slot has its physical surface material"))) return;
		if (!Check(Weapon->Details[6]->GetMaterial(0) == Weapon->Definition->Skins[0].Materials[0], TEXT("Sniper stock receives the same camo"))) return;
		PC->ActivateMenuRow(PC->DesertRow);
		if (!Check(Weapon->SkinId == TEXT("Desert") && Weapon->Ammo == Ammo, TEXT("Desert changes appearance without changing ammunition"))) return;
		auto* AuthoredMesh = Weapon->Definition->PresentationMesh.Get();
		if (!Check(AuthoredMesh && Pawn->Inventory->Weapons[1]->Definition->PresentationMesh && Pawn->Inventory->Weapons[2]->Definition->PresentationMesh && Pawn->Inventory->Weapons[1]->Definition->PresentationMesh != Pawn->Inventory->Weapons[2]->Definition->PresentationMesh, TEXT("Sniper, AR and SMG have authored, distinct models"))) return;
		Weapon->UpdatePresentation(0);
		if (!Check(Weapon->PresentationMesh->IsVisible() && !Weapon->Mesh->IsVisible() && !Weapon->Details[6]->IsVisible(), TEXT("New sniper model replaces all primitive geometry"))) return;
		Weapon->UpdatePresentation(1);
		if (!Check(!Weapon->PresentationMesh->IsVisible(), TEXT("Scope view hides the authored sniper"))) return;
		Weapon->Definition->PresentationMesh = nullptr;
		const bool Prototype = Weapon->Definition->bUsePrototypeGeometry;
		Weapon->Definition->bUsePrototypeGeometry = false;
		Weapon->UpdatePresentation(0);
		if (!Check(Weapon->Mesh->IsVisible() && !Weapon->Details[6]->IsVisible(), TEXT("Authored model path displays its mesh without prototype geometry"))) return;
		Weapon->Definition->PresentationMesh = AuthoredMesh;
		Weapon->Definition->bUsePrototypeGeometry = Prototype;
		Weapon->UpdatePresentation(0);
		Pawn->Inventory->Equip(1); Pawn->Inventory->Equip(0); Pawn->Attempt->ResetAttempt();
		if (!Check(Weapon->SkinId == TEXT("Desert"), TEXT("Finish survives weapon switching and attempt reset"))) return;
		Disk = Cast<UCrosshairSaveGame>(UGameplayStatics::LoadGameFromSlot(TEXT("CrosshairSmoke_v1"), 0));
		if (!Check(Disk && Disk->Settings.WeaponSkins.FindRef(Weapon->Definition->GetFName()) == TEXT("Desert"), TEXT("Per-weapon finish selection persists to disk"))) return;
		Key(EKeys::Escape, IE_Pressed);
		if (FParse::Param(FCommandLine::Get(), TEXT("CrosshairFollowupVisual"))) { PC->SetControlRotation(FRotator::ZeroRotator); Next(86); return; }
		Replay->ChangePracticeMap(TEXT("/Game/Crosshair/Maps/L_Nuketown")); Next(85); return;
	}
	case 86:
		if (Elapsed < .6) return;
		FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir()/TEXT("Screenshots/CrosshairDesert.png"),false,false);
		Next(87); return;
	case 87:
		if (Elapsed < .6) return;
		Key(EKeys::Escape, IE_Pressed); PC->SetMenuTab(5);
		FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir()/TEXT("Screenshots/CrosshairWeaponsTab.png"),false,false);
		Next(88); return;
	case 88:
		if (Elapsed < .6) return;
		PC->SetMenuTab(1);
		FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir()/TEXT("Screenshots/CrosshairInversionTab.png"),false,false);
		Next(89); return;
	case 89:
		if (Elapsed < .6) return;
		Key(EKeys::Escape, IE_Pressed);
		Pawn->Inventory->Equip(1);
		Pawn->Inventory->GetCurrent()->SetSkin(TEXT("Woodland"));
		Next(90); return;
	case 90:
		if (Elapsed < .6) return;
		FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir()/TEXT("Screenshots/CrosshairWoodland.png"),false,false);
		Next(91); return;
	case 91:
		if (Elapsed < .6) return;
		Key(EKeys::RightMouseButton, IE_Pressed); Next(92); return;
	case 92:
		if (Elapsed < .6) return;
		FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir()/TEXT("Screenshots/CrosshairAR_ADS.png"),false,false);
		Next(93); return;
	case 93:
		if (Elapsed < .6) return;
		Key(EKeys::RightMouseButton, IE_Released);
		Pawn->Inventory->GetCurrent()->SetSkin(NAME_None);
		Pawn->Inventory->Equip(2); Pawn->Inventory->GetCurrent()->SetSkin(TEXT("Desert")); Next(94); return;
	case 94:
		if (Elapsed < .6) return;
		FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir()/TEXT("Screenshots/CrosshairSMG.png"),false,false);
		Next(95); return;
	case 95:
		if (Elapsed < .6) return;
		Key(EKeys::RightMouseButton, IE_Pressed); Next(96); return;
	case 96:
		if (Elapsed < .6) return;
		FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir()/TEXT("Screenshots/CrosshairSMG_ADS.png"),false,false);
		Next(97); return;
	case 97:
		if (Elapsed < .6) return;
		Key(EKeys::RightMouseButton, IE_Released); Pawn->Inventory->GetCurrent()->SetSkin(NAME_None);
		Pawn->Inventory->Equip(0);
		Replay->ChangePracticeMap(TEXT("/Game/Crosshair/Maps/L_Nuketown")); Next(85); return;
	case 85:
	{
		if (!World->GetOutermost()->GetName().Contains(TEXT("L_Nuketown")) || World->GetTimeSeconds() < 2) return;
		int32 GrassCount = 0;
		for (TActorIterator<AActor> It(World); It; ++It)
		{
			TArray<UStaticMeshComponent*> Components; It->GetComponents(Components);
			for (auto* Component : Components)
				if (Component->GetStaticMesh() && Component->GetStaticMesh()->GetPathName().StartsWith(TEXT("/Game/ArchViz/Grass/Grass_grass_")))
				{
					++GrassCount;
					if (!Check(Component->GetCollisionEnabled() == ECollisionEnabled::NoCollision, TEXT("Raised grass foliage cannot block movement or shots"))) return;
				}
		}
		if (!Check(GrassCount == 4 && Pawn->GetCharacterMovement()->IsMovingOnGround(), TEXT("All four grass types are nonblocking while ground remains solid"))) return;
		if (!Check(Weapon->SkinId == TEXT("Desert"), TEXT("Finish is reapplied to the new weapon after map travel"))) return;
		Weapon->SetSkin(NAME_None);
		if (!Check(Weapon->GetSkinName().ToString() == TEXT("Original"), TEXT("Original finish can be restored"))) return;
		UE_LOG(LogTemp, Display, TEXT("CROSSHAIR_FOLLOWUP_SMOKE_OK"));
		bFinished = true; FPlatformMisc::RequestExitWithStatus(false,0); return;
	}
	case 70:
	{
		InitialPosition = Pawn->GetActorLocation();
		PC->SetControlRotation(FRotator::ZeroRotator);
		PC->InputKey(FInputKeyEventArgs::CreateSimulated(EKeys::Gamepad_LeftY, IE_Axis, 1.f, 1));
		PC->InputKey(FInputKeyEventArgs::CreateSimulated(EKeys::Gamepad_RightX, IE_Axis, .5f, 1));
		Next(71); return;
	}
	case 71:
	{
		if (Elapsed < .4) return;
		auto* Input = Cast<UEnhancedPlayerInput>(PC->PlayerInput);
		if (!Check(Input && Input->GetActionValue(Pawn->Inputs->Move).Get<FVector2D>().Y > .9f, TEXT("Controller left stick reaches saved movement mapping"))) return;
		if (!Check(FVector::Dist(Pawn->GetActorLocation(), InitialPosition) > 20 && PC->GetControlRotation().Yaw > 1, TEXT("Controller sticks move and turn the pawn"))) return;
		PC->InputKey(FInputKeyEventArgs::CreateSimulated(EKeys::Gamepad_LeftY, IE_Axis, 0.f, 1));
		PC->InputKey(FInputKeyEventArgs::CreateSimulated(EKeys::Gamepad_RightX, IE_Axis, 0.f, 1));
		Pawn->Attempt->ResetAttempt();
		// A deterministic elevated platform, independent of designer changes to the map.
		AActor* Platform = World->SpawnActor<AActor>();
		UBoxComponent* Floor = NewObject<UBoxComponent>(Platform);
		Platform->SetRootComponent(Floor);
		Floor->SetBoxExtent(FVector(200,200,20));
		Floor->SetCollisionProfileName(TEXT("BlockAll")); Floor->RegisterComponent();
		Platform->SetActorLocation(FVector(-1500,1000,700));
		Pawn->SetActorLocation(FVector(-1500,1000,850));
		Pawn->GetCharacterMovement()->SetMovementMode(MOVE_Falling);
		Next(72); return;
	}
	case 72:
	{
		if (Elapsed < .5) return;
		Key(EKeys::Gamepad_Special_Right, IE_Pressed);
		if (!Check(PC->bMenuOpen && PC->bShowMouseCursor && PC->MenuTab == 0, TEXT("Start opens mouse-accessible Practice tab"))) return;
		Key(EKeys::Gamepad_RightShoulder, IE_Pressed);
		if (!Check(PC->MenuTab == 1 && PC->GetVisibleMenuRows().Num() == 9, TEXT("Controller bumper opens Controls tab"))) return;
		const float Old = Replay->GetSettings().StickYawSpeed;
		Key(EKeys::Gamepad_DPad_Right, IE_Pressed);
		if (!Check(Replay->GetSettings().StickYawSpeed > Old, TEXT("Controller adjusts sensitivity in its tab"))) return;
		if (auto* HUD = Cast<ACrosshairHUD>(PC->GetHUD())) HUD->NotifyHitBoxClick(TEXT("Tab_3"));
		if (!Check(PC->MenuTab == 3 && PC->GetVisibleMenuRows() == TArray<int32>({11,12}), TEXT("Mouse tab action isolates target controls"))) return;
		PC->SetMenuTab(ACrosshairPlayerController::MenuTabCount - 1);
		Key(EKeys::Gamepad_RightShoulder, IE_Pressed);
		if (!Check(PC->MenuTab == 0, TEXT("Tab navigation wraps to Practice"))) return;
		PC->SetControlRotation(FRotator(-12,37,0));
		if (auto* HUD = Cast<ACrosshairHUD>(PC->GetHUD())) HUD->NotifyHitBoxClick(TEXT("Row_9"));
		if (!Check(Pawn->Attempt->bHasSavedStart && Pawn->Attempt->StartTransform.GetLocation().Z > 250, TEXT("Save position accepts elevated platform"))) return;
		InitialPosition = Pawn->Attempt->StartTransform.GetLocation();
		Next(73); return;
	}
	case 73:
	{
		if (Elapsed < .4) return;
		for (int32 i=0; i<3; ++i)
		{
			Pawn->SetActorLocation(InitialPosition + FVector(100,0,100));
			PC->SetControlRotation(FRotator::ZeroRotator);
			Pawn->Attempt->ResetAttempt();
			if (!Check(Pawn->GetActorLocation().Equals(InitialPosition, 2) && PC->GetControlRotation().Equals(FRotator(-12,37,0), .01), TEXT("Repeated reset restores saved position and view"))) return;
		}
		Pawn->SetActorLocation(InitialPosition + FVector(300,0,100));
		Key(EKeys::Gamepad_DPad_Down, IE_Pressed);
		Pawn->Inventory->Equip(1);
		Key(EKeys::Gamepad_LeftTrigger, IE_Pressed);
		Next(74); return;
	}
	case 74:
	{
		if (Elapsed < .4) return;
		Key(EKeys::Gamepad_DPad_Down, IE_Released);
		if (!Check(FVector::Dist(Pawn->GetActorLocation(), InitialPosition) < 5, TEXT("Controller D-pad resets to the saved platform"))) return;
		if (!Check(Pawn->GetAimAlpha() > .95f, TEXT("Controller trigger aims down sights"))) return;
		const FTransform Grip = Pawn->GetFirstPersonMesh()->GetSocketTransform(TEXT("HandGrip_R"));
		const FTransform Gun = Weapon->Mesh->GetComponentTransform();
		UE_LOG(LogTemp, Display, TEXT("GRIP_PROBE grip=%s gun=%s"), *Grip.ToString(), *Gun.ToString());
		if (!Check(Grip.GetLocation().Equals(Gun.GetLocation(), .1f), TEXT("Animated rifle grip remains aligned at full ADS"))) return;
		Key(EKeys::Gamepad_LeftTrigger, IE_Released);
		Key(EKeys::Escape, IE_Pressed);
		FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir()/TEXT("Screenshots/CrosshairMenu.png"),false,false);
		Next(75); return;
	}
	case 75:
		if (Elapsed < .5) return;
		UE_LOG(LogTemp, Display, TEXT("CROSSHAIR_POLISH_SMOKE_OK"));
		bFinished = true; FPlatformMisc::RequestExitWithStatus(false,0); return;
	case 60:
		if (Elapsed < .5) return;
		if (!Check(!Replay->ChangePracticeMap(TEXT("/Game/Crosshair/Maps/MissingMap")), TEXT("Missing map is rejected without travel"))) return;
		Key(EKeys::Escape, IE_Pressed);
		PC->MenuSelection = ACrosshairPlayerController::MapRow;
		Key(EKeys::Right, IE_Pressed);
		if (!Check(PC->bMenuOpen && PC->GetMenuRows()[PC->MapRow].Contains(TEXT("Nuketown")), TEXT("Map row selects Nuketown without loading on adjustment"))) return;
		Replay->GetSettings().MouseSensitivity = .37f;
		Key(EKeys::Enter, IE_Pressed); Next(61); return;
	case 61:
		if (!World->GetOutermost()->GetName().Contains(TEXT("L_Nuketown")) || World->GetTimeSeconds() < 3) return;
		if (!Check(Pawn->Inventory->Weapons.Num() == 3 && Pawn->Inputs->IsComplete() && !PC->bMenuOpen, TEXT("Nuketown initializes practice pawn, loadout and closed menu"))) return;
		if (!Pawn->GetCharacterMovement()->IsMovingOnGround())
		{
			UE_LOG(LogTemp, Display, TEXT("NUKETOWN_SPAWN_PROBE player=%s velocity=%s mode=%d"), *Pawn->GetActorLocation().ToString(), *Pawn->GetVelocity().ToString(), int32(Pawn->GetCharacterMovement()->MovementMode));
			for (TActorIterator<APlayerStart> It(World); It; ++It)
			{
				UE_LOG(LogTemp, Display, TEXT("NUKETOWN_SPAWN_PROBE start=%s"), *It->GetActorLocation().ToString());
				TArray<FOverlapResult> Overlaps;
				World->OverlapMultiByChannel(Overlaps, It->GetActorLocation(), FQuat::Identity, ECC_Pawn, FCollisionShape::MakeCapsule(55,96));
				for (const auto& O : Overlaps) UE_LOG(LogTemp, Display, TEXT("NUKETOWN_SPAWN_PROBE overlap=%s blocking=%d"), *GetNameSafe(O.GetActor()), O.bBlockingHit);
			}
			for (int32 GX = -2000; GX <= 2000; GX += 1000) for (int32 GY = -2000; GY <= 2000; GY += 1000)
			{
				FHitResult Ground;
				FCollisionQueryParams Probe(SCENE_QUERY_STAT(NuketownSpawnProbe), true, Pawn);
				if (World->LineTraceSingleByChannel(Ground, FVector(GX,GY,2500), FVector(GX,GY,-1500), ECC_Visibility, Probe))
					UE_LOG(LogTemp, Display, TEXT("NUKETOWN_SPAWN_PROBE ground=%s actor=%s"), *Ground.ImpactPoint.ToString(), *GetNameSafe(Ground.GetActor()));
			}
		}
		if (!Check(Pawn->GetCharacterMovement()->IsMovingOnGround(), TEXT("Nuketown start rests on collidable ground"))) return;
		if (!Check(FVector2D(Pawn->GetActorLocation()).Equals(FVector2D(-600,1300), 5), TEXT("Nuketown spawn is in the central bus/truck gap"))) return;
		if (!Check(FMath::IsNearlyEqual(Replay->GetSettings().MouseSensitivity, .37f), TEXT("Settings survive travel to Nuketown"))) return;
		if (FParse::Param(FCommandLine::Get(), TEXT("CrosshairMapVisual"))) FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir() / TEXT("Screenshots/CrosshairNuketown.png"), false, false);
		InitialPosition = Pawn->GetActorLocation();
		Key(EKeys::W, IE_Pressed); Next(62); return;
	case 62:
		if (Elapsed < .5) return;
		Key(EKeys::W, IE_Released);
		if (!Check(FVector::Dist(Pawn->GetActorLocation(), InitialPosition) > 20, TEXT("Nuketown accepts walking input"))) return;
		Pawn->Attempt->ResetAttempt();
		AmmoBefore = Weapon->Ammo;
		Weapon->StartFire(); Weapon->StopFire();
		if (!Check(Weapon->Ammo == AmmoBefore - 1, TEXT("Weapon fires on Nuketown"))) return;
		PC->SetControlRotation(FRotator(-45, -90, 0)); Pawn->Placement->Toggle(); Next(63); return;
	case 63:
		if (Elapsed < .5) return;
		if (!Check(Pawn->Placement->bValidPlacement, TEXT("Nuketown street supports target placement"))) return;
		Pawn->Placement->Confirm();
		if (!Check(Pawn->Placement->GetLayout().Num() == 1, TEXT("Target can be placed on Nuketown"))) return;
		Pawn->Attempt->SaveStart();
		Pawn->Attempt->ResetAttempt();
		if (!Check(FVector::Dist(Pawn->GetActorLocation(), Pawn->Attempt->StartTransform.GetLocation()) < 2, TEXT("Attempt reset uses Nuketown start"))) return;
		Key(EKeys::Escape, IE_Pressed);
		PC->MenuSelection = ACrosshairPlayerController::MapRow;
		Key(EKeys::Gamepad_DPad_Left, IE_Pressed);
		if (!Check(PC->GetMenuRows()[PC->MapRow].Contains(TEXT("Testing Map")), TEXT("Controller selects Testing Map"))) return;
		Key(EKeys::Gamepad_FaceButton_Bottom, IE_Pressed); Next(64); return;
	case 64:
		if (!World->GetOutermost()->GetName().Contains(TEXT("L_Practice")) || World->GetTimeSeconds() < 2) return;
		if (!Check(Pawn->Inventory->Weapons.Num() == 3 && Pawn->Inputs->IsComplete(), TEXT("Testing map remains playable after return travel"))) return;
		if (!Check(Pawn->Placement->GetLayout().Num() == 3, TEXT("Nuketown target layout does not leak into Testing Map"))) return;
		if (!Check(FMath::IsNearlyEqual(Replay->GetSettings().MouseSensitivity, .37f), TEXT("Settings survive round-trip map travel"))) return;
		UE_LOG(LogTemp, Display, TEXT("CROSSHAIR_MAP_SMOKE_OK"));
		bFinished = true; FPlatformMisc::RequestExitWithStatus(false, 0); return;
	case 0:
		if (Elapsed < 2) return;
		if (!Check(Pawn->Inputs && Pawn->Inputs->IsComplete() && Pawn->Inventory->Weapons.Num() == 3, TEXT("Blueprint assets and three-weapon loadout initialized"))) return;
				if (FParse::Param(FCommandLine::Get(), TEXT("CrosshairFollowupSmoke")))
				{
						if (!Check(!Replay->GetSettings().bInvertControllerHorizontal && !Replay->GetSettings().bInvertControllerVertical, TEXT("Fresh profile defaults to non-inverted look"))) return;
						Replay->GetSettings().bContinuousPractice = true; Replay->BeginAttempt(); TargetCount = 0;
						Next(76); return;
				}
		if (FParse::Param(FCommandLine::Get(), TEXT("CrosshairPolishSmoke")))
		{
			Replay->GetSettings().bContinuousPractice = true;
			Replay->BeginAttempt();
			Next(70); return;
		}
		if (FParse::Param(FCommandLine::Get(), TEXT("CrosshairMapSmoke")))
		{
			Replay->GetSettings().bContinuousPractice = true;
			Replay->BeginAttempt();
			Next(60); return;
		}
		if (FParse::Param(FCommandLine::Get(), TEXT("CrosshairSmokeSaved")))
		{
			if (!Check(!Replay->GetReplays().IsEmpty(), TEXT("Saved replay available after restart"))) return;
			Replay->PlaySaved(0); Next(51); return;
		}
				if (FParse::Param(FCommandLine::Get(), TEXT("CrosshairVisual")) || FParse::Param(FCommandLine::Get(), TEXT("CrosshairFollowupVisual")))
        {
            Replay->GetSettings().bContinuousPractice = true;
            Replay->BeginAttempt();
            Pawn->Attempt->ResetAttempt();
            PC->SetControlRotation(FRotator(0,0,0));
            Next(30); return;
        }
		Replay->GetSettings().bContinuousPractice = true;
		Replay->BeginAttempt();
		InitialPosition = Pawn->GetActorLocation();
		if (FParse::Param(FCommandLine::Get(), TEXT("CrosshairCapture"))) FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir() / TEXT("Screenshots/CrosshairPractice.png"), false, false);
        // Exercise actual saved mappings, not injected action values.
        PC->SetControlRotation(FRotator(0,37,0));
        Key(EKeys::W, IE_Pressed); Next(14); break;
    case 14: case 15: case 16: case 17: case 18: case 19:
    {
        if (Elapsed < .2) return;
        const FVector2D Expected[] = {FVector2D(0,1), FVector2D(0,-1), FVector2D(-1,0), FVector2D(1,0), FVector2D(0,0), FVector2D(1,1)};
        auto* Input = Cast<UEnhancedPlayerInput>(PC->PlayerInput);
        if (!Check(Input && Input->GetActionValue(Pawn->Inputs->Move).Get<FVector2D>().Equals(Expected[Step-14], .001), TEXT("Saved WASD mapping produces correct signed axes"))) return;
        if (Step < 18)
        {
            const FVector LocalDisplacement = FRotator(0,37,0).UnrotateVector(Pawn->GetActorLocation() - InitialPosition);
            const FVector2D Wanted = Expected[Step-14];
            if (!Check(LocalDisplacement.X * Wanted.Y + LocalDisplacement.Y * Wanted.X > 1, TEXT("Movement follows camera yaw and requested direction"))) return;
        }
        for (FKey K : {EKeys::W,EKeys::S,EKeys::A,EKeys::D}) Key(K,IE_Released);
        Pawn->Attempt->ResetAttempt();
        InitialPosition = Pawn->GetActorLocation();
        PC->SetControlRotation(FRotator(0,37,0));
        const int32 Previous = Step;
        if (Previous == 14) Key(EKeys::S,IE_Pressed);
        if (Previous == 15) Key(EKeys::A,IE_Pressed);
        if (Previous == 16) Key(EKeys::D,IE_Pressed);
        if (Previous == 17) { Key(EKeys::W,IE_Pressed); Key(EKeys::S,IE_Pressed); }
        if (Previous == 18) { Key(EKeys::W,IE_Pressed); Key(EKeys::D,IE_Pressed); }
        if (Previous < 19) { Next(Previous+1); break; }
        // Live damage state and menu actions.
        ACrosshairDummy* Target = nullptr;
        for (TActorIterator<ACrosshairDummy> It(World); It; ++It) { Target = *It; break; }
        if (!Check(Target != nullptr, TEXT("Target available for damage regression"))) return;
        auto* AR = Pawn->Inventory->Weapons[1]->Definition.Get();
        FDamageEvent Damage;
        Target->ResetTarget();
        for (int32 i=0; i<4; ++i) Target->TakeDamage(AR->BodyDamage,Damage,PC,Weapon);
        if (!Check(!Target->bHit && FMath::IsNearlyEqual(Target->Health,20.f), TEXT("AR survives four body hits"))) return;
        Target->TakeDamage(AR->BodyDamage,Damage,PC,Weapon);
        if (!Check(Target->bHit && Target->Health == 0, TEXT("AR defeats on fifth body hit"))) return;
        Target->ResetTarget();
        for (int32 i=0; i<3; ++i) Target->TakeDamage(AR->HeadDamage,Damage,PC,Weapon);
        if (!Check(Target->bHit && Target->Health == 0, TEXT("AR defeats on third head hit"))) return;
        Target->ResetTarget();
        if (!Check(Target->IsHeadImpact(Target->Head->GetComponentLocation()) && !Target->IsHeadImpact(Target->GetActorLocation()), TEXT("Head and body regions are distinct"))) return;
        Key(EKeys::Escape,IE_Pressed);
        PC->MenuSelection = ACrosshairPlayerController::WeaponRow;
        Key(EKeys::Enter,IE_Pressed);
        if (!Check(PC->bMenuOpen && Pawn->Inventory->ActiveIndex == 1, TEXT("Menu equips next weapon and stays open"))) return;
        PC->MenuSelection = ACrosshairPlayerController::WeaponRow;
        Key(EKeys::Left,IE_Pressed);
        if (!Check(Pawn->Inventory->ActiveIndex == 0, TEXT("Menu cycles back to sniper"))) return;
        if (!Check(PC->GetMenuRows()[ACrosshairPlayerController::QuitRow] == TEXT("Quit to Desktop"), TEXT("Quit action is visible"))) return;
        Key(EKeys::Escape,IE_Pressed);
        if (FParse::Param(FCommandLine::Get(),TEXT("CrosshairQuitSmoke")))
        {
            Key(EKeys::Escape,IE_Pressed);
            PC->MenuSelection = ACrosshairPlayerController::QuitRow;
            UE_LOG(LogTemp, Display, TEXT("CROSSHAIR_QUIT_SMOKE invoking normal menu quit"));
            Key(EKeys::Enter,IE_Pressed);
            bFinished = true;
            break;
        }
        PC->SetControlRotation(FRotator::ZeroRotator);
        PC->InputKey(FInputKeyEventArgs::CreateSimulated(EKeys::MouseY, IE_Axis, 100.f, 1));
        Next(20); break;
    }
    case 20:
        if (Elapsed < .1) return;
        if (!Check(FRotator::NormalizeAxis(PC->GetControlRotation().Pitch) > 1, TEXT("Actual mouse Y input looks upward with usable sensitivity"))) return;
        Pawn->Attempt->ResetAttempt();
        InitialPosition = Pawn->GetActorLocation();
        Key(EKeys::W, IE_Pressed); Next(1); break;
    case 30:
        if (Elapsed < .8) return;
        FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir()/TEXT("Screenshots/CrosshairSniperHip.png"),false,false);
        Key(EKeys::RightMouseButton,IE_Pressed); Next(31); break;
    case 31:
        if (Elapsed < .8) return;
        FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir()/TEXT("Screenshots/CrosshairSniperScope.png"),false,false);
        Next(32); break;
    case 32:
        if (Elapsed < .8) return;
        Key(EKeys::RightMouseButton,IE_Released); Pawn->Inventory->Equip(1);
        Key(EKeys::RightMouseButton,IE_Pressed); Next(33); break;
    case 33:
        if (Elapsed < .8) return;
        FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir()/TEXT("Screenshots/CrosshairARIron.png"),false,false);
        Next(34); break;
    case 34:
        if (Elapsed < .8) return;
        Key(EKeys::C,IE_Pressed); Key(EKeys::C,IE_Released); Next(35); break;
    case 35:
        if (Elapsed < .8) return;
        FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir()/TEXT("Screenshots/CrosshairARCrouch.png"),false,false);
        Next(36); break;
    case 36:
        if (Elapsed < .8) return;
        Key(EKeys::Escape,IE_Pressed); PC->MenuSelection = ACrosshairPlayerController::QuitRow;
        FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir()/TEXT("Screenshots/CrosshairMenu.png"),false,false);
        Next(37); break;
    case 37:
        if (Elapsed < .8) return;
        UE_LOG(LogTemp,Display,TEXT("CROSSHAIR_VISUAL_OK"));
        bFinished = true; FPlatformMisc::RequestExitWithStatus(false,0); break;
    case 1:
		if (Elapsed < .6) return;
		Key(EKeys::W, IE_Released);
		if (!Check(FVector::Dist(Pawn->GetActorLocation(), InitialPosition) > 20, TEXT("Keyboard mapping moves the character"))) return;
		Pawn->Attempt->ResetAttempt();
		Key(EKeys::C, IE_Pressed); Next(2); break;
	case 2:
		if (Elapsed < .3) return;
		Key(EKeys::C, IE_Released);
		if (!Check(Pawn->bIsCrouched, TEXT("Crouch input changes capsule state"))) return;
		Next(3); break;
	case 3:
		Key(EKeys::C, IE_Pressed); Next(4); break;
	case 4:
		if (Elapsed < .3) return;
		Key(EKeys::C, IE_Released); Key(EKeys::SpaceBar, IE_Pressed); Next(5); break;
	case 5:
		if (Elapsed < .15) return;
		Key(EKeys::SpaceBar, IE_Released);
		if (!Check(Pawn->GetCharacterMovement()->IsFalling() && Pawn->GetVelocity().Z > 0, TEXT("Jump input produces airborne movement"))) return;
		PC->SetControlRotation(FRotator(80, 0, 0));
		AmmoBefore = Weapon->Ammo;
		Weapon->StartFire(); Weapon->StopFire();
		if (!Check(Weapon->Ammo == AmmoBefore - 1, TEXT("Airborne sniper shot consumes one round"))) return;
		Weapon->StartFire(); Weapon->StopFire();
		if (!Check(Weapon->Ammo == AmmoBefore - 1, TEXT("Shot cooldown prevents double fire"))) return;
		Weapon->Reload();
		if (!Check(Weapon->bReloading, TEXT("Reload starts"))) return;
		Pawn->Inventory->Cycle();
		if (!Check(!Weapon->bReloading && Weapon->Ammo == AmmoBefore - 1, TEXT("Switch cancels reload without granting ammo"))) return;
		Weapon = Pawn->Inventory->GetCurrent(); AmmoBefore = Weapon->Ammo;
		Weapon->StartFire(); Weapon->StopFire();
		if (!Check(Weapon->Ammo == AmmoBefore, TEXT("Equip delay blocks immediate fire"))) return;
		Next(6); break;
	case 6:
		if (Elapsed < .4) return;
		Weapon->StartFire(); Next(7); break;
	case 7:
		if (Elapsed < .45) return;
		PC->FlushPressedKeys();
		if (!Check(Weapon->Ammo <= AmmoBefore - 3, TEXT("Automatic weapon fires while held"))) return;
		AmmoBefore = Weapon->Ammo; Next(8); break;
	case 8:
		if (Elapsed < .25) return;
		if (!Check(Weapon->Ammo == AmmoBefore, TEXT("Focus/input flush stops automatic fire"))) return;
		Weapon->Reload(); Next(9); break;
	case 9:
		if (Elapsed < 2.1) return;
		if (!Check(!Weapon->bReloading && Weapon->Ammo == Weapon->Definition->MagazineSize, TEXT("Completed reload refills magazine"))) return;
		Pawn->Attempt->ResetAttempt();
		{
			AActor* Block = World->SpawnActor<AActor>();
			UBoxComponent* Box = NewObject<UBoxComponent>(Block);
			Block->SetRootComponent(Box); Box->SetBoxExtent(FVector(1000)); Box->SetCollisionProfileName(TEXT("BlockAll")); Box->RegisterComponent();
			Block->SetActorLocation(Pawn->Attempt->StartTransform.GetLocation());
			Pawn->Attempt->bSucceeded = true; Pawn->Attempt->ResetAttempt();
			const bool Recovered = !Pawn->Attempt->bSucceeded && Pawn->CanAct();
			Block->Destroy();
			if (!Check(Recovered, TEXT("Blocked reset releases success lock"))) return;
		}
		Pawn->Attempt->ResetAttempt();
		PC->SetControlRotation(FRotator(-30, 45, 0));
		Next(12); break;
	case 12:
		if (Elapsed < .1) return;
		Pawn->Placement->Toggle(); Pawn->Placement->UpdatePreview();
		TargetCount = Pawn->Placement->GetLayout().Num();
		Pawn->Placement->Confirm();
		if (!Check(Pawn->Placement->GetLayout().Num() == TargetCount + 1, TEXT("Clear ground placement spawns one target"))) return;
		Pawn->Placement->Toggle(); PC->SetControlRotation(FRotator(80,0,0));
		Next(13); break;
	case 13:
		if (Elapsed < .1) return;
		Pawn->Placement->Confirm();
		if (!Check(Pawn->Placement->GetLayout().Num() == TargetCount + 1, TEXT("Unsupported placement rejected"))) return;
		Pawn->Placement->Toggle();
		TargetCount = Pawn->Placement->GetLayout().Num();
		Pawn->Inventory->Equip(0);
		Replay->GetSettings().bContinuousPractice = false;
		Pawn->Attempt->ResetAttempt();
		Next(10); break;
	case 10:
		if (Elapsed < 2 || !Replay->IsRecording()) return;
		{
			ACrosshairDummy* Target = nullptr;
			for (TActorIterator<ACrosshairDummy> It(World); It; ++It)
			{
				FHitResult Sight;
				FCollisionQueryParams Query(SCENE_QUERY_STAT(SmokeVisibility), true, Pawn);
				World->LineTraceSingleByChannel(Sight, Pawn->GetFirstPersonCameraComponent()->GetComponentLocation(), It->GetActorLocation(), ECC_Visibility, Query);
				if (Sight.GetActor() == *It) { Target = *It; break; }
			}
			if (!Check(Target != nullptr, TEXT("Target exists for recording"))) return;
			// Deterministic ADS shot: no spread and enough time for the view to settle.
			PC->SetControlRotation((Target->GetActorLocation() - Pawn->GetFirstPersonCameraComponent()->GetComponentLocation()).Rotation());
			Key(EKeys::RightMouseButton, IE_Pressed);
			Next(11);
		}
		break;
	case 11:
		if (Elapsed < .4) return;
		Weapon->StartFire(); Weapon->StopFire();
		Key(EKeys::RightMouseButton, IE_Released);
		if (!Check(Pawn->Attempt->bSucceeded && Replay->IsFinishing(), TEXT("Hitscan hit completes attempt and queues replay"))) return;
		Pawn->Attempt->ResetAttempt();
		if (!Check(Replay->IsFinishing(), TEXT("Early reset preserves successful recording"))) return;
		Next(50); break;
	default: break;
	}
}
