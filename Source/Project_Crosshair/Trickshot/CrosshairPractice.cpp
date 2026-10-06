#include "CrosshairPractice.h"
#include "CrosshairCharacter.h"
#include "CrosshairDummy.h"
#include "CrosshairWindow.h"
#include "CrosshairWeapon.h"
#include "CrosshairThrowable.h"
#include "CrosshairReplaySubsystem.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "EngineUtils.h"
#include "DrawDebugHelpers.h"
#include "TimerManager.h"
#include "Components/SkeletalMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Engine/StaticMesh.h"
#include "Components/CapsuleComponent.h"

UCrosshairPlacementComponent::UCrosshairPlacementComponent() { PrimaryComponentTick.bCanEverTick = true; }
void UCrosshairPlacementComponent::Toggle()
{
	if (ACrosshairCharacter* Player = Cast<ACrosshairCharacter>(GetOwner()))
	{
		if (!Player->CanAct()) return;
        if (!Player->GetGameInstance()->GetSubsystem<UCrosshairReplaySubsystem>()->GetSettings().bSandboxTargets)
        { Player->Notify(TEXT("Enable sandbox target editing in Settings / Controls.")); return; }
		bPlacing = !bPlacing;
		Player->StopActions();
		Player->Notify(bPlacing ? TEXT("Place target: aim at ground, Fire to confirm, ADS to cancel") : TEXT("Placement cancelled"));
	}
}
void UCrosshairPlacementComponent::BeginPlay()
{
	Super::BeginPlay();
	PreviewMesh = NewObject<USkeletalMeshComponent>(GetOwner(), TEXT("PlacementPreview"));
    const auto* Template = TargetClass ? TargetClass->GetDefaultObject<ACrosshairDummy>() : GetDefault<ACrosshairDummy>();
    PreviewMesh->SetSkeletalMesh(Template->Model->GetSkeletalMeshAsset());
	PreviewMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	PreviewMesh->SetCastShadow(false);
	PreviewMesh->RegisterComponent();
	if (Template->TargetIdle) PreviewMesh->PlayAnimation(Template->TargetIdle,true);
    PreviewMesh->VisibilityBasedAnimTickOption=EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
	PreviewMesh->SetVisibility(false);
}
void UCrosshairPlacementComponent::EndPlay(EEndPlayReason::Type Reason)
{
	if (PreviewMesh) PreviewMesh->DestroyComponent();
	Super::EndPlay(Reason);
}
void UCrosshairPlacementComponent::UpdatePreview()
{
	ACrosshairCharacter* Player = Cast<ACrosshairCharacter>(GetOwner());
	bValidPlacement = false;
	if (!Player || !Player->CanAct() || !bPlacing) return;
	const UCameraComponent* Camera = Player->GetFirstPersonCameraComponent();
	FCollisionQueryParams Params(SCENE_QUERY_STAT(TargetPlacement), false);
	FHitResult Ground;
	const FVector Start = Camera->GetComponentLocation();
	Params.AddIgnoredActor(Player);
	GetWorld()->LineTraceSingleByChannel(Ground, Start, Start + Camera->GetForwardVector() * PlacementRange, ECC_Visibility, Params);
	Preview.SetLocation(Ground.ImpactPoint + FVector(0, 0, 94));
	// Include the player in clearance checks; do not place a target inside the player capsule.
	FCollisionQueryParams Clearance(SCENE_QUERY_STAT(TargetClearance), false);
	bValidPlacement = Ground.bBlockingHit && IsSupportedSurface(Ground.ImpactNormal) && !GetWorld()->OverlapBlockingTestByChannel(Preview.GetLocation(), Preview.GetRotation(), ECC_Pawn, FCollisionShape::MakeCapsule(28, 90), Clearance);
}
void UCrosshairPlacementComponent::TickComponent(float Delta, ELevelTick Type, FActorComponentTickFunction* Function)
{
	Super::TickComponent(Delta, Type, Function);
	UpdatePreview();
	if (PreviewMesh)
	{
		PreviewMesh->SetVisibility(bPlacing);
		PreviewMesh->SetWorldLocationAndRotation(Preview.GetLocation()-FVector(0,0,90), (Preview.Rotator()+FRotator(0,-90,0)).Quaternion());
        const auto* Template = TargetClass ? TargetClass->GetDefaultObject<ACrosshairDummy>() : GetDefault<ACrosshairDummy>();
        if (Template->TargetMaterial)
        {
            auto* Tint=Cast<UMaterialInstanceDynamic>(PreviewMesh->GetOverlayMaterial());
            if (!Tint) { Tint=UMaterialInstanceDynamic::Create(Template->TargetMaterial,this); PreviewMesh->SetOverlayMaterial(Tint); }
            Tint->SetVectorParameterValue(TEXT("Color"),bValidPlacement ? FLinearColor(.08f,.6f,.25f) : FLinearColor(.8f,.08f,.04f));
        }
	}
}
void UCrosshairPlacementComponent::Confirm()
{
	ACrosshairCharacter* Player = Cast<ACrosshairCharacter>(GetOwner());
	if (!bPlacing || !Player || !Player->CanAct() || !Player->GetGameInstance()->GetSubsystem<UCrosshairReplaySubsystem>()->GetSettings().bSandboxTargets) return;
	UpdatePreview();
	if (!bValidPlacement) { Player->Notify(TEXT("Choose clear, supported ground")); return; }
	if (GetLayout().Num() >= MaximumTargets) { Player->Notify(TEXT("Target limit reached; remove a target first")); return; }
	if (!TargetClass) { Player->Notify(TEXT("Target class is missing: check practice Blueprint setup")); return; }
	FActorSpawnParameters Spawn;
	Spawn.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::DontSpawnIfColliding;
	if (!GetWorld()->SpawnActor<ACrosshairDummy>(TargetClass, Preview, Spawn)) { Player->Notify(TEXT("Target could not be placed; choose clear ground")); return; }
	bPlacing = false;
	Player->Notify(TEXT("Target placed"));
}
void UCrosshairPlacementComponent::Rotate() { if (bPlacing) Preview.SetRotation((Preview.Rotator() + FRotator(0, 15, 0)).Quaternion()); }
void UCrosshairPlacementComponent::RemoveAimedTarget()
{
	ACrosshairCharacter* Player = Cast<ACrosshairCharacter>(GetOwner());
	if (!Player || !Player->CanAct()) return;
    if (!Player->GetGameInstance()->GetSubsystem<UCrosshairReplaySubsystem>()->GetSettings().bSandboxTargets) { Player->Notify(TEXT("Enable sandbox target editing in Settings / Controls.")); return; }
	const UCameraComponent* Camera = Player->GetFirstPersonCameraComponent();
	FHitResult Hit;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(RemoveTarget), false, GetOwner());
	GetWorld()->LineTraceSingleByChannel(Hit, Camera->GetComponentLocation(), Camera->GetComponentLocation() + Camera->GetForwardVector() * PlacementRange, ECC_Visibility, Params);
	if (ACrosshairDummy* Target = Cast<ACrosshairDummy>(Hit.GetActor())) { Target->Destroy(); Player->Notify(TEXT("Target removed")); }
}
void UCrosshairPlacementComponent::ClearTargets()
{
	ACrosshairCharacter* Player = Cast<ACrosshairCharacter>(GetOwner());
	if (!Player || !Player->CanAct()) return;
    if (!Player->GetGameInstance()->GetSubsystem<UCrosshairReplaySubsystem>()->GetSettings().bSandboxTargets) { Player->Notify(TEXT("Enable sandbox target editing in Settings / Controls.")); return; }
	for (TActorIterator<ACrosshairDummy> It(GetWorld()); It; ++It) It->Destroy();
	Player->Notify(TEXT("Targets cleared"));
}
TArray<FTransform> UCrosshairPlacementComponent::GetLayout() const
{
	TArray<FTransform> Result;
	for (TActorIterator<ACrosshairDummy> It(GetWorld()); It; ++It) if (!It->IsActorBeingDestroyed()) Result.Add(It->GetActorTransform());
	return Result;
}
void UCrosshairPlacementComponent::RestoreLayout(const TArray<FTransform>& Layout)
{
	for (TActorIterator<ACrosshairDummy> It(GetWorld()); It; ++It) It->Destroy();
	for (const FTransform& Transform : Layout) if (TargetClass) GetWorld()->SpawnActor<ACrosshairDummy>(TargetClass, Transform);
}

void UCrosshairAttemptComponent::BeginPlay() { Super::BeginPlay(); StartTransform = GetOwner()->GetActorTransform(); InitialStart = StartTransform; }
void UCrosshairAttemptComponent::SaveStart()
{
	ACrosshairCharacter* Player = Cast<ACrosshairCharacter>(GetOwner());
	if (!Player || Player->IsReplayPlayback()) return;
	if (Player->GetGameInstance()->GetSubsystem<UCrosshairReplaySubsystem>()->IsFinishing())
	{
		Player->Notify(TEXT("Wait for the successful replay to finish before saving a position"));
		return;
	}
	if (Player->bIsCrouched || !Player->GetCharacterMovement()->IsMovingOnGround())
	{
		if (Player) Player->Notify(TEXT("Stand upright on a floor or platform before saving your position"));
		return;
	}
	StartTransform = FTransform(Player->GetControlRotation(), Player->GetActorLocation());
	bHasSavedStart = true;
	Player->Notify(TEXT("Position saved. T / D-pad Down resets here."));
	ResetAttempt();
}
void UCrosshairAttemptComponent::ResetAttempt()
{
	ACrosshairCharacter* Player = Cast<ACrosshairCharacter>(GetOwner());
	if (!Player || Player->IsReplayPlayback()) return;
	if (Player->GetGameInstance()->GetSubsystem<UCrosshairReplaySubsystem>()->IsFinishing())
	{
		Player->Notify(TEXT("Finishing your successful replay; reset will be available shortly"));
		return;
	}
	bSucceeded = false;
	Player->Lethals->Reset();
	Player->StopActions();
	Player->Placement->bPlacing = false;
	Player->UnCrouch();
	Player->GetCharacterMovement()->StopMovementImmediately();
	for (TActorIterator<ACrosshairWindow> It(GetWorld()); It; ++It) It->ResetGlass();
	FVector Position = StartTransform.GetLocation();
	const FRotator Rotation(0, StartTransform.Rotator().Yaw, 0);
	if (!GetWorld()->FindTeleportSpot(Player, Position, Rotation))
	{
		Position = InitialStart.GetLocation();
		if (!GetWorld()->FindTeleportSpot(Player, Position, Rotation))
		{
			Player->Notify(TEXT("Start blocked. Move to clear ground and save a new start."));
			return;
		}
		StartTransform = InitialStart;
		bHasSavedStart = false;
		Player->Notify(TEXT("Saved start blocked; restored original spawn."));
	}
	Player->GetCharacterMovement()->SetMovementMode(MOVE_Falling);
	Player->SetActorLocationAndRotation(Position, FRotator(0, StartTransform.Rotator().Yaw, 0), false, nullptr, ETeleportType::TeleportPhysics);
	if (Player->GetController()) Player->GetController()->SetControlRotation(StartTransform.Rotator());
	Player->Inventory->ResetWeapons();
	for (TActorIterator<ACrosshairDummy> It(GetWorld()); It; ++It) It->ResetTarget();
	bSucceeded = false;
	Player->GetGameInstance()->GetSubsystem<UCrosshairReplaySubsystem>()->BeginAttempt();
}
void UCrosshairAttemptComponent::HandleTargetHit(ACrosshairDummy* Target)
{
	ACrosshairCharacter* Player = Cast<ACrosshairCharacter>(GetOwner());
	if (!Player || Player->IsReplayPlayback() || bSucceeded) return;
	UCrosshairReplaySubsystem* Replay = Player->GetGameInstance()->GetSubsystem<UCrosshairReplaySubsystem>();
	if (Replay->GetSettings().bContinuousPractice)
	{
		FTimerHandle Timer;
		GetWorld()->GetTimerManager().SetTimer(Timer, FTimerDelegate::CreateWeakLambda(Target, [Target]() { Target->ResetTarget(); }), 0.3f, false);
		return;
	}
	bSucceeded = true;
	Player->StopActions();
	Replay->CompleteAttempt();
}
