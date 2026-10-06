#pragma once
#include "CoreMinimal.h"
#include "Engine/EngineTypes.h"
#include "CollisionQueryParams.h"
class UCrosshairWeaponDefinition;
/** Trace the remainder of a bullet without disabling or ignoring solid cover. */
namespace CrosshairBallistics
{
    FHitResult ContinueShot(UWorld* World, FHitResult Hit, FVector Direction, FVector End,
        const FCollisionQueryParams& Params, const UCrosshairWeaponDefinition* Definition,
        bool bAllowPenetration, float& DamageScale, int32& Layers, bool bPreview = false);
}
