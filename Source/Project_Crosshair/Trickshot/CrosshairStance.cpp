#include "CrosshairStance.h"
#include "CrosshairCharacter.h"
#include "CrosshairPractice.h"
#include "CrosshairThrowable.h"
#include "CrosshairTraversal.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Net/UnrealNetwork.h"

UCrosshairStanceComponent::UCrosshairStanceComponent()
{
 PrimaryComponentTick.bCanEverTick=true;
 SetIsReplicatedByDefault(true);
}
void UCrosshairStanceComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
 Super::GetLifetimeReplicatedProps(OutLifetimeProps);
 DOREPLIFETIME(UCrosshairStanceComponent,Stance);
}
ACrosshairCharacter* UCrosshairStanceComponent::GetPlayer() const { return Cast<ACrosshairCharacter>(GetOwner()); }
void UCrosshairStanceComponent::OnRep_Stance()
{
 // The owner already changed stance when it made the decision.
 const ACrosshairCharacter* Player=GetPlayer();
 if (Player && !Player->IsLocallyControlled()) { Elapsed=0; ApplyStance(); }
}
void UCrosshairStanceComponent::ServerSetStance_Implementation(ECrosshairStance NewStance,FVector_NetQuantizeNormal NewDirection)
{
 const bool bDive=NewStance==ECrosshairStance::Dive && Stance!=ECrosshairStance::Dive;
 Stance=NewStance; Direction=NewDirection; Elapsed=0;
 ApplyStance();
 if (ACrosshairCharacter* Player=GetPlayer(); Player && bDive) Player->LaunchCharacter(Direction*DiveForwardSpeed+FVector::UpVector*DiveUpSpeed,true,true);
}
void UCrosshairStanceComponent::SetStance(ECrosshairStance NewStance,const FVector& NewDirection)
{
 ACrosshairCharacter* Player=GetPlayer();
 if (!Player || NewStance==Stance) return;
 const bool bDive=NewStance==ECrosshairStance::Dive;
 Stance=NewStance; Direction=NewDirection; Elapsed=0;
 ApplyStance();
 if (bDive) Player->LaunchCharacter(Direction*DiveForwardSpeed+FVector::UpVector*DiveUpSpeed,true,true);
 if (!Player->HasAuthority() && Player->IsLocallyControlled()) ServerSetStance(NewStance,NewDirection);
}
void UCrosshairStanceComponent::ApplyStance()
{
 ACrosshairCharacter* Player=GetPlayer();
 if (!Player) return;
 UCharacterMovementComponent* Move=Player->GetCharacterMovement();
 Move->SetCrouchedHalfHeight(CrosshairStanceRules::HalfHeight(Stance));
 if (Stance!=ECrosshairStance::Normal) Player->Crouch();
 // Already crouched at another height: resize now. Simulated copies follow the replicated crouch state.
 if (Player->bIsCrouched || Stance!=ECrosshairStance::Normal) Move->Crouch(Player->GetLocalRole()==ROLE_SimulatedProxy);
}
bool UCrosshairStanceComponent::HasRoom(float HalfHeight) const
{
 const ACrosshairCharacter* Player=GetPlayer();
 const UCapsuleComponent* Capsule=Player->GetCapsuleComponent();
 const float Radius=Capsule->GetScaledCapsuleRadius(),Current=Capsule->GetScaledCapsuleHalfHeight();
 FCollisionQueryParams Params(SCENE_QUERY_STAT(CrosshairStanceRoom),false,Player);
 return !GetWorld()->OverlapBlockingTestByChannel(Player->GetActorLocation()+FVector(0,0,HalfHeight-Current+1.f),FQuat::Identity,ECC_Pawn,FCollisionShape::MakeCapsule(Radius-1.f,HalfHeight-1.f),Params);
}
void UCrosshairStanceComponent::PressProne()
{
 ACrosshairCharacter* Player=GetPlayer();
 if (!Player || !Player->CanAct() || BlocksMoveInput() || Player->Traversal->bMantling || Player->Placement->bPlacing) return;
 if (Stance==ECrosshairStance::Prone)
 {
  const float Standing=Player->GetClass()->GetDefaultObject<ACrosshairCharacter>()->GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight();
  if (HasRoom(Standing)) { SetStance(ECrosshairStance::Normal,Direction); Player->UnCrouch(); }
  else ProneToCrouch();
  return;
 }
 if (!Player->GetCharacterMovement()->IsMovingOnGround()) return;
 if (Player->IsSprinting()) SetStance(ECrosshairStance::Dive,Player->GetVelocity().GetSafeNormal2D());
 else SetStance(ECrosshairStance::Prone,Direction);
}
bool UCrosshairStanceComponent::TrySlide()
{
 ACrosshairCharacter* Player=GetPlayer();
 if (!Player || !Player->CanAct() || IsActive() || !Player->IsSprinting() || !Player->GetCharacterMovement()->IsMovingOnGround() || Player->Traversal->bMantling) return false;
 SetStance(ECrosshairStance::Slide,Player->GetVelocity().GetSafeNormal2D());
 return true;
}
void UCrosshairStanceComponent::ProneToCrouch()
{
 if (Stance!=ECrosshairStance::Prone) return;
 if (HasRoom(CrosshairStanceRules::CrouchHalfHeight)) SetStance(ECrosshairStance::Normal,Direction);
 else if (ACrosshairCharacter* Player=GetPlayer()) Player->Notify(TEXT("No room to get up here"));
}
bool UCrosshairStanceComponent::HandleJump()
{
 ACrosshairCharacter* Player=GetPlayer();
 if (!Player || !IsActive()) return false;
 if (Stance==ECrosshairStance::Dive) return true;
 if (Stance==ECrosshairStance::Prone) { PressProne(); return true; }
 // A slide gives way to a jump: Traversal buffers the press until the capsule has stood back up.
 SetStance(ECrosshairStance::Normal,Direction);
 Player->UnCrouch();
 return false;
}
float UCrosshairStanceComponent::GetCrouchedSpeed(float CrouchSpeed) const
{
 if (Stance==ECrosshairStance::Prone) return ProneSpeed;
 if (Stance==ECrosshairStance::Slide) return CrosshairStanceRules::SlideSpeed(SlideEntrySpeed,CrouchSpeed,Elapsed/FMath::Max(.1f,SlideSeconds));
 return CrouchSpeed;
}
void UCrosshairStanceComponent::Interrupt()
{
 if (Stance==ECrosshairStance::Slide) SetStance(ECrosshairStance::Normal,Direction);
}
void UCrosshairStanceComponent::Reset()
{
 if (!IsActive()) return;
 // Also runs on the host for a joined player's soldier, which the client cannot be asked to do.
 Stance=ECrosshairStance::Normal; Elapsed=0;
 ApplyStance();
 ACrosshairCharacter* Player=GetPlayer();
 if (Player && !Player->HasAuthority() && Player->IsLocallyControlled()) ServerSetStance(ECrosshairStance::Normal,Direction);
}
void UCrosshairStanceComponent::TickComponent(float Delta,ELevelTick Type,FActorComponentTickFunction* Function)
{
 Super::TickComponent(Delta,Type,Function);
 ACrosshairCharacter* Player=GetPlayer();
 if (!IsActive() || !Player || Player->IsReplayPlayback()) return;
 Elapsed+=Delta;
 // Only the owner decides; the host mirrors what it is told.
 if (!Player->IsLocallyControlled()) return;
 const UCharacterMovementComponent* Move=Player->GetCharacterMovement();
 if (Stance==ECrosshairStance::Slide)
 {
  Player->AddMovementInput(Direction,1.f);
  if (!Move->IsMovingOnGround()) { SetStance(ECrosshairStance::Normal,Direction); Player->UnCrouch(); }
  else if (Elapsed>=SlideSeconds || (Elapsed>.15f && Player->GetVelocity().Size2D()<Player->CrouchSpeed*1.1f)) SetStance(ECrosshairStance::Normal,Direction);
 }
 else if (Stance==ECrosshairStance::Dive && Elapsed>.1f && Move->IsMovingOnGround()) SetStance(ECrosshairStance::Prone,Direction);
}
