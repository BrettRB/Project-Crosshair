#include "CrosshairCharacter.h"
#include "CrosshairData.h"
#include "CrosshairWeapon.h"
#include "CrosshairPractice.h"
#include "CrosshairReplaySubsystem.h"
#include "CrosshairGame.h"
#include "CrosshairDummy.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Animation/AnimSequence.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputActionValue.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Engine/LocalPlayer.h"
#include "GameFramework/PlayerInput.h"
#include "Engine/DemoNetDriver.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"
#include "UObject/ConstructorHelpers.h"

ACrosshairCharacter::ACrosshairCharacter()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;
	bAlwaysRelevant = true;
	SetNetUpdateFrequency(120);
	SetMinNetUpdateFrequency(60);
	Inventory = CreateDefaultSubobject<UCrosshairInventoryComponent>(TEXT("Inventory"));
	Placement = CreateDefaultSubobject<UCrosshairPlacementComponent>(TEXT("Placement"));
	Attempt = CreateDefaultSubobject<UCrosshairAttemptComponent>(TEXT("Attempt"));
	GetCharacterMovement()->GetNavAgentPropertiesRef().bCanCrouch = true;
	GetCharacterMovement()->SetCrouchedHalfHeight(58.f);
	GetCharacterMovement()->JumpZVelocity = 460;
	GetCharacterMovement()->AirControl = 0.25f;
	GetCharacterMovement()->BrakingDecelerationFalling = 0;
	GetCharacterMovement()->GravityScale = 1.2f;
	GetCharacterMovement()->MaxAcceleration = 2400;
	GetCharacterMovement()->BrakingDecelerationWalking = 2200;
	GetCharacterMovement()->MaxWalkSpeedCrouched = CrouchSpeed;
	GetFirstPersonCameraComponent()->SetupAttachment(GetCapsuleComponent());
	GetFirstPersonCameraComponent()->SetRelativeLocationAndRotation(FVector(0, 0, 64), FRotator::ZeroRotator);
	GetFirstPersonCameraComponent()->bEnableFirstPersonFieldOfView = false;
	GetFirstPersonCameraComponent()->bEnableFirstPersonScale = false;
	GetFirstPersonMesh()->SetupAttachment(GetFirstPersonCameraComponent());
	GetFirstPersonMesh()->SetRelativeLocationAndRotation(FVector(-12, 0, -155), FRotator(0, -90, 0));
	GetFirstPersonMesh()->SetOnlyOwnerSee(false);
	GetFirstPersonMesh()->SetCastShadow(false);
	GetFirstPersonMesh()->FirstPersonPrimitiveType = EFirstPersonPrimitiveType::None;
	GetMesh()->SetVisibility(false);

}
void ACrosshairCharacter::PostInitializeComponents()
{
	Super::PostInitializeComponents();
	Inventory->Loadout = DefaultLoadout;
	Placement->TargetClass = TargetClass;
	GetCharacterMovement()->MaxWalkSpeedCrouched = CrouchSpeed;
}
void ACrosshairCharacter::BeginPlay()
{
	Super::BeginPlay();
	if (IdleAnimation)
	{
		auto* Arms = GetFirstPersonMesh();
		for (int32 i=0; i<FirstPersonArmMaterials.Num(); ++i)
			if (FirstPersonArmMaterials[i]) Arms->SetMaterial(i, FirstPersonArmMaterials[i]);
		// This template mesh includes a full body; keep the head and legs out of the viewmodel.
		Arms->HideBoneByName(TEXT("neck_01"), PBO_None);
		Arms->HideBoneByName(TEXT("thigh_l"), PBO_None);
		Arms->HideBoneByName(TEXT("thigh_r"), PBO_None);
		Arms->PlayAnimation(IdleAnimation, true);
		Arms->TickAnimation(0.f, false);
		Arms->RefreshBoneTransforms();
		// The rifle and mannequin use the same mesh basis. The socket's rotation
		// belongs to the template attachment convention; only its grip position is needed.
		if (Arms->DoesSocketExist(TEXT("HandGrip_R")))
			ArmsFromGrip = FTransform(-Arms->GetSocketTransform(TEXT("HandGrip_R"), RTS_Component).GetLocation());
		ArmsPoseHandle = Arms->RegisterOnBoneTransformsFinalizedDelegate(
			FOnBoneTransformsFinalizedMultiCast::FDelegate::CreateUObject(this, &ACrosshairCharacter::UpdateArmsPresentation));
	}
	if (IsReplayPlayback()) return;
	if (APlayerController* PC = Cast<APlayerController>(GetController()))
	{
		if (ULocalPlayer* Local = PC->GetLocalPlayer())
			if (auto* Input = Local->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>()) if (Inputs && Inputs->Mapping) Input->AddMappingContext(Inputs->Mapping, 10);
	}
	GetWorldTimerManager().SetTimerForNextTick(FTimerDelegate::CreateWeakLambda(this, [this]()
	{
		GetGameInstance()->GetSubsystem<UCrosshairReplaySubsystem>()->PracticeReady(this);
	}));
}
void ACrosshairCharacter::EndPlay(EEndPlayReason::Type Reason)
{
	GetFirstPersonMesh()->UnregisterOnBoneTransformsFinalizedDelegate(ArmsPoseHandle);
	if (APlayerController* PC = Cast<APlayerController>(GetController()))
		if (ULocalPlayer* Local = PC->GetLocalPlayer())
			if (auto* Input = Local->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>())
				if (Inputs && Inputs->Mapping) Input->RemoveMappingContext(Inputs->Mapping);
	if (!IsReplayPlayback()) for (ACrosshairWeapon* Weapon : Inventory->Weapons) if (IsValid(Weapon)) Weapon->Destroy();
	Super::EndPlay(Reason);
}
void ACrosshairCharacter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME_CONDITION(ACrosshairCharacter, RecordedView, COND_ReplayOnly);
}
bool ACrosshairCharacter::IsReplayPlayback() const
{
	return GetWorld() && GetWorld()->GetDemoNetDriver() && GetWorld()->GetDemoNetDriver()->IsPlaying();
}
bool ACrosshairCharacter::CanAct() const
{
	const ACrosshairPlayerController* PC = Cast<ACrosshairPlayerController>(GetController());
	return !IsReplayPlayback() && PC && !PC->bMenuOpen && !Attempt->bSucceeded;
}
void ACrosshairCharacter::Notify(const FString& Text) { Notice = Text; NoticeUntil = GetWorld()->GetTimeSeconds() + 4; }
void ACrosshairCharacter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	UCameraComponent* Camera = GetFirstPersonCameraComponent();
	ACrosshairWeapon* Weapon = Inventory->GetCurrent();
	if (IsReplayPlayback())
	{
		Camera->bUsePawnControlRotation = false;
		Camera->SetWorldLocationAndRotation(RecordedView.Location, RecordedView.Rotation);
		Camera->SetFieldOfView(RecordedView.FOV);
		AimAlpha = RecordedView.AimAlpha;
		if (RecordedView.HitSequence != PresentedHit) { PresentedHit = RecordedView.HitSequence; HitMarkerUntil = GetWorld()->GetTimeSeconds() + 0.25f; }
	}
	else
	{
		const FCrosshairSettings& Settings = GetGameInstance()->GetSubsystem<UCrosshairReplaySubsystem>()->GetSettings();
		const float AimSeconds = Weapon && Weapon->Definition ? Weapon->Definition->AimSeconds : 0.2f;
		const bool bWantsAim = bAimHeld && CanAct() && !Placement->bPlacing && Weapon && !Weapon->bReloading;
		AimAlpha = FMath::FInterpConstantTo(AimAlpha, bWantsAim ? 1.f : 0.f, DeltaSeconds, 1.f / FMath::Max(0.01f, AimSeconds));
		Camera->SetFieldOfView(FMath::Lerp(Settings.FieldOfView, Weapon && Weapon->Definition ? Weapon->Definition->AimFOV : Settings.FieldOfView, AimAlpha));
		const float EyeZ = bIsCrouched ? 30.f : 64.f;
		Camera->SetRelativeLocation(FVector(0, 0, FMath::FInterpTo(Camera->GetRelativeLocation().Z, EyeZ, DeltaSeconds, 12)));
		GetCharacterMovement()->MaxWalkSpeedCrouched = CrouchSpeed;
		GetCharacterMovement()->MaxWalkSpeed = bSprintHeld && !bWantsAim && !bIsCrouched ? SprintSpeed : WalkSpeed * FMath::Lerp(1.f, 0.6f, AimAlpha);
		RecordedView.Location = Camera->GetComponentLocation();
		RecordedView.Rotation = Camera->GetComponentRotation();
		RecordedView.FOV = Camera->FieldOfView;
		RecordedView.AimAlpha = AimAlpha;
		if (GetActorLocation().Z < -1500) Attempt->ResetAttempt();
	}
	if (Weapon)
	{
		// Attachment to a camera component is reconstructed explicitly for replay actors as well.
		if (Weapon->GetRootComponent()->GetAttachParent() != Camera) Weapon->AttachToComponent(Camera, FAttachmentTransformRules::SnapToTargetNotIncludingScale);
		Weapon->UpdatePresentation(AimAlpha);
		GetFirstPersonMesh()->SetRelativeTransform(ArmsFromGrip * Weapon->Mesh->GetRelativeTransform());
		GetFirstPersonMesh()->SetVisibility(!(Weapon->Definition && Weapon->Definition->AimStyle == ECrosshairAimStyle::Scope && AimAlpha >= .95f));
		if (Weapon->bReloading != bArmsReloading)
		{
			bArmsReloading = Weapon->bReloading;
			GetFirstPersonMesh()->PlayAnimation(bArmsReloading ? ReloadAnimation : IdleAnimation, !bArmsReloading);
		}
	}
}
void ACrosshairCharacter::UpdateArmsPresentation()
{
	auto* Weapon = Inventory->GetCurrent();
	auto* Arms = GetFirstPersonMesh();
	if (!Weapon || !Arms->DoesSocketExist(TEXT("HandGrip_R"))) return;
	// Correct idle breathing after the animated pose is evaluated. Reload keeps the
	// idle grip reference so its free-hand movement is preserved.
	if (!Weapon->bReloading)
		ArmsFromGrip = FTransform(-Arms->GetSocketTransform(TEXT("HandGrip_R"), RTS_Component).GetLocation());
	Arms->SetRelativeTransform(ArmsFromGrip * Weapon->Mesh->GetRelativeTransform());
}
void ACrosshairCharacter::SetupPlayerInputComponent(UInputComponent* Input)
{
	// The base template binds its own input assets; this mode supplies a complete independent mapping.
	UEnhancedInputComponent* Enhanced = Cast<UEnhancedInputComponent>(Input);
	if (!Enhanced || !Inputs || !Inputs->IsComplete()) { UE_LOG(LogTemp, Error, TEXT("Crosshair input assets are missing. Run Scripts/create_crosshair_assets.py.")); return; }
	Enhanced->BindAction(Inputs->Move, ETriggerEvent::Triggered, this, &ACrosshairCharacter::Move);
#define BIND_START(Field, Method) Enhanced->BindAction(Inputs->Field, ETriggerEvent::Started, this, &ACrosshairCharacter::Method)
#define BIND_END(Field, Method) Enhanced->BindAction(Inputs->Field, ETriggerEvent::Completed, this, &ACrosshairCharacter::Method); Enhanced->BindAction(Inputs->Field, ETriggerEvent::Canceled, this, &ACrosshairCharacter::Method)
	BIND_START(Jump, JumpPressed); BIND_END(Jump, JumpReleased);
	BIND_START(Sprint, SprintPressed); BIND_END(Sprint, SprintReleased);
	BIND_START(Crouch, CrouchPressed);
	BIND_START(Fire, FirePressed); BIND_END(Fire, FireReleased);
	BIND_START(Aim, AimPressed); BIND_END(Aim, AimReleased);
	BIND_START(Reload, ReloadPressed); BIND_START(SwitchWeapon, SwitchPressed);
	BIND_START(Placement, PlacementPressed); BIND_START(RotateTarget, RotatePressed);
	BIND_START(RemoveTarget, RemovePressed); BIND_START(ClearTargets, ClearPressed);
	BIND_START(SaveStart, SavePressed); BIND_START(Reset, ResetPressed); // Menu belongs to the controller, including during replay.
#undef BIND_START
#undef BIND_END
}
void ACrosshairCharacter::Move(const FInputActionValue& Value)
{
	if (!CanAct()) return;
	const FVector2D Axis = Value.Get<FVector2D>().GetClampedToMaxSize(1.f);
	const FRotationMatrix Rotation(FRotator(0, GetControlRotation().Yaw, 0));
	AddMovementInput(Rotation.GetUnitAxis(EAxis::X), Axis.Y);
	AddMovementInput(Rotation.GetUnitAxis(EAxis::Y), Axis.X);
}
void ACrosshairCharacter::ApplyLookInput(FVector2D MouseDelta, FVector2D StickAxis, float DeltaSeconds)
{
	if (!CanAct()) return;
	const auto& Settings = GetGameInstance()->GetSubsystem<UCrosshairReplaySubsystem>()->GetSettings();
	// Native viewport mouse displacement and standard gamepad axes are independent.
	// Compose them once; controller inversion never reaches the mouse path.
	const FVector2D Delta = CrosshairRules::MouseDelta(MouseDelta, Settings, AimAlpha)
		+ CrosshairRules::StickDelta(StickAxis, Settings, AimAlpha, DeltaSeconds);
	FRotator Rotation = GetControlRotation();
	Rotation.Yaw += Delta.X;
	Rotation.Pitch = FMath::Clamp(FRotator::NormalizeAxis(Rotation.Pitch) + Delta.Y, -85.f, 85.f);
	GetController()->SetControlRotation(Rotation);
}
void ACrosshairCharacter::JumpPressed() { if (CanAct()) Jump(); }
void ACrosshairCharacter::JumpReleased() { StopJumping(); }
void ACrosshairCharacter::SprintPressed() { if (CanAct()) bSprintHeld = true; }
void ACrosshairCharacter::SprintReleased() { bSprintHeld = false; }
void ACrosshairCharacter::CrouchPressed() { if (CanAct()) { if (bIsCrouched) UnCrouch(); else Crouch(); } }
void ACrosshairCharacter::FirePressed() { if (!CanAct()) return; bSprintHeld = false; if (Placement->bPlacing) Placement->Confirm(); else if (auto* W = Inventory->GetCurrent()) W->StartFire(); }
void ACrosshairCharacter::FireReleased() { if (auto* W = Inventory->GetCurrent()) W->StopFire(); }
void ACrosshairCharacter::AimPressed() { if (!CanAct()) return; if (Placement->bPlacing) Placement->Toggle(); else { bAimHeld = true; bSprintHeld = false; } }
void ACrosshairCharacter::AimReleased() { bAimHeld = false; }
void ACrosshairCharacter::ReloadPressed() { if (CanAct()) { if (Placement->bPlacing) Placement->Rotate(); else if (auto* W = Inventory->GetCurrent()) W->Reload(); } }
void ACrosshairCharacter::SwitchPressed() { if (CanAct()) { bAimHeld = false; Inventory->Cycle(); } }
void ACrosshairCharacter::PlacementPressed() { Placement->Toggle(); }
void ACrosshairCharacter::RotatePressed() { Placement->Rotate(); }
void ACrosshairCharacter::RemovePressed() { Placement->RemoveAimedTarget(); }
void ACrosshairCharacter::ClearPressed() { Placement->ClearTargets(); }
void ACrosshairCharacter::SavePressed() { Attempt->SaveStart(); }
void ACrosshairCharacter::ResetPressed() { if (!IsReplayPlayback()) Attempt->ResetAttempt(); }
void ACrosshairCharacter::MenuPressed() { if (auto* PC = Cast<ACrosshairPlayerController>(GetController())) PC->ToggleMenu(); }
void ACrosshairCharacter::ApplyRecoil(float Degrees)
{
	if (!GetController()) return;
	FRotator Rotation = GetControlRotation();
	Rotation.Pitch = FMath::Clamp(FRotator::NormalizeAxis(Rotation.Pitch) + Degrees, -85.f, 85.f);
	GetController()->SetControlRotation(Rotation);
}
void ACrosshairCharacter::TargetHit(ACrosshairDummy* Target)
{
	++RecordedView.HitSequence;
	PresentedHit = RecordedView.HitSequence;
	HitMarkerUntil = GetWorld()->GetTimeSeconds() + 0.25f;
	if (Target && Target->bHit) Attempt->HandleTargetHit(Target);
}
void ACrosshairCharacter::StopActions() { FireReleased(); bAimHeld = false; bSprintHeld = false; StopJumping(); }
