#pragma once
#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Tickable.h"
#include "CrosshairLethalTest.generated.h"
class ACrosshairDummy;
class ACrosshairThrowable;
class ACrosshairWindow;
UCLASS()
class UCrosshairLethalTest : public UGameInstanceSubsystem,public FTickableGameObject
{
    GENERATED_BODY()
public:
    virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Tick(float Delta) override;
    virtual bool IsTickable() const override { return !IsTemplate() && !bFinished; }
    virtual UWorld* GetTickableGameObjectWorld() const override { return GetWorld(); }
    virtual TStatId GetStatId() const override { RETURN_QUICK_DECLARE_CYCLE_STAT(UCrosshairLethalTest,STATGROUP_Tickables); }
private:
    bool Check(bool Condition,const TCHAR* Text);
    void Next(int32 Value);
    int32 Step=0,Ammo=0;
    double Started=0,StepStarted=0;
    bool bFinished=false,bSawFlight=false,bSawImpact=false;
    FVector Origin=FVector(12000,12000,1000);
    UPROPERTY() TObjectPtr<ACrosshairDummy> Target;
    UPROPERTY() TObjectPtr<ACrosshairDummy> Covered;
    UPROPERTY() TObjectPtr<ACrosshairDummy> Edge;
    UPROPERTY() TObjectPtr<ACrosshairThrowable> Projectile;
    UPROPERTY() TObjectPtr<AActor> Blocker;
    UPROPERTY() TObjectPtr<ACrosshairWindow> Pane;
};
