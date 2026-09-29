#include "CrosshairPractice.h"
#include "CrosshairCharacter.h"
#include "CrosshairDummy.h"
#include "CrosshairWeapon.h"
#include "CrosshairReplaySubsystem.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "EngineUtils.h"
#include "DrawDebugHelpers.h"
#include "TimerManager.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Components/CapsuleComponent.h"

UCrosshairPlacementComponent::UCrosshairPlacementComponent() { PrimaryComponentTick.bCanEverTick = true; }
void UCrosshairPlacementComponent::Toggle()
{
	if (ACrosshairCharacter* Player = Cast<ACrosshairCharacter>(GetOwner()))
	{
		if (!Player->CanAct()) return;
		bPlacing = !bPlacing;
		Player->StopActions();
		Player->Notify(bPlacing ? TEXT("Place target: aim at ground, Fire to confirm, ADS to cancel") : TEXT("Placement cancelled"));
	}
}
void UCrosshairPlacementComponent::BeginPlay()
{
	Super::BeginPlay();
	PreviewMesh = NewObject<UStaticMeshComponent>(GetOwner(), TEXT("PlacementPreview"));
	PreviewMesh->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cylinder.Cylinder")));
	PreviewMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	PreviewMesh->SetCastShadow(false);
	PreviewMesh->RegisterComponent();
	PreviewMesh->SetWorldScale3D(FVector(0.56, 0.56, 1.8));
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
		PreviewMesh->SetWorldLocationAndRotation(Preview.GetLocation(), Preview.GetRotation());
	}
}
void UCrosshairPlacementComponent::Confirm()
{
	ACrosshairCharacter* Player = Cast<ACrosshairCharacter>(GetOwner());
	if (!bPlacing || !Player || !Player->CanAct()) return;
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
	if (!Player || Player->IsReplayPlayback() || !Player->GetCharacterMovement()->IsMovingOnGround())
	{
		if (Player) Player->Notify(TEXT("Stand on the ground before saving your start"));
		return;
	}
	StartTransform = FTransform(Player->GetControlRotation(), Player->GetActorLocation());
	Player->Notify(TEXT("Attempt start saved"));
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
	Player->StopActions();
	Player->Placement->bPlacing = false;
	Player->UnCrouch();
	Player->GetCharacterMovement()->StopMovementImmediately();
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
