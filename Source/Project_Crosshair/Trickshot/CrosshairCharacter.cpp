#include "CrosshairCharacter.h"
#include "CrosshairStartup.h"
#include "CrosshairCombatAppearance.h"
#include "CrosshairData.h"
#include "CrosshairWeapon.h"
#include "CrosshairPractice.h"
#include "CrosshairReplaySubsystem.h"
#include "CrosshairGame.h"
#include "CrosshairDummy.h"
#include "CrosshairTraversal.h"
#include "CrosshairThrowable.h"
#include "CrosshairMatch.h"
#include "CrosshairSoldierAppearance.h"
#include "CrosshairTrickshot.h"
#include "CrosshairLocomotion.h"
#include "AIController.h"
#include "Animation/AnimInstance.h"
#include "GameFramework/PlayerState.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
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
	CombatAppearance=CreateDefaultSubobject<UCrosshairCombatAppearance>(TEXT("Combat appearance"));
	Inventory = CreateDefaultSubobject<UCrosshairInventoryComponent>(TEXT("Inventory"));
	Lethals = CreateDefaultSubobject<UCrosshairLethalComponent>(TEXT("Lethals"));
	Placement = CreateDefaultSubobject<UCrosshairPlacementComponent>(TEXT("Placement"));
	Traversal = CreateDefaultSubobject<UCrosshairTraversalComponent>(TEXT("Traversal"));
	Attempt = CreateDefaultSubobject<UCrosshairAttemptComponent>(TEXT("Attempt"));
	Health = CreateDefaultSubobject<UCrosshairHealthComponent>(TEXT("Health"));
	MatchAppearance = CreateDefaultSubobject<UCrosshairSoldierAppearance>(TEXT("Match appearance"));
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
 if (IsInMatch()) ConfigureMatchBody();
 else if (GetMesh()->GetSkeletalMeshAsset()) { GetMesh()->SetVisibility(true); GetMesh()->SetOwnerNoSee(true); GetMesh()->SetAnimInstanceClass(UCrosshairLocomotionAnimInstance::StaticClass()); }
	if (IdleAnimation)
	{
		auto* Arms = GetFirstPersonMesh();
		for (int32 i=0; i<FirstPersonArmMaterials.Num(); ++i)
			if (FirstPersonArmMaterials[i]) Arms->SetMaterial(i, FirstPersonArmMaterials[i]);
		// This template mesh includes a full body; keep the head and legs out of the viewmodel.
		Arms->HideBoneByName(TEXT("neck_01"), PBO_None);
		Arms->HideBoneByName(TEXT("thigh_l"), PBO_None);
		Arms->HideBoneByName(TEXT("thigh_r"), PBO_None);
		Arms->SetAnimInstanceClass(UCrosshairViewmodelAnimInstance::StaticClass());
		if (auto* View = Cast<UCrosshairViewmodelAnimInstance>(Arms->GetAnimInstance())) View->Play(IdleAnimation, true);
		else Arms->PlayAnimation(IdleAnimation, true);
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
	GetWorldTimerManager().SetTimerForNextTick(FTimerDelegate::CreateWeakLambda(this, [this]()
	{
		if (!IsInMatch()) { Inventory->ApplyClass(); GetGameInstance()->GetSubsystem<UCrosshairReplaySubsystem>()->PracticeReady(this); return; }
		// Match spawns never record practice replays. Bots carry a random two-weapon class.
		if (!HasAuthority()) return;
		if (!IsBot() && IsLocallyControlled()) GetGameInstance()->GetSubsystem<UCrosshairReplaySubsystem>()->BeginMatchRecording(this);
		if (IsBot())
		{
			const int32 Count = FMath::Max(1, Inventory->Weapons.Num());
			const int32 Primary = FMath::RandRange(0, Count - 1);
			Inventory->ApplyLoadout(Primary, (Primary + FMath::RandRange(1, FMath::Max(1, Count - 1))) % Count, FMath::RandBool() ? ECrosshairLethalType::Frag : ECrosshairLethalType::Tomahawk);
		}
		// A joined player's class comes from their own machine (PawnClientRestart -> ServerApplyLoadout).
		else if (!IsRemotePlayer()) Inventory->ApplyClass();
	}));
}
void ACrosshairCharacter::PawnClientRestart()
{
	Super::PawnClientRestart();
	// Controls are added when a local player takes this soldier, not at BeginPlay: a LAN host's lobby and
	// respawn soldiers begin play before they are possessed, so BeginPlay would leave them without controls.
	if (const APlayerController* PC = Cast<APlayerController>(GetController()); PC && !IsReplayPlayback())
		if (ULocalPlayer* Local = PC->GetLocalPlayer())
			if (auto* Input = Local->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>()) if (Inputs && Inputs->Mapping) Input->AddMappingContext(Inputs->Mapping, 10);
	if (HasAuthority() || IsReplayPlayback() || !IsLocallyControlled()) return;
	const FCrosshairSettings& S = GetGameInstance()->GetSubsystem<UCrosshairReplaySubsystem>()->GetSettings();
	if (S.Classes.IsValidIndex(S.ActiveClass)) ServerApplyLoadout(S.Classes[S.ActiveClass].Primary, S.Classes[S.ActiveClass].Secondary, S.Classes[S.ActiveClass].Lethal);
}
void ACrosshairCharacter::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);
	bReplayFocus = NewController && NewController->IsLocalPlayerController();
	// TDM uniforms: Alpha wears the first half of the soldier catalog, Bravo the second.
	const auto* Match = NewController ? NewController->GetPlayerState<ACrosshairMatchPlayerState>() : nullptr;
	if (!Match || !MatchAppearance->Catalog || MatchAppearance->Catalog->Designs.IsEmpty()) return;
	const int32 Count = MatchAppearance->Catalog->Designs.Num(), Half = FMath::Max(1, Count / 2);
	if (Match->Team == CrosshairMatchRules::Alpha) MatchAppearance->SetDesign(FMath::RandRange(0, Half - 1));
	else if (Match->Team == CrosshairMatchRules::Bravo) MatchAppearance->SetDesign(FMath::RandRange(FMath::Min(Half, Count - 1), Count - 1));
}
void ACrosshairCharacter::ConfigureMatchBody()
{
	USkeletalMeshComponent* Body = GetMesh();
	bMatchBodyConfigured = true;
	MatchAppearance->InitializeAppearance(Body);
	Body->SetVisibility(true);
	Body->SetOwnerNoSee(true);
	// Rifle locomotion driven by replicated movement: bots, LAN players and replays all step properly.
	Body->SetAnimInstanceClass(UCrosshairLocomotionAnimInstance::StaticClass());
	// Like practice targets: posed physics bodies take shots (head/body), the capsule only blocks movement.
	Body->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
	Body->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Body->SetCollisionResponseToAllChannels(ECR_Ignore);
	Body->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
	GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Visibility, ECR_Ignore);
}
bool ACrosshairCharacter::IsBot() const
{
	const APlayerState* State = GetPlayerState();
	return (State && State->IsABot()) || (GetController() && GetController()->IsA<AAIController>());
}
bool ACrosshairCharacter::IsInMatch() const { return ACrosshairMatchState::Get(this) != nullptr; }
bool ACrosshairCharacter::UsesWorldPresentation() const
{
	// In a match replay only the recording player keeps the first-person view.
	if (IsReplayPlayback()) return IsInMatch() && !bReplayFocus;
	return IsInMatch() && (IsBot() || !IsLocallyControlled());
}
float ACrosshairCharacter::GetRecentSpin(double Window) const
{
	const double Now = GetWorld()->GetTimeSeconds();
	float Total = 0;
	for (const TPair<double, float>& Step : YawHistory) if (Now - Step.Key <= Window) Total += Step.Value;
	return Total;
}
void ACrosshairCharacter::RecordMatchKill(const ACrosshairCharacter* Victim, bool bHeadshot, bool bWallbang, const FString& Weapon, bool bScopedWeapon, bool bLethal)
{
	// A joined player's kill is labelled on their machine, which knows their spin, jump and aim.
	if (IsRemotePlayer()) { ClientMatchKill(Victim, bHeadshot, bWallbang, Weapon, bScopedWeapon, bLethal); return; }
	if (!IsInMatch() || IsBot() || !IsLocallyControlled() || IsReplayPlayback()) return;
	CrosshairTrickshotRules::FKillContext Kill;
	Kill.Weapon = Weapon; Kill.bHeadshot = bHeadshot; Kill.bWallbang = bWallbang; Kill.bScopedWeapon = bScopedWeapon; Kill.bLethal = bLethal;
	Kill.bAirborne = GetCharacterMovement()->IsFalling();
	Kill.bAiming = AimAlpha >= .5f;
	Kill.SpinDegrees = GetRecentSpin();
	// On a LAN client the victim may already be gone by the time the kill message arrives.
	Kill.DistanceMeters = Victim ? FVector::Dist(GetActorLocation(), Victim->GetActorLocation()) / 100.f : 0.f;
	const FString Label = CrosshairTrickshotRules::Describe(Kill);
	if (CrosshairTrickshotRules::IsTrickshot(Kill)) { TrickshotBanner = Label; TrickshotUntil = GetWorld()->GetTimeSeconds() + 2.5f; }
	UE_LOG(LogTemp, Display, TEXT("Crosshair match kill: %s"), *Label);
	GetGameInstance()->GetSubsystem<UCrosshairReplaySubsystem>()->SaveMatchKill(Label);
}
void ACrosshairCharacter::HandleDeath()
{
	StopActions();
	if (HasAuthority()) for (ACrosshairWeapon* Weapon : Inventory->Weapons) if (Weapon) { Weapon->SetEquipped(false); Weapon->bHolstered = false; }
	for (ACrosshairWeapon* Weapon : Inventory->Weapons) if (Weapon) Weapon->SetActorHiddenInGame(true);
	GetCharacterMovement()->StopMovementImmediately();
	GetCharacterMovement()->DisableMovement();
	GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	GetFirstPersonMesh()->SetVisibility(false);
	for (UStaticMeshComponent* Gear : CombatAppearance->Gear) Gear->SetVisibility(false);
	USkeletalMeshComponent* Body = GetMesh();
	if (Body->GetPhysicsAsset())
	{
		Body->SetCollisionProfileName(TEXT("Ragdoll"));
		Body->SetAllBodiesSimulatePhysics(true);
		Body->SetSimulatePhysics(true);
		Body->WakeAllRigidBodies();
	}
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
	DOREPLIFETIME_CONDITION(ACrosshairCharacter, bReplayFocus, COND_ReplayOnly);
}
bool ACrosshairCharacter::IsReplayPlayback() const
{
	return GetWorld() && GetWorld()->GetDemoNetDriver() && GetWorld()->GetDemoNetDriver()->IsPlaying();
}
bool ACrosshairCharacter::CanAct() const
{
	if (Health->bDead) return false;
	if (const auto* Match = ACrosshairMatchState::Get(this); Match && Match->bMatchOver) return false;
	if (IsBot()) return !IsReplayPlayback();
	const ACrosshairPlayerController* PC = Cast<ACrosshairPlayerController>(GetController());
	return !GetGameInstance()->GetSubsystem<UCrosshairStartup>()->IsPreparing() && !IsReplayPlayback() && PC && !PC->bMenuOpen && !Attempt->bSucceeded;
}
void ACrosshairCharacter::SelectWeaponSlot(int32 Slot)
{
	if (!CanAct() || Lethals->IsBusy()) return;
	bAimHeld=false;
	// LAN client: the host owns the inventory; the switch replicates back.
	if (!HasAuthority()) { if (IsLocallyControlled()) ServerSelectWeaponSlot(Slot); return; }
	if (Slot<0) Inventory->Cycle(); else Inventory->EquipSlot(Slot);
}
void ACrosshairCharacter::ControllerSprintPressed() { if (CanAct()) { bControllerSprint=!bControllerSprint; bSprintHeld=bControllerSprint; } }
void ACrosshairCharacter::Notify(const FString& Text) { Notice = Text; NoticeUntil = GetWorld()->GetTimeSeconds() + 4; }
void ACrosshairCharacter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (Health->bDead) return;
	if (bMatchBodyConfigured)
	{
		// Your own soldier body and gear (helmet, pouches, pads) never block your first-person view.
		// Live play hides them from the owner; a replay viewer is not the owner, so hide them there too.
		const bool bOwnReplayView = IsReplayPlayback() && !UsesWorldPresentation();
		GetMesh()->bCastHiddenShadow = true;
		GetMesh()->SetHiddenInGame(bOwnReplayView);
		for (UStaticMeshComponent* Part : MatchAppearance->Gear)
			if (Part) { Part->SetOwnerNoSee(true); Part->bCastHiddenShadow = true; Part->SetHiddenInGame(bOwnReplayView); }
	}
 if (bControllerSprint && (GetLastMovementInputVector().IsNearlyZero() || bIsCrouched)) { bControllerSprint=false; bSprintHeld=false; }
	UCameraComponent* Camera = GetFirstPersonCameraComponent();
	ACrosshairWeapon* Weapon = Inventory->GetCurrent();
	if (IsReplayPlayback())
	{
		// Replay actors can begin play before the match game state arrives.
		if (!bMatchBodyConfigured && IsInMatch()) ConfigureMatchBody();
		Camera->bUsePawnControlRotation = false;
		Camera->SetWorldLocationAndRotation(RecordedView.Location, RecordedView.Rotation);
		Camera->SetFieldOfView(RecordedView.FOV);
		AimAlpha = RecordedView.AimAlpha;
		if (RecordedView.HitSequence != PresentedHit) { PresentedHit = RecordedView.HitSequence; bLastHitHeadshot = RecordedView.bHeadshot; bLastHitWallbang = RecordedView.bWallbang;
	HitMarkerUntil = GetWorld()->GetTimeSeconds() + (bLastHitHeadshot ? 0.4f : 0.25f); }
	}
	else
	{
		// LAN: the host simulates this pawn too, so it needs the same sprint/aim intent to compute the same speed.
		if (IsLocallyControlled() && !HasAuthority() && (bSprintHeld != bSentSprint || bAimHeld != bSentAim))
		{ bSentSprint = bSprintHeld; bSentAim = bAimHeld; ServerSetMoveIntent(bSprintHeld, bAimHeld); }
		const FCrosshairSettings& Settings = GetGameInstance()->GetSubsystem<UCrosshairReplaySubsystem>()->GetSettings();
		const float AimSeconds = Weapon && Weapon->Definition ? Weapon->Definition->AimSeconds : 0.2f;
		const bool bWantsAim = bAimHeld && CanAct() && !Placement->bPlacing && !Lethals->IsBusy() && Weapon && !Weapon->bReloading && !Weapon->IsSwitching();
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
		if (IsLocallyControlled() && !IsBot())
		{
			// Yaw history for spin (360) detection on kills.
			const float Yaw = GetControlRotation().Yaw;
			const double Now = GetWorld()->GetTimeSeconds();
			if (bHasLastYaw) YawHistory.Add({Now, FMath::Abs(FRotator::NormalizeAxis(Yaw - LastYaw))});
			LastYaw = Yaw; bHasLastYaw = true;
			YawHistory.RemoveAll([Now](const TPair<double, float>& Step) { return Now - Step.Key > 2.0; });
		}
		if (GetActorLocation().Z < -1500)
		{
			if (!IsInMatch()) Attempt->ResetAttempt();
			else if (HasAuthority()) Health->Kill(TEXT("Fell"));
		}
	}
	Weapon=Inventory->GetPresentationWeapon();
	if (UsesWorldPresentation())
	{
		// The weapon rides the soldier body (see ACrosshairWeapon::UpdateCarry); no viewmodel.
		GetFirstPersonMesh()->SetVisibility(false);
		for (UStaticMeshComponent* Gear:CombatAppearance->Gear) Gear->SetVisibility(false);
		if (Weapon) Weapon->UpdateCarry();
	}
	else if (Weapon)
	{
		Weapon->UpdateCarry();
		// Attachment to a camera component is reconstructed explicitly for replay actors as well.
		if (Weapon->GetRootComponent()->GetAttachParent() != Camera) Weapon->AttachToComponent(Camera, FAttachmentTransformRules::SnapToTargetNotIncludingScale);
		Weapon->UpdatePresentation(AimAlpha);
		GetFirstPersonMesh()->SetRelativeTransform(ArmsFromGrip * Weapon->Mesh->GetRelativeTransform());
		GetFirstPersonMesh()->SetVisibility(!(Weapon->Definition && Weapon->Definition->AimStyle == ECrosshairAimStyle::Scope && AimAlpha >= .95f && !Weapon->IsSwitching()));
        for (UStaticMeshComponent* Gear:CombatAppearance->Gear) Gear->SetVisibility(GetFirstPersonMesh()->IsVisible());
		if (Weapon->bReloading != bArmsReloading)
		{
			bArmsReloading = Weapon->bReloading;
			if (auto* View = Cast<UCrosshairViewmodelAnimInstance>(GetFirstPersonMesh()->GetAnimInstance())) View->Play(bArmsReloading ? ReloadAnimation : IdleAnimation, !bArmsReloading);
			else GetFirstPersonMesh()->PlayAnimation(bArmsReloading ? ReloadAnimation : IdleAnimation, !bArmsReloading);
		}
		// Left hand on the fore-end (viewmodel IK); free during reloads, switches and lethal throws.
		if (auto* View = Cast<UCrosshairViewmodelAnimInstance>(GetFirstPersonMesh()->GetAnimInstance()))
		{
			const UStaticMesh* Model = Weapon->Definition ? Weapon->Definition->PresentationMesh.Get() : nullptr;
			View->bLeftHandIK = Model && !Weapon->bReloading && !Weapon->IsSwitching() && !Lethals->IsHolding() && GetFirstPersonMesh()->IsVisible();
			if (View->bLeftHandIK)
			{
				FVector Grip = Weapon->Definition->LeftHandGrip;
				if (Grip.IsNearlyZero())
				{
					// Default: palm under the fore-end, a third of the way up the barrel (model X forward, grip at origin).
					const FBox Bounds = Model->GetBoundingBox();
					Grip = FVector(Bounds.Max.X * .3f, 0.f, Bounds.Min.Z - 1.5f);
				}
				const FTransform& Arms = GetFirstPersonMesh()->GetComponentTransform();
				const FVector Goal = Weapon->PresentationMesh->GetComponentTransform().TransformPosition(Grip);
				View->LeftHandTarget = Arms.InverseTransformPosition(Goal);
				View->LeftElbowHint = Arms.InverseTransformPosition(Goal - Camera->GetUpVector() * 25.f - Camera->GetRightVector() * 25.f - Camera->GetForwardVector() * 15.f);
			}
		}
	}
}
void ACrosshairCharacter::UpdateArmsPresentation()
{
	auto* Weapon = Inventory->GetPresentationWeapon();
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
	if (!CanAct() || Traversal->bMantling) return;
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
void ACrosshairCharacter::JumpPressed() { if (CanAct()) Traversal->RequestJump(); }
void ACrosshairCharacter::JumpReleased() { Traversal->ReleaseJump(); }
void ACrosshairCharacter::SprintPressed() { if (CanAct()) { bControllerSprint=false; bSprintHeld = true; } }
void ACrosshairCharacter::SprintReleased() { bSprintHeld = false; bControllerSprint=false; }
void ACrosshairCharacter::CrouchPressed() { if (CanAct()) { if (bIsCrouched) UnCrouch(); else Crouch(); } }
void ACrosshairCharacter::FirePressed()
{
	if (!CanAct() || Lethals->IsBusy()) return;
	bSprintHeld = false; bControllerSprint=false;
	if (!HasAuthority()) { ServerFire(true, GetControlRotation()); return; }   // LAN client: the host fires and decides the hit
	if (Placement->bPlacing) Placement->Confirm(); else if (auto* W = Inventory->GetCurrent()) W->StartFire();
}
void ACrosshairCharacter::FireReleased()
{
	if (!HasAuthority()) { if (IsLocallyControlled()) ServerFire(false, GetControlRotation()); return; }
	if (auto* W = Inventory->GetCurrent()) W->StopFire();
}
void ACrosshairCharacter::AimPressed() { if (!CanAct() || Lethals->IsBusy()) return; if (Placement->bPlacing) Placement->Toggle(); else { bAimHeld = true; bSprintHeld = false; bControllerSprint=false; } }
void ACrosshairCharacter::AimReleased() { bAimHeld = false; }
void ACrosshairCharacter::ReloadPressed()
{
	if (!CanAct() || Lethals->IsBusy()) return;
	if (!HasAuthority()) { ServerReload(); return; }
	if (Placement->bPlacing) Placement->Rotate(); else if (auto* W = Inventory->GetCurrent()) W->Reload();
}
void ACrosshairCharacter::SwitchPressed() { SelectWeaponSlot(-1); }
// Target editing and attempt save/reset are practice tools; matches ignore them.
void ACrosshairCharacter::PlacementPressed() { if (!IsInMatch()) Placement->Toggle(); }
void ACrosshairCharacter::RotatePressed() { if (!IsInMatch()) Placement->Rotate(); }
void ACrosshairCharacter::RemovePressed() { if (!IsInMatch()) Placement->RemoveAimedTarget(); }
void ACrosshairCharacter::ClearPressed() { if (!IsInMatch()) Placement->ClearTargets(); }
void ACrosshairCharacter::SavePressed() { if (!IsInMatch()) Attempt->SaveStart(); }
void ACrosshairCharacter::ResetPressed() { if (!IsReplayPlayback() && !IsInMatch()) Attempt->ResetAttempt(); }
void ACrosshairCharacter::MenuPressed() { if (auto* PC = Cast<ACrosshairPlayerController>(GetController())) PC->ToggleMenu(); }
void ACrosshairCharacter::ApplyRecoil(float Degrees)
{
	if (!GetController()) return;
	FRotator Rotation = GetControlRotation();
	Rotation.Pitch = FMath::Clamp(FRotator::NormalizeAxis(Rotation.Pitch) + Degrees, -85.f, 85.f);
	GetController()->SetControlRotation(Rotation);
}
void ACrosshairCharacter::TargetHit(ACrosshairDummy* Target, bool bHeadshot, bool bWallbang)
{
	if (IsRemotePlayer()) ClientTargetHit(bHeadshot, bWallbang);
	RecordedView.bHeadshot = bHeadshot; RecordedView.bWallbang = bWallbang;
	++RecordedView.HitSequence;
	PresentedHit = RecordedView.HitSequence;
	bLastHitHeadshot = bHeadshot; bLastHitWallbang = bWallbang;
	HitMarkerUntil = GetWorld()->GetTimeSeconds() + (bHeadshot ? 0.4f : 0.25f);
	if (Target && Target->bHit) Attempt->HandleTargetHit(Target);
}
void ACrosshairCharacter::StopActions()
{
	Lethals->Cancel(); Traversal->CancelMantle(); FireReleased(); StopJumping();
	// A joined player's sprint/aim intent is owned by their machine (ServerSetMoveIntent sends only changes);
	// clearing it here would leave the host simulating them at walk speed.
	if (!IsRemotePlayer()) { bAimHeld = false; bSprintHeld = false; bControllerSprint=false; }
}

bool ACrosshairCharacter::CanJumpWhileFalling() const { return Traversal && Traversal->AllowsCoyoteJump(); }
void ACrosshairCharacter::ServerSetMoveIntent_Implementation(bool bSprint, bool bAim)
{
	bSprintHeld = bSprint;
	bAimHeld = bAim;
}
// The host re-checks every request against its own copy of this soldier (alive, not mid-throw, match running).
void ACrosshairCharacter::ServerFire_Implementation(bool bPressed, FRotator AimRotation)
{
	if (!bPressed) { FireReleased(); return; }
	if (!CanAct() || Lethals->IsBusy()) return;
	// Fire along the view the player had when they pulled the trigger, not the last movement update.
	if (AController* Brain = GetController()) Brain->SetControlRotation(FRotator(FMath::ClampAngle(FRotator::NormalizeAxis(AimRotation.Pitch), -85.f, 85.f), AimRotation.Yaw, 0));
	bSprintHeld = false; bControllerSprint = false;
	if (auto* W = Inventory->GetCurrent()) W->StartFire();
}
void ACrosshairCharacter::ServerReload_Implementation() { ReloadPressed(); }
void ACrosshairCharacter::ServerSelectWeaponSlot_Implementation(int32 Slot) { SelectWeaponSlot(FMath::Clamp(Slot, -1, 1)); }
void ACrosshairCharacter::ServerLethal_Implementation(uint8 Action)
{
	if (Action == 0) Lethals->Prepare();
	else if (Action == 1) Lethals->Throw();
	else Lethals->Cycle();
}
void ACrosshairCharacter::ServerApplyLoadout_Implementation(int32 Primary, int32 Secondary, ECrosshairLethalType Lethal)
{
	const int32 Count = Inventory->Weapons.Num();
	if (Count < 2 || !IsInMatch()) return;
	Primary = FMath::Clamp(Primary, 0, Count - 1);
	Secondary = FMath::Clamp(Secondary, 0, Count - 1);
	if (Secondary == Primary) Secondary = (Primary + 1) % Count;
	Inventory->ApplyLoadout(Primary, Secondary, Lethal == ECrosshairLethalType::Tomahawk ? Lethal : ECrosshairLethalType::Frag);
}
void ACrosshairCharacter::ClientTargetHit_Implementation(bool bHeadshot, bool bWallbang)
{
	bLastHitHeadshot = bHeadshot; bLastHitWallbang = bWallbang;
	HitMarkerUntil = GetWorld()->GetTimeSeconds() + (bHeadshot ? 0.4f : 0.25f);
}
void ACrosshairCharacter::ClientMatchKill_Implementation(const ACrosshairCharacter* Victim, bool bHeadshot, bool bWallbang, const FString& Weapon, bool bScopedWeapon, bool bLethal)
{ RecordMatchKill(Victim, bHeadshot, bWallbang, Weapon, bScopedWeapon, bLethal); }
