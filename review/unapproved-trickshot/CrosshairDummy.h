#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "CrosshairDummy.generated.h"

class UCapsuleComponent;
class UStaticMeshComponent;

UCLASS(Blueprintable)
class PROJECT_CROSSHAIR_API ACrosshairDummy : public AActor
{
	GENERATED_BODY()
public:
	ACrosshairDummy();
	virtual float TakeDamage(float Amount, const FDamageEvent& Event, AController* Instigator, AActor* Causer) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& Out) const override;
	UFUNCTION(BlueprintCallable) void ResetTarget();
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TObjectPtr<UCapsuleComponent> Collision;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TObjectPtr<UStaticMeshComponent> Body;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TObjectPtr<UStaticMeshComponent> Head;
	UPROPERTY(ReplicatedUsing=OnRep_Hit, BlueprintReadOnly) bool bHit = false;
protected:
	virtual void BeginPlay() override;
	UFUNCTION() void OnRep_Hit();
};
