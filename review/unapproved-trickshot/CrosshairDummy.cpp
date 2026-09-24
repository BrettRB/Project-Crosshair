#include "CrosshairDummy.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Engine/DemoNetDriver.h"
#include "Net/UnrealNetwork.h"
#include "UObject/ConstructorHelpers.h"

ACrosshairDummy::ACrosshairDummy()
{
	bReplicates = true;
	bAlwaysRelevant = true;
	SetReplicateMovement(true);
	Collision = CreateDefaultSubobject<UCapsuleComponent>(TEXT("Target collision"));
	SetRootComponent(Collision);
	Collision->InitCapsuleSize(28.f, 90.f);
	Collision->SetCollisionProfileName(TEXT("BlockAllDynamic"));
	Body = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Body"));
	Body->SetupAttachment(Collision);
	Body->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Body->SetRelativeScale3D(FVector(0.45f, 0.45f, 1.25f));
	Body->SetRelativeLocation(FVector(0, 0, -15));
	Head = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Head"));
	Head->SetupAttachment(Collision);
	Head->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Head->SetRelativeScale3D(FVector(0.42f));
	Head->SetRelativeLocation(FVector(0, 0, 66));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cylinder(TEXT("/Engine/BasicShapes/Cylinder"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Sphere(TEXT("/Engine/BasicShapes/Sphere"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> Material(TEXT("/Game/Crosshair/Targets/M_Target"));
	Body->SetStaticMesh(Cylinder.Object);
	Head->SetStaticMesh(Sphere.Object);
	if (Material.Succeeded()) { Body->SetMaterial(0, Material.Object); Head->SetMaterial(0, Material.Object); }
}

void ACrosshairDummy::BeginPlay() { Super::BeginPlay(); OnRep_Hit(); }
void ACrosshairDummy::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& Out) const
{
	Super::GetLifetimeReplicatedProps(Out);
	DOREPLIFETIME(ACrosshairDummy, bHit);
}
float ACrosshairDummy::TakeDamage(float Amount, const FDamageEvent& Event, AController* Instigator, AActor* Causer)
{
	if (!HasAuthority() || bHit || Amount <= 0 || (GetWorld()->GetDemoNetDriver() && GetWorld()->GetDemoNetDriver()->IsPlaying())) return 0;
	bHit = true;
	OnRep_Hit();
	ForceNetUpdate();
	return Amount;
}
void ACrosshairDummy::ResetTarget() { bHit = false; OnRep_Hit(); ForceNetUpdate(); }
void ACrosshairDummy::OnRep_Hit()
{
	const FLinearColor Color = bHit ? FLinearColor(1.f, 0.15f, 0.04f) : FLinearColor(0.05f, 0.7f, 0.85f);
	for (UStaticMeshComponent* Part : {Body.Get(), Head.Get()})
	{
		if (UMaterialInstanceDynamic* Material = Part->CreateAndSetMaterialInstanceDynamic(0)) Material->SetVectorParameterValue(TEXT("Color"), Color);
	}
}
