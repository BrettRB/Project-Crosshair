#pragma once
#include "CoreMinimal.h"
#include "AIController.h"
#include "CrosshairBotMatch.h"
#include "CrosshairBot.generated.h"

class ACrosshairCharacter;

/**
 * Match bot: line-of-sight perception, difficulty-limited reaction/aim, navmesh roaming.
 * Uses the same character, weapons and damage path as the player; no wall vision.
 */
UCLASS()
class PROJECT_CROSSHAIR_API ACrosshairBotController : public AAIController
{
 GENERATED_BODY()
public:
 ACrosshairBotController();
 virtual void OnPossess(APawn* InPawn) override;
 virtual void OnUnPossess() override;
 virtual void Tick(float DeltaSeconds) override;
 /** The engine levels an AI's view every tick unless it focuses a pawn; this bot aims itself, so keep its pitch while engaging. */
 virtual void UpdateControlRotation(float DeltaTime, bool bUpdatePawn = true) override;
 /** True when Target's head or chest is visible from the bot's eyes. */
 bool CanSee(const ACrosshairCharacter* Target) const;
 /** Sends the bot to a point as its next roam destination. */
 void TravelTo(const FVector& Goal);
 /** Follows a staircase route (waypoints) from its foot; the next roam goal follows when it arrives. */
 void StartStairTrip(const TArray<FVector>& Route);
 /** Tests: keeps roaming and ignores enemies. */
 bool bPassive=false;
 /** How often a new roam destination is a floor reached by stairs, when the map has any. */
 UPROPERTY(EditAnywhere,Category="Bot") float UpstairsChance=.3f;
 UPROPERTY(Transient) TObjectPtr<ACrosshairCharacter> Target;
 int32 ShotsFired=0;
 /** Live difficulty tuning, refreshed every decision. */
 FCrosshairBotTuning Tuning;
private:
 void Decide();
 void Engage(float DeltaSeconds);
 void Roam();
 FVector AimPointOn(const ACrosshairCharacter* Character) const;
 double NextDecisionAt=0,TargetSeenAt=0,LastProgressAt=0;
 FVector AimError=FVector::ZeroVector;
 FVector Destination=FVector::ZeroVector,LastKnown=FVector::ZeroVector,LastProgressLocation=FVector::ZeroVector;
 bool bHasDestination=false,bDirectMove=false;
 /** Kill Confirmed: the dog tag this bot is walking to (enemy tags to confirm, its team's to deny). */
 TWeakObjectPtr<AActor> GoalTag,SkippedTag;
 bool bChasingTag=false;
 /** Staircase trip: 1 walking to the foot of the flight, 2 following TripRoute up or down it. */
 int32 TripStage=0,TripIndex=0,TripStallsLeft=0;
 TArray<FVector> TripRoute;
 bool SeekTag();
};
