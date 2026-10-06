#include "CrosshairLethalTest.h"
#include "../CrosshairThrowable.h"
#include "../CrosshairCharacter.h"
#include "../CrosshairGame.h"
#include "../CrosshairPractice.h"
#include "../CrosshairWeapon.h"
#include "../CrosshairDummy.h"
#include "../CrosshairWindow.h"
#include "../CrosshairReplaySubsystem.h"
#include "Camera/CameraComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/GameInstance.h"
#include "Engine/DemoNetDriver.h"
#include "EngineUtils.h"
#include "InputKeyEventArgs.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "UnrealClient.h"
#if WITH_EDITOR
#include "ShaderCompiler.h"
#endif
bool UCrosshairLethalTest::ShouldCreateSubsystem(UObject* Outer) const
{
#if UE_BUILD_SHIPPING
    return false;
#else
    return FParse::Param(FCommandLine::Get(),TEXT("CrosshairSmoke")) && FParse::Param(FCommandLine::Get(),TEXT("CrosshairLethalSmoke"));
#endif
}
void UCrosshairLethalTest::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection); Collection.InitializeDependency<UCrosshairReplaySubsystem>(); Started=StepStarted=FPlatformTime::Seconds();
}
bool UCrosshairLethalTest::Check(bool Condition,const TCHAR* Text)
{
    UE_LOG(LogTemp,Display,TEXT("CROSSHAIR_LETHAL_%s step=%d %s"),Condition ? TEXT("PASS") : TEXT("FAIL"),Step,Text);
    if (!Condition) { bFinished=true; FPlatformMisc::RequestExitWithStatus(false,1); } return Condition;
}
void UCrosshairLethalTest::Next(int32 Value) { Step=Value; StepStarted=FPlatformTime::Seconds(); }
void UCrosshairLethalTest::Tick(float Delta)
{
    auto* World=GetWorld(); if (!World || !World->IsGameWorld() || bFinished) return;
    if (FPlatformTime::Seconds()-Started>180) { Check(false,TEXT("Timeout")); return; }
    const double Elapsed=FPlatformTime::Seconds()-StepStarted;
    auto* Replay=GetGameInstance()->GetSubsystem<UCrosshairReplaySubsystem>();
    if (Step==15 && Replay->IsPlayback())
    {
        for (TActorIterator<ACrosshairThrowable> It(World);It;++It)
        {
            if (!It->bHeld && It->Spec.Type==ECrosshairLethalType::Tomahawk && It->FlightAge>0) bSawFlight=true;
            if (It->bResting) bSawImpact=true;
        }
        return;
    }
    auto* PC=Cast<ACrosshairPlayerController>(World->GetFirstPlayerController());
    auto* Pawn=PC ? Cast<ACrosshairCharacter>(PC->GetPawn()) : nullptr;
    if (!Pawn || (!Pawn->CanAct() && Step!=1 && Step!=2 && Step!=14 && Step!=15)) return;
    auto* Lethals=Pawn->Lethals.Get();
    auto Key=[&](FKey K,EInputEvent Event) { PC->InputKey(FInputKeyEventArgs::CreateSimulated(K,Event,Event==IE_Released ? 0.f : 1.f)); };
    auto Place=[&](float Yaw=0.f)
    {
        Pawn->StopActions(); Pawn->SetActorLocation(Origin+FVector(0,0,96),false,nullptr,ETeleportType::TeleportPhysics);
        Pawn->GetCharacterMovement()->StopMovementImmediately(); Pawn->GetCharacterMovement()->SetMovementMode(MOVE_Walking);
        PC->SetControlRotation(FRotator(0,Yaw,0));
    };
    auto Box=[&](FVector At,FVector Half)->AActor*
    {
        auto* A=World->SpawnActor<AActor>(At,FRotator::ZeroRotator);
        auto* M=NewObject<UStaticMeshComponent>(A); A->SetRootComponent(M);
        M->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cube")));
        M->SetCollisionProfileName(TEXT("BlockAll")); M->RegisterComponent(); A->SetActorLocation(At); A->SetActorScale3D(Half/50); return A;
    };
    switch (Step)
    {
    case 0:
        if (Elapsed<2 || !Pawn->Inventory->GetCurrent()) return;
        Replay->GetSettings().bContinuousPractice=true; Replay->BeginAttempt();
        Box(Origin-FVector(0,0,50),FVector(1800,1800,50));
        Target=World->SpawnActor<ACrosshairDummy>(Pawn->TargetClass,Origin+FVector(500,0,96),FRotator(0,180,0));
        Covered=World->SpawnActor<ACrosshairDummy>(Pawn->TargetClass,Origin+FVector(500,250,96),FRotator(0,180,0));
        Edge=World->SpawnActor<ACrosshairDummy>(Pawn->TargetClass,Origin+FVector(500,-300,96),FRotator(0,180,0));
        if (!Check(Target && Covered && Edge,TEXT("Lethal test targets created"))) return;
        Blocker=Box(Origin+FVector(480,125,140),FVector(60,10,150));
        Place(); Lethals->Selected=ECrosshairLethalType::Frag;
        Ammo=Pawn->Inventory->GetCurrent()->Ammo; Key(EKeys::G,IE_Pressed); Next(1); return;
    case 1:
        if (Elapsed<.2) return;
        if (!Check(Lethals->IsHolding() && Lethals->Remaining==2 && Lethals->GetCookedSeconds()>=.15,TEXT("G prepares grenade and cooks without spending supply yet"))) return;
        Key(EKeys::LeftMouseButton,IE_Pressed); Key(EKeys::LeftMouseButton,IE_Released);
        Key(EKeys::Escape,IE_Pressed);
        if (!Check(!Lethals->IsHolding() && Lethals->Remaining==2 && Pawn->Inventory->GetCurrent()->Ammo==Ammo,TEXT("Gun fire blocked while primed; opening menu cancels without throwing"))) return;
        PC->SetMenuTab(0); PC->ActivateMenuRow(PC->GetLethalRow());
        if (!Check(Lethals->Selected==ECrosshairLethalType::Tomahawk && PC->GetFirstReplayRow()==PC->GetImportRow()+1,TEXT("Practice equipment row cycles type without moving camo/replay actions"))) return;
        {
            auto* Save=Cast<UCrosshairSaveGame>(UGameplayStatics::LoadGameFromSlot(TEXT("CrosshairSmoke_v1"),0));
            if (!Check(Save && Save->Settings.LethalType==ECrosshairLethalType::Tomahawk,TEXT("Lethal selection saved to isolated profile"))) return;
        }
        if (FParse::Param(FCommandLine::Get(),TEXT("CrosshairLethalVisual"))) FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir()/TEXT("Screenshots/LethalMenu.png"),false,false);
        Next(2); return;
    case 2:
        if (Elapsed<.2) return;
        Key(EKeys::Escape,IE_Pressed); Key(EKeys::Gamepad_RightShoulder,IE_Pressed); Next(3); return;
    case 3:
#if WITH_EDITOR
        if (FParse::Param(FCommandLine::Get(),TEXT("CrosshairLethalVisual")) && GShaderCompilingManager && GShaderCompilingManager->IsCompiling()) return;
#endif
        if (Elapsed<.2) return;
        if (!Check(Lethals->IsHolding() && Lethals->HeldActor->Spec.Type==ECrosshairLethalType::Tomahawk,TEXT("Controller RB prepares selected axe"))) return;
        if (FParse::Param(FCommandLine::Get(),TEXT("CrosshairLethalVisual"))) FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir()/TEXT("Screenshots/LethalTomahawk.png"),false,false);
        Next(16); return;
    case 16:
        if (Elapsed<.15) return;
        Projectile=Lethals->HeldActor; Key(EKeys::Gamepad_RightShoulder,IE_Released); Next(4); return;
    case 4:
        if (!IsValid(Projectile) || !Projectile->bResting) return;
        if (!Check(Target->bHit && Lethals->Remaining==1 && Pawn->Inventory->GetCurrent()->Ammo==Ammo,TEXT("Swept axe impact kills target and spends one lethal, not gun ammo"))) return;
        Pawn->Attempt->ResetAttempt(); Place();
        if (!Check(Lethals->Remaining==2 && !IsValid(Projectile),TEXT("Attempt reset replenishes and removes thrown/stuck lethals"))) return;
        Key(EKeys::Gamepad_LeftShoulder,IE_Pressed);
        if (!Check(Lethals->Selected==ECrosshairLethalType::Frag,TEXT("Controller LB cycles equipment"))) return;
        Lethals->Frag.Fuse=1.6f; Place(90); Key(EKeys::G,IE_Pressed); Next(5); return;
    case 5:
        if (Elapsed<.6) return;
        if (FParse::Param(FCommandLine::Get(),TEXT("CrosshairLethalVisual"))) FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir()/TEXT("Screenshots/LethalFrag.png"),false,false);
        Next(17); return;
    case 17:
        if (Elapsed<.1) return;
        Projectile=Lethals->HeldActor; Key(EKeys::G,IE_Released); Next(6); return;
    case 6:
        if (Elapsed<.65) return;
        if (!Check(IsValid(Projectile) && !Projectile->bDetonated && Projectile->Movement->Velocity.Z>0,TEXT("Cooked frag bounces on floor before remaining fuse expires"))) return;
        Projectile->SetActorLocation(Origin+FVector(450,0,130)); Projectile->Movement->StopMovementImmediately();
        Next(7); return;
    case 7:
        if (!IsValid(Projectile) || !Projectile->bDetonated) return;
        if (!Check(Target->bHit && Covered->Health==Covered->MaxHealth && Edge->Health<Edge->MaxHealth && !Edge->bHit,TEXT("Frag kills nearby target, falloff wounds outer target, solid cover protects target"))) return;
        Pawn->Attempt->ResetAttempt(); Place(); Lethals->Frag.Fuse=.15f; Key(EKeys::G,IE_Pressed); Next(8); return;
    case 8:
        if (Elapsed<.2) return;
        if (!Check(!Lethals->IsHolding() && Lethals->Remaining==1,TEXT("Holding past fuse detonates once and consumes one grenade"))) return;
        Lethals->Remaining=0; Key(EKeys::G,IE_Pressed);
        if (!Check(!Lethals->IsHolding(),TEXT("Empty lethal supply cannot prepare another throwable"))) return;
        Pawn->Attempt->ResetAttempt(); Place(); Key(EKeys::F,IE_Pressed);
        Blocker->Destroy(); Blocker=Box(Origin+FVector(30,0,160),FVector(10,80,80));
        Key(EKeys::G,IE_Pressed); Next(9); return;
    case 9:
        if (Elapsed<.15) return;
        Projectile=Lethals->HeldActor; Key(EKeys::G,IE_Released); Next(10); return;
    case 10:
        if (!IsValid(Projectile) || !Projectile->bResting) return;
        if (!Check(!Target->bHit && Projectile->GetActorLocation().X<Origin.X+50,TEXT("Near-wall release cannot launch axe through solid cover"))) return;
        Pawn->Attempt->ResetAttempt(); Place(); Blocker->Destroy();
        Pane=World->SpawnActor<ACrosshairWindow>(Origin+FVector(250,0,140),FRotator::ZeroRotator);
        Pane->Glass->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cube"))); Pane->SetActorScale3D(FVector(.04f,3,3));
        Replay->GetSettings().bContinuousPractice=false; Next(11); return;
    case 11:
        if (Elapsed<.25) return;
        Pawn->Attempt->SaveStart(); Next(12); return;
    case 12:
        if (!Replay->IsRecording() || Elapsed<.5) return;
        Key(EKeys::G,IE_Pressed); Next(13); return;
    case 13:
        if (Elapsed<.2) return;
        Projectile=Lethals->HeldActor; Key(EKeys::G,IE_Released); Next(14); return;
    case 14:
        if (!IsValid(Target) || !Target->bHit) return;
        if (!Check(Pane->bBroken && Pawn->Attempt->bSucceeded,TEXT("Thrown axe breaks window and successful target impact records attempt"))) return;
        Next(15); return;
    case 15:
        if (!bSawFlight || !bSawImpact || !Pawn->CanAct()) return;
        if (!Check(Lethals->Remaining==2 && !Lethals->IsHolding() && Lethals->Selected==ECrosshairLethalType::Tomahawk,TEXT("Replay shows flight/impact and restores ready lethal loadout"))) return;
        for (TActorIterator<ACrosshairThrowable> It(World);It;++It) if (!Check(false,TEXT("Replay return left an old throwable behind"))) return;
        UE_LOG(LogTemp,Display,TEXT("CROSSHAIR_LETHAL_OK")); bFinished=true; FPlatformMisc::RequestExitWithStatus(false,0); return;
    }
}
