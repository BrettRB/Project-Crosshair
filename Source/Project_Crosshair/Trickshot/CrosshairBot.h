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
 /** True when Target's head or chest is visible from the bot's eyes. */
 bool CanSee(const ACrosshairCharacter* Target) const;
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
 bool SeekTag();
};
