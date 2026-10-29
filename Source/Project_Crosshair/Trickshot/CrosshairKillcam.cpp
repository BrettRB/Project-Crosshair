#include "CrosshairKillcam.h"
#include "CrosshairCharacter.h"
#include "CrosshairData.h"
#include "CrosshairGame.h"
#include "CrosshairLocomotion.h"
#include "CrosshairMatch.h"
#include "CrosshairReplaySubsystem.h"
#include "CrosshairWeapon.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/DemoNetDriver.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerState.h"
#include "Kismet/GameplayStatics.h"

FCrosshairKillcamPose FCrosshairKillcamPose::Blend(const FCrosshairKillcamPose& A,const FCrosshairKillcamPose& B,float Alpha)
{
 FCrosshairKillcamPose Out=Alpha<.5f ? A : B;   // discrete state (weapon, falling, dead) from the nearer sample
 Out.Location=FMath::Lerp(A.Location,B.Location,Alpha);
 Out.Facing=FMath::Lerp(A.Facing,B.Facing,Alpha);   // FRotator lerp takes the shortest way round
 Out.Velocity=FMath::Lerp(A.Velocity,B.Velocity,Alpha);
 Out.Eye=FMath::Lerp(A.Eye,B.Eye,Alpha);
 Out.View=FMath::Lerp(A.View,B.View,Alpha);
 Out.Aim=FMath::Lerp(A.Aim,B.Aim,Alpha);
 Out.Shots=A.Shots;   // shots count when the later sample is reached
 return Out;
}

ACrosshairKillcamGhost::ACrosshairKillcamGhost()
{
 bReplicates=false;
 SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("Root")));
 Body=CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("Body"));
 Body->SetupAttachment(GetRootComponent());
 Body->SetCollisionEnabled(ECollisionEnabled::NoCollision);
 Body->VisibilityBasedAnimTickOption=EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
 Weapon=CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Weapon"));
 Weapon->SetupAttachment(Body);
 Weapon->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}
void ACrosshairKillcamGhost::CopyLook(const ACrosshairCharacter* Source)
{
 const USkeletalMeshComponent* Live=Source->GetMesh();
 Body->SetSkeletalMesh(Live->GetSkeletalMeshAsset());
 // The standing offset, not the live one: a killed soldier's mesh is a ragdoll by now.
 Body->SetRelativeTransform(FTransform(Source->GetBaseRotationOffset(),Source->GetBaseTranslationOffset()));
 for (int32 i=0;i<Live->GetNumMaterials();++i) Body->SetMaterial(i,Live->GetMaterial(i));
 Body->SetAnimInstanceClass(UCrosshairLocomotionAnimInstance::StaticClass());
 if (auto* Anim=Cast<UCrosshairLocomotionAnimInstance>(Body->GetAnimInstance())) Anim->bDriven=true;
 // Helmet, pouches and pads: every static mesh the soldier's own body carries (weapons are separate actors).
 for (USceneComponent* Child:Live->GetAttachChildren())
 {
  const auto* Part=Cast<UStaticMeshComponent>(Child);
  if (!Part || Part->GetOwner()!=Source || !Part->GetStaticMesh()) continue;
  auto* Copy=NewObject<UStaticMeshComponent>(this);
  Copy->SetStaticMesh(Part->GetStaticMesh());
  for (int32 i=0;i<Part->GetNumMaterials();++i) Copy->SetMaterial(i,Part->GetMaterial(i));
  Copy->SetCollisionEnabled(ECollisionEnabled::NoCollision);
  Copy->SetupAttachment(Body,Part->GetAttachSocketName());
  Copy->SetRelativeTransform(Part->GetRelativeTransform());
  Copy->RegisterComponent();
 }
 const FName Socket=Body->DoesSocketExist(TEXT("HandGrip_R")) ? FName(TEXT("HandGrip_R")) : FName(TEXT("hand_r"));
 Weapon->AttachToComponent(Body,FAttachmentTransformRules::SnapToTargetNotIncludingScale,Socket);
 if (const ACrosshairWeapon* Held=Source->Inventory->GetCurrent(); Held && Held->Definition)
 {
  SkinnedWeapon=Held->Definition;
  for (int32 i=0;i<Held->PresentationMesh->GetNumMaterials();++i) SkinMaterials.Add(Held->PresentationMesh->GetMaterial(i));
 }
}
void ACrosshairKillcamGhost::ApplyPose(const FCrosshairKillcamPose& Pose)
{
 if (bRagdoll) return;
 if (Pose.bDead && Body->GetPhysicsAsset())
 {
  // The recorded death: drop like the live soldier did.
  bRagdoll=true; Weapon->SetVisibility(false);
  Body->SetCollisionProfileName(TEXT("Ragdoll"));
  Body->SetAllBodiesSimulatePhysics(true); Body->SetSimulatePhysics(true); Body->WakeAllRigidBodies();
  return;
 }
 SetActorLocationAndRotation(Pose.Location,FRotator(0,Pose.Facing.Yaw,0));
 if (auto* Anim=Cast<UCrosshairLocomotionAnimInstance>(Body->GetAnimInstance()))
 { Anim->DrivenVelocity=Pose.Velocity; Anim->DrivenFacing=FRotator(0,Pose.Facing.Yaw,0); Anim->DrivenPitch=FRotator::NormalizeAxis(Pose.View.Pitch); Anim->bDrivenFalling=Pose.bFalling; }
 if (Pose.Weapon!=Shown)
 {
  Shown=Pose.Weapon;
  Weapon->SetStaticMesh(Shown ? Shown->PresentationMesh.Get() : nullptr);
  if (Shown && Shown==SkinnedWeapon) for (int32 i=0;i<SkinMaterials.Num();++i) Weapon->SetMaterial(i,SkinMaterials[i]);
 }
}

UCrosshairKillcam::UCrosshairKillcam()
{
 PrimaryComponentTick.bCanEverTick=true;
 PrimaryComponentTick.bTickEvenWhenPaused=false;
}
float UCrosshairKillcam::GetRemaining() const { return bPlaying && Playback.Num() ? float(Playback.Last().Time-PlayTime) : 0.f; }
void UCrosshairKillcam::Skip() { if (bPlaying) Finish(); }
void UCrosshairKillcam::EndPlay(const EEndPlayReason::Type Reason)
{
 if (bPlaying) Finish();
 Super::EndPlay(Reason);
}
void UCrosshairKillcam::TickComponent(float DeltaSeconds,ELevelTick TickType,FActorComponentTickFunction* ThisTickFunction)
{
 Super::TickComponent(DeltaSeconds,TickType,ThisTickFunction);
 auto* PC=Cast<APlayerController>(GetOwner());
 UWorld* World=GetWorld();
 const auto* Match=ACrosshairMatchState::Get(this);
 if (!PC || !PC->IsLocalController() || !Match || (World->GetDemoNetDriver() && World->GetDemoNetDriver()->IsPlaying())) return;
 const double Now=World->GetTimeSeconds();
 // Keep recording during the killcam too: the live match goes on underneath.
 if (Now-LastSample>=1.0/SampleRate) Record(Now);
 const auto* Me=Cast<ACrosshairCharacter>(PC->GetPawn());
 const bool bDead=Me && Me->Health->bDead;
 if (bDead && CountedDeath.Get()!=Me) { CountedDeath=Me; DiedAt=Now; bReported=false; }
 if (!bPlaying && !bReported && Now>=DiedAt+StartDelay) Begin();
 else if (bPlaying) Present(DeltaSeconds);
}
void UCrosshairKillcam::Record(double Now)
{
 LastSample=Now;
 FCrosshairKillcamFrame& Frame=Frames.AddDefaulted_GetRef();
 Frame.Time=Now;
 for (TActorIterator<ACrosshairCharacter> It(GetWorld());It;++It)
 {
  const ACrosshairCharacter* Soldier=*It;
  FCrosshairKillcamPose Pose;
  Pose.Location=Soldier->GetActorLocation(); Pose.Facing=Soldier->GetActorRotation(); Pose.Velocity=Soldier->GetVelocity();
  Pose.Eye=Soldier->GetFirstPersonCameraComponent()->GetComponentLocation();
  Pose.View=Soldier->IsLocallyControlled() && Soldier->GetController() ? Soldier->GetControlRotation() : Soldier->GetBaseAimRotation();
  Pose.Aim=Soldier->GetSharedAimAlpha();
  Pose.bFalling=Soldier->GetCharacterMovement()->IsFalling();
  Pose.bDead=Soldier->Health->bDead;
  if (const ACrosshairWeapon* Held=Soldier->Inventory->GetCurrent()) { Pose.Weapon=Held->Definition; Pose.Shots=Held->GetShotSequence(); }
  Frame.Soldiers.Add(*It,Pose);
 }
 // A little more than the replayed window, so a late start still has its first seconds.
 const double Oldest=Now-(SecondsBefore+StartDelay+1.0);
 int32 Drop=0; while (Drop<Frames.Num() && Frames[Drop].Time<Oldest) ++Drop;
 if (Drop) Frames.RemoveAt(0,Drop,EAllowShrinking::No);
}
void UCrosshairKillcam::Begin()
{
 auto* PC=Cast<APlayerController>(GetOwner());
 const auto* Me=Cast<ACrosshairCharacter>(PC->GetPawn());
 const auto* Match=ACrosshairMatchState::Get(this);
 KillerName=Me ? Me->Health->KilledBy : FString();
 Killer.Reset();
 // Only a kill by another soldier has a killcam (not falls, own grenades or a soldier no longer recorded).
 if (!Frames.IsEmpty() && Me && !KillerName.IsEmpty())
  for (const auto& Entry:Frames.Last().Soldiers)
   if (Entry.Key.IsValid() && Entry.Key.Get()!=Me && Entry.Key->GetPlayerState() && Entry.Key->GetPlayerState()->GetPlayerName()==KillerName) Killer=Entry.Key;
 Playback.Reset();
 for (const FCrosshairKillcamFrame& Frame:Frames) if (Frame.Time>=DiedAt-SecondsBefore) Playback.Add(Frame);
 if (!Killer.IsValid() || Playback.Num()<2 || Match->bMatchOver) { Report(); return; }
 WeaponName.Reset();
 const FString MyName=Me->GetPlayerState() ? Me->GetPlayerState()->GetPlayerName() : FString();
 for (int32 i=Match->KillFeed.Num()-1;i>=0;--i) if (Match->KillFeed[i].Victim==MyName) { WeaponName=Match->KillFeed[i].Weapon; break; }
 // Stand-ins for everyone recorded whose soldier still exists to copy the look from.
 for (const FCrosshairKillcamFrame& Frame:Playback)
  for (const auto& Entry:Frame.Soldiers)
  {
   if (Ghosts.Contains(Entry.Key) || !Entry.Key.IsValid()) continue;
   FActorSpawnParameters Params; Params.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn; Params.ObjectFlags|=RF_Transient;
   auto* Ghost=GetWorld()->SpawnActor<ACrosshairKillcamGhost>(ACrosshairKillcamGhost::StaticClass(),Entry.Value.Location,FRotator::ZeroRotator,Params);
   if (!Ghost) continue;
   Ghost->CopyLook(Entry.Key.Get());
   // The camera sits in the killer's head.
   if (Entry.Key==Killer) Ghost->SetActorHiddenInGame(true);
   Ghosts.Add(Entry.Key,Ghost);
  }
 FActorSpawnParameters Params; Params.ObjectFlags|=RF_Transient;
 Camera=GetWorld()->SpawnActor<ACameraActor>(ACameraActor::StaticClass(),Playback[0].Soldiers.FindRef(Killer).Eye,Playback[0].Soldiers.FindRef(Killer).View,Params);
 Camera->GetCameraComponent()->bConstrainAspectRatio=false;
 Viewmodel=NewObject<UStaticMeshComponent>(Camera);
 Viewmodel->SetupAttachment(Camera->GetCameraComponent());
 Viewmodel->SetCollisionEnabled(ECollisionEnabled::NoCollision);
 Viewmodel->SetCastShadow(false);
 Viewmodel->RegisterComponent();
 ShownWeapon=nullptr; Kick=0; Aim=0;
 PlayTime=Playback[0].Time;
 LastShots=Playback[0].Soldiers.FindRef(Killer).Shots;
 bPlaying=true;
 HideLiveWorld();
 PC->SetViewTarget(Camera);
 UE_LOG(LogTemp,Display,TEXT("Crosshair killcam: %s killed the player with %s; replaying %.1fs over %d stand-ins"),*KillerName,*WeaponName,Playback.Last().Time-Playback[0].Time,Ghosts.Num());
}
void UCrosshairKillcam::HideLiveWorld()
{
 // Hidden from this player's view only; the match itself (and every other player) is untouched.
 auto* PC=Cast<APlayerController>(GetOwner());
 for (TActorIterator<ACrosshairCharacter> It(GetWorld());It;++It)
 {
  TArray<AActor*> Parts={*It};
  for (ACrosshairWeapon* W:It->Inventory->Weapons) if (W) Parts.Add(W);
  for (AActor* Part:Parts) if (!PC->HiddenActors.Contains(Part)) { PC->HiddenActors.Add(Part); Hidden.Add(Part); }
 }
}
void UCrosshairKillcam::Present(float DeltaSeconds)
{
 auto* PC=Cast<APlayerController>(GetOwner());
 const auto* Match=ACrosshairMatchState::Get(this);
 const auto* Me=Cast<ACrosshairCharacter>(PC->GetPawn());
 PlayTime+=DeltaSeconds;
 // Ends with the recording, at match end, or if the player is already back in (host timeout).
 if (PlayTime>=Playback.Last().Time+.4 || Match->bMatchOver || (Me && !Me->Health->bDead)) { Finish(); return; }
 HideLiveWorld();   // soldiers that respawned during the killcam
 int32 Index=0; while (Index+2<Playback.Num() && Playback[Index+1].Time<=PlayTime) ++Index;
 const FCrosshairKillcamFrame& A=Playback[Index]; const FCrosshairKillcamFrame& B=Playback[FMath::Min(Index+1,Playback.Num()-1)];
 const float Alpha=B.Time>A.Time ? FMath::Clamp(float((PlayTime-A.Time)/(B.Time-A.Time)),0.f,1.f) : 1.f;
 for (const auto& Entry:Ghosts)
 {
  ACrosshairKillcamGhost* Ghost=Entry.Value.Get(); if (!Ghost) continue;
  const FCrosshairKillcamPose* From=A.Soldiers.Find(Entry.Key); const FCrosshairKillcamPose* To=B.Soldiers.Find(Entry.Key);
  if (!From && !To) { Ghost->SetActorHiddenInGame(true); continue; }
  if (Entry.Key!=Killer) Ghost->SetActorHiddenInGame(false);
  Ghost->ApplyPose(From && To ? FCrosshairKillcamPose::Blend(*From,*To,Alpha) : From ? *From : *To);
 }
 const FCrosshairKillcamPose* From=A.Soldiers.Find(Killer); const FCrosshairKillcamPose* To=B.Soldiers.Find(Killer);
 if (!From && !To) return;
 const FCrosshairKillcamPose View=From && To ? FCrosshairKillcamPose::Blend(*From,*To,Alpha) : From ? *From : *To;
 if (View.Shots>LastShots && View.Weapon)
 {
  Kick=5;
  if (View.Weapon->FireSound) UGameplayStatics::PlaySoundAtLocation(this,View.Weapon->FireSound,View.Eye,.35f);
 }
 LastShots=View.Shots;
 Kick=FMath::FInterpTo(Kick,0,DeltaSeconds,14);
 Aim=View.Aim; ShownWeapon=View.Weapon;
 Camera->SetActorLocationAndRotation(View.Eye,View.View);
 const float BaseFOV=GetWorld()->GetGameInstance()->GetSubsystem<UCrosshairReplaySubsystem>()->GetSettings().FieldOfView;
 Camera->GetCameraComponent()->SetFieldOfView(FMath::Lerp(BaseFOV,ShownWeapon ? ShownWeapon->AimFOV : BaseFOV,Aim));
 // The killer's gun in view, hip to sights like the live viewmodel; the scope view hides it (the HUD draws the scope).
 Viewmodel->SetStaticMesh(ShownWeapon ? ShownWeapon->PresentationMesh.Get() : nullptr);
 if (ShownWeapon) Viewmodel->SetRelativeLocation(FMath::Lerp(ShownWeapon->HipOffset,ShownWeapon->AimOffset,Aim)-FVector(Kick,0,0));
 Viewmodel->SetVisibility(!(ShownWeapon && ShownWeapon->AimStyle==ECrosshairAimStyle::Scope && Aim>=.95f));
}
void UCrosshairKillcam::Finish()
{
 auto* PC=Cast<APlayerController>(GetOwner());
 bPlaying=false;
 for (const auto& Entry:Ghosts) if (Entry.Value.IsValid()) Entry.Value->Destroy();
 Ghosts.Reset(); Playback.Reset();
 for (const TWeakObjectPtr<AActor>& Part:Hidden) if (Part.IsValid()) PC->HiddenActors.Remove(Part.Get());
 PC->HiddenActors.RemoveAll([](const TObjectPtr<AActor>& Actor){ return !IsValid(Actor); });
 Hidden.Reset();
 if (PC->GetPawn()) PC->SetViewTarget(PC->GetPawn());
 if (Camera) { Camera->Destroy(); Camera=nullptr; Viewmodel=nullptr; }
 ShownWeapon=nullptr; Aim=0;
 Report();
}
void UCrosshairKillcam::Report()
{
 if (bReported) return;
 bReported=true;
 if (auto* PC=Cast<ACrosshairPlayerController>(GetOwner())) PC->ServerKillcamDone();
}
