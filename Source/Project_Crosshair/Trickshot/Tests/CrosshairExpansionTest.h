#pragma once
#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Tickable.h"
#include "CrosshairExpansionTest.generated.h"
class ACrosshairDummy;
class ACrosshairWindow;
/** Opt-in local integration coverage. Requires -CrosshairSmoke to isolate saves. */
UCLASS()
class UCrosshairExpansionTest : public UGameInstanceSubsystem, public FTickableGameObject
{
    GENERATED_BODY()
public:
    virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Tick(float DeltaSeconds) override;
    virtual bool IsTickable() const override { return !IsTemplate() && !bFinished; }
    virtual UWorld* GetTickableGameObjectWorld() const override { return GetWorld(); }
    virtual TStatId GetStatId() const override { RETURN_QUICK_DECLARE_CYCLE_STAT(UCrosshairExpansionTest, STATGROUP_Tickables); }
private:
    bool Check(bool Condition,const TCHAR* Message);
    void Next(int32 NewStep);
    int32 Step=0,AmmoBefore=0;
    double Started=0,StepStarted=0;
    bool bFinished=false,bSawBrokenReplay=false,bSawMantleReplay=false;
    FName ReplayWindow;
    FVector Origin=FVector(12000,12000,1000),WindowStart=FVector::ZeroVector,WindowNormal=FVector::ZeroVector;
    UPROPERTY() TObjectPtr<ACrosshairDummy> Target;
    UPROPERTY() TObjectPtr<ACrosshairWindow> Pane;
    UPROPERTY() TObjectPtr<AActor> Obstacle;
};
