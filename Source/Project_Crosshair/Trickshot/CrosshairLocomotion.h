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
 /** Soldier physique: thickness multipliers (bone-local Y/Z; X runs along the bone). Clavicles lengthen for broader shoulders. */
 struct FBulk { const TCHAR* Bone; FVector Scale; };
 inline const TArray<FBulk>& Physique()
 {
  static const TArray<FBulk> Bulk={
   {TEXT("spine_02"),FVector(1,1.08,1.08)},{TEXT("spine_03"),FVector(1,1.14,1.14)},{TEXT("neck_01"),FVector(1,1.2,1.2)},
   {TEXT("clavicle_l"),FVector(1.1,1.04,1.04)},{TEXT("clavicle_r"),FVector(1.1,1.04,1.04)},
   {TEXT("upperarm_l"),FVector(1,1.18,1.18)},{TEXT("upperarm_r"),FVector(1,1.18,1.18)},
   {TEXT("lowerarm_l"),FVector(1,1.15,1.15)},{TEXT("lowerarm_r"),FVector(1,1.15,1.15)},
   {TEXT("thigh_l"),FVector(1,1.15,1.15)},{TEXT("thigh_r"),FVector(1,1.15,1.15)},
   {TEXT("calf_l"),FVector(1,1.1,1.1)},{TEXT("calf_r"),FVector(1,1.1,1.1)}};
  return Bulk;
 }
 /** Applies the physique in component space: each bulked bone thickens (its twist bones follow) while
  *  the next joint (hand, head, foot, next limb segment) keeps its own size and moves with the wider frame. */
 PROJECT_CROSSHAIR_API void ApplyPhysique(FCompactPose& Pose);
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

USTRUCT()
struct FCrosshairViewmodelProxy : public FAnimInstanceProxy
{
 GENERATED_BODY()
 FCrosshairViewmodelProxy() {}
 FCrosshairViewmodelProxy(UAnimInstance* Instance) : FAnimInstanceProxy(Instance) {}
 virtual void PreUpdate(UAnimInstance* InAnimInstance,float DeltaSeconds) override;
 virtual bool Evaluate(FPoseContext& Output) override;
private:
 const UAnimSequence* Sequence=nullptr;
 double Time=0;
 bool bLeftHandIK=false;
 FVector Target=FVector::ZeroVector,Hint=FVector::ZeroVector;
};

/**
 * First-person arms: plays the viewmodel idle/reload, applies the soldier physique, and puts the
 * left hand on the weapon's fore-end with two-bone IK (off during reloads, switches and throws).
 */
UCLASS(Transient)
class PROJECT_CROSSHAIR_API UCrosshairViewmodelAnimInstance : public UAnimInstance
{
 GENERATED_BODY()
public:
 void Play(UAnimSequence* InSequence,bool bInLoop);
 virtual void NativeUpdateAnimation(float DeltaSeconds) override;
 virtual FAnimInstanceProxy* CreateAnimInstanceProxy() override { return new FCrosshairViewmodelProxy(this); }
 virtual void DestroyAnimInstanceProxy(FAnimInstanceProxy* InProxy) override { delete InProxy; }
 UPROPERTY() TObjectPtr<UAnimSequence> Sequence;
 bool bLoop=true;
 double Time=0;
 /** Component-space left-hand goal and elbow pole, set by the character each frame. */
 bool bLeftHandIK=false;
 FVector LeftHandTarget=FVector::ZeroVector,LeftElbowHint=FVector::ZeroVector;
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
 /** Practice targets carry no weapon: use the unarmed idle instead of the rifle stance. */
 UPROPERTY(Transient,BlueprintReadWrite) bool bUnarmed=false;
 bool bLoadedUnarmed=false;
 /** Fraction of the pose coming from walk/jog cycles (0 = standing idle). */
 float GetMovingWeight() const { return 1.f-Weights.Idle; }
 CrosshairLocomotion::FWeights Weights;
 double Phase=0,IdleTime=0,FallTime=0;
 UPROPERTY() TObjectPtr<UAnimSequence> Idle;
 UPROPERTY() TObjectPtr<UAnimSequence> Fall;
 UPROPERTY() TArray<TObjectPtr<UAnimSequence>> Walk;
 UPROPERTY() TArray<TObjectPtr<UAnimSequence>> Jog;
};
