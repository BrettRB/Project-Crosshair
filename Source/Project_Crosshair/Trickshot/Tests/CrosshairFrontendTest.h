#pragma once
#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Tickable.h"
#include "CrosshairFrontendTest.generated.h"
UCLASS()
class UCrosshairFrontendTest : public UGameInstanceSubsystem,public FTickableGameObject
{
 GENERATED_BODY()
public:
 virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
 virtual void Initialize(FSubsystemCollectionBase& Collection) override;
 virtual void Tick(float Delta) override;
 virtual bool IsTickable() const override { return !IsTemplate() && !bDone; }
 virtual UWorld* GetTickableGameObjectWorld() const override { return GetWorld(); }
 virtual TStatId GetStatId() const override { RETURN_QUICK_DECLARE_CYCLE_STAT(UCrosshairFrontendTest,STATGROUP_Tickables); }
private:
 bool Check(bool Value,const TCHAR* Message);
 void Next(int32 NewStep);
 int32 Step=0;
 double Started=0,StepStarted=0;
 bool bDone=false,bSawImportedReplay=false;
 FString ImportedId;
};
