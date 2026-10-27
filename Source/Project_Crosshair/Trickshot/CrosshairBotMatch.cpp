#include "CrosshairBotMatch.h"
#include "CrosshairReplaySubsystem.h"
#include "Engine/GameInstance.h"
#include "CrosshairMatch.h"
#include "Engine/World.h"
FCrosshairBotTuning UCrosshairBotMatch::TuningFor(ECrosshairBotDifficulty Difficulty)
{
 FCrosshairBotTuning T;
 switch(Difficulty)
 {
 case ECrosshairBotDifficulty::Easy: T.ReactionSeconds=.9f; T.AimErrorDegrees=7.f; T.TrackingDegreesPerSecond=65; T.DecisionSeconds=.5f; T.MovementSpeed=380; break;
 case ECrosshairBotDifficulty::Hardened: T.ReactionSeconds=.3f; T.AimErrorDegrees=2.f; T.TrackingDegreesPerSecond=160; T.DecisionSeconds=.2f; T.MovementSpeed=500; break;
 case ECrosshairBotDifficulty::Veteran: T.ReactionSeconds=.18f; T.AimErrorDegrees=.9f; T.TrackingDegreesPerSecond=220; T.DecisionSeconds=.12f; T.MovementSpeed=550; break;
 default: break;
 }
 return T;
}
FString UCrosshairBotMatch::DifficultyName(ECrosshairBotDifficulty Difficulty)
{
 switch(Difficulty) { case ECrosshairBotDifficulty::Easy:return TEXT("Easy"); case ECrosshairBotDifficulty::Hardened:return TEXT("Hardened"); case ECrosshairBotDifficulty::Veteran:return TEXT("Veteran"); default:return TEXT("Regular"); }
}
FString UCrosshairBotMatch::ModeName(ECrosshairMatchMode Mode)
{ return Mode==ECrosshairMatchMode::KillConfirmed ? TEXT("Kill Confirmed") : Mode==ECrosshairMatchMode::TeamDeathmatch ? TEXT("Team Deathmatch") : TEXT("Free-for-all"); }
ECrosshairMatchMode UCrosshairBotMatch::NextMode(ECrosshairMatchMode Mode,int32 Direction)
{ return ECrosshairMatchMode((uint8(Mode)+(Direction<0 ? 2 : 1))%3); }
FString UCrosshairBotMatch::ScoreGoal(ECrosshairMatchMode Mode,int32 Limit)
{
 if (Mode==ECrosshairMatchMode::KillConfirmed) return FString::Printf(TEXT("First team to %d confirmed kills"),Limit);
 if (Mode==ECrosshairMatchMode::TeamDeathmatch) return FString::Printf(TEXT("First team to %d kills"),Limit);
 return FString::Printf(TEXT("First to %d kills"),Limit);
}
int32& UCrosshairBotMatch::ScoreLimitFor(FCrosshairBotMatchOptions& O)
{ return O.Mode==ECrosshairMatchMode::KillConfirmed ? O.ConfirmScoreLimit : O.Mode==ECrosshairMatchMode::TeamDeathmatch ? O.TeamScoreLimit : O.ScoreLimit; }
FString UCrosshairBotMatch::MapName(ECrosshairMatchMap Map) { return Map==ECrosshairMatchMap::Grid ? TEXT("Grid") : Map==ECrosshairMatchMap::SpaceStation ? TEXT("Space Station") : TEXT("Nuketown"); }
FName UCrosshairBotMatch::MapPackage(ECrosshairMatchMap Map)
{ return Map==ECrosshairMatchMap::Grid ? FName(TEXT("/Game/Crosshair/Maps/L_Grid")) : Map==ECrosshairMatchMap::SpaceStation ? FName(TEXT("/Game/Crosshair/Maps/L_SpaceStation")) : FName(TEXT("/Game/Crosshair/Maps/L_Nuketown")); }
void UCrosshairBotMatch::Validate(FCrosshairBotMatchOptions& O)
{
 if (uint8(O.Difficulty)>uint8(ECrosshairBotDifficulty::Veteran)) O.Difficulty=ECrosshairBotDifficulty::Regular;
 O.BotCount=FMath::Clamp(O.BotCount,1,11); O.ScoreLimit=FMath::Clamp(O.ScoreLimit,5,100); O.TimeLimitMinutes=FMath::Clamp(O.TimeLimitMinutes,1,30);
 if (uint8(O.Mode)>uint8(ECrosshairMatchMode::KillConfirmed)) O.Mode=ECrosshairMatchMode::FreeForAll;
 if (uint8(O.Map)>uint8(ECrosshairMatchMap::SpaceStation)) O.Map=ECrosshairMatchMap::Nuketown;
 O.TeamSize=FMath::Clamp(O.TeamSize,1,6); O.TeamScoreLimit=FMath::Clamp(O.TeamScoreLimit,10,200); O.ConfirmScoreLimit=FMath::Clamp(O.ConfirmScoreLimit,10,200);
}
void UCrosshairBotMatch::SetDifficulty(ECrosshairBotDifficulty Difficulty)
{
 auto* Replay=GetGameInstance()->GetSubsystem<UCrosshairReplaySubsystem>();
 if (Replay->IsPlayback() || Replay->IsFinishing()) return;
 auto& O=Replay->GetSettings().BotMatch; const auto Previous=O.Difficulty; O.Difficulty=Difficulty; Validate(O); Replay->SaveSettings();
 if (Previous!=O.Difficulty) OnDifficultyChanged.Broadcast(O.Difficulty);
}
FCrosshairBotTuning UCrosshairBotMatch::GetCurrentTuning() const
{
 // Read live from the saved setup the running match uses, so pause-menu changes apply mid-match.
 const FCrosshairSettings& Settings=GetGameInstance()->GetSubsystem<UCrosshairReplaySubsystem>()->GetSettings();
 const UWorld* World=GetGameInstance()->GetWorld();
 const auto* Mode=World ? World->GetAuthGameMode<ACrosshairMatchGameMode>() : nullptr;
 return TuningFor(Mode && Mode->IsLanMatch() ? Settings.Lan.Match.Difficulty : Settings.BotMatch.Difficulty);
}
void UCrosshairBotMatch::Validate(FCrosshairLanOptions& Options)
{
 Validate(Options.Match);
 Options.MaxPlayers=FMath::Clamp(Options.MaxPlayers,2,12);
}
FString UCrosshairBotMatch::CleanPlayerName(const FString& Name)
{
 FString Clean;
 for (const TCHAR C:Name) if (FChar::IsAlnum(C) || C==TCHAR(' ') || C==TCHAR('-') || C==TCHAR('_') || C==TCHAR('.')) Clean.AppendChar(C);
 Clean.TrimStartAndEndInline();
 Clean=Clean.Left(16).TrimEnd();
 return Clean.IsEmpty() ? FString(TEXT("Player")) : Clean;
}
const TArray<FString>& UCrosshairBotMatch::Callsigns()
{
 static const TArray<FString> Names={TEXT("Player"),TEXT("Maverick"),TEXT("Nova"),TEXT("Specter"),TEXT("Blaze"),TEXT("Atlas"),TEXT("Onyx"),TEXT("Jinx"),TEXT("Kestrel"),TEXT("Havoc"),TEXT("Quickscope"),TEXT("Trickshot")};
 return Names;
}
