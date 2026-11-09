#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "CrosshairStance.generated.h"

class ACrosshairCharacter;

UENUM(BlueprintType)
enum class ECrosshairStance : uint8 { Normal, Prone, Slide, Dive };

/** Capsule half-height (cm) and eye height above the capsule centre for each stance. */
namespace CrosshairStanceRules
{
 constexpr float CrouchHalfHeight=58.f,ProneHalfHeight=34.f,SlideHalfHeight=44.f,DiveHalfHeight=42.f;
 inline float HalfHeight(ECrosshairStance Stance)
 {
  return Stance==ECrosshairStance::Prone ? ProneHalfHeight : Stance==ECrosshairStance::Slide ? SlideHalfHeight : Stance==ECrosshairStance::Dive ? DiveHalfHeight : CrouchHalfHeight;
 }
 /** Eye offset from the capsule centre; -1 for Normal, where the character picks standing or crouched. */
 inline float EyeOffset(ECrosshairStance Stance)
 {
  return Stance==ECrosshairStance::Prone ? 8.f : Stance==ECrosshairStance::Slide ? 22.f : Stance==ECrosshairStance::Dive ? 20.f : -1.f;
 }
 /** Slide top speed easing from the entry speed down to crouch speed. */
 inline float SlideSpeed(float Start,float End,float T) { T=FMath::Clamp(T,0.f,1.f); return FMath::Lerp(Start,End,T*T); }
}

/**
 * Prone, slide and dive. Left Ctrl lies down, or dives while sprinting; the crouch button slides while sprinting.
 * Each stance sets the movement capsule here; the third-person pose in UCrosshairLocomotionAnimInstance moves the
 * physics bodies that shots actually hit. The owning machine decides transitions and tells the host, as with sprint.
 */
UCLASS(ClassGroup=(Crosshair),meta=(BlueprintSpawnableComponent))
class PROJECT_CROSSHAIR_API UCrosshairStanceComponent : public UActorComponent
{
 GENERATED_BODY()
public:
 UCrosshairStanceComponent();
 virtual void TickComponent(float Delta,ELevelTick Type,FActorComponentTickFunction* Function) override;
 virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
 ECrosshairStance GetStance() const { return Stance; }
 bool IsActive() const { return Stance!=ECrosshairStance::Normal; }
 /** Slides and dives steer themselves, so movement input is ignored during them. */
 bool BlocksMoveInput() const { return Stance==ECrosshairStance::Slide || Stance==ECrosshairStance::Dive; }
 /** Left Ctrl: dive when sprinting, lie down otherwise, get up when already prone. */
 void PressProne();
 /** Crouch button: true when this press started a slide. */
 bool TrySlide();
 /** Crouch button while prone: come up to a crouch if there is room. */
 void ProneToCrouch();
 /** Jump button during a stance: true when the press was used up here. A slide ends so the jump can follow. */
 bool HandleJump();
 /** Current top speed while crouched (slide decay, crawl speed, or the plain crouch speed). */
 float GetCrouchedSpeed(float CrouchSpeed) const;
 /** Ends a slide (menus, match end). A prone player stays down and a dive still lands. */
 void Interrupt();
 /** Back to Normal now: death and attempt resets. */
 void Reset();
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Stance") float ProneSpeed=110;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Stance") float SlideEntrySpeed=950;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Stance") float SlideSeconds=.85f;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Stance") float DiveForwardSpeed=880;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Stance") float DiveUpSpeed=330;
private:
 UFUNCTION() void OnRep_Stance();
 UFUNCTION(Server,Reliable) void ServerSetStance(ECrosshairStance NewStance,FVector_NetQuantizeNormal NewDirection);
 ACrosshairCharacter* GetPlayer() const;
 /** Decides a change on this machine: applies it and tells the host. */
 void SetStance(ECrosshairStance NewStance,const FVector& NewDirection);
 void ApplyStance();
 bool HasRoom(float HalfHeight) const;
 UPROPERTY(ReplicatedUsing=OnRep_Stance) ECrosshairStance Stance=ECrosshairStance::Normal;
 FVector Direction=FVector::ForwardVector;
 float Elapsed=0;
};
