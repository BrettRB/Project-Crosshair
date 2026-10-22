#include "CrosshairLocomotion.h"
#include "Animation/AnimNodeBase.h"
#include "Animation/AnimSequence.h"
#include "Animation/AnimationPoseData.h"
#include "AnimationRuntime.h"
#include "BonePose.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"

namespace
{
 const TCHAR* const Suffixes[CrosshairLocomotion::Directions]={TEXT("Fwd"),TEXT("Fwd_Right"),TEXT("Right"),TEXT("Bwd_Right"),TEXT("Bwd"),TEXT("Bwd_Left"),TEXT("Left"),TEXT("Fwd_Left")};
 UAnimSequence* LoadRifle(const FString& Path) { return LoadObject<UAnimSequence>(nullptr,*(TEXT("/Game/Characters/Mannequins/Anims/Rifle/")+Path)); }
}

UCrosshairLocomotionAnimInstance::UCrosshairLocomotionAnimInstance() {}

void UCrosshairLocomotionAnimInstance::NativeUpdateAnimation(float DeltaSeconds)
{
 Super::NativeUpdateAnimation(DeltaSeconds);
 if (!Idle)
 {
  // Mannequin rifle set (template content); cooked via DirectoriesToAlwaysCook.
  Idle=LoadRifle(TEXT("MF_Rifle_Idle_ADS.MF_Rifle_Idle_ADS"));
  Fall=LoadRifle(TEXT("Jump/MM_Rifle_Jump_Fall_Loop.MM_Rifle_Jump_Fall_Loop"));
  Walk.Reset(); Jog.Reset();
  for (const TCHAR* Suffix:Suffixes)
  {
   Walk.Add(LoadRifle(FString::Printf(TEXT("Walk/MF_Rifle_Walk_%s.MF_Rifle_Walk_%s"),Suffix,Suffix)));
   Jog.Add(LoadRifle(FString::Printf(TEXT("Jog/MF_Rifle_Jog_%s.MF_Rifle_Jog_%s"),Suffix,Suffix)));
  }
 }
 const APawn* Pawn=TryGetPawnOwner();
 if (!Pawn) return;
 // Replicated movement only: velocity, falling state and aim pitch exist on every machine and in replays.
 const FVector Velocity=Pawn->GetVelocity();
 Speed=Velocity.Size2D();
 const FVector Local=Pawn->GetActorRotation().UnrotateVector(Velocity);
 Direction=Speed>1.f ? FMath::RadiansToDegrees(FMath::Atan2(Local.Y,Local.X)) : 0.f;
 AimPitch=FRotator::NormalizeAxis(Pawn->GetBaseAimRotation().Pitch);
 const auto* Character=Cast<ACharacter>(Pawn);
 const bool bFalling=Character && Character->GetCharacterMovement() && Character->GetCharacterMovement()->IsFalling();
 FallAlpha=FMath::FInterpConstantTo(FallAlpha,bFalling ? 1.f : 0.f,DeltaSeconds,6.f);
 FallTime=bFalling ? FallTime+DeltaSeconds : 0.0;
 IdleTime+=DeltaSeconds;
 Weights=CrosshairLocomotion::Compute(Speed,Direction);
 // One shared step phase keeps walk and jog feet in step while blending.
 float WalkSum=0,JogSum=0;
 for (int32 i=0;i<CrosshairLocomotion::Directions;++i) { WalkSum+=Weights.Walk[i]; JogSum+=Weights.Jog[i]; }
 const double WalkCycle=Walk.IsValidIndex(0) && Walk[0] ? Walk[0]->GetPlayLength() : 1.0;
 const double JogCycle=Jog.IsValidIndex(0) && Jog[0] ? Jog[0]->GetPlayLength() : 1.0;
 if (WalkSum+JogSum>KINDA_SMALL_NUMBER)
 {
  const double Cycle=FMath::Max(.1,(WalkSum*WalkCycle+JogSum*JogCycle)/(WalkSum+JogSum));
  Phase=FMath::Fmod(Phase+DeltaSeconds*CrosshairLocomotion::PlayRate(Speed)/Cycle,1.0);
 }
}

void FCrosshairLocomotionProxy::PreUpdate(UAnimInstance* InAnimInstance,float DeltaSeconds)
{
 Super::PreUpdate(InAnimInstance,DeltaSeconds);
 const auto* Instance=CastChecked<UCrosshairLocomotionAnimInstance>(InAnimInstance);
 Samples.Reset();
 AimPitch=Instance->AimPitch;
 const float Ground=1.f-Instance->FallAlpha;
 auto Add=[this](const UAnimSequence* Sequence,double Time,float Weight)
 {
  if (!Sequence || Weight<=.001f) return;
  const double Length=FMath::Max(.01,double(Sequence->GetPlayLength()));
  Samples.Add({Sequence,FMath::Fmod(Time,Length),Weight});
 };
 Add(Instance->Idle,Instance->IdleTime,Instance->Weights.Idle*Ground);
 for (int32 i=0;i<CrosshairLocomotion::Directions;++i)
 {
  if (Instance->Walk.IsValidIndex(i) && Instance->Walk[i]) Add(Instance->Walk[i],Instance->Phase*Instance->Walk[i]->GetPlayLength(),Instance->Weights.Walk[i]*Ground);
  if (Instance->Jog.IsValidIndex(i) && Instance->Jog[i]) Add(Instance->Jog[i],Instance->Phase*Instance->Jog[i]->GetPlayLength(),Instance->Weights.Jog[i]*Ground);
 }
 Add(Instance->Fall,Instance->FallTime,Instance->FallAlpha);
 float Total=0; for (const FSample& Sample:Samples) Total+=Sample.Weight;
 if (Total>0) for (FSample& Sample:Samples) Sample.Weight/=Total;
}

bool FCrosshairLocomotionProxy::Evaluate(FPoseContext& Output)
{
 if (Samples.IsEmpty()) { Output.ResetToRefPose(); return true; }
 FMemMark Mark(FMemStack::Get());
 const int32 Count=Samples.Num();
 TArray<FCompactPose,TInlineAllocator<8>> Poses; Poses.SetNum(Count);
 TArray<FBlendedCurve,TInlineAllocator<8>> Curves; Curves.SetNum(Count);
 TArray<UE::Anim::FStackAttributeContainer,TInlineAllocator<8>> Attributes; Attributes.SetNum(Count);
 TArray<float,TInlineAllocator<8>> Weights;
 for (int32 i=0;i<Count;++i)
 {
  Poses[i].SetBoneContainer(&Output.Pose.GetBoneContainer());
  Curves[i].InitFrom(Output.Curve);
  FAnimationPoseData Data(Poses[i],Curves[i],Attributes[i]);
  Samples[i].Sequence->GetAnimationPose(Data,FAnimExtractContext(Samples[i].Time,false,{},true));
  Weights.Add(Samples[i].Weight);
 }
 FAnimationPoseData Result(Output);
 FAnimationRuntime::BlendPosesTogether(Poses,Curves,Attributes,Weights,Result);
 // Upper-body aim: tilt spine_03 about the mesh's sideways axis (the mannequin faces +Y).
 const FBoneContainer& Bones=Output.Pose.GetBoneContainer();
 const int32 MeshIndex=Bones.GetPoseBoneIndexForBoneName(TEXT("spine_03"));
 if (MeshIndex!=INDEX_NONE && FMath::Abs(AimPitch)>.5f)
 {
  const FCompactPoseBoneIndex Spine=Bones.MakeCompactPoseIndex(FMeshPoseBoneIndex(MeshIndex));
  const FCompactPoseBoneIndex Parent=Spine.IsValid() ? Output.Pose.GetParentBoneIndex(Spine) : FCompactPoseBoneIndex(INDEX_NONE);
  if (Spine.IsValid() && Parent.IsValid())
  {
   FCSPose<FCompactPose> Component; Component.InitPose(Output.Pose);
   const FQuat ParentRotation=Component.GetComponentSpaceTransform(Parent).GetRotation();
   const FQuat Tilt(FVector(1,0,0),FMath::DegreesToRadians(FMath::Clamp(AimPitch,-50.f,50.f)));
   FTransform& Local=Output.Pose[Spine];
   Local.SetRotation((ParentRotation.Inverse()*Tilt*ParentRotation*Local.GetRotation()).GetNormalized());
  }
 }
 return true;
}
