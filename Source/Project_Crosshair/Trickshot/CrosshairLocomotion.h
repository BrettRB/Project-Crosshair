#pragma once
#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimInstanceProxy.h"
#include "CrosshairStance.h"
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
 /**
  * One frame of a stance animation. Tilt turns the whole body about its pelvis (negative = face down, head forward;
  * positive = leaning back, feet forward). Chest turns both shoulders together, so the hands stay on the weapon.
  * Limb goals are directions in mesh space: +Y forward, +Z up, +X the soldier's left. Heights are cm above the feet.
  */
 struct FLay
 {
  float Tilt=0,PelvisZ=0,Chest=0,Neck=0,Spine=0;
  FVector ThighL=FVector::ZeroVector,CalfL=FVector::ZeroVector,ThighR=FVector::ZeroVector,CalfR=FVector::ZeroVector;
  /** 1 frees the left arm from the weapon and points it along UpperArmL and LowerArmL. */
  float LeftArm=0;
  FVector UpperArmL=FVector::ZeroVector,LowerArmL=FVector::ZeroVector;
 };
 inline FLay BlendLay(const FLay& A,const FLay& B,float T)
 {
  FLay Out;
  Out.Tilt=FMath::Lerp(A.Tilt,B.Tilt,T); Out.PelvisZ=FMath::Lerp(A.PelvisZ,B.PelvisZ,T); Out.Chest=FMath::Lerp(A.Chest,B.Chest,T);
  Out.Neck=FMath::Lerp(A.Neck,B.Neck,T); Out.Spine=FMath::Lerp(A.Spine,B.Spine,T); Out.LeftArm=FMath::Lerp(A.LeftArm,B.LeftArm,T);
  Out.ThighL=FMath::Lerp(A.ThighL,B.ThighL,T); Out.CalfL=FMath::Lerp(A.CalfL,B.CalfL,T); Out.ThighR=FMath::Lerp(A.ThighR,B.ThighR,T); Out.CalfR=FMath::Lerp(A.CalfR,B.CalfR,T);
  Out.UpperArmL=FMath::Lerp(A.UpperArmL,B.UpperArmL,T); Out.LowerArmL=FMath::Lerp(A.LowerArmL,B.LowerArmL,T);
  return Out;
 }
 /** The frame for a stance. CrawlPhase is 0..1 through a crawl cycle, CrawlAmount 0 (still) to 1, and FallSpeed the upward velocity during a dive. */
 inline FLay LayFor(ECrosshairStance Stance,float CrawlPhase=0.f,float CrawlAmount=0.f,float FallSpeed=0.f)
 {
  FLay Lay;
  if (Stance==ECrosshairStance::Prone)
  {
   // Combat crawl: chest just off the ground on the elbows, drawing one knee up beside the body and then the other.
   Lay.Tilt=-78.f; Lay.PelvisZ=18.f; Lay.Chest=64.f; Lay.Neck=55.f;
   const float Swing=FMath::Sin(CrawlPhase*UE_TWO_PI)*CrawlAmount;
   auto Leg=[](float Side,float Draw,FVector& Thigh,FVector& Calf)
   {
    Thigh=FMath::Lerp(FVector(Side*.14,-.99,-.04),FVector(Side*.78,.28,-.08),Draw);
    Calf=FMath::Lerp(FVector(Side*.06,-1,-.02),FVector(Side*-.08,-.98,-.1),Draw);
   };
   // Resting, the left knee stays a little drawn up: the classic low crawl position.
   Leg(1.f,FMath::Max(.3f*(1.f-CrawlAmount),FMath::Max(0.f,Swing)),Lay.ThighL,Lay.CalfL);
   Leg(-1.f,FMath::Max(0.f,-Swing),Lay.ThighR,Lay.CalfR);
   Lay.Chest+=Swing*5.f;
  }
  else if (Stance==ECrosshairStance::Slide)
  {
   // Baseball slide: leaning back, right leg out straight in front, left leg tucked under it, left hand down behind.
   Lay.Tilt=52.f; Lay.PelvisZ=26.f; Lay.Chest=-34.f; Lay.Neck=-30.f; Lay.Spine=-18.f;
   Lay.ThighR=FVector(-.06,.93,-.36); Lay.CalfR=FVector(-.02,.99,-.12);
   Lay.ThighL=FVector(.66,.68,-.3); Lay.CalfL=FVector(-.68,-.66,-.2);
   Lay.LeftArm=1.f; Lay.UpperArmL=FVector(.3,-.35,-.89); Lay.LowerArmL=FVector(.1,-.25,-.96);
  }
  else if (Stance==ECrosshairStance::Dive)
  {
   // Head-first dive: stretched out behind the weapon, then the nose drops and the arms brace for the landing.
   const float Brace=FMath::Clamp(-FallSpeed/500.f,0.f,1.f);
   Lay.Tilt=FMath::Lerp(-72.f,-84.f,Brace); Lay.PelvisZ=46.f; Lay.Chest=FMath::Lerp(64.f,50.f,Brace); Lay.Neck=55.f;
   Lay.ThighL=FVector(.1,-.98,.12); Lay.CalfL=FVector(.06,-.88,.46);
   Lay.ThighR=FVector(-.1,-.98,.06); Lay.CalfR=FVector(-.05,-.93,.3);
  }
  return Lay;
 }
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
 CrosshairLocomotion::FLay Lay;
 float LayAlpha=0,LayShift=0;
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
 /** Killcam stand-ins have no pawn: they set these recorded values each frame instead. */
 bool bDriven=false,bDrivenFalling=false;
 FVector DrivenVelocity=FVector::ZeroVector;
 FRotator DrivenFacing=FRotator::ZeroRotator;
 float DrivenPitch=0;
 ECrosshairStance DrivenStance=ECrosshairStance::Normal;
 /** The animation frame being shown. LayShift slides a prone body along itself, away from walls. */
 CrosshairLocomotion::FLay Lay,LayFrom;
 float LayAlpha=0,LayShift=0,LayBlend=1,CrawlPhase=0,CrawlAmount=0;
 ECrosshairStance LastStance=ECrosshairStance::Normal;
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
