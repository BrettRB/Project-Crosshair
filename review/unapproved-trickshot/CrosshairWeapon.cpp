#include "CrosshairWeapon.h"
#include "CrosshairCharacter.h"
#include "CrosshairData.h"
#include "CrosshairDummy.h"
#include "Camera/CameraComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/DemoNetDriver.h"
#include "Kismet/GameplayStatics.h"
#include "Net/UnrealNetwork.h"
#include "DrawDebugHelpers.h"

ACrosshairWeapon::ACrosshairWeapon()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;
	bAlwaysRelevant = true;
	NetUpdateFrequency = 60;
	Mesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("Weapon mesh"));
	SetRootComponent(Mesh);
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Mesh->SetCastShadow(false);
}
void ACrosshairWeapon::BeginPlay() { Super::BeginPlay(); OnRep_Definition(); }
void ACrosshairWeapon::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& Out) const
{
	Super::GetLifetimeReplicatedProps(Out);
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
	Ammo = Definition ? Definition->MagazineSize : 0;
	NextShotAt = GetWorld()->GetTimeSeconds();
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
	ReloadEndsAt = ReloadStartedAt + Definition->ReloadSeconds;
}
void ACrosshairWeapon::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	Kick = FMath::FInterpTo(Kick, 0, DeltaSeconds, 14);
	SetActorHiddenInGame(!bEquipped);
	if (!HasAuthority() || (GetWorld()->GetDemoNetDriver() && GetWorld()->GetDemoNetDriver()->IsPlaying())) return;
	if (bReloading && GetWorld()->GetTimeSeconds() >= ReloadEndsAt)
	{
		Ammo = Definition ? Definition->MagazineSize : 0;
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
		if (UGameplayStatics::ApplyPointDamage(Dummy, 1, Direction, Hit, Player->GetController(), this, nullptr) > 0) Player->TargetHit(Dummy);
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
		const float Phase = FMath::Clamp((GetWorld()->GetTimeSeconds() - ReloadStartedAt) / Definition->ReloadSeconds, 0.f, 1.f);
		Offset.Z -= FMath::Sin(Phase * PI) * 20;
		Rotation.Roll += FMath::Sin(Phase * PI) * 30;
	}
	Mesh->SetRelativeLocationAndRotation(Offset, Rotation);
}

UCrosshairInventoryComponent::UCrosshairInventoryComponent() { SetIsReplicatedByDefault(true); }
void UCrosshairInventoryComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& Out) const
{
	Super::GetLifetimeReplicatedProps(Out);
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
