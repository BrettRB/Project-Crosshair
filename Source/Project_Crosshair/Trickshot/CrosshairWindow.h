#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "CrosshairWindow.generated.h"
class UStaticMeshComponent;

/** A glass pane only. Frames/walls remain separate, solid map geometry. */
UCLASS(Blueprintable)
class PROJECT_CROSSHAIR_API ACrosshairWindow : public AActor
{
    GENERATED_BODY()
public:
    ACrosshairWindow();
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaSeconds) override;
    virtual float TakeDamage(float Amount,const FDamageEvent& Event,AController* EventInstigator,AActor* Causer) override;
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
    UFUNCTION(BlueprintCallable) void ResetGlass();
    UFUNCTION(BlueprintCallable) void RestoreBroken(bool Broken);
    bool GetOpening(FVector& Center,FVector& Normal,float& HalfWidth,float& Bottom,float& Top) const;
    UPROPERTY(VisibleAnywhere,BlueprintReadOnly) TObjectPtr<UStaticMeshComponent> Glass;
    UPROPERTY(ReplicatedUsing=OnRep_Broken,BlueprintReadOnly) bool bBroken = false;
    // Optional oriented bounds for panes whose rotation is baked into the mesh.
    UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Opening") bool bConfiguredOpening = false;
    UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Opening") FVector OpeningCenter = FVector::ZeroVector;
    UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Opening") FVector OpeningNormal = FVector::ForwardVector;
    UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Opening") float OpeningHalfWidth = 0;
    UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Opening") float OpeningHalfHeight = 0;
private:
    UFUNCTION() void OnRep_Broken();
    UPROPERTY() TArray<TObjectPtr<UStaticMeshComponent>> Shards;
    TArray<FVector> ShardVelocity;
    float ShardAge = 1;
};
