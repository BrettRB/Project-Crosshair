#include "CrosshairWeapon.h"
#include "CrosshairCharacter.h"
#include "CrosshairData.h"
#include "CrosshairDummy.h"
#include "Camera/CameraComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "UObject/ConstructorHelpers.h"
#include "Engine/DemoNetDriver.h"
#include "Kismet/GameplayStatics.h"
#include "Net/UnrealNetwork.h"
#include "DrawDebugHelpers.h"
#include "Materials/MaterialInstanceDynamic.h"

ACrosshairWeapon::ACrosshairWeapon()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;
	bAlwaysRelevant = true;
	SetNetUpdateFrequency(60);
	Mesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("Weapon mesh"));
	SetRootComponent(Mesh);
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Mesh->SetCastShadow(false);
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cylinder(TEXT("/Engine/BasicShapes/Cylinder"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Sphere(TEXT("/Engine/BasicShapes/Sphere"));
	for (int32 i = 0; i < 19; ++i)
	{
		UStaticMeshComponent* Detail = CreateDefaultSubobject<UStaticMeshComponent>(*FString::Printf(TEXT("SightDetail%d"), i));
		Detail->SetupAttachment(Mesh);
		Detail->SetStaticMesh(i == 16 ? Sphere.Object : (i < 2 || i == 11 || i == 12 || i >= 17 ? Cylinder.Object : Cube.Object));
		Detail->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Detail->SetCastShadow(false);
		Details.Add(Detail);
	}
}
void ACrosshairWeapon::BeginPlay()
{
	Super::BeginPlay();
	OnRep_Definition();
	// Reuse the local color-parameter material for dark metal prototype sights.
	if (UMaterialInterface* Material = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Crosshair/Targets/M_Target.M_Target")))
		for (int32 i = 0; i < Details.Num(); ++i)
		{
			UStaticMeshComponent* Detail = Details[i];
			Detail->SetMaterial(0, Material);
			if (UMaterialInstanceDynamic* Metal = Detail->CreateAndSetMaterialInstanceDynamic(0))
				Metal->SetVectorParameterValue(TEXT("Color"), i >= 6 && i <= 8 ? FLinearColor(.055f,.075f,.045f) : (i == 18 ? FLinearColor(.025f,.10f,.16f) : FLinearColor(.025f,.03f,.04f)));
		}
}
void ACrosshairWeapon::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ACrosshairWeapon, Definition);
	DOREPLIFETIME(ACrosshairWeapon, Ammo);
	DOREPLIFETIME(ACrosshairWeapon, bReloading);
	DOREPLIFETIME(ACrosshairWeapon, bEquipped);
	DOREPLIFETIME(ACrosshairWeapon, ReloadStartedAt);
	DOREPLIFETIME(ACrosshairWeapon, LastImpact);
	DOREPLIFETIME(ACrosshairWeapon, ShotSequence);
}
void ACrosshairWeapon::Initialize(UCrosshairWeaponDefinition* InDefinition)
{
	Definition = InDefinition;
	OnRep_Definition();
	ResetWeapon();
}
void ACrosshairWeapon::OnRep_Definition()
{
	if (Definition) Mesh->SetSkeletalMesh(Definition->Mesh);
}
void ACrosshairWeapon::SetEquipped(bool Equipped)
{
	bEquipped = Equipped;
	StopFire();
	// Cancelling a reload never transfers ammunition; only a finished reload does.
	bReloading = false;
	if (Equipped && Definition) NextShotAt = FMath::Max(NextShotAt, GetWorld()->GetTimeSeconds() + Definition->EquipSeconds);
	SetActorHiddenInGame(!Equipped);
}
void ACrosshairWeapon::ResetWeapon()
{
	StopFire();
	bReloading = false;
	Ammo = Definition ? FMath::Max(1, Definition->MagazineSize) : 0;
	NextShotAt = GetWorld()->GetTimeSeconds();
	Kick = 0;
}
void ACrosshairWeapon::StartFire()
{
	bTriggerHeld = true;
	TryFire();
}
void ACrosshairWeapon::StopFire() { bTriggerHeld = false; }
void ACrosshairWeapon::Reload()
{
	if (!Definition || bReloading || !bEquipped || Ammo >= Definition->MagazineSize) return;
	StopFire();
	bReloading = true;
	ReloadStartedAt = GetWorld()->GetTimeSeconds();
	ReloadEndsAt = ReloadStartedAt + FMath::Max(0.1f, Definition->ReloadSeconds);
}
void ACrosshairWeapon::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	Kick = FMath::FInterpTo(Kick, 0, DeltaSeconds, 14);
	SetActorHiddenInGame(!bEquipped);
	if (!HasAuthority() || (GetWorld()->GetDemoNetDriver() && GetWorld()->GetDemoNetDriver()->IsPlaying())) return;
	if (bReloading && GetWorld()->GetTimeSeconds() >= ReloadEndsAt)
	{
		Ammo = Definition ? FMath::Max(1, Definition->MagazineSize) : 0;
		bReloading = false;
	}
	if (bTriggerHeld && Definition && Definition->bAutomatic) TryFire();
}
void ACrosshairWeapon::TryFire()
{
	ACrosshairCharacter* Player = Cast<ACrosshairCharacter>(GetOwner());
	if (!HasAuthority() || !Player || !Definition || !bEquipped || !Player->CanAct() || Player->IsReplayPlayback()) return;
	const double Now = GetWorld()->GetTimeSeconds();
	if (!CrosshairRules::CanFire(Ammo, bReloading, Now, NextShotAt)) return;
	--Ammo;
	NextShotAt = Now + FMath::Max(0.03f, Definition->ShotInterval);
	const UCameraComponent* Camera = Player->GetFirstPersonCameraComponent();
	const float Spread = FMath::Lerp(Definition->HipSpreadDegrees, Definition->AimSpreadDegrees, Player->GetAimAlpha());
	const FVector Direction = FMath::VRandCone(Camera->GetForwardVector(), FMath::DegreesToRadians(Spread));
	const FVector Start = Camera->GetComponentLocation();
	FCollisionQueryParams Params(SCENE_QUERY_STAT(CrosshairShot), true, Player);
	Params.AddIgnoredActor(this);
	FHitResult CameraHit;
	const FVector TraceEnd = Start + Direction * Definition->Range;
	GetWorld()->LineTraceSingleByChannel(CameraHit, Start, TraceEnd, ECC_Visibility, Params);
	const FVector AimPoint = CameraHit.bBlockingHit ? CameraHit.ImpactPoint : TraceEnd;
	// Trace from the visible gun as well: the camera must not shoot around an obstructed barrel.
	const FVector Muzzle = Start + Camera->GetForwardVector() * 40.f + Camera->GetRightVector() * 10.f - Camera->GetUpVector() * 10.f;
	FHitResult MuzzleHit;
	FHitResult NearHit;
	const bool bNearBlocked = GetWorld()->LineTraceSingleByChannel(NearHit, Start, Muzzle, ECC_Visibility, Params);
	const bool bMuzzleBlocked = GetWorld()->LineTraceSingleByChannel(MuzzleHit, Muzzle, AimPoint, ECC_Visibility, Params);
	const FHitResult& Hit = bNearBlocked ? NearHit : (bMuzzleBlocked ? MuzzleHit : CameraHit);
	LastImpact = Hit.bBlockingHit ? Hit.ImpactPoint : AimPoint;
	++ShotSequence;
	OnRep_Shot();
	Player->ApplyRecoil(Definition->RecoilDegrees);
	if (ACrosshairDummy* Dummy = Cast<ACrosshairDummy>(Hit.GetActor()))
	{
		if (UGameplayStatics::ApplyPointDamage(Dummy, Dummy->IsHeadImpact(Hit.ImpactPoint) ? Definition->HeadDamage : Definition->BodyDamage, Direction, Hit, Player->GetController(), this, nullptr) > 0) Player->TargetHit(Dummy);
	}
	ForceNetUpdate();
}
void ACrosshairWeapon::OnRep_Shot()
{
	if (ShotSequence <= PresentedShot) { PresentedShot = ShotSequence; return; }
	PresentedShot = ShotSequence;
	Kick = 5;
	if (Definition && Definition->FireSound) UGameplayStatics::PlaySoundAtLocation(this, Definition->FireSound, GetActorLocation(), 0.35f);
	DrawDebugPoint(GetWorld(), LastImpact, 10.f, FColor(255, 190, 80), false, 0.15f);
}
void ACrosshairWeapon::UpdatePresentation(float AimAlpha)
{
	if (!Definition) return;
	FVector Offset = FMath::Lerp(Definition->HipOffset, Definition->AimOffset, AimAlpha);
	Offset.X -= Kick;
	FRotator Rotation = Definition->MeshRotation;
	if (bReloading)
	{
		const float Phase = FMath::Clamp((GetWorld()->GetTimeSeconds() - ReloadStartedAt) / FMath::Max(0.1f, Definition->ReloadSeconds), 0.f, 1.f);
		Offset.Z -= FMath::Sin(Phase * PI) * 20;
		Rotation.Roll += FMath::Sin(Phase * PI) * 30;
	}
	Mesh->SetRelativeLocationAndRotation(Offset, Rotation);
	const bool Scoped = Definition->AimStyle == ECrosshairAimStyle::Scope;
	const bool ScopeView = Scoped && AimAlpha >= .95f;
	// The scoped prototype has its own bolt-action silhouette; automatic weapons retain their mesh.
	Mesh->SetVisibility(!Scoped);
	// Dimensions below are in centimetres before conversion to the template mesh's local axes.
	const FVector SniperPositions[] = {
		FVector(-2,0,9), FVector(54,0,1),
		FVector::ZeroVector, FVector::ZeroVector, FVector::ZeroVector,
		FVector(0,0,0), FVector(23,0,-2), FVector(-21,0,-3), FVector(-33,0,-3),
		FVector(-9,0,-8), FVector(5,0,-8), FVector(12,0,9), FVector(-16,0,9),
		FVector(-10,0,6), FVector(6,0,6), FVector(-5,4,-1), FVector(-5,7,-1),
		FVector(89,0,1), FVector(15.2,0,9)};
	const FVector Scales[] = {
		FVector(.03,.03,.24), FVector(.019,.019,.68),
		FVector(.015,.008,.03), FVector(.015,.008,.03), FVector(.015,.008,.03),
		FVector(.28,.045,.055), FVector(.26,.05,.045), FVector(.24,.055,.085), FVector(.018,.065,.105),
		FVector(.05,.04,.12), FVector(.08,.035,.10), FVector(.052,.052,.06), FVector(.04,.04,.05),
		FVector(.025,.055,.05), FVector(.025,.055,.05), FVector(.02,.055,.012), FVector(.025),
		FVector(.026,.026,.045), FVector(.048,.048,.006)};
	const FVector IronPositions[] = {FVector(55,-.65,-1.5), FVector(55,.65,-1.5), FVector(95,0,-1.5)};
	for (int32 i = 0; i < Details.Num(); ++i)
	{
		const bool IronSight = i >= 2 && i <= 4;
		const bool Visible = bEquipped && !ScopeView && (IronSight ? !Scoped && AimAlpha >= .95f && !bReloading : Scoped);
		Details[i]->SetVisibility(Visible);
		const FVector LocalOffset = IronSight ? IronPositions[i-2] - Offset : SniperPositions[i];
		Details[i]->SetRelativeLocation(Definition->MeshRotation.UnrotateVector(LocalOffset));
		const bool AlongBarrel = i < 2 || i == 11 || i == 12 || i >= 17;
		const FQuat DetailRotation = AlongBarrel ? FRotator(90,0,0).Quaternion() : (i == 9 ? FRotator(-15,0,0).Quaternion() : FQuat::Identity);
		Details[i]->SetRelativeRotation(Definition->MeshRotation.Quaternion().Inverse() * DetailRotation);
		Details[i]->SetRelativeScale3D(Scales[i]);
	}
}

UCrosshairInventoryComponent::UCrosshairInventoryComponent() { SetIsReplicatedByDefault(true); }
void UCrosshairInventoryComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UCrosshairInventoryComponent, Weapons);
	DOREPLIFETIME(UCrosshairInventoryComponent, ActiveIndex);
}
void UCrosshairInventoryComponent::BeginPlay()
{
	Super::BeginPlay();
	ACrosshairCharacter* Player = Cast<ACrosshairCharacter>(GetOwner());
	if (!Player || !GetOwner()->HasAuthority() || Player->IsReplayPlayback()) return;
	for (UCrosshairWeaponDefinition* Definition : Loadout)
	{
		if (!Definition) continue;
		FActorSpawnParameters Params;
		Params.Owner = Player;
		Params.Instigator = Player;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		ACrosshairWeapon* Weapon = GetWorld()->SpawnActor<ACrosshairWeapon>(ACrosshairWeapon::StaticClass(), Params);
		if (!Weapon) { UE_LOG(LogTemp, Error, TEXT("Unable to spawn loadout weapon")); continue; }
		Weapon->Initialize(Definition);
		Weapon->AttachToComponent(Player->GetFirstPersonCameraComponent(), FAttachmentTransformRules::SnapToTargetNotIncludingScale);
		Weapon->SetEquipped(false);
		Weapons.Add(Weapon);
	}
	Equip(0);
}
ACrosshairWeapon* UCrosshairInventoryComponent::GetCurrent() const { return Weapons.IsValidIndex(ActiveIndex) ? Weapons[ActiveIndex].Get() : nullptr; }
void UCrosshairInventoryComponent::Equip(int32 Index)
{
	if (!Weapons.IsValidIndex(Index)) return;
	if (ACrosshairWeapon* Previous = GetCurrent()) Previous->SetEquipped(false);
	ActiveIndex = Index;
	GetCurrent()->SetEquipped(true);
}
void UCrosshairInventoryComponent::Cycle() { if (!Weapons.IsEmpty()) Equip((ActiveIndex + 1) % Weapons.Num()); }
void UCrosshairInventoryComponent::ResetWeapons() { for (ACrosshairWeapon* Weapon : Weapons) if (Weapon) Weapon->ResetWeapon(); }
