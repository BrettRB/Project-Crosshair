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
 /** LAN: walk, aim and switch weapons with real key presses through this player's own input mapping. True when done. */
 bool TickRealInput(const TCHAR* Who);
 void TickGraphics();
 void ProbeSoldier();
 int32 Step=0;
 double Started=0,StepStarted=0;
 bool bDone=false,bSawInvalid=false;
 TWeakObjectPtr<AController> Victim;
 bool bSawBotKillsBot=false,bSawBotHitsPlayer=false;
 int32 ReplaysBefore=0;
 int32 DamageShots=0;
 /** LAN combat check: the host's health before the joined player's shot. */
 float LanHealthBefore=0;
 double LanFiredAt=0;
 int32 AmmoBefore=0;
 float PitchBefore=0;
 bool bSawLookUpTarget=false;
 FVector LookUpBotAt=FVector::ZeroVector;
 TMap<TWeakObjectPtr<class ACrosshairCharacter>,FVector> ParkedBots;
 double WorldMark=0;
 bool bCapturedHome=false;
 float RemoteSpeedMax=0,HostSpeedMax=0,HostSteppingMax=0;
 FVector RunGoal=FVector::ZeroVector;
 int32 InputPhase=0;
 double InputPhaseAt=0;
 FVector InputFrom=FVector::ZeroVector;
 TWeakObjectPtr<class ACrosshairWeapon> InputWeapon;
 bool bSawBotStepping=false;
 // Difficulty duel (Easy vs Veteran) on Grid's open snow plane.
 TWeakObjectPtr<class ACrosshairBotController> DuelShooter;
 TWeakObjectPtr<class ACrosshairCharacter> DuelTarget;
 TWeakObjectPtr<AController> DuelTargetController;
 double RunStart=0,FirstHit=-1,EasyFirst=99;
 float EasyDamage=0;
 int32 RunShotsBefore=0,EasyShots=0;
 TMap<TWeakObjectPtr<AController>,FVector> SpawnedAt;
};
