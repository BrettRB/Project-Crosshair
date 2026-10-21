#pragma once
#include "CoreMinimal.h"

/** Labels for match kills saved to Replays. Pure rules so automation tests can cover them. */
namespace CrosshairTrickshotRules
{
 struct FKillContext
 {
  FString Weapon;
  bool bHeadshot=false;
  bool bWallbang=false;
  bool bAirborne=false;
  /** The weapon has a scope (sniper); not aiming it is a no-scope. */
  bool bScopedWeapon=false;
  bool bAiming=false;
  /** Frag or tomahawk kill. */
  bool bLethal=false;
  /** Yaw turned in the moments before the kill (absolute degrees). */
  float SpinDegrees=0.f;
  float DistanceMeters=0.f;
 };
 constexpr float LongShotMeters=40.f;
 constexpr float LongLethalMeters=15.f;

 inline TArray<FString> Tricks(const FKillContext& K)
 {
  TArray<FString> Result;
  if (K.bAirborne) Result.Add(TEXT("Mid-air"));
  if (K.SpinDegrees>=720.f) Result.Add(TEXT("720"));
  else if (K.SpinDegrees>=360.f) Result.Add(TEXT("360"));
  if (!K.bLethal && !K.bAiming) Result.Add(K.bScopedWeapon ? TEXT("No-scope") : TEXT("Hip-fire"));
  if (K.bWallbang) Result.Add(TEXT("Wallbang"));
  if (K.bHeadshot) Result.Add(TEXT("Headshot"));
  if (K.DistanceMeters>=(K.bLethal ? LongLethalMeters : LongShotMeters)) Result.Add(TEXT("Long shot"));
  return Result;
 }
 /** True for kills worth a TRICKSHOT banner. Every match kill is still saved. */
 inline bool IsTrickshot(const FKillContext& K)
 {
  return K.bAirborne || K.SpinDegrees>=360.f || K.bWallbang || (!K.bLethal && K.bScopedWeapon && !K.bAiming)
   || K.DistanceMeters>=(K.bLethal ? LongLethalMeters : LongShotMeters);
 }
 /** e.g. "Mid-air 360 No-scope Headshot - Bolt-Action Sniper, 42 m". Never contains ": " (menu rows split on it). */
 inline FString Describe(const FKillContext& K)
 {
  const TArray<FString> Found=Tricks(K);
  return FString::Printf(TEXT("%s - %s, %.0f m"),Found.IsEmpty() ? TEXT("Kill") : *FString::Join(Found,TEXT(" ")),K.Weapon.IsEmpty() ? TEXT("Weapon") : *K.Weapon,K.DistanceMeters);
 }
}
