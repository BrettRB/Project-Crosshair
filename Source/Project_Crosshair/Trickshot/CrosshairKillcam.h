#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameFramework/Actor.h"
#include "CrosshairStance.h"
#include "CrosshairKillcam.generated.h"

class ACrosshairCharacter;
class UCrosshairWeaponDefinition;
class USkeletalMeshComponent;
class UStaticMeshComponent;
class UMaterialInterface;
class ACameraActor;

/** One soldier at one moment, as this machine saw it (replicated movement, view and weapon). */
struct FCrosshairKillcamPose
{
 FVector Location=FVector::ZeroVector;
 FRotator Facing=FRotator::ZeroRotator;
 FVector Velocity=FVector::ZeroVector;
 FVector Eye=FVector::ZeroVector;
 FRotator View=FRotator::ZeroRotator;
 float Aim=0;
 ECrosshairStance Stance=ECrosshairStance::Normal;
 bool bFalling=false,bDead=false;
 const UCrosshairWeaponDefinition* Weapon=nullptr;
 int32 Shots=0;
 static FCrosshairKillcamPose Blend(const FCrosshairKillcamPose& A,const FCrosshairKillcamPose& B,float Alpha);
};

struct FCrosshairKillcamFrame
{
 double Time=0;
 TMap<TWeakObjectPtr<ACrosshairCharacter>,FCrosshairKillcamPose> Soldiers;
};

/** A killcam stand-in: the soldier's body, uniform, gear and gun, posed from the recording. Local only, never replicated. */
UCLASS(NotPlaceable,Transient)
class PROJECT_CROSSHAIR_API ACrosshairKillcamGhost : public AActor
{
 GENERATED_BODY()
public:
 ACrosshairKillcamGhost();
 void CopyLook(const ACrosshairCharacter* Source);
 void ApplyPose(const FCrosshairKillcamPose& Pose);
 UPROPERTY() TObjectPtr<USkeletalMeshComponent> Body;
 UPROPERTY() TObjectPtr<UStaticMeshComponent> Weapon;
private:
 /** The gun the soldier held when the killcam began keeps its camo; other guns use their default finish. */
 UPROPERTY() TObjectPtr<UCrosshairWeaponDefinition> SkinnedWeapon;
 UPROPERTY() TArray<TObjectPtr<UMaterialInterface>> SkinMaterials;
 const UCrosshairWeaponDefinition* Shown=nullptr;
 bool bRagdoll=false;
};

/**
 * Killcam for the local player in a match. Records every soldier for the last few seconds (on every machine:
 * offline, LAN host and joined players) and, after this player is killed by someone else, replays those seconds
 * from the killer's eyes on stand-in soldiers while the live match keeps running unseen. F skips it.
 * The host holds this player's respawn until the killcam reports done (ACrosshairPlayerController::ServerKillcamDone).
 */
UCLASS(ClassGroup=(Crosshair))
class PROJECT_CROSSHAIR_API UCrosshairKillcam : public UActorComponent
{
 GENERATED_BODY()
public:
 UCrosshairKillcam();
 virtual void TickComponent(float DeltaSeconds,ELevelTick TickType,FActorComponentTickFunction* ThisTickFunction) override;
 virtual void EndPlay(const EEndPlayReason::Type Reason) override;
 bool IsPlaying() const { return bPlaying; }
 void Skip();
 /** Seconds replayed before the death, and the pause after the death before the killcam starts. */
 static constexpr double SecondsBefore=5.0;
 static constexpr double StartDelay=1.2;
 static constexpr double SampleRate=30.0;
 FString KillerName,WeaponName;
 float GetAim() const { return Aim; }
 const UCrosshairWeaponDefinition* GetWeapon() const { return ShownWeapon; }
 float GetRemaining() const;
 /** Frames currently held (diagnostics and tests). */
 int32 GetRecordedFrames() const { return Frames.Num(); }
private:
 void Record(double Now);
 void Begin();
 void Present(float DeltaSeconds);
 void Finish();
 void Report();
 void HideLiveWorld();
 TArray<FCrosshairKillcamFrame> Frames,Playback;
 double LastSample=-1,DiedAt=-1,PlayTime=0;
 bool bPlaying=false,bReported=true;
 /** The soldier whose death started the current killcam: each soldier dies once, even if killed the frame it spawned. */
 TWeakObjectPtr<const APawn> CountedDeath;
 TWeakObjectPtr<ACrosshairCharacter> Killer;
 TMap<TWeakObjectPtr<ACrosshairCharacter>,TWeakObjectPtr<ACrosshairKillcamGhost>> Ghosts;
 TArray<TWeakObjectPtr<AActor>> Hidden;
 UPROPERTY() TObjectPtr<ACameraActor> Camera;
 UPROPERTY() TObjectPtr<UStaticMeshComponent> Viewmodel;
 const UCrosshairWeaponDefinition* ShownWeapon=nullptr;
 int32 LastShots=0;
 float Kick=0,Aim=0;
};
