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
#include "Components/StaticMeshComponent.h"
#include "Camera/CameraComponent.h"
#include "InputKeyEventArgs.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "HAL/PlatformMisc.h"
#include "EngineUtils.h"
#include "UnrealClient.h"
#include "EnhancedPlayerInput.h"
#include "Engine/DamageEvents.h"
#include "Engine/OverlapResult.h"
#include "GameFramework/PlayerStart.h"
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
	if (Now - Started > (FParse::Param(FCommandLine::Get(), TEXT("CrosshairMapVisual")) ? 300 : 100)) { Check(false, TEXT("Runtime test timed out")); return; }
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
		PC->SetControlRotation(FRotator(-25, PC->GetControlRotation().Yaw, 0)); Pawn->Placement->Toggle(); Next(63); return;
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
        if (FParse::Param(FCommandLine::Get(), TEXT("CrosshairVisual")))
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
