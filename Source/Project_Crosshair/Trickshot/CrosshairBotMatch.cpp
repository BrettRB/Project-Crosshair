#include "CrosshairBotMatch.h"
#include "CrosshairReplaySubsystem.h"
#include "Engine/GameInstance.h"
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
{ return Mode==ECrosshairMatchMode::TeamDeathmatch ? TEXT("Team Deathmatch") : TEXT("Free-for-all"); }
FString UCrosshairBotMatch::MapName(ECrosshairMatchMap Map) { return Map==ECrosshairMatchMap::Grid ? TEXT("Grid") : TEXT("Nuketown"); }
FName UCrosshairBotMatch::MapPackage(ECrosshairMatchMap Map)
{ return Map==ECrosshairMatchMap::Grid ? FName(TEXT("/Game/Crosshair/Maps/L_Grid")) : FName(TEXT("/Game/Crosshair/Maps/L_Nuketown")); }
void UCrosshairBotMatch::Validate(FCrosshairBotMatchOptions& O)
{
 if (uint8(O.Difficulty)>uint8(ECrosshairBotDifficulty::Veteran)) O.Difficulty=ECrosshairBotDifficulty::Regular;
 O.BotCount=FMath::Clamp(O.BotCount,1,11); O.ScoreLimit=FMath::Clamp(O.ScoreLimit,5,100); O.TimeLimitMinutes=FMath::Clamp(O.TimeLimitMinutes,1,30);
 if (uint8(O.Mode)>uint8(ECrosshairMatchMode::TeamDeathmatch)) O.Mode=ECrosshairMatchMode::FreeForAll;
 if (uint8(O.Map)>uint8(ECrosshairMatchMap::Grid)) O.Map=ECrosshairMatchMap::Nuketown;
 O.TeamSize=FMath::Clamp(O.TeamSize,1,6); O.TeamScoreLimit=FMath::Clamp(O.TeamScoreLimit,10,200);
}
void UCrosshairBotMatch::SetDifficulty(ECrosshairBotDifficulty Difficulty)
{
 auto* Replay=GetGameInstance()->GetSubsystem<UCrosshairReplaySubsystem>();
 if (Replay->IsPlayback() || Replay->IsFinishing()) return;
 auto& O=Replay->GetSettings().BotMatch; const auto Previous=O.Difficulty; O.Difficulty=Difficulty; Validate(O); Replay->SaveSettings();
 if (Previous!=O.Difficulty) OnDifficultyChanged.Broadcast(O.Difficulty);
}
FCrosshairBotTuning UCrosshairBotMatch::GetCurrentTuning() const
{ return TuningFor(GetGameInstance()->GetSubsystem<UCrosshairReplaySubsystem>()->GetSettings().BotMatch.Difficulty); }
