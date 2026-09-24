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
void UCrosshairPlacementComponent::TickComponent(float Delta, ELevelTick Type, FActorComponentTickFunction* Function)
{
	Super::TickComponent(Delta, Type, Function);
	ACrosshairCharacter* Player = Cast<ACrosshairCharacter>(GetOwner());
	if (!bPlacing || !Player || Player->IsReplayPlayback()) return;
	const UCameraComponent* Camera = Player->GetFirstPersonCameraComponent();
	FCollisionQueryParams Params(SCENE_QUERY_STAT(TargetPlacement), false, GetOwner());
	FHitResult Ground;
	const FVector Start = Camera->GetComponentLocation();
	GetWorld()->LineTraceSingleByChannel(Ground, Start, Start + Camera->GetForwardVector() * PlacementRange, ECC_Visibility, Params);
	Preview.SetLocation(Ground.ImpactPoint + FVector(0, 0, 94));
	bValidPlacement = Ground.bBlockingHit && IsSupportedSurface(Ground.ImpactNormal) && !GetWorld()->OverlapBlockingTestByChannel(Preview.GetLocation(), Preview.GetRotation(), ECC_Pawn, FCollisionShape::MakeCapsule(28, 90), Params);
	const FColor Color = bValidPlacement ? FColor::Green : FColor::Red;
	DrawDebugCapsule(GetWorld(), Preview.GetLocation(), 90, 28, Preview.GetRotation(), Color, false, 0, 0, 2);
	DrawDebugDirectionalArrow(GetWorld(), Preview.GetLocation(), Preview.GetLocation() + Preview.GetRotation().GetForwardVector() * 100, 20, Color, false, 0, 0, 3);
}
void UCrosshairPlacementComponent::Confirm()
{
	ACrosshairCharacter* Player = Cast<ACrosshairCharacter>(GetOwner());
	if (!bPlacing || !Player || !Player->CanAct()) return;
	if (!bValidPlacement) { Player->Notify(TEXT("Choose clear, supported ground")); return; }
	if (GetLayout().Num() >= MaximumTargets) { Player->Notify(TEXT("Target limit reached; remove a target first")); return; }
	GetWorld()->SpawnActor<ACrosshairDummy>(ACrosshairDummy::StaticClass(), Preview);
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
	for (const FTransform& Transform : Layout) GetWorld()->SpawnActor<ACrosshairDummy>(ACrosshairDummy::StaticClass(), Transform);
}

void UCrosshairAttemptComponent::BeginPlay() { Super::BeginPlay(); StartTransform = GetOwner()->GetActorTransform(); }
void UCrosshairAttemptComponent::SaveStart()
{
	ACrosshairCharacter* Player = Cast<ACrosshairCharacter>(GetOwner());
	if (!Player || !Player->CanAct() || !Player->GetCharacterMovement()->IsMovingOnGround())
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
	Player->StopActions();
	Player->Placement->bPlacing = false;
	Player->UnCrouch();
	Player->GetCharacterMovement()->StopMovementImmediately();
	FVector Position = StartTransform.GetLocation();
	const FRotator Rotation(0, StartTransform.Rotator().Yaw, 0);
	if (!GetWorld()->FindTeleportSpot(Player, Position, Rotation)) { Player->Notify(TEXT("Start position is blocked; choose a new start")); return; }
	Player->SetActorLocationAndRotation(Position, Rotation, false, nullptr, ETeleportType::TeleportPhysics);
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
