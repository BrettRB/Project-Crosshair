#pragma once
#include "CoreMinimal.h"
#include "CrosshairGame.h"
#include "CrosshairBotMatch.h"
#include "Components/ActorComponent.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerState.h"
#include "CrosshairMatch.generated.h"

class UCapsuleComponent;
class UArrowComponent;
class ACrosshairCharacter;

/** Pure match rules; kept free of world state so automation tests can cover them. */
namespace CrosshairMatchRules
{
 constexpr uint8 NoTeam=0, Alpha=1, Bravo=2;
 struct FSpawnContext
 {
  float NearestEnemy=1e6f;
  int32 VisibleEnemies=0;
  float NearestFriend=1e6f;
  float SecondsSinceUsed=1e6f;
  bool bTeamMode=false;
 };
 /** COD-style spawn safety: away from and out of sight of enemies, near teammates in TDM. */
 inline float ScoreSpawn(const FSpawnContext& C)
 {
  float Score=FMath::Min(C.NearestEnemy,4000.f)/40.f;
  if (C.NearestEnemy<1000.f) Score-=200.f;
  Score-=250.f*C.VisibleEnemies;
  if (C.bTeamMode && C.NearestFriend<2500.f) Score+=40.f;
  if (C.SecondsSinceUsed<5.f) Score-=120.f;
  return Score;
 }
 /** Friendly fire is off in TDM; self-damage is always allowed. */
 inline bool CanDamage(bool bTeamMode,uint8 InstigatorTeam,uint8 VictimTeam,bool bSelf)
 { return bSelf || !bTeamMode || InstigatorTeam==NoTeam || InstigatorTeam!=VictimTeam; }
 inline bool ReachedLimit(int32 Score,int32 Limit) { return Limit>0 && Score>=Limit; }
 /** COD health regeneration: nothing until the delay passes, then a steady refill. */
 inline float RegenerateHealth(float Health,float MaxHealth,float SecondsSinceDamage,float Delta,float Delay=4.f,float Rate=40.f)
 { return SecondsSinceDamage<Delay ? Health : FMath::Min(MaxHealth,Health+Rate*Delta); }
 /** Bots needed so each team has TeamSize players; humans are already assigned. */
 inline int32 BotsForTeam(int32 TeamSize,int32 HumansOnTeam) { return FMath::Max(0,TeamSize-HumansOnTeam); }
}

UENUM(BlueprintType)
enum class ECrosshairSpawnTeam : uint8 { None, Alpha, Bravo };

/**
 * Designer-placed match spawn. Not an APlayerStart, so practice spawning is unaffected.
 * InitialTeam marks the TDM start area; FFA/TDM flags control which modes may respawn here.
 */
UCLASS(Blueprintable)
class PROJECT_CROSSHAIR_API ACrosshairSpawnPoint : public AActor
{
 GENERATED_BODY()
public:
 ACrosshairSpawnPoint();
 UPROPERTY(VisibleAnywhere) TObjectPtr<UCapsuleComponent> Capsule;
#if WITH_EDITORONLY_DATA
 UPROPERTY(VisibleAnywhere) TObjectPtr<UArrowComponent> Arrow;
#endif
 UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="Spawn") bool bFreeForAll=true;
 UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="Spawn") bool bTeamDeathmatch=true;
 UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="Spawn") ECrosshairSpawnTeam InitialTeam=ECrosshairSpawnTeam::None;
 /** Short area name shown in spawn diagnostics, e.g. "Yellow backyard". */
 UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="Spawn") FString Area;
 double LastUsedAt=-1000;
};

/** Match health with COD-style delayed regeneration. Only matches apply damage to players. */
UCLASS(ClassGroup=(Crosshair),meta=(BlueprintSpawnableComponent))
class PROJECT_CROSSHAIR_API UCrosshairHealthComponent : public UActorComponent
{
 GENERATED_BODY()
public:
 UCrosshairHealthComponent();
 virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
 virtual void TickComponent(float Delta,ELevelTick Type,FActorComponentTickFunction* Function) override;
 /** Returns damage applied; zero when blocked by team rules, death or match end. */
 float ApplyDamage(float Amount,AController* Instigator,AActor* Causer,bool bHeadshot,const FString& Weapon);
 /** Environmental death such as falling out of the map. */
 void Kill(const FString& Reason);
 UPROPERTY(EditAnywhere,BlueprintReadOnly,meta=(ClampMin="1")) float MaxHealth=100.f;
 UPROPERTY(EditAnywhere,BlueprintReadOnly) float RegenDelay=4.f;
 UPROPERTY(EditAnywhere,BlueprintReadOnly) float RegenRate=40.f;
 UPROPERTY(Replicated,BlueprintReadOnly) float Health=100.f;
 UPROPERTY(ReplicatedUsing=OnRep_Dead,BlueprintReadOnly) bool bDead=false;
 UPROPERTY(Replicated,BlueprintReadOnly) FString KilledBy;
 double LastDamageAt=-1000;
 /** Server-side record of who last hurt this player and total damage taken (diagnostics/tests). */
 TWeakObjectPtr<AController> LastAttacker;
 float TotalDamageTaken=0;
private:
 UFUNCTION() void OnRep_Dead();
};

USTRUCT(BlueprintType)
struct FCrosshairKillEntry
{
 GENERATED_BODY()
 UPROPERTY(BlueprintReadOnly) FString Killer;
 UPROPERTY(BlueprintReadOnly) FString Victim;
 UPROPERTY(BlueprintReadOnly) FString Weapon;
 UPROPERTY(BlueprintReadOnly) uint8 KillerTeam=0;
 UPROPERTY(BlueprintReadOnly) uint8 VictimTeam=0;
 UPROPERTY(BlueprintReadOnly) bool bHeadshot=false;
 UPROPERTY(BlueprintReadOnly) float ServerTime=0;
};

UCLASS()
class PROJECT_CROSSHAIR_API ACrosshairMatchPlayerState : public APlayerState
{
 GENERATED_BODY()
public:
 virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
 UPROPERTY(Replicated,BlueprintReadOnly) int32 Kills=0;
 UPROPERTY(Replicated,BlueprintReadOnly) int32 Deaths=0;
 UPROPERTY(Replicated,BlueprintReadOnly) uint8 Team=CrosshairMatchRules::NoTeam;
};

/** Replicated scoreboard, clock and kill feed. Its presence marks a match world. */
UCLASS()
class PROJECT_CROSSHAIR_API ACrosshairMatchState : public AGameStateBase
{
 GENERATED_BODY()
public:
 virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
 static ACrosshairMatchState* Get(const UObject* WorldContext);
 bool IsTeamMode() const { return Mode==ECrosshairMatchMode::TeamDeathmatch; }
 float GetRemainingSeconds() const;
 int32 GetTeamScore(uint8 Team) const { return Team==CrosshairMatchRules::Alpha ? AlphaScore : Team==CrosshairMatchRules::Bravo ? BravoScore : 0; }
 static FString TeamName(uint8 Team);
 /** Highest kills first; ties keep join order. */
 TArray<ACrosshairMatchPlayerState*> GetStandings() const;
 UPROPERTY(Replicated,BlueprintReadOnly) ECrosshairMatchMode Mode=ECrosshairMatchMode::FreeForAll;
 UPROPERTY(Replicated,BlueprintReadOnly) int32 ScoreLimit=30;
 UPROPERTY(Replicated,BlueprintReadOnly) float MatchEndsAt=0;
 UPROPERTY(Replicated,BlueprintReadOnly) int32 AlphaScore=0;
 UPROPERTY(Replicated,BlueprintReadOnly) int32 BravoScore=0;
 UPROPERTY(Replicated,BlueprintReadOnly) bool bMatchOver=false;
 UPROPERTY(Replicated,BlueprintReadOnly) FString Result;
 UPROPERTY(Replicated,BlueprintReadOnly) FString MapName;
 UPROPERTY(Replicated,BlueprintReadOnly) TArray<FCrosshairKillEntry> KillFeed;
};

/** Offline/LAN FFA and TDM rules for the COD maps. The host is authoritative. */
UCLASS()
class PROJECT_CROSSHAIR_API ACrosshairMatchGameMode : public ACrosshairGameMode
{
 GENERATED_BODY()
public:
 ACrosshairMatchGameMode();
 virtual void InitGame(const FString& MapName,const FString& Options,FString& ErrorMessage) override;
 virtual void InitGameState() override;
 virtual void StartPlay() override;
 virtual void Tick(float DeltaSeconds) override;
 /** Teams are assigned here because login asks for a start spot before HandleStartingNewPlayer. */
 virtual FString InitNewPlayer(APlayerController* NewPlayerController,const FUniqueNetIdRepl& UniqueId,const FString& Options,const FString& Portal) override;
 virtual void RestartPlayerAtPlayerStart(AController* NewPlayer,AActor* StartSpot) override;
 virtual AActor* ChoosePlayerStart_Implementation(AController* Player) override;
 virtual bool ShouldSpawnAtStartSpot(AController* Player) override { return false; }
 static bool IsMatchMap(const FString& MapName);
 bool CanDamage(AController* Attacker,AController* Victim) const;
 void NotifyKilled(AController* Killer,AController* Victim,bool bHeadshot,const FString& Weapon);
 void EndMatch(const FString& Result);
 const FCrosshairBotMatchOptions& GetOptions() const { return MatchOptions; }
 uint8 TeamOf(const AController* Controller) const;
 const TArray<TObjectPtr<ACrosshairSpawnPoint>>& GetSpawnPoints() const { return SpawnPoints; }
 /** The spawn point most recently chosen for a controller (diagnostics and tests). */
 ACrosshairSpawnPoint* GetLastSpawn(const AController* Controller) const { return LastSpawn.FindRef(Controller).Get(); }
 /** Seconds between death and respawn. */
 UPROPERTY(EditDefaultsOnly) float RespawnDelay=3.f;
 /** Results screen time before returning to Home. */
 UPROPERTY(EditDefaultsOnly) float ResultsSeconds=20.f;
 bool bInvalidMap=false;
private:
 void AssignTeam(AController* Controller);
 void AddBot(uint8 Team,int32 Number);
 void Respawn(AController* Controller);
 void CollectSpawnPoints();
 FCrosshairBotMatchOptions MatchOptions;
 UPROPERTY() TArray<TObjectPtr<ACrosshairSpawnPoint>> SpawnPoints;
 TSet<TWeakObjectPtr<AController>> HasSpawned;
 TMap<TWeakObjectPtr<const AController>,TWeakObjectPtr<ACrosshairSpawnPoint>> LastSpawn;
};
