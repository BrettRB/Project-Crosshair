#include "CrosshairSmokeTest.h"
#include "../CrosshairCharacter.h"
#include "../CrosshairData.h"
#include "../CrosshairDummy.h"
#include "../CrosshairGame.h"
#include "../CrosshairPractice.h"
#include "../CrosshairReplaySubsystem.h"
#include "../CrosshairWeapon.h"
#include "Engine/GameInstance.h"
#include "Engine/DemoNetDriver.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/BoxComponent.h"
#include "Camera/CameraComponent.h"
#include "InputKeyEventArgs.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "HAL/PlatformMisc.h"
#include "EngineUtils.h"
#include "UnrealClient.h"
#include "Misc/Paths.h"

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
	if (Now - Started > 100) { Check(false, TEXT("Runtime test timed out")); return; }
	auto* Replay = GetGameInstance()->GetSubsystem<UCrosshairReplaySubsystem>();
	auto* PC = Cast<ACrosshairPlayerController>(World->GetFirstPlayerController());
	auto* Pawn = PC ? Cast<ACrosshairCharacter>(PC->GetPawn()) : nullptr;
	auto Key = [PC](FKey K, EInputEvent Event) { if (PC) PC->InputKey(FInputKeyEventArgs::CreateSimulated(K, Event, Event == IE_Released ? 0.f : 1.f)); };
	if (Step >= 50)
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
	case 0:
		if (Elapsed < 2) return;
		if (!Check(Pawn->Inputs && Pawn->Inputs->IsComplete() && Pawn->Inventory->Weapons.Num() == 3, TEXT("Blueprint assets and three-weapon loadout initialized"))) return;
		if (FParse::Param(FCommandLine::Get(), TEXT("CrosshairSmokeSaved")))
		{
			if (!Check(!Replay->GetReplays().IsEmpty(), TEXT("Saved replay available after restart"))) return;
			Replay->PlaySaved(0); Next(51); return;
		}
		Replay->GetSettings().bContinuousPractice = true;
		Replay->BeginAttempt();
		InitialPosition = Pawn->GetActorLocation();
		if (FParse::Param(FCommandLine::Get(), TEXT("CrosshairCapture"))) FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir() / TEXT("Screenshots/CrosshairPractice.png"), false, false);
		Key(EKeys::W, IE_Pressed); Next(1); break;
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
