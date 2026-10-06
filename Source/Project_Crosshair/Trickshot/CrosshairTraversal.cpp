#include "CrosshairTraversal.h"
#include "CrosshairCharacter.h"
#include "CrosshairDummy.h"
#include "CrosshairPractice.h"
#include "CrosshairWindow.h"
#include "CrosshairThrowable.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "EngineUtils.h"

UCrosshairTraversalComponent::UCrosshairTraversalComponent() { PrimaryComponentTick.bCanEverTick=true; }
bool UCrosshairTraversalComponent::AllowsCoyoteJump() const
{
    return !bMantling && GetWorld() && GetWorld()->GetTimeSeconds()-LastGrounded<=CoyoteSeconds;
}
void UCrosshairTraversalComponent::RequestJump()
{
    auto* Player=Cast<ACrosshairCharacter>(GetOwner());
    if (!Player || !Player->CanAct() || bMantling) return;
    if (TryMantle()) return;
    JumpQueuedUntil=GetWorld()->GetTimeSeconds()+JumpBufferSeconds;
}
void UCrosshairTraversalComponent::ReleaseJump()
{
    JumpQueuedUntil=-100;
    if (auto* Player=Cast<ACrosshairCharacter>(GetOwner())) Player->StopJumping();
}
void UCrosshairTraversalComponent::CancelMantle()
{
    JumpQueuedUntil=-100;
    if (!bMantling) return;
    bMantling=false; LastGrounded=-100;
    if (auto* Player=Cast<ACrosshairCharacter>(GetOwner()))
    {
        Player->GetCharacterMovement()->GravityScale=PreviousGravityScale;
        Player->GetCharacterMovement()->SetMovementMode(MOVE_Falling);
        if (bAutoCrouched) Player->UnCrouch();
    }
}
bool UCrosshairTraversalComponent::BeginMantle(const FVector& End,const FVector& Over,bool Crouched)
{
    auto* Player=Cast<ACrosshairCharacter>(GetOwner());
    if (Crouched && !Player->bIsCrouched)
    {
        Player->Crouch(); Player->GetCharacterMovement()->Crouch(false); bAutoCrouched=Player->bIsCrouched;
        if (!Player->bIsCrouched) return false;
    }
    else bAutoCrouched=false;
    Start=Player->GetActorLocation(); Landing=End; Across=Over;
    Lift=FVector(Start.X,Start.Y,Over.Z);
    FCollisionQueryParams Params(SCENE_QUERY_STAT(MantlePath),false,Player);
    const auto* Capsule=Player->GetCapsuleComponent();
    const FCollisionShape Shape=FCollisionShape::MakeCapsule(Capsule->GetScaledCapsuleRadius(),Capsule->GetScaledCapsuleHalfHeight()-.5f);
    FHitResult Hit;
    for (const auto& Segment : {TPair<FVector,FVector>(Start,Lift),TPair<FVector,FVector>(Lift,Across),TPair<FVector,FVector>(Across,Landing)})
        if (GetWorld()->SweepSingleByChannel(Hit,Segment.Key,Segment.Value,FQuat::Identity,ECC_Pawn,Shape,Params))
        {
            if (bAutoCrouched) Player->UnCrouch();
            return false;
        }
    Player->GetCharacterMovement()->StopMovementImmediately();
    Player->StopJumping(); JumpQueuedUntil=-100; Elapsed=0; bMantling=true;
    // Flying automatically uncrouches Unreal characters. Falling with gravity
    // temporarily disabled preserves the clearance-tested crouched capsule.
    PreviousGravityScale=Player->GetCharacterMovement()->GravityScale;
    Player->GetCharacterMovement()->GravityScale=0;
    Player->GetCharacterMovement()->SetMovementMode(MOVE_Falling);
    return true;
}
bool UCrosshairTraversalComponent::TryMantle()
{
    auto* Player=Cast<ACrosshairCharacter>(GetOwner());
    if (!Player || !Player->CanAct() || bMantling || Player->Lethals->IsBusy() || Player->Placement->bPlacing) return false;
    const auto* Capsule=Player->GetCapsuleComponent();
    const float Radius=Capsule->GetScaledCapsuleRadius();
    const float Standing=Player->GetClass()->GetDefaultObject<ACrosshairCharacter>()->GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight();
    const float Small=Player->GetCharacterMovement()->GetCrouchedHalfHeight();
    const FVector Feet=Player->GetActorLocation()-FVector(0,0,Capsule->GetScaledCapsuleHalfHeight());
    const FVector Forward=FRotator(0,Player->GetControlRotation().Yaw,0).Vector();
    FCollisionQueryParams Params(SCENE_QUERY_STAT(MantleSearch),false,Player);
    auto Clear=[&](FVector At,float Half) { return !GetWorld()->OverlapBlockingTestByChannel(At,FQuat::Identity,ECC_Pawn,FCollisionShape::MakeCapsule(Radius,Half-.5f),Params); };
    // Broken windows can lead to a lower floor; plan a lift/across/drop path.
    for (TActorIterator<ACrosshairWindow> It(GetWorld());It;++It)
    {
        if (!It->bBroken) continue;
        FVector Center,Normal; float Width,Bottom,Top;
        if (!It->GetOpening(Center,Normal,Width,Bottom,Top)) continue;
        const float Dot=FVector::DotProduct(Forward,Normal);
        if (FMath::Abs(Dot)<.65f) continue;
        const float Distance=FVector::DotProduct(Center-Feet,Normal)/Dot;
        if (Distance<15 || Distance>Reach || Bottom-Feet.Z>MaximumHeight || Top<Feet.Z+80) continue;
        const FVector Intersection=Feet+Forward*Distance;
        const FVector Side=FVector::CrossProduct(FVector::UpVector,Normal);
        if (FMath::Abs(FVector::DotProduct(Intersection-Center,Side))+Radius+4>Width) continue;
        const float CrossFeet=FMath::Max(Feet.Z,Bottom+6);
        const bool Crouched=Player->bIsCrouched || CrossFeet+Standing*2>Top-6;
        const float Half=Crouched ? Small : Standing;
        if (CrossFeet+Half*2>Top-4) continue;
        FVector Over=Feet+Forward*(Distance+Radius+22); Over.Z=CrossFeet+Half+2;
        FHitResult Floor;
        if (!GetWorld()->LineTraceSingleByChannel(Floor,Over,Over-FVector(0,0,Half+MaximumHeight+60),ECC_Pawn,Params) || Floor.ImpactNormal.Z<.7f) continue;
        FVector End=Over; End.Z=Floor.ImpactPoint.Z+Half+2;
        if (!Clear(Over,Half) || !Clear(End,Half)) continue;
        if (BeginMantle(End,Over,Crouched)) return true;
    }
    FHitResult Wall;
    const FVector Probe=Feet+FVector(0,0,35);
    if (!GetWorld()->SweepSingleByChannel(Wall,Probe,Probe+Forward*Reach,FQuat::Identity,ECC_Pawn,FCollisionShape::MakeSphere(10),Params)
        || FMath::Abs(Wall.ImpactNormal.Z)>.35f || Cast<ACrosshairDummy>(Wall.GetActor()) || Cast<ACrosshairWindow>(Wall.GetActor())) return false;
    FVector Top=Wall.ImpactPoint+Forward*(Radius+14); Top.Z=Feet.Z+MaximumHeight+12;
    FHitResult Ledge;
    if (!GetWorld()->LineTraceSingleByChannel(Ledge,Top,Top-FVector(0,0,MaximumHeight+20),ECC_Pawn,Params) || Ledge.ImpactNormal.Z<.7f) return false;
    const float Height=Ledge.ImpactPoint.Z-Feet.Z;
    if (Height<MinimumHeight || Height>MaximumHeight) return false;
    FVector End=Ledge.ImpactPoint+FVector(0,0,Standing+2);
    bool Crouched=Player->bIsCrouched;
    if (Crouched || !Clear(End,Standing)) { Crouched=true; End.Z=Ledge.ImpactPoint.Z+Small+2; }
    const float Half=Crouched ? Small : Standing;
    if (!Clear(End,Half)) return false;
    return BeginMantle(End,End+FVector(0,0,3),Crouched);
}
void UCrosshairTraversalComponent::TickComponent(float Delta,ELevelTick Type,FActorComponentTickFunction* Function)
{
    Super::TickComponent(Delta,Type,Function);
    auto* Player=Cast<ACrosshairCharacter>(GetOwner());
    if (!Player || Player->IsReplayPlayback()) return;
    const double Now=GetWorld()->GetTimeSeconds();
    if (!bMantling)
    {
        if (Player->GetCharacterMovement()->IsMovingOnGround()) LastGrounded=Now;
        if (JumpQueuedUntil>=Now && Player->CanAct() && Now-LastGrounded<=CoyoteSeconds)
        { Player->Jump(); JumpQueuedUntil=-100; }
        return;
    }
    if (!Player->CanAct()) { CancelMantle(); return; }
    Player->GetCharacterMovement()->StopMovementImmediately();
    const float PreviousT=Elapsed/FMath::Max(.1f,Duration);
    Elapsed+=Delta;
    const float T=FMath::Clamp(Elapsed/FMath::Max(.1f,Duration),0.f,1.f);
    auto Smooth=[](float A){ A=FMath::Clamp(A,0.f,1.f); return A*A*(3-2*A); };
    const FVector Position=T<.35f ? FMath::Lerp(Start,Lift,Smooth(T/.35f)) :
        (T<.8f ? FMath::Lerp(Lift,Across,Smooth((T-.35f)/.45f)) : FMath::Lerp(Across,Landing,Smooth((T-.8f)/.2f)));
    auto MoveTo=[&](const FVector& At)
    {
        FHitResult Hit; Player->SetActorLocation(At,true,&Hit);
        if (Hit.bBlockingHit) { CancelMantle(); return false; }
        return true;
    };
    // A hitch may cross a path corner in one tick. Sweep via each waypoint
    // instead of cutting diagonally through a sill that the planned path cleared.
    if (PreviousT<.35f && T>=.35f && !MoveTo(Lift)) return;
    if (PreviousT<.8f && T>=.8f && !MoveTo(Across)) return;
    if (!MoveTo(Position)) return;
    if (T>=1)
    {
        bMantling=false; Player->GetCharacterMovement()->GravityScale=PreviousGravityScale;
        Player->GetCharacterMovement()->SetMovementMode(MOVE_Falling);
        if (bAutoCrouched) Player->UnCrouch();
    }
}
