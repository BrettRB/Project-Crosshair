#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Components/ActorComponent.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "CrosshairData.h"
#include "CrosshairThrowable.generated.h"
class USphereComponent;
class UStaticMeshComponent;
class UProjectileMovementComponent;
class ACrosshairCharacter;

/** Broken glass must not apply the engine's subsequent wall-slide deflection. */
UCLASS()
class PROJECT_CROSSHAIR_API UCrosshairLethalMovement : public UProjectileMovementComponent
{
    GENERATED_BODY()
protected:
    virtual bool HandleDeflection(FHitResult& Hit,const FVector& OldVelocity,const uint32 NumBounces,float& SubTickTimeRemaining) override;
};

USTRUCT(BlueprintType)
struct FCrosshairLethalSpec
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere,BlueprintReadWrite) ECrosshairLethalType Type=ECrosshairLethalType::Frag;
    UPROPERTY(EditAnywhere,BlueprintReadWrite) TObjectPtr<UStaticMesh> VisualMesh;
    UPROPERTY(EditAnywhere,BlueprintReadWrite,meta=(ClampMin="100")) float Speed=1400;
    UPROPERTY(EditAnywhere,BlueprintReadWrite,meta=(ClampMin="0")) float GravityScale=1;
    UPROPERTY(EditAnywhere,BlueprintReadWrite,meta=(ClampMin="0",ClampMax="1")) float Bounciness=.45f;
    UPROPERTY(EditAnywhere,BlueprintReadWrite,meta=(ClampMin=".1")) float Fuse=3;
    UPROPERTY(EditAnywhere,BlueprintReadWrite,meta=(ClampMin="0")) float Damage=150;
    UPROPERTY(EditAnywhere,BlueprintReadWrite,meta=(ClampMin="1")) float BlastRadius=350;
    UPROPERTY(EditAnywhere,BlueprintReadWrite,meta=(ClampMin="0")) float InnerRadius=120;
};

/** Replicated flight/impact state; replay actors present it without simulating damage. */
UCLASS(Blueprintable)
class PROJECT_CROSSHAIR_API ACrosshairThrowable : public AActor
{
    GENERATED_BODY()
public:
    ACrosshairThrowable();
    virtual void Tick(float DeltaSeconds) override;
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
    void Release(FVector Velocity,float CookedSeconds);
    UFUNCTION(BlueprintCallable) void Detonate();
    UPROPERTY(VisibleAnywhere,BlueprintReadOnly) TObjectPtr<USphereComponent> Collision;
    UPROPERTY(VisibleAnywhere,BlueprintReadOnly) TObjectPtr<UProjectileMovementComponent> Movement;
    UPROPERTY(ReplicatedUsing=OnRep_State,BlueprintReadOnly) FCrosshairLethalSpec Spec;
    UPROPERTY(ReplicatedUsing=OnRep_State,BlueprintReadOnly) bool bHeld=true;
    UPROPERTY(ReplicatedUsing=OnRep_State,BlueprintReadOnly) bool bResting=false;
    UPROPERTY(ReplicatedUsing=OnRep_State,BlueprintReadOnly) bool bDetonated=false;
    UPROPERTY(Replicated,BlueprintReadOnly) float FlightAge=0;
    UPROPERTY() TArray<TObjectPtr<UStaticMeshComponent>> Parts;
protected:
    virtual void BeginPlay() override;
    UFUNCTION() void OnRep_State();
    UFUNCTION() void OnBounce(const FHitResult& Hit,const FVector& Velocity);
    void Impact(const FHitResult& Hit);
    bool IsPlayback() const;
    FVector PreviousPosition;
    float FuseLeft=3,EffectAge=0;
};

/** Separate lethal inventory: hold/release, cooking, cooldown and reset cleanup. */
UCLASS(ClassGroup=(Crosshair),meta=(BlueprintSpawnableComponent))
class PROJECT_CROSSHAIR_API UCrosshairLethalComponent : public UActorComponent
{
    GENERATED_BODY()
public:
    UCrosshairLethalComponent();
    virtual void BeginPlay() override;
    virtual void TickComponent(float Delta,ELevelTick Type,FActorComponentTickFunction* Function) override;
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
    UFUNCTION(BlueprintCallable) void Prepare();
    UFUNCTION(BlueprintCallable) void Throw();
    UFUNCTION(BlueprintCallable) void Cycle();
    UFUNCTION(BlueprintCallable) void Cancel();
    UFUNCTION(BlueprintCallable) void Reset();
    bool IsHolding() const { return IsValid(HeldActor) && HeldActor->bHeld; }
    bool IsBusy() const;
    float GetCookedSeconds() const;
    FString GetName() const;
    UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Lethals") FCrosshairLethalSpec Frag;
    UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Lethals") FCrosshairLethalSpec Tomahawk;
    UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Lethals",meta=(ClampMin="1")) int32 Capacity=2;
    UPROPERTY(Replicated,BlueprintReadOnly) int32 Remaining=2;
    UPROPERTY(Replicated,BlueprintReadOnly) ECrosshairLethalType Selected=ECrosshairLethalType::Frag;
    UPROPERTY(Replicated,BlueprintReadOnly) TObjectPtr<ACrosshairThrowable> HeldActor;
private:
    double PreparedAt=0,ReadyAt=0;
};
