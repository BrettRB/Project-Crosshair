#pragma once
#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Tickable.h"
#include "CrosshairStartup.generated.h"
struct FStreamableHandle;
class UPrimitiveComponent;
/** Prepares shipped content once per game instance, retaining assets across travel. */
UCLASS()
class PROJECT_CROSSHAIR_API UCrosshairStartup : public UGameInstanceSubsystem, public FTickableGameObject
{
 GENERATED_BODY()
public:
 void BeginPreparation();
 virtual void Deinitialize() override;
 virtual void Tick(float DeltaTime) override;
 virtual bool IsTickable() const override { return !IsTemplate() && bStarted && !bReady && Error.IsEmpty(); }
 virtual UWorld* GetTickableGameObjectWorld() const override { return GetWorld(); }
 virtual TStatId GetStatId() const override { RETURN_QUICK_DECLARE_CYCLE_STAT(UCrosshairStartup, STATGROUP_Tickables); }
 bool IsReady() const { return bReady; }
 bool IsPreparing() const { return bStarted && !bReady; }
 FString GetStatus() const;
 int32 GetCachedAssetCount() const { return CachedAssets.Num(); }
 int32 GetPreparationCount() const { return PreparationCount; }
 bool HasCachedAsset(const FString& Path) const;
private:
 void CollectLoadedContent();
 void Fail(const FString& Message);
 UPROPERTY(Transient) TArray<TObjectPtr<UObject>> CachedAssets;
 // Uninitialized map worlds are held only until pipeline warmup finishes.
 UPROPERTY(Transient) TArray<TObjectPtr<UObject>> PreviewWorlds;
 UPROPERTY(Transient) TArray<TObjectPtr<UPrimitiveComponent>> PreviewComponents;
 TArray<FSoftObjectPath> Paths;
 TSharedPtr<FStreamableHandle> Handle;
 FString Error;
 bool bStarted=false, bCollected=false, bRequestedPSOs=false, bReady=false;
 double Started=0, QuietSince=0;
 int32 PendingWork=0, PreparationCount=0;
};

