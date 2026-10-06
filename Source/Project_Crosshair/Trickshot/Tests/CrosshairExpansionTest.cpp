#include "CrosshairExpansionTest.h"
#include "../CrosshairCharacter.h"
#include "../CrosshairData.h"
#include "../CrosshairDummy.h"
#include "../CrosshairGame.h"
#include "../CrosshairPractice.h"
#include "../CrosshairReplaySubsystem.h"
#include "../CrosshairTraversal.h"
#include "../CrosshairWeapon.h"
#include "../CrosshairWindow.h"
#include "../CrosshairBallistics.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/CapsuleComponent.h"
#include "Camera/CameraComponent.h"
#include "Engine/GameInstance.h"
#include "Engine/StaticMesh.h"
#include "Engine/DamageEvents.h"
#include "Engine/DemoNetDriver.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "InputKeyEventArgs.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "UnrealClient.h"
#if WITH_EDITOR
#include "ShaderCompiler.h"
#endif
bool UCrosshairExpansionTest::ShouldCreateSubsystem(UObject* Outer) const
{
#if UE_BUILD_SHIPPING
    return false;
#else
    return FParse::Param(FCommandLine::Get(),TEXT("CrosshairSmoke")) && FParse::Param(FCommandLine::Get(),TEXT("CrosshairExpansionSmoke"));
#endif
}
void UCrosshairExpansionTest::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection); Collection.InitializeDependency<UCrosshairReplaySubsystem>();
    Started=StepStarted=FPlatformTime::Seconds();
}
bool UCrosshairExpansionTest::Check(bool Condition,const TCHAR* Message)
{
    UE_LOG(LogTemp,Display,TEXT("CROSSHAIR_EXPANSION_%s step=%d %s"),Condition ? TEXT("PASS") : TEXT("FAIL"),Step,Message);
    if (!Condition) { bFinished=true; FPlatformMisc::RequestExitWithStatus(false,1); }
    return Condition;
}
void UCrosshairExpansionTest::Next(int32 NewStep) { Step=NewStep; StepStarted=FPlatformTime::Seconds(); }
void UCrosshairExpansionTest::Tick(float DeltaSeconds)
{
    auto* World=GetWorld(); if (!World || !World->IsGameWorld() || bFinished) return;
    const double Elapsed=FPlatformTime::Seconds()-StepStarted;
    if (FPlatformTime::Seconds()-Started>180) { Check(false,TEXT("Timeout")); return; }
    auto* PC=Cast<ACrosshairPlayerController>(World->GetFirstPlayerController());
    auto* Pawn=PC ? Cast<ACrosshairCharacter>(PC->GetPawn()) : nullptr;
    auto* Replay=GetGameInstance()->GetSubsystem<UCrosshairReplaySubsystem>();
    Replay->GetSettings().bSandboxTargets=true;
    if (Step==22 && World->GetDemoNetDriver() && World->GetDemoNetDriver()->IsPlaying())
    {
        for (TActorIterator<ACrosshairWindow> It(World);It;++It)
            if (It->GetFName()==ReplayWindow && It->bBroken) bSawBrokenReplay=true;
        for (TActorIterator<ACrosshairCharacter> It(World);It;++It)
            if (FVector::DotProduct(It->GetActorLocation()-WindowStart,WindowNormal)>130) bSawMantleReplay=true;
        return;
    }
    if (!Pawn || (!Pawn->CanAct() && Step!=16)) return;
    auto* Weapon=Pawn->Inventory->GetCurrent();
    auto Key=[&](FKey K,EInputEvent Event){ PC->InputKey(FInputKeyEventArgs::CreateSimulated(K,Event,Event==IE_Released ? 0.f : 1.f)); };
    auto Place=[&](FVector Position,float Yaw=0.f)
    {
        Pawn->StopActions(); Pawn->UnCrouch(); Pawn->GetCharacterMovement()->UnCrouch(false);
        Pawn->SetActorLocation(Position,false,nullptr,ETeleportType::TeleportPhysics);
        Pawn->GetCharacterMovement()->StopMovementImmediately(); Pawn->GetCharacterMovement()->SetMovementMode(MOVE_Walking);
        PC->SetControlRotation(FRotator(0,Yaw,0));
    };
    auto Box=[&](FVector Position,FVector Half)->AActor*
    {
        auto* Actor=World->SpawnActor<AActor>(Position,FRotator::ZeroRotator);
        auto* Mesh=NewObject<UStaticMeshComponent>(Actor); Actor->SetRootComponent(Mesh);
        Mesh->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cube.Cube")));
        Mesh->SetCollisionProfileName(TEXT("BlockAll")); Mesh->RegisterComponent();
        Actor->SetActorLocation(Position); Actor->SetActorScale3D(Half/50); return Actor;
    };
    auto Aim=[&]() { PC->SetControlRotation((Target->Model->GetBoneLocation(TEXT("head"))-Pawn->GetFirstPersonCameraComponent()->GetComponentLocation()).Rotation()); };
    switch (Step)
    {
    case 0:
        if (Elapsed<2 || !Weapon || !Weapon->Definition) return;
        Replay->GetSettings().bContinuousPractice=true; Replay->BeginAttempt();
        {
        int32 Upright=0;
        for (TActorIterator<ACrosshairDummy> It(World);It;++It)
        {
            if (!Check(It->GetActorUpVector().Z>.99f && It->Model->GetBoneLocation(TEXT("head")).Z>It->GetActorLocation().Z,TEXT("Placed practice dummies are upright with heads above bodies"))) return;
            ++Upright;
        }
        if (!Check(Upright==3,TEXT("All three practice-map targets checked"))) return;
        }
        for (int32 i=0;i<3;++i)
        {
            Pawn->Inventory->Equip(i); auto* W=Pawn->Inventory->GetCurrent();
            if (!Check(W->Definition->Skins.Num()==4,TEXT("Four camos available on each weapon"))) return;
            for (const FName Skin : {FName(TEXT("RedTiger")),FName(TEXT("Arctic"))})
            {
                if (!Check(W->SetSkin(Skin),TEXT("New camo equips"))) return;
                const auto& Slots=W->Definition->PresentationMesh->GetStaticMaterials();
                for (int32 Slot=0;Slot<Slots.Num();++Slot)
                    if (Slots[Slot].MaterialSlotName!=TEXT("Paint") && !Check(W->PresentationMesh->GetMaterial(Slot)==Slots[Slot].MaterialInterface,TEXT("Camo preserves metal rubber and optics"))) return;
            }
        }
        Pawn->Inventory->Equip(0); PC->ToggleMenu(); PC->SetMenuTab(5);
        if (!Check(PC->GetVisibleMenuRows().Num()==6,TEXT("Expanded camo menu"))) return;
        PC->ActivateMenuRow(22);
        if (!Check(Pawn->Inventory->GetCurrent()->SkinId==TEXT("RedTiger"),TEXT("Red Tiger menu selection"))) return;
        PC->ActivateMenuRow(23);
        if (!Check(Pawn->Inventory->GetCurrent()->SkinId==TEXT("Arctic"),TEXT("Arctic menu selection"))) return;
        PC->ToggleMenu(); Pawn->Attempt->ResetAttempt();
        if (!Check(Pawn->Inventory->GetCurrent()->SkinId==TEXT("Arctic"),TEXT("New camo survives reset"))) return;
        Box(Origin-FVector(0,0,20),FVector(2000,2000,20));
        Target=World->SpawnActor<ACrosshairDummy>(Pawn->TargetClass,Origin+FVector(400,0,94),FRotator::ZeroRotator);
        if (!Check(Target && Target->Model->GetPhysicsAsset(),TEXT("Humanoid target with authored physics bodies"))) return;
        Next(1); return;
    case 1:
    {
        if (Elapsed<.3) return;
        FCollisionQueryParams Params(SCENE_QUERY_STAT(ExpansionHitbox),true,Pawn);
        for (const FName Bone : {FName(TEXT("head")),FName(TEXT("spine_03")),FName(TEXT("calf_l"))})
        {
            const FVector At=Target->Model->GetBoneLocation(Bone);
            for (FVector Direction : {FVector::ForwardVector,FVector::RightVector})
            {
                FHitResult Hit;
                World->LineTraceSingleByChannel(Hit,At+Direction*200,At-Direction*200,ECC_Visibility,Params);
                UE_LOG(LogTemp,Display,TEXT("EXPANSION_REGION expected=%s actual=%s"),*Bone.ToString(),*Hit.BoneName.ToString());
                if (!Check(Hit.GetActor()==Target && Hit.GetComponent()==Target->Model && (Bone==TEXT("head") ? Target->IsHeadHit(Hit) : !Target->IsHeadHit(Hit)),TEXT("Head torso and leg trace from two angles"))) return;
            }
        }
        FHitResult Gap; const FVector At=Target->GetActorLocation()+FVector(0,27,80);
        World->LineTraceSingleByChannel(Gap,At+FVector(200,0,0),At-FVector(200,0,0),ECC_Visibility,Params);
        if (!Check(Gap.GetActor()!=Target,TEXT("Old capsule shoulder/head gap no longer scores a hit"))) return;
        Pane=World->SpawnActor<ACrosshairWindow>(Origin+FVector(0,0,140),FRotator::ZeroRotator);
        Pane->Glass->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cube.Cube")));
        Pane->SetActorScale3D(FVector(.04f,3,3));
        Place(Origin+FVector(-350,0,96)); Pawn->GetCharacterMovement()->SetMovementMode(MOVE_Flying);
        Key(EKeys::RightMouseButton,IE_Pressed); Next(2); return;
    }
    case 2:
        if (Elapsed<.5) return;
        Aim(); Next(37); return;
    case 37:
        if (Elapsed<.08) return;
        AmmoBefore=Weapon->Ammo; Weapon->ResetWeapon(); Weapon->StartFire(); Weapon->StopFire(); Next(3); return;
    case 3:
        if (!Check(Pane->bBroken && Target->bHit && Weapon->Ammo==AmmoBefore-1,TEXT("Single shot breaks glass and hits target beyond it"))) return;
        Pane->ResetGlass(); Target->ResetTarget();
        if (!Check(Pane->Glass->GetCollisionEnabled()!=ECollisionEnabled::NoCollision,TEXT("Reset restores glass collision"))) return;
        if (!Check(Pawn->bLastHitHeadshot && !Pawn->bLastHitWallbang,TEXT("Direct head hit chooses headshot marker"))) return;
        if (FParse::Param(FCommandLine::Get(),TEXT("CrosshairExpansionVisual"))) FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir()/TEXT("Screenshots/FeedbackHeadshot.png"),false,false);
        Obstacle=Box(Origin+FVector(-100,0,140),FVector(10,150,150));
        Aim(); Next(41); return;
    case 41:
        if (Elapsed<.1) return;
        Weapon->ResetWeapon(); Weapon->StartFire(); Weapon->StopFire(); Next(35); return;
    case 35:
        if (!Check(Pane->bBroken && !Target->bHit && FMath::IsNearlyEqual(Target->Health,Target->MaxHealth-Weapon->Definition->HeadDamage*.75f,.1f)
            && Pawn->bLastHitHeadshot && Pawn->bLastHitWallbang,TEXT("Thin wall plus glass deals reduced head damage and wallbang feedback"))) return;
        Target->ResetTarget();
        PC->SetControlRotation((Target->Model->GetBoneLocation(TEXT("spine_03"))-Pawn->GetFirstPersonCameraComponent()->GetComponentLocation()).Rotation());
        Next(38); return;
    case 38:
        if (Elapsed<.08) return;
        Weapon->ResetWeapon(); Weapon->StartFire(); Weapon->StopFire(); Next(36); return;
    case 36:
        if (!Check(!Pawn->bLastHitHeadshot && Pawn->bLastHitWallbang && Target->Health<Target->MaxHealth,TEXT("Body wallbang selects ordinary marker"))) return;
        {
            const FVector Site=Origin+FVector(0,600,140);
            auto* A=Box(Site+FVector(-100,0,0),FVector(5,80,80));
            auto* B=Box(Site,FVector(5,80,80));
            FCollisionQueryParams Params(SCENE_QUERY_STAT(WallbangLimits),true,Pawn);
            auto Trace=[&](bool Allowed,float& Scale,int32& Layers)
            {
                FHitResult Hit; const FVector From=Site-FVector(300,0,0),To=Site+FVector(300,0,0);
                World->LineTraceSingleByChannel(Hit,From,To,ECC_Visibility,Params);
                return CrosshairBallistics::ContinueShot(World,Hit,FVector::ForwardVector,To,Params,Weapon->Definition,Allowed,Scale,Layers);
            };
            float Scale; int32 Layers;
            FHitResult Hit=Trace(true,Scale,Layers);
            if (!Check(!Hit.bBlockingHit && Layers==2 && FMath::IsNearlyEqual(Scale,.5625f),TEXT("Two thin surfaces penetrate with compounded damage reduction"))) return;
            auto* C=Box(Site+FVector(100,0,0),FVector(5,80,80));
            Hit=Trace(true,Scale,Layers);
            if (!Check(Hit.GetActor()==C && Layers==2,TEXT("Third wall stops shot at layer limit"))) return;
            C->Destroy(); A->Tags.Add(TEXT("NoWallbang")); Hit=Trace(true,Scale,Layers);
            if (!Check(Hit.GetActor()==A && Layers==0,TEXT("NoWallbang tagged cover remains solid"))) return;
            A->Tags.Reset(); Hit=Trace(false,Scale,Layers);
            if (!Check(Hit.GetActor()==A && Layers==0,TEXT("Near-barrel gate forbids penetration"))) return;
            A->SetActorScale3D(FVector(.3f,1.6f,1.6f)); B->SetActorScale3D(FVector(.6f,1.6f,1.6f));
            Hit=Trace(true,Scale,Layers);
            if (!Check(Hit.GetActor()==B && Layers==1,TEXT("Combined thickness exhausts penetration budget"))) return;
            A->Destroy(); B->Destroy();
        }
        Target->ResetTarget(); Pane->ResetGlass();
        Obstacle->SetActorScale3D(FVector(.6f,3,3));
        Aim(); Next(42); return;
    case 42:
        if (Elapsed<.1) return;
        Weapon->ResetWeapon(); Weapon->StartFire(); Weapon->StopFire(); Next(4); return;
    case 4:
        if (!Check(!Pane->bBroken && Target->Health==Target->MaxHealth,TEXT("Opaque wall stops shot before glass and target"))) return;
        Pane->Destroy(); Target->Destroy(); Obstacle->Destroy(); Key(EKeys::RightMouseButton,IE_Released);
        Obstacle=Box(Origin+FVector(0,0,40),FVector(140,200,40));
        Place(Origin+FVector(-220,0,96)); Next(5); return;
    case 5:
        if (Elapsed<.15) return;
        Key(EKeys::SpaceBar,IE_Pressed); Next(6); return;
    case 6:
        if (Elapsed<.08) return;
        if (!Check(Pawn->Traversal->bMantling,TEXT("Keyboard Jump starts mantle on reachable ledge"))) return;
        Key(EKeys::SpaceBar,IE_Released); Next(7); return;
    case 7:
        if (Elapsed<.85) return;
        if (!Check(!Pawn->Traversal->bMantling && Pawn->GetActorLocation().X>Origin.X-140 && Pawn->GetActorLocation().Z>Origin.Z+160,TEXT("Mantle ends on ledge without clipping"))) return;
        Pawn->Attempt->SaveStart(); Pawn->Attempt->ResetAttempt();
        if (!Check(Pawn->GetActorLocation().Z>Origin.Z+160,TEXT("Save and reset work after mantle"))) return;
        Place(Origin+FVector(-220,0,96)); Next(8); return;
    case 8:
        if (Elapsed<.15) return;
        Key(EKeys::Gamepad_FaceButton_Bottom,IE_Pressed); Next(9); return;
    case 9:
        if (Elapsed<.08) return;
        if (!Check(Pawn->Traversal->bMantling,TEXT("Controller A starts same mantle"))) return;
        Key(EKeys::Gamepad_FaceButton_Bottom,IE_Released); Pawn->Attempt->ResetAttempt();
        if (!Check(!Pawn->Traversal->bMantling,TEXT("Attempt reset cancels active traversal"))) return;
        Place(Origin+FVector(-220,0,96));
        // Even crouching cannot pass a ceiling only 75cm above the landing.
        Box(Origin+FVector(0,0,175),FVector(140,200,20));
        if (!Check(!Pawn->Traversal->TryMantle(),TEXT("Low ceiling rejects obstructed mantle"))) return;
        Obstacle->Destroy();
        Place(Origin+FVector(-600,0,96)); Next(10); return;
    case 10:
        if (Elapsed<.2) return;
        Key(EKeys::SpaceBar,IE_Pressed); Next(11); return;
    case 11:
        if (Elapsed<.1) return;
        if (!Check(Pawn->GetCharacterMovement()->IsFalling() && Pawn->GetCharacterMovement()->Velocity.Z>0,TEXT("Ordinary keyboard jump still works"))) return;
        Key(EKeys::SpaceBar,IE_Released);
        Place(Origin+FVector(-600,0,96)); Next(30); return;
    case 30:
        if (Elapsed<.2) return;
        // Simulate walking off a lip: no jump has been consumed yet.
        Pawn->SetActorLocation(Origin+FVector(-600,0,112));
        Pawn->GetCharacterMovement()->SetMovementMode(MOVE_Falling);
        Pawn->GetCharacterMovement()->Velocity=FVector(0,0,-10); Next(31); return;
    case 31:
        if (Elapsed<.035) return;
        Key(EKeys::SpaceBar,IE_Pressed); Next(32); return;
    case 32:
        if (Elapsed<.05) return;
        if (!Check(Pawn->GetCharacterMovement()->Velocity.Z>0,TEXT("Coyote jump works shortly after leaving ground"))) return;
        Key(EKeys::SpaceBar,IE_Released); Pawn->StopJumping();
        Pawn->SetActorLocation(Origin+FVector(-600,0,296));
        Pawn->GetCharacterMovement()->Velocity=FVector(0,0,-800); Next(33); return;
    case 33:
        if (Pawn->GetActorLocation().Z-Origin.Z>136) return;
        Key(EKeys::SpaceBar,IE_Pressed); Next(34); return;
    case 34:
        if (Elapsed<.08) return;
        if (!Check(Pawn->GetCharacterMovement()->Velocity.Z>0,TEXT("Buffered jump fires on landing"))) return;
        Key(EKeys::SpaceBar,IE_Released);
        Replay->ChangePracticeMap(TEXT("/Game/Crosshair/Maps/L_Nuketown")); Next(12); return;
    case 12:
    {
        if (!World->GetOutermost()->GetName().Contains(TEXT("L_Nuketown")) || World->GetTimeSeconds()<.5 || Elapsed<2) return;
        int32 Flowers=0;
        for (TActorIterator<AActor> Actor(World);Actor;++Actor)
        {
            TArray<UStaticMeshComponent*> Components; Actor->GetComponents(Components);
            for (auto* C : Components) if (C->GetStaticMesh() && C->GetStaticMesh()->GetPathName().StartsWith(TEXT("/Game/BlackOPSIK/2/models_bo1_nuketown_outside_mc_t5_foliage_flowers")))
            {
                if (!Check(C->GetCollisionEnabled()==ECollisionEnabled::NoCollision,TEXT("Flower foliage does not block movement or shots"))) return;
                ++Flowers;
            }
        }
        if (!Check(Flowers==82,TEXT("All 82 flower-bed decorations retain collision repair"))) return;
        int32 Count=0; bool Found=false;
        for (TActorIterator<ACrosshairWindow> It(World);It;++It)
        {
            ++Count;
            if (Found) continue;
            FVector Center,Normal; float Width,Bottom,Top;
            if (!It->GetOpening(Center,Normal,Width,Bottom,Top) || Width<65 || Top-Bottom<125) continue;
            for (float Side : {-1.f,1.f})
            {
                const FVector Direction=Normal*Side;
                FVector At=Center-Direction*90; At.Z=Bottom+50;
                FHitResult Floor; FCollisionQueryParams Params(SCENE_QUERY_STAT(ExpansionWindow),false,Pawn);
                if (!World->LineTraceSingleByChannel(Floor,At,At-FVector(0,0,200),ECC_Pawn,Params) || Floor.ImpactNormal.Z<.7f) continue;
                At.Z=Floor.ImpactPoint.Z+96;
                if (World->OverlapBlockingTestByChannel(At,FQuat::Identity,ECC_Pawn,FCollisionShape::MakeCapsule(55,95),Params)) continue;
                Place(At,Direction.Rotation().Yaw); It->RestoreBroken(true);
                if (Pawn->Traversal->TryMantle())
                {
                    Pane=*It; WindowStart=At; WindowNormal=Direction; Found=true;
                    UE_LOG(LogTemp,Display,TEXT("EXPANSION_NUKETOWN_WINDOW %s"),*It->GetName()); break;
                }
                It->ResetGlass();
            }
        }
        if (!Check(Count>=40 && Found,TEXT("Nuketown panes converted and a real house opening supports mantle"))) return;
        Next(13); return;
    }
    case 13:
        if (Elapsed<1) return;
        if (!Check(!Pawn->Traversal->bMantling && FVector::DotProduct(Pawn->GetActorLocation()-WindowStart,WindowNormal)>130,TEXT("Traverse real Nuketown window to the other side"))) return;
        Pawn->Attempt->ResetAttempt();
        if (!Check(!Pane->bBroken,TEXT("Attempt reset restores Nuketown glass"))) return;
        if (!Check(Weapon->SkinId==TEXT("Arctic"),TEXT("New camo persists across map travel"))) return;
        if (FParse::Param(FCommandLine::Get(),TEXT("CrosshairExpansionVisual")))
        {
            // Pick clear street positions so vehicles cannot hide the target.
            bool Visible=false;
            FCollisionQueryParams Params(SCENE_QUERY_STAT(ExpansionPresentation),true,Pawn);
            for (const FVector Site : {FVector(-600,1900,0),FVector(-600,600,0),FVector(-600,2600,0)})
            {
                if (Visible) break;
                for (float Yaw : {0.f,90.f,180.f,-90.f})
                {
                    FVector View=Site,Spot=Site+FRotator(0,Yaw,0).Vector()*550;
                    FHitResult FloorA,FloorB,Obstruction;
                    if (!World->LineTraceSingleByChannel(FloorA,View+FVector(0,0,1500),View-FVector(0,0,200),ECC_Pawn,Params)
                        || !World->LineTraceSingleByChannel(FloorB,Spot+FVector(0,0,1500),Spot-FVector(0,0,200),ECC_Pawn,Params)
                        || FloorA.ImpactPoint.Z>200 || FloorB.ImpactPoint.Z>200) continue;
                    View.Z=FloorA.ImpactPoint.Z+98; Spot.Z=FloorB.ImpactPoint.Z+94;
                    if (World->OverlapBlockingTestByChannel(View,FQuat::Identity,ECC_Pawn,FCollisionShape::MakeCapsule(55,96),Params)
                        || World->OverlapBlockingTestByChannel(Spot,FQuat::Identity,ECC_Pawn,FCollisionShape::MakeCapsule(28,90),Params)
                        || World->LineTraceSingleByChannel(Obstruction,View+FVector(0,0,64),Spot+FVector(0,0,45),ECC_Visibility,Params)) continue;
                    Place(View,Yaw);
                    Target=World->SpawnActor<ACrosshairDummy>(Pawn->TargetClass,Spot,FRotator(0,Yaw+180,0));
                    Visible=Target!=nullptr;
                    PC->SetControlRotation((Spot+FVector(0,0,45)-(View+FVector(0,0,64))).Rotation()); break;
                }
            }
            if (!Check(Visible,TEXT("Humanoid presentation has a clear line of sight"))) return;
            Pawn->Inventory->Equip(1); Pawn->Inventory->GetCurrent()->SetSkin(TEXT("RedTiger"));
            Next(14); return;
        }
        Next(18); return;
    case 14:
#if WITH_EDITOR
        if (GShaderCompilingManager && GShaderCompilingManager->IsCompiling()) return;
#endif
        if (Elapsed<2) return;
        FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir()/TEXT("Screenshots/ExpansionRedTigerTarget.png"),false,false);
        Next(15); return;
    case 15:
        if (Elapsed<1) return;
        PC->ToggleMenu(); PC->SetMenuTab(5);
        FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir()/TEXT("Screenshots/ExpansionCamos.png"),false,false);
        Next(16); return;
    case 16:
        if (Elapsed<1) return;
        PC->ToggleMenu(); Pawn->Inventory->Equip(2); Pawn->Inventory->GetCurrent()->SetSkin(TEXT("Arctic")); Next(17); return;
    case 17:
        if (Elapsed<1) return;
        FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir()/TEXT("Screenshots/ExpansionArctic.png"),false,false); Next(19); return;
    case 19:
        if (Elapsed<1) return;
        Next(18); return;
    case 18:
        Place(WindowStart,WindowNormal.Rotation().Yaw);
        ReplayWindow=Pane->GetFName(); Replay->GetSettings().bContinuousPractice=false; Replay->BeginAttempt(); Next(20); return;
    case 20:
        if (Elapsed<1 || !Replay->IsRecording()) return;
        Pane->TakeDamage(1,FDamageEvent(),PC,Pawn);
        if (!Check(Pane->bBroken && Pawn->Traversal->TryMantle(),TEXT("Recorded glass break and window mantle begin"))) return;
        Next(21); return;
    case 21:
        if (Elapsed<1.2) return;
        if (!Check(!Pawn->Traversal->bMantling,TEXT("Recorded mantle finishes"))) return;
        Replay->CompleteAttempt(); Next(22); return;
    case 22:
        if (!bSawBrokenReplay || !bSawMantleReplay || Replay->IsPlayback() || Elapsed<2) return;
        if (!Check(bSawBrokenReplay && bSawMantleReplay,TEXT("Replay contains broken glass and traversal movement"))) return;
        {
            ACrosshairWindow* Restored=nullptr;
            for (TActorIterator<ACrosshairWindow> It(World);It;++It) if (It->GetFName()==ReplayWindow) Restored=*It;
            if (!Check(Restored && Restored->bBroken,TEXT("Return from replay preserves broken window state"))) return;
            Pawn->Attempt->ResetAttempt();
            if (!Check(!Restored->bBroken,TEXT("Next attempt restores glass after replay"))) return;
        }
        UE_LOG(LogTemp,Display,TEXT("CROSSHAIR_EXPANSION_OK")); bFinished=true; FPlatformMisc::RequestExitWithStatus(false,0); return;
    }
}
