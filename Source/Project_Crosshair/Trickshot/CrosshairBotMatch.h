#pragma once
#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "CrosshairBotMatch.generated.h"

UENUM(BlueprintType)
enum class ECrosshairBotDifficulty : uint8 { Easy, Regular, Hardened, Veteran };
/** Kill Confirmed is a team mode: kills drop dog tags, and a team scores only by collecting enemy tags. */
UENUM(BlueprintType)
enum class ECrosshairMatchMode : uint8 { FreeForAll, TeamDeathmatch, KillConfirmed };
/** Matches run only on the COD maps; the Testing map and local imports are practice-only. Random picks one at match start. */
UENUM(BlueprintType)
enum class ECrosshairMatchMap : uint8 { Nuketown, Grid, SpaceStation, Random };

/** Shared tuning for future AI controllers. No difficulty grants wall vision. */
USTRUCT(BlueprintType)
struct FCrosshairBotTuning
{
 GENERATED_BODY()
 UPROPERTY(EditAnywhere,BlueprintReadOnly) float ReactionSeconds=.55f;
 UPROPERTY(EditAnywhere,BlueprintReadOnly) float AimErrorDegrees=4.f;
 UPROPERTY(EditAnywhere,BlueprintReadOnly) float TrackingDegreesPerSecond=100.f;
 UPROPERTY(EditAnywhere,BlueprintReadOnly) float DecisionSeconds=.3f;
 UPROPERTY(EditAnywhere,BlueprintReadOnly) float MovementSpeed=450.f;
};

USTRUCT(BlueprintType)
struct FCrosshairBotMatchOptions
{
 GENERATED_BODY()
 UPROPERTY(EditAnywhere,BlueprintReadWrite) ECrosshairBotDifficulty Difficulty=ECrosshairBotDifficulty::Regular;
 /** Free-for-all opponents; the COD default lobby is eight players. */
 UPROPERTY(EditAnywhere,BlueprintReadWrite) int32 BotCount=7;
 /** Free-for-all kill limit. */
 UPROPERTY(EditAnywhere,BlueprintReadWrite) int32 ScoreLimit=30;
 UPROPERTY(EditAnywhere,BlueprintReadWrite) int32 TimeLimitMinutes=10;
 UPROPERTY(EditAnywhere,BlueprintReadWrite) ECrosshairMatchMode Mode=ECrosshairMatchMode::FreeForAll;
 UPROPERTY(EditAnywhere,BlueprintReadWrite) ECrosshairMatchMap Map=ECrosshairMatchMap::Nuketown;
 /** Players per team, including the local player; COD default 6v6. */
 UPROPERTY(EditAnywhere,BlueprintReadWrite) int32 TeamSize=6;
 UPROPERTY(EditAnywhere,BlueprintReadWrite) int32 TeamScoreLimit=75;
 /** Kill Confirmed team score (collected enemy tags) needed to win; COD default 65. */
 UPROPERTY(EditAnywhere,BlueprintReadWrite) int32 ConfirmScoreLimit=65;
 /** Fill the lobby with bots (always on offline; a LAN host can turn them off). */
 UPROPERTY(EditAnywhere,BlueprintReadWrite) bool bBots=true;
};

/** LAN host setup: the match rules plus who may join. */
USTRUCT(BlueprintType)
struct FCrosshairLanOptions
{
 GENERATED_BODY()
 UPROPERTY(EditAnywhere,BlueprintReadWrite) FCrosshairBotMatchOptions Match;
 /** Human players allowed, including the host. */
 UPROPERTY(EditAnywhere,BlueprintReadWrite) int32 MaxPlayers=8;
};
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FCrosshairBotDifficultyChanged,ECrosshairBotDifficulty,Difficulty);

/** Persistent match configuration and difficulty presets shared by match bots. */
UCLASS()
class PROJECT_CROSSHAIR_API UCrosshairBotMatch : public UGameInstanceSubsystem
{
 GENERATED_BODY()
public:
 UFUNCTION(BlueprintPure) static FCrosshairBotTuning TuningFor(ECrosshairBotDifficulty Difficulty);
 static FString DifficultyName(ECrosshairBotDifficulty Difficulty);
 static FString ModeName(ECrosshairMatchMode Mode);
 /** Team Deathmatch and Kill Confirmed play Alpha vs Bravo. */
 static bool IsTeamMode(ECrosshairMatchMode Mode) { return Mode==ECrosshairMatchMode::TeamDeathmatch || Mode==ECrosshairMatchMode::KillConfirmed; }
 /** Menu order: Free-for-all, Team Deathmatch, Kill Confirmed. */
 static ECrosshairMatchMode NextMode(ECrosshairMatchMode Mode,int32 Direction);
 /** The score limit the mode plays to: kills (FFA), team kills (TDM) or confirmed tags (KC). */
 static int32& ScoreLimitFor(FCrosshairBotMatchOptions& Options);
 static int32 ScoreLimitFor(const FCrosshairBotMatchOptions& Options) { return ScoreLimitFor(const_cast<FCrosshairBotMatchOptions&>(Options)); }
 /** "First team to 65 confirmed kills" and similar, for menus and the match HUD. */
 static FString ScoreGoal(ECrosshairMatchMode Mode,int32 Limit);
 static FString MapName(ECrosshairMatchMap Map);
 static FName MapPackage(ECrosshairMatchMap Map);
 /** Menu order: Nuketown, Grid, Space Station, Random. */
 static ECrosshairMatchMap NextMap(ECrosshairMatchMap Map,int32 Direction);
 /** A playable map: Random becomes Nuketown, Grid or Space Station (Roll in [0,1) picks which). */
 static ECrosshairMatchMap ResolveMap(ECrosshairMatchMap Map,float Roll);
 static ECrosshairMatchMap ResolveMap(ECrosshairMatchMap Map) { return ResolveMap(Map,FMath::FRand()); }
 static void Validate(FCrosshairBotMatchOptions& Options);
 static void Validate(FCrosshairLanOptions& Options);
 /** Username shown in matches: letters, digits, space, - _ . only, 16 characters, never empty. */
 static FString CleanPlayerName(const FString& Name);
 /** Callsigns a controller player can cycle through instead of typing. */
 static const TArray<FString>& Callsigns();
 UFUNCTION(BlueprintCallable) void SetDifficulty(ECrosshairBotDifficulty Difficulty);
 UFUNCTION(BlueprintPure) FCrosshairBotTuning GetCurrentTuning() const;
 UPROPERTY(BlueprintAssignable) FCrosshairBotDifficultyChanged OnDifficultyChanged;
};
