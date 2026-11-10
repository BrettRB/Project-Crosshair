#include "CrosshairBot.h"
#include "CrosshairCharacter.h"
#include "CrosshairData.h"
#include "CrosshairMatch.h"
#include "CrosshairStairLinks.h"
#include "CrosshairWeapon.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/GameInstance.h"
#include "EngineUtils.h"
#include "NavigationSystem.h"
#include "Navigation/PathFollowingComponent.h"

ACrosshairBotController::ACrosshairBotController()
{
 bWantsPlayerState=true;
 // Aim pitch is driven here; never reset it to the pawn's level orientation.
 bSetControlRotationFromPawnOrientation=false;
 PrimaryActorTick.bCanEverTick=true;
}
void ACrosshairBotController::OnPossess(APawn* InPawn)
{
 Super::OnPossess(InPawn);
 Target=nullptr; TripStage=0; bHasDestination=false; bDirectMove=false; bChasingTag=false; GoalTag=nullptr; NextDecisionAt=0; LastKnown=FVector::ZeroVector;
 Tuning=GetGameInstance()->GetSubsystem<UCrosshairBotMatch>()->GetCurrentTuning();
 if (auto* Soldier=Cast<ACrosshairCharacter>(InPawn))
 {
  Soldier->WalkSpeed=Tuning.MovementSpeed;
  Soldier->SprintSpeed=Tuning.MovementSpeed*1.35f;
  SetControlRotation(FRotator(0,InPawn->GetActorRotation().Yaw,0));
 }
 LastProgressAt=GetWorld()->GetTimeSeconds(); LastProgressLocation=InPawn ? InPawn->GetActorLocation() : FVector::ZeroVector;
}
void ACrosshairBotController::UpdateControlRotation(float DeltaTime, bool bUpdatePawn)
{
 // While engaging, Engage() owns the aim (pitch included): only turn the body to the aim's yaw.
 // Before this, the engine reset pitch to level every tick, so bots could not aim at anyone above or below them.
 if (Target)
 {
  if (APawn* Self=GetPawn(); Self && bUpdatePawn) Self->FaceRotation(GetControlRotation(),DeltaTime);
  return;
 }
 Super::UpdateControlRotation(DeltaTime,bUpdatePawn);
}
void ACrosshairBotController::OnUnPossess()
{
 StopMovement(); Target=nullptr;
 Super::OnUnPossess();
}
FVector ACrosshairBotController::AimPointOn(const ACrosshairCharacter* Soldier) const
{
 const USkeletalMeshComponent* Body=Soldier->GetMesh();
 return Body && Body->GetBoneIndex(TEXT("spine_03"))!=INDEX_NONE ? Body->GetBoneLocation(TEXT("spine_03")) : Soldier->GetActorLocation()+FVector(0,0,40);
}
bool ACrosshairBotController::CanSee(const ACrosshairCharacter* Other) const
{
 const APawn* Self=GetPawn();
 if (!Self || !Other) return false;
 FCollisionQueryParams Params(SCENE_QUERY_STAT(CrosshairBotSight),true,Self);
 if (const auto* Soldier=Cast<ACrosshairCharacter>(Self))
  for (const ACrosshairWeapon* Weapon:Soldier->Inventory->Weapons) Params.AddIgnoredActor(Weapon);
 const FVector Eye=Self->GetPawnViewLocation();
 const USkeletalMeshComponent* Body=Other->GetMesh();
 for (const FName Bone:{FName(TEXT("head")),FName(TEXT("spine_03"))})
 {
  const FVector Point=Body && Body->GetBoneIndex(Bone)!=INDEX_NONE ? Body->GetBoneLocation(Bone) : Other->GetPawnViewLocation();
  FHitResult Hit;
  if (!GetWorld()->LineTraceSingleByChannel(Hit,Eye,Point,ECC_Visibility,Params) || Hit.GetActor()==Other) return true;
 }
 return false;
}
void ACrosshairBotController::Decide()
{
 auto* Self=Cast<ACrosshairCharacter>(GetPawn());
 const auto* Mode=GetWorld()->GetAuthGameMode<ACrosshairMatchGameMode>();
 const double Now=GetWorld()->GetTimeSeconds();
 // Read live: difficulty changed from the pause menu applies without restarting the match.
 Tuning=GetGameInstance()->GetSubsystem<UCrosshairBotMatch>()->GetCurrentTuning();
 Self->WalkSpeed=Tuning.MovementSpeed; Self->SprintSpeed=Tuning.MovementSpeed*1.35f;
 const FVector Eye=Self->GetPawnViewLocation();
 const FVector Forward=GetControlRotation().Vector();
 ACrosshairCharacter* Best=nullptr; float BestDistance=FLT_MAX;
 for (TActorIterator<ACrosshairCharacter> It(GetWorld());It;++It)
 {
  if (*It==Self || It->Health->bDead || !It->GetController() || (Mode && !Mode->CanDamage(this,It->GetController()))) continue;
  const FVector To=It->GetPawnViewLocation()-Eye;
  const float Distance=To.Size();
  // Close enemies are noticed in any direction (footsteps); farther ones need to be in the bot's field of view.
  // The view cone is horizontal only: bots check up and down, so a player on a roof, ledge or mid-jump above
  // them is noticed as long as they are ahead and in line of sight.
  const FVector Flat=To.GetSafeNormal2D();
  const bool Aware=Distance<1500.f || Flat.IsNearlyZero() || FVector::DotProduct(Flat,Forward.GetSafeNormal2D())>FMath::Cos(FMath::DegreesToRadians(75.f));
  if (!Aware || Distance>=BestDistance || !CanSee(*It)) continue;
  Best=*It; BestDistance=Distance;
 }
 if (bPassive) Best=nullptr;
 if (Best!=Target) { Target=Best; TargetSeenAt=Now; }
 if (Target) { LastKnown=Target->GetActorLocation(); bHasDestination=false; }
 // Aim error is an offset at the target's range, re-rolled each decision.
 AimError=FMath::VRand()*FMath::Tan(FMath::DegreesToRadians(Tuning.AimErrorDegrees))*FMath::Max(BestDistance==FLT_MAX ? 0.f : BestDistance,300.f)*FMath::FRandRange(.3f,1.f);
 NextDecisionAt=Now+Tuning.DecisionSeconds;
}
void ACrosshairBotController::Tick(float DeltaSeconds)
{
 Super::Tick(DeltaSeconds);
 auto* Self=Cast<ACrosshairCharacter>(GetPawn());
 const auto* State=ACrosshairMatchState::Get(this);
 if (!Self || Self->Health->bDead || !State || State->bMatchOver)
 {
  if (Self && Self->Inventory->GetCurrent()) Self->Inventory->GetCurrent()->StopFire();
  return;
 }
 if (GetWorld()->GetTimeSeconds()>=NextDecisionAt) Decide();
 if (Target && (!IsValid(Target) || Target->Health->bDead)) Target=nullptr;
 if (Target) Engage(DeltaSeconds);
 else
 {
  Self->SetBotInput(false,true);
  if (ACrosshairWeapon* Weapon=Self->Inventory->GetCurrent())
  {
   Weapon->StopFire();
   if (Weapon->Definition && Weapon->Ammo<Weapon->Definition->MagazineSize) Weapon->Reload();
  }
  Roam();
 }
}
void ACrosshairBotController::Engage(float DeltaSeconds)
{
 auto* Self=Cast<ACrosshairCharacter>(GetPawn());
 if (bHasDestination || bDirectMove) { StopMovement(); bHasDestination=false; bDirectMove=false; TripStage=0; }
 ClearFocus(EAIFocusPriority::Move);
 Self->SetBotInput(true,false);
 const double Now=GetWorld()->GetTimeSeconds();
 const FVector Eye=Self->GetPawnViewLocation();
 const FVector Aim=AimPointOn(Target)+AimError;
 FRotator Rotation=FMath::RInterpConstantTo(GetControlRotation(),(Aim-Eye).Rotation(),DeltaSeconds,Tuning.TrackingDegreesPerSecond);
 Rotation.Pitch=FMath::ClampAngle(Rotation.Pitch,-85.f,85.f); Rotation.Roll=0;
 SetControlRotation(Rotation);
 ACrosshairWeapon* Weapon=Self->Inventory->GetCurrent();
 if (!Weapon || !Weapon->Definition) return;
 if (Weapon->Ammo<=0) { Weapon->StopFire(); Weapon->Reload(); return; }
 const float OffDegrees=FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(FVector::DotProduct(Rotation.Vector(),(Aim-Eye).GetSafeNormal()),-1.f,1.f)));
 const bool Ready=Now-TargetSeenAt>=Tuning.ReactionSeconds && OffDegrees<=FMath::Max(1.5f,Tuning.AimErrorDegrees);
 if (!Ready) { Weapon->StopFire(); return; }
 const int32 Before=Weapon->Ammo;
 Weapon->StartFire();
 if (Weapon->Ammo<Before) ++ShotsFired;
 // Semi-automatic weapons need a fresh trigger pull for each shot.
 if (!Weapon->Definition->bAutomatic) Weapon->StopFire();
}
void ACrosshairBotController::StartStairTrip(const TArray<FVector>& Route)
{
 auto* Self=Cast<ACrosshairCharacter>(GetPawn());
 if (!Self || Route.IsEmpty()) return;
 StopMovement(); TripRoute=Route; TripIndex=0; TripStallsLeft=3; TripStage=2; Destination=Route[0]; bHasDestination=true; bDirectMove=true; bChasingTag=false; LastKnown=FVector::ZeroVector;
 LastProgressAt=GetWorld()->GetTimeSeconds(); LastProgressLocation=Self->GetActorLocation();
}
void ACrosshairBotController::TravelTo(const FVector& Goal)
{
 auto* Self=Cast<ACrosshairCharacter>(GetPawn());
 if (!Self) return;
 LastKnown=FVector::ZeroVector; Destination=Goal; bHasDestination=true; bChasingTag=false;
 bDirectMove=MoveToLocation(Goal,120.f,true,true,true,false)==EPathFollowingRequestResult::Failed;
 LastProgressAt=GetWorld()->GetTimeSeconds(); LastProgressLocation=Self->GetActorLocation();
}
bool ACrosshairBotController::SeekTag()
{
 // Kill Confirmed: head for the nearest tag in reach, even mid-route; tags score (or deny) the match.
 const auto* State=ACrosshairMatchState::Get(this);
 auto* Self=Cast<ACrosshairCharacter>(GetPawn());
 if (bChasingTag && (!GoalTag.IsValid() || !bHasDestination)) bChasingTag=false;
 if (bChasingTag || !State || !State->IsKillConfirmed() || !Self) return bChasingTag;
 ACrosshairDogTag* Nearest=nullptr; float Best=2500.f;
 for (TActorIterator<ACrosshairDogTag> It(GetWorld());It;++It)
  if (const float Distance=FVector::Dist(It->GetActorLocation(),Self->GetActorLocation()); Distance<Best && *It!=SkippedTag.Get()) { Best=Distance; Nearest=*It; }
 if (!Nearest) return false;
 GoalTag=Nearest; bChasingTag=bHasDestination=true; Destination=Nearest->GetActorLocation();
 bDirectMove=MoveToLocation(Destination,30.f,false,true,true,false)==EPathFollowingRequestResult::Failed;
 LastProgressAt=GetWorld()->GetTimeSeconds(); LastProgressLocation=Self->GetActorLocation();
 return true;
}
void ACrosshairBotController::Roam()
{
 auto* Self=Cast<ACrosshairCharacter>(GetPawn());
 const double Now=GetWorld()->GetTimeSeconds();
 SeekTag();
 const double FeetZ=Self->GetActorLocation().Z-Self->GetDefaultHalfHeight();
 if (TripStage==1 && FVector::Dist2D(Self->GetActorLocation(),Destination)<160.f && FMath::Abs(FeetZ-Destination.Z)<70.f)
 {
  // At the foot of the flight: the steps have no navmesh, so follow the walked route up them.
  StopMovement(); TripStage=2; TripIndex=0; TripStallsLeft=3; Destination=TripRoute[0]; bDirectMove=true;
  LastProgressAt=Now; LastProgressLocation=Self->GetActorLocation();
 }
 if (!bHasDestination)
 {
  FVector Goal=Self->GetActorLocation();
  if (!LastKnown.IsZero()) { Goal=LastKnown; LastKnown=FVector::ZeroVector; }
  else
  {
   FNavLocation Point;
   auto* Navigation=FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());
   const auto* Mode=GetWorld()->GetAuthGameMode<ACrosshairMatchGameMode>();
   // Random points follow floor area, so upper levels would almost never come up; aim for them on purpose.
   const ACrosshairStairLinks* Stairs=Mode ? Mode->GetStairLinks() : nullptr;
   if (Stairs && FMath::FRand()<UpstairsChance && Stairs->PickTrip(Self->GetActorLocation(),TripRoute)) { Goal=TripRoute[0]; TripStage=1; }
   else if (Stairs && FMath::FRand()<UpstairsChance && Stairs->PickUpperGoal(Self->GetActorLocation(),Goal)) {}
   else if (Navigation && Navigation->GetRandomReachablePointInRadius(Self->GetActorLocation(),3500.f,Point)) Goal=Point.Location;
   else if (Mode && !Mode->GetSpawnPoints().IsEmpty())
    if (const ACrosshairSpawnPoint* Spawn=Mode->GetSpawnPoints()[FMath::RandRange(0,Mode->GetSpawnPoints().Num()-1)]) Goal=Spawn->GetActorLocation();
  }
  Destination=Goal; bHasDestination=true;
  // Without navigation data, walk straight and hop over small obstacles.
  bDirectMove=MoveToLocation(Goal,120.f,true,true,true,false)==EPathFollowingRequestResult::Failed;
  LastProgressAt=Now; LastProgressLocation=Self->GetActorLocation();
 }
 if (bDirectMove)
 {
  const FVector Direction=(Destination-Self->GetActorLocation()).GetSafeNormal2D();
  Self->AddMovementInput(Direction,1.f);
  SetControlRotation(FRotator(0,Direction.Rotation().Yaw,0));
 }
 else if (GetMoveStatus()==EPathFollowingStatus::Idle) { bHasDestination=false; TripStage=0; }
 if (TripStage==2)
 {
  // Waypoint by waypoint; the last one (the top or bottom) goes back to normal roaming.
  if (FVector::Dist2D(Self->GetActorLocation(),Destination)<70.f && FMath::Abs(FeetZ-Destination.Z)<90.f)
  {
   if (TripRoute.IsValidIndex(TripIndex+1)) Destination=TripRoute[++TripIndex];
   else { TripStage=0; bDirectMove=false; bHasDestination=false; }
  }
 }
 else if (TripStage==0 && FVector::Dist2D(Self->GetActorLocation(),Destination)<(bChasingTag ? 40.f : 150.f)) bHasDestination=false;
 if (FVector::DistSquared2D(Self->GetActorLocation(),LastProgressLocation)>FMath::Square(100.f)) { LastProgressAt=Now; LastProgressLocation=Self->GetActorLocation(); }
 else if (TripStage==2 && Now-LastProgressAt>1.2 && TripStallsLeft>0 && TripRoute.IsValidIndex(TripIndex+1))
 {
  // Caught on a rail or corner: hop and aim for the next waypoint before giving the trip up.
  --TripStallsLeft; Destination=TripRoute[++TripIndex]; Self->Jump(); LastProgressAt=Now;
 }
 else if (Now-LastProgressAt>3.0)
 {
  if (TripStage==2) UE_LOG(LogTemp,Log,TEXT("Crosshair bot gave up a stair trip at waypoint %d of %d, %s"),TripIndex,TripRoute.Num(),*Self->GetActorLocation().ToCompactString());
  // A tag the bot cannot reach (a roof, a ledge) is left for others.
  if (bChasingTag) { SkippedTag=GoalTag; bChasingTag=false; }
  StopMovement(); bHasDestination=false; TripStage=0; if (bDirectMove) Self->Jump(); bDirectMove=false; LastProgressAt=Now;
 }
}
