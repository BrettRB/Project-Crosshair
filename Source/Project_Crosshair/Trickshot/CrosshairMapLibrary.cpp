#include "CrosshairMapLibrary.h"
#include "CrosshairGame.h"
#include "CrosshairReplaySubsystem.h"
#include "ProceduralMeshComponent.h"
#include "Engine/GameInstance.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/Parse.h"
#include "Misc/CommandLine.h"
#include "HAL/FileManager.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Dom/JsonObject.h"
#include "Net/UnrealNetwork.h"
#include "ImageUtils.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Widgets/SWindow.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Text/STextBlock.h"
#include "Framework/Application/SlateApplication.h"
namespace
{
 bool RelativeFile(const FString& Folder,const FString& Name,FString& Out)
 {
  if (Name.IsEmpty() || !FPaths::IsRelative(Name) || Name.Contains(TEXT("..")) || Name.Contains(TEXT(":"))) return false;
  Out=FPaths::ConvertRelativePathToFull(Folder/Name); FPaths::NormalizeFilename(Out);
  FString Base=FPaths::ConvertRelativePathToFull(Folder); FPaths::NormalizeFilename(Base);
  return Out.StartsWith(Base+TEXT("/")) && IFileManager::Get().FileExists(*Out);
 }
 bool LoadJson(const FString& File,TSharedPtr<FJsonObject>& Json)
 {
  if (IFileManager::Get().FileSize(*File)>65536) return false;
  FString Text; return FFileHelper::LoadFileToString(Text,*File) && FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text),Json) && Json.IsValid();
 }
 bool ReadMeta(const FString& File,FCrosshairLocalMap& Map)
 {
  TSharedPtr<FJsonObject> Json; if (!LoadJson(File,Json)) return false;
  if (!Json->TryGetStringField(TEXT("name"),Map.Name) || !Json->TryGetStringField(TEXT("mesh"),Map.Mesh)) return false;
  double Scale=1,Yaw=0; Json->TryGetNumberField(TEXT("scale"),Scale); Json->TryGetNumberField(TEXT("yaw"),Yaw);
  if (!FMath::IsFinite(Scale) || Scale<.001 || Scale>1000 || !FMath::IsFinite(Yaw)) return false;
  Map.Scale=Scale; Map.Yaw=Yaw; Json->TryGetStringField(TEXT("texture"),Map.Texture);
  const TArray<TSharedPtr<FJsonValue>>* Spawn=nullptr;
  if (Json->TryGetArrayField(TEXT("spawn"),Spawn))
  {
   if (Spawn->Num()!=3) return false;
   double X,Y,Z; if (!(*Spawn)[0]->TryGetNumber(X) || !(*Spawn)[1]->TryGetNumber(Y) || !(*Spawn)[2]->TryGetNumber(Z)) return false;
   Map.Spawn=FVector(X,Y,Z); if (Map.Spawn.ContainsNaN() || Map.Spawn.GetAbsMax()>1000000 || Z<-1000) return false;
  }
  Map.Folder=FPaths::GetPath(File); return !Map.Name.IsEmpty() && Map.Name.Len()<=80;
 }
}
FString UCrosshairMapLibrary::RootDirectory() const
{
 return FPaths::ProjectPersistentDownloadDir()/(FParse::Param(FCommandLine::Get(),TEXT("CrosshairSmoke")) ? TEXT("SmokeMaps") : TEXT("UserMaps"));
}
void UCrosshairMapLibrary::Initialize(FSubsystemCollectionBase& Collection) { Super::Initialize(Collection); Refresh(); }
void UCrosshairMapLibrary::Refresh()
{
 Maps.Empty(); TArray<FString> Folders; IFileManager::Get().FindFiles(Folders,*(RootDirectory()/TEXT("*")),false,true); Folders.Sort();
 for (const auto& Folder:Folders)
 {
  FGuid Guid; if (!FGuid::ParseExact(Folder,EGuidFormats::Digits,Guid)) continue;
  FCrosshairLocalMap Map; FString MeshPath;
  if (ReadMeta(RootDirectory()/Folder/TEXT("map.json"),Map) && RelativeFile(Map.Folder,Map.Mesh,MeshPath)) { Map.Id=Folder; Maps.Add(Map); }
 }
}
const FCrosshairLocalMap* UCrosshairMapLibrary::Find(const FString& Id) const { return Maps.FindByPredicate([&](const auto& Map){return Map.Id==Id;}); }
FString UCrosshairMapLibrary::MapName(int32 Index) const { return Index==0 ? TEXT("Testing Map") : Index==1 ? TEXT("Nuketown") : Maps.IsValidIndex(Index-2) ? Maps[Index-2].Name : TEXT("Unavailable"); }
bool UCrosshairMapLibrary::ParseObj(const FString& Text,float Scale,FCrosshairMapMesh& Out,FString& Error)
{
 Out=FCrosshairMapMesh(); TArray<FVector> Positions; TArray<FVector2D> UVs; TArray<FString> Lines; Text.ParseIntoArrayLines(Lines);
 auto Fail=[&](const TCHAR* Message){Error=Message; Out=FCrosshairMapMesh(); return false;};
 if (!FMath::IsFinite(Scale) || Scale<.001 || Scale>1000 || Text.Len()>32*1024*1024) return Fail(TEXT("Map exceeds size/scale limits."));
 for (FString Line:Lines)
 {
  const int32 Comment=Line.Find(TEXT("#")); if (Comment!=INDEX_NONE) Line=Line.Left(Comment);
  Line.TrimStartAndEndInline(); TArray<FString> Words; Line.ParseIntoArrayWS(Words);
  if (Words.IsEmpty() || Words[0].StartsWith(TEXT("#"))) continue;
  if (Words[0]==TEXT("v"))
  {
   double X,Y,Z;
   if (Words.Num()<4 || !LexTryParseString(X,*Words[1]) || !LexTryParseString(Y,*Words[2]) || !LexTryParseString(Z,*Words[3])) return Fail(TEXT("Invalid OBJ vertex."));
   FVector V(X*Scale,Y*Scale,Z*Scale);
   if (V.ContainsNaN() || V.GetAbsMax()>1000000 || Positions.Num()>=200000) return Fail(TEXT("Vertex outside supported map limits."));
   Positions.Add(V);
  }
  else if (Words[0]==TEXT("vt"))
  {
   double U,V; if (Words.Num()<3 || !LexTryParseString(U,*Words[1]) || !LexTryParseString(V,*Words[2]) || !FMath::IsFinite(U) || !FMath::IsFinite(V)) return Fail(TEXT("Invalid OBJ UV."));
   if (UVs.Num()>=600000) return Fail(TEXT("Too many UV coordinates.")); UVs.Add(FVector2D(U,1-V));
  }
  else if (Words[0]==TEXT("f"))
  {
   if (Words.Num()<4 || Words.Num()>9) return Fail(TEXT("Triangulate complex faces before export."));
   TArray<int32> Face,Tex;
   for (int32 i=1;i<Words.Num();++i)
   {
    TArray<FString> Parts; Words[i].ParseIntoArray(Parts,TEXT("/"),false); int32 Index=0,T=0;
    if (Parts.IsEmpty() || !LexTryParseString(Index,*Parts[0]) || Index==0) return Fail(TEXT("Invalid OBJ face index."));
    Index=Index>0 ? Index-1 : Positions.Num()+Index; if (!Positions.IsValidIndex(Index)) return Fail(TEXT("OBJ face references missing vertex."));
    if (Parts.Num()>1 && !Parts[1].IsEmpty()) { if (!LexTryParseString(T,*Parts[1]) || T==0) return Fail(TEXT("Invalid UV index.")); T=T>0 ? T-1 : UVs.Num()+T; if (!UVs.IsValidIndex(T)) return Fail(TEXT("OBJ face references missing UV.")); } else T=INDEX_NONE;
    Face.Add(Index); Tex.Add(T);
   }
   for (int32 i=1;i<Face.Num()-1;++i)
   {
    if (Out.Triangles.Num()/3>=200000) return Fail(TEXT("Map exceeds 200,000 triangles."));
    const FVector A=Positions[Face[0]],B=Positions[Face[i]],C=Positions[Face[i+1]];
    const FVector Normal=FVector::CrossProduct(B-A,C-A).GetSafeNormal(); if (Normal.IsNearlyZero()) continue;
    // Unreal procedural collision flips its geometric normals; OBJ faces are counter-clockwise.
    for (int32 Corner:{0,i+1,i}) { Out.Triangles.Add(Out.Vertices.Num()); Out.Vertices.Add(Positions[Face[Corner]]); Out.Normals.Add(Normal); Out.UV.Add(Tex[Corner]==INDEX_NONE ? FVector2D(Positions[Face[Corner]].X,Positions[Face[Corner]].Y)/100 : UVs[Tex[Corner]]); }
   }
  }
 }
 return !Out.Triangles.IsEmpty() || Fail(TEXT("OBJ contains no usable faces."));
}
bool UCrosshairMapLibrary::Import(const FString& Input,FString& Error)
{
 FString File=Input.TrimStartAndEnd(); File.TrimQuotesInline(); File=FPaths::ConvertRelativePathToFull(File);
 FCrosshairLocalMap Map; Map.Folder=FPaths::GetPath(File);
 if (FPaths::GetExtension(File).Equals(TEXT("json"),ESearchCase::IgnoreCase)) { if (!ReadMeta(File,Map)) { Error=TEXT("Invalid map.json: name, mesh, optional scale/spawn/yaw/texture required."); return false; } }
 else if (FPaths::GetExtension(File).Equals(TEXT("obj"),ESearchCase::IgnoreCase)) { Map.Mesh=FPaths::GetCleanFilename(File); Map.Name=FPaths::GetBaseFilename(File); }
 else { Error=TEXT("Select an OBJ or map.json package. Unreal assets and FBX require editor conversion."); return false; }
 FString MeshFile,TextureFile,Text; FCrosshairMapMesh Geometry;
 if (!RelativeFile(Map.Folder,Map.Mesh,MeshFile) || IFileManager::Get().FileSize(*MeshFile)>32*1024*1024 || !FFileHelper::LoadFileToString(Text,*MeshFile) || !ParseObj(Text,Map.Scale,Geometry,Error)) { if (Error.IsEmpty()) Error=TEXT("Cannot read local mesh."); return false; }
 if (!Map.Texture.IsEmpty())
 {
  if (!RelativeFile(Map.Folder,Map.Texture,TextureFile) || IFileManager::Get().FileSize(*TextureFile)>16*1024*1024 || !(FPaths::GetExtension(TextureFile).Equals(TEXT("png"),ESearchCase::IgnoreCase) || FPaths::GetExtension(TextureFile).Equals(TEXT("jpg"),ESearchCase::IgnoreCase))) { Error=TEXT("Texture must be a local PNG/JPG under 16 MB inside the map folder."); return false; }
 }
 if (FPaths::GetExtension(File).Equals(TEXT("obj"),ESearchCase::IgnoreCase)) { FBox Bounds(ForceInit); for (const auto& V:Geometry.Vertices) Bounds+=V; Map.Spawn=Bounds.GetCenter(); Map.Spawn.Z=Bounds.Max.Z+100; }
 Map.Id=FGuid::NewGuid().ToString(EGuidFormats::Digits); const FString Destination=RootDirectory()/Map.Id;
 if (!IFileManager::Get().MakeDirectory(*Destination,true) || IFileManager::Get().Copy(*(Destination/TEXT("map.obj")),*MeshFile)!=COPY_OK) { Error=TEXT("Unable to store imported map. Check disk space."); return false; }
 if (!TextureFile.IsEmpty() && IFileManager::Get().Copy(*(Destination/TEXT("texture.")+FPaths::GetExtension(TextureFile)),*TextureFile)!=COPY_OK) { Error=TEXT("Cannot copy map texture."); return false; }
 auto Json=MakeShared<FJsonObject>(); Json->SetStringField(TEXT("name"),Map.Name.Left(80)); Json->SetStringField(TEXT("mesh"),TEXT("map.obj")); Json->SetNumberField(TEXT("scale"),Map.Scale); Json->SetNumberField(TEXT("yaw"),Map.Yaw);
 Json->SetArrayField(TEXT("spawn"),{MakeShared<FJsonValueNumber>(Map.Spawn.X),MakeShared<FJsonValueNumber>(Map.Spawn.Y),MakeShared<FJsonValueNumber>(Map.Spawn.Z)});
 if (!TextureFile.IsEmpty()) Json->SetStringField(TEXT("texture"),TEXT("texture.")+FPaths::GetExtension(TextureFile));
 FString Metadata; FJsonSerializer::Serialize(Json,TJsonWriterFactory<>::Create(&Metadata));
 if (!FFileHelper::SaveStringToFile(Metadata,*(Destination/TEXT("map.json")))) { Error=TEXT("Could not save map metadata."); return false; }
 Refresh(); Error=TEXT("Imported ")+Map.Name+TEXT(". Select it in the Map row to play."); return true;
}
bool UCrosshairMapLibrary::PlayMap(int32 Index)
{
 auto* Replay=GetGameInstance()->GetSubsystem<UCrosshairReplaySubsystem>();
 if (Replay->IsPlayback() || Replay->IsFinishing() || Index<0 || Index>=MapCount()) return false;
 const FString Previous=ActiveMapId; ActiveMapId=Index>=2 ? Maps[Index-2].Id : FString(); bHomeVisited=true;
 if (!Replay->ChangePracticeMap(FName(Index==0 ? TEXT("/Game/Crosshair/Maps/L_Practice") : Index==1 ? TEXT("/Game/Crosshair/Maps/L_Nuketown") : TEXT("/Game/Crosshair/Maps/L_UserMap")))) { ActiveMapId=Previous; return false; }
 return true;
}
void UCrosshairMapLibrary::ShowImportDialog(ACrosshairPlayerController* Controller)
{
 if (!FSlateApplication::IsInitialized()) return;
 auto Window=SNew(SWindow).Title(FText::FromString(TEXT("Import local map"))).ClientSize(FVector2D(680,245)).SupportsMaximize(false).SupportsMinimize(false);
 TSharedPtr<SEditableTextBox> Path; TSharedPtr<STextBlock> Result; TWeakObjectPtr<UCrosshairMapLibrary> WeakThis=this;
 Window->SetContent(SNew(SVerticalBox)
 +SVerticalBox::Slot().AutoHeight().Padding(20)[SNew(STextBlock).Text(FText::FromString(TEXT("Paste the full path to an OBJ or map.json. Coordinates: X forward, Z up.")))]
 +SVerticalBox::Slot().AutoHeight().Padding(20,5)[SAssignNew(Path,SEditableTextBox).HintText(FText::FromString(TEXT("C:/Maps/MyMap/map.json")))]
 +SVerticalBox::Slot().AutoHeight().Padding(20,10)[SAssignNew(Result,STextBlock).AutoWrapText(true)]
 +SVerticalBox::Slot().AutoHeight().Padding(20,10)[SNew(SButton).Text(FText::FromString(TEXT("Import"))).OnClicked_Lambda([WeakThis,Path,Result](){ FString Message; if (WeakThis.IsValid()) WeakThis->Import(Path->GetText().ToString(),Message); Result->SetText(FText::FromString(Message)); return FReply::Handled(); })]);
 FSlateApplication::Get().AddWindow(Window); FSlateApplication::Get().SetKeyboardFocus(Path);
}
ACrosshairImportedMap::ACrosshairImportedMap()
{
 bReplicates=true; bAlwaysRelevant=true;
 Mesh=CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("Imported geometry")); SetRootComponent(Mesh);
 Mesh->bUseComplexAsSimpleCollision=true; Mesh->bUseAsyncCooking=false;
 Mesh->SetCollisionObjectType(ECC_WorldStatic); Mesh->SetCollisionResponseToAllChannels(ECR_Block);
}
void ACrosshairImportedMap::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const { Super::GetLifetimeReplicatedProps(OutLifetimeProps); DOREPLIFETIME(ACrosshairImportedMap,MapId); }
void ACrosshairImportedMap::BeginPlay() { Super::BeginPlay(); OnRep_MapId(); }
void ACrosshairImportedMap::OnRep_MapId()
{
 auto* Library=GetGameInstance()->GetSubsystem<UCrosshairMapLibrary>(); const auto* Map=Library->Find(MapId); if (!Map) return;
 FString File,Text,Error; FCrosshairMapMesh Geometry;
 if (!RelativeFile(Map->Folder,Map->Mesh,File) || !FFileHelper::LoadFileToString(Text,*File) || !UCrosshairMapLibrary::ParseObj(Text,Map->Scale,Geometry,Error)) return;
 Mesh->CreateMeshSection_LinearColor(0,Geometry.Vertices,Geometry.Triangles,Geometry.Normals,Geometry.UV,TArray<FLinearColor>(),TArray<FProcMeshTangent>(),true);
 if (!Map->Texture.IsEmpty() && RelativeFile(Map->Folder,Map->Texture,File))
 {
  auto* Texture=FImageUtils::ImportFileAsTexture2D(File); auto* Material=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/Crosshair/Maps/M_ImportedMap"));
  if (Texture && Material) { auto* Instance=UMaterialInstanceDynamic::Create(Material,this); Instance->SetTextureParameterValue(TEXT("Diffuse"),Texture); Mesh->SetMaterial(0,Instance); }
 }
 else Mesh->SetMaterial(0,LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/Crosshair/Maps/M_ImportedMap")));
}
