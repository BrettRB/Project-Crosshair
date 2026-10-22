#pragma once
#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimInstanceProxy.h"
#include "CrosshairLocomotion.generated.h"

class UAnimSequence;

/** Pure locomotion blend rules (shared by the anim instance and automation tests). */
namespace CrosshairLocomotion
{
 constexpr int32 Directions=8;
 /** Reference speeds (cm/s) of the mannequin rifle walk and jog cycles. */
 constexpr float WalkSpeed=200.f, JogSpeed=430.f;
 struct FWeights
 {
  float Idle=1.f;
  float Walk[Directions]={};
  float Jog[Directions]={};
 };
 /** Direction index order: Fwd, Fwd-Right, Right, Bwd-Right, Bwd, Bwd-Left, Left, Fwd-Left (45 degree steps). */
 inline FWeights Compute(float Speed,float DirectionDegrees)
 {
  FWeights Out;
  Speed=FMath::Max(0.f,Speed);
  if (Speed<5.f) return Out;
  const float Wrapped=FMath::Fmod(FMath::Fmod(DirectionDegrees,360.f)+360.f,360.f);
  const int32 A=FMath::FloorToInt(Wrapped/45.f)%Directions, B=(A+1)%Directions;
  const float Side=(Wrapped-A*45.f)/45.f;
  float Idle=0.f,Walk=0.f,Jog=0.f;
  if (Speed<=WalkSpeed) { Walk=Speed/WalkSpeed; Idle=1.f-Walk; }
  else { Jog=FMath::Clamp((Speed-WalkSpeed)/(JogSpeed-WalkSpeed),0.f,1.f); Walk=1.f-Jog; }
  Out.Idle=Idle;
  Out.Walk[A]+=Walk*(1.f-Side); Out.Walk[B]+=Walk*Side;
  Out.Jog[A]+=Jog*(1.f-Side); Out.Jog[B]+=Jog*Side;
  return Out;
 }
 /** Cycle play rate: faster than jog speed speeds the jog cycle up (sprint) instead of sliding. */
 inline float PlayRate(float Speed) { return Speed>JogSpeed ? FMath::Min(Speed/JogSpeed,1.7f) : 1.f; }
}

USTRUCT()
struct FCrosshairLocomotionProxy : public FAnimInstanceProxy
{
 GENERATED_BODY()
 FCrosshairLocomotionProxy() {}
 FCrosshairLocomotionProxy(UAnimInstance* Instance) : FAnimInstanceProxy(Instance) {}
 virtual void PreUpdate(UAnimInstance* InAnimInstance,float DeltaSeconds) override;
 virtual bool Evaluate(FPoseContext& Output) override;
private:
 struct FSample { const UAnimSequence* Sequence=nullptr; double Time=0; float Weight=0; };
 TArray<FSample,TInlineAllocator<8>> Samples;
 float AimPitch=0;
};

/**
 * Third-person soldier locomotion in C++ (no Animation Blueprint): idle, 8-direction rifle
 * walk/jog blended by speed and travel direction with a shared step phase, a fall loop when
 * airborne, and upper-body aim pitch. Driven only by replicated movement (velocity, falling,
 * aim pitch), so bots, LAN players and replays animate the same way.
 */
UCLASS(Transient)
class PROJECT_CROSSHAIR_API UCrosshairLocomotionAnimInstance : public UAnimInstance
{
 GENERATED_BODY()
public:
 UCrosshairLocomotionAnimInstance();
 virtual void NativeUpdateAnimation(float DeltaSeconds) override;
 virtual FAnimInstanceProxy* CreateAnimInstanceProxy() override { return new FCrosshairLocomotionProxy(this); }
 virtual void DestroyAnimInstanceProxy(FAnimInstanceProxy* InProxy) override { delete InProxy; }
 UPROPERTY(Transient,BlueprintReadOnly) float Speed=0;
 UPROPERTY(Transient,BlueprintReadOnly) float Direction=0;
 UPROPERTY(Transient,BlueprintReadOnly) float AimPitch=0;
 UPROPERTY(Transient,BlueprintReadOnly) float FallAlpha=0;
 /** Fraction of the pose coming from walk/jog cycles (0 = standing idle). */
 float GetMovingWeight() const { return 1.f-Weights.Idle; }
 CrosshairLocomotion::FWeights Weights;
 double Phase=0,IdleTime=0,FallTime=0;
 UPROPERTY() TObjectPtr<UAnimSequence> Idle;
 UPROPERTY() TObjectPtr<UAnimSequence> Fall;
 UPROPERTY() TArray<TObjectPtr<UAnimSequence>> Walk;
 UPROPERTY() TArray<TObjectPtr<UAnimSequence>> Jog;
};
