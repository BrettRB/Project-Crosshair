#include "CrosshairBallistics.h"
#include "CrosshairData.h"
#include "CrosshairDummy.h"
#include "CrosshairWindow.h"
#include "Components/PrimitiveComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/World.h"
#include "Engine/DamageEvents.h"

FHitResult CrosshairBallistics::ContinueShot(UWorld* World,FHitResult Hit,FVector Direction,FVector End,
    const FCollisionQueryParams& InputParams,const UCrosshairWeaponDefinition* Definition,
    bool bAllowPenetration,float& DamageScale,int32& Layers,bool bPreview)
{
    DamageScale=1; Layers=0;
    FCollisionQueryParams Params=InputParams;
    float Remaining=FMath::Max(0.f,Definition->PenetrationDepth);
    // Bound panes and walls independently; glass retains its existing full damage.
    for (int32 Step=0; Hit.bBlockingHit && Step<12; ++Step)
    {
        auto* Actor=Hit.GetActor(); auto* Component=Hit.GetComponent();
        if (auto* Glass=Cast<ACrosshairWindow>(Actor))
        {
            if (bPreview) Params.AddIgnoredComponent(Component);
            else if (Glass->TakeDamage(1,FDamageEvent(),nullptr,nullptr)<=0) return Hit;
            World->LineTraceSingleByChannel(Hit,Hit.ImpactPoint+Direction*.1f,End,ECC_Visibility,Params);
            continue;
        }
        if (!bAllowPenetration || !Component || Cast<ACrosshairDummy>(Actor)
            || (Actor && Actor->ActorHasTag(TEXT("NoWallbang"))) || Component->ComponentHasTag(TEXT("NoWallbang"))
            || Layers>=FMath::Clamp(Definition->PenetrationLayers,0,4) || Remaining<=0
            || FMath::Abs(Hit.ImpactNormal.Z)>.7f) return Hit;
        // Reverse trace starts beyond the allowed thickness. It must find an
        // outward-facing exit, so starting inside thick cover cannot fake an exit.
        const FVector Entry=Hit.ImpactPoint;
        FHitResult Exit;
        if (!Component->LineTraceComponent(Exit,Entry+Direction*(Remaining+.5f),Entry-Direction*.5f,Params)
            || FVector::DotProduct(Exit.ImpactNormal,Direction)<.1f) return Hit;
        const float Thickness=FVector::DotProduct(Exit.ImpactPoint-Entry,Direction);
        if (Thickness<.1f || Thickness>Remaining) return Hit;
        Remaining-=Thickness; ++Layers;
        DamageScale*=FMath::Clamp(Definition->PenetrationDamageScale,0.f,1.f);
        // Other cover inside this thickness still blocks: do not jump across it.
        FCollisionQueryParams Interior=Params; Interior.AddIgnoredComponent(Component);
        FHitResult Nested;
        if (World->LineTraceSingleByChannel(Nested,Entry+Direction*.1f,Exit.ImpactPoint,ECC_Visibility,Interior)) return Nested;
        World->LineTraceSingleByChannel(Hit,Exit.ImpactPoint+Direction*.5f,End,ECC_Visibility,Params);
    }
    return Hit;
}
