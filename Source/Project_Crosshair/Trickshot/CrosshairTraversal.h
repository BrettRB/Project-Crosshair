#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "CrosshairTraversal.generated.h"
class ACrosshairCharacter;

/** Collision-tested ledge/window traversal and short arcade jump forgiveness. */
UCLASS(ClassGroup=(Crosshair),meta=(BlueprintSpawnableComponent))
class PROJECT_CROSSHAIR_API UCrosshairTraversalComponent : public UActorComponent
{
    GENERATED_BODY()
public:
    UCrosshairTraversalComponent();
    virtual void TickComponent(float Delta,ELevelTick Type,FActorComponentTickFunction* Function) override;
    UFUNCTION(BlueprintCallable) bool TryMantle();
    bool AllowsCoyoteJump() const;
    void RequestJump();
    void ReleaseJump();
    UFUNCTION(BlueprintCallable) void CancelMantle();
    UPROPERTY(BlueprintReadOnly) bool bMantling=false;
    UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Mantling") float Reach=120;
    UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Mantling") float MaximumHeight=160;
    UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Mantling") float MinimumHeight=45;
    UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Mantling") float Duration=.55f;
    UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Jump") float CoyoteSeconds=.10f;
    UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Jump") float JumpBufferSeconds=.12f;
private:
    bool BeginMantle(const FVector& Landing,const FVector& Across,bool Crouched);
    FVector Start,Lift,Across,Landing;
    float Elapsed=0;
    float PreviousGravityScale=1;
    double LastGrounded=-100,JumpQueuedUntil=-100;
    bool bAutoCrouched=false;
};
