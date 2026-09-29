#pragma once
#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Tickable.h"
#include "CrosshairSmokeTest.generated.h"

/** Opt-in local integration test. Never created in Shipping or without an explicit command-line flag. */
UCLASS()
class UCrosshairSmokeTest : public UGameInstanceSubsystem, public FTickableGameObject
{
	GENERATED_BODY()
public:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Tick(float DeltaSeconds) override;
	virtual bool IsTickable() const override { return !IsTemplate() && !bFinished; }
	virtual UWorld* GetTickableGameObjectWorld() const override { return GetWorld(); }
	virtual TStatId GetStatId() const override { RETURN_QUICK_DECLARE_CYCLE_STAT(UCrosshairSmokeTest, STATGROUP_Tickables); }
private:
	bool Check(bool Condition, const TCHAR* Message);
	void Next(int32 NewStep);
	int32 Step = 0;
	double Started = 0, StepStarted = 0;
	bool bFinished = false, bSawPlayback = false;
	FVector InitialPosition = FVector::ZeroVector;
	int32 TargetCount = 0, AmmoBefore = 0;
};
