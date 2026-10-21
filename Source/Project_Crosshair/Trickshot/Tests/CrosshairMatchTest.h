#pragma once
#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Tickable.h"
#include "CrosshairMatchTest.generated.h"
class AController;
/**
 * Opt-in match smoke test (-CrosshairSmoke -CrosshairMatchSmoke) and in-game layout probe
 * (-CrosshairSmoke -CrosshairMatchLayoutProbe) used to place spawns on real collision.
 */
UCLASS()
class UCrosshairMatchTest : public UGameInstanceSubsystem,public FTickableGameObject
{
 GENERATED_BODY()
public:
 virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
 virtual void Initialize(FSubsystemCollectionBase& Collection) override;
 virtual void Tick(float Delta) override;
 virtual bool IsTickable() const override { return !IsTemplate() && !bDone; }
 virtual UWorld* GetTickableGameObjectWorld() const override { return GetWorld(); }
 virtual TStatId GetStatId() const override { RETURN_QUICK_DECLARE_CYCLE_STAT(UCrosshairMatchTest,STATGROUP_Tickables); }
private:
 bool Check(bool Value,const TCHAR* Message);
 void Next(int32 NewStep);
 void Probe(const TCHAR* Map,FVector2D Center,float HalfSize,float Cell);
 void TickProbe();
 void TickLan();
 int32 Step=0;
 double Started=0,StepStarted=0;
 bool bDone=false,bSawInvalid=false;
 TWeakObjectPtr<AController> Victim;
 bool bSawBotKillsBot=false,bSawBotHitsPlayer=false;
 int32 ReplaysBefore=0;
 double WorldMark=0;
 bool bCapturedHome=false;
 // Difficulty duel (Easy vs Veteran) on Grid's open snow plane.
 TWeakObjectPtr<class ACrosshairBotController> DuelShooter;
 TWeakObjectPtr<class ACrosshairCharacter> DuelTarget;
 TWeakObjectPtr<AController> DuelTargetController;
 double RunStart=0,FirstHit=-1,EasyFirst=99;
 float EasyDamage=0;
 int32 RunShotsBefore=0,EasyShots=0;
 TMap<TWeakObjectPtr<AController>,FVector> SpawnedAt;
};
