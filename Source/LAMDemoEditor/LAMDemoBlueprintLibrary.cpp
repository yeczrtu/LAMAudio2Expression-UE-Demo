// SPDX-License-Identifier: MIT
// Editor-only authoring tool: all behavior it writes executes as Blueprint nodes.
#include "LAMDemoBlueprintLibrary.h"
#include "LAMAudio2ExpressionComponent.h"
#include "LAMAnalyzeAsync.h"
#include "Camera/CameraComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SimpleConstructionScript.h"
#include "Engine/SCS_Node.h"
#include "GameFramework/HUD.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetArrayLibrary.h"
#include "Kismet/KismetMathLibrary.h"
#include "Kismet/KismetStringLibrary.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "EdGraphSchema_K2.h"
#include "EdGraphNode_Comment.h"
#include "K2Node_Event.h"
#include "K2Node_CustomEvent.h"
#include "K2Node_ComponentBoundEvent.h"
#include "K2Node_CallFunction.h"
#include "K2Node_CallArrayFunction.h"
#include "K2Node_AsyncAction.h"
#include "K2Node_VariableGet.h"
#include "K2Node_VariableSet.h"
#include "K2Node_IfThenElse.h"
#include "K2Node_BreakStruct.h"
#include "K2Node_GetArrayItem.h"
#include "K2Node_DynamicCast.h"
#include "K2Node_InputKey.h"
#include "K2Node_SwitchName.h"
#include "K2Node_Self.h"
#include "K2Node_FunctionEntry.h"
#include "Sound/SoundWave.h"
#include "Sound/SoundSubmix.h"
#include "Animation/AnimBlueprint.h"
#include "Misc/PackageName.h"
#include "UObject/SavePackage.h"

namespace LAMDemoGraphs
{
const FString Base = TEXT("/Game/LAMFaceDemo");
const UEdGraphSchema_K2 *Schema()
{
    return GetDefault<UEdGraphSchema_K2>();
}
int32 Errors = 0;
UEdGraphPin *Pin(UEdGraphNode *N, const TCHAR *Name)
{
    auto *P = N->FindPin(FName(Name));
    checkf(P, TEXT("Missing pin %s on %s (%s)"), Name, *N->GetName(),
           *N->GetNodeTitle(ENodeTitleType::FullTitle).ToString());
    return P;
}
void Link(UEdGraphPin *A, UEdGraphPin *B)
{
    if (!A || !B || !Schema()->TryCreateConnection(A, B))
    {
        ++Errors;
        UE_LOG(LogTemp, Error, TEXT("Demo graph connection failed: %s.%s -> %s.%s"),
               A ? *A->GetOwningNode()->GetName() : TEXT("null"), A ? *A->PinName.ToString() : TEXT("null"),
               B ? *B->GetOwningNode()->GetName() : TEXT("null"), B ? *B->PinName.ToString() : TEXT("null"));
    }
}
void Value(UEdGraphNode *N, const TCHAR *Name, const FString &V)
{
    Schema()->TrySetDefaultValue(*Pin(N, Name), V);
}
FEdGraphPinType Type(FName Category, UObject *Object = nullptr, bool Array = false)
{
    FEdGraphPinType T;
    T.PinCategory = Category;
    T.PinSubCategoryObject = Object;
    if (Category == UEdGraphSchema_K2::PC_Real)
        T.PinSubCategory = UEdGraphSchema_K2::PC_Float;
    if (Array)
        T.ContainerType = EPinContainerType::Array;
    return T;
}
void Variable(UBlueprint *BP, const TCHAR *Name, const FEdGraphPinType &T, const FString &Default = TEXT(""))
{
    FBlueprintEditorUtils::AddMemberVariable(BP, FName(Name), T, Default);
    FBlueprintEditorUtils::SetBlueprintVariableCategory(BP, FName(Name), nullptr, FText::FromString(TEXT("Demo")));
    FBlueprintEditorUtils::SetBlueprintOnlyEditableFlag(BP, FName(Name), false);
}
bool Compile(UBlueprint *BP)
{
    FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BP);
    FKismetEditorUtilities::CompileBlueprint(BP);
    return BP->Status != BS_Error && Errors == 0;
}
bool Save(UObject *O)
{
    FSavePackageArgs A;
    A.TopLevelFlags = RF_Public | RF_Standalone;
    return UPackage::SavePackage(O->GetOutermost(), O,
                                 *FPackageName::LongPackageNameToFilename(O->GetOutermost()->GetName(),
                                                                          FPackageName::GetAssetPackageExtension()),
                                 A);
}
UBlueprint *Blueprint(const TCHAR *Name, UClass *Parent)
{
    const FString Path = Base + TEXT("/Blueprints/") + Name;
    auto *BP = LoadObject<UBlueprint>(nullptr, *Path, nullptr, LOAD_NoWarn);
    if (BP)
    {
        BP->LastEditedDocuments.Empty();
        TArray<UEdGraph *> Graphs = BP->UbergraphPages;
        Graphs.Append(BP->FunctionGraphs);
        for (auto *G : Graphs)
            FBlueprintEditorUtils::RemoveGraph(BP, G);
        BP->NewVariables.Empty();
        const auto Nodes = BP->SimpleConstructionScript->GetAllNodes();
        for (auto *N : Nodes)
            BP->SimpleConstructionScript->RemoveNode(N);
        BP->ParentClass = Parent;
        FBlueprintEditorUtils::RefreshAllNodes(BP);
    }
    else
        BP =
            FKismetEditorUtilities::CreateBlueprint(Parent, CreatePackage(*Path), FName(Name), BPTYPE_Normal,
                                                    UBlueprint::StaticClass(), UBlueprintGeneratedClass::StaticClass());
    return BP;
}
struct Graph
{
    UBlueprint *BP;
    UEdGraph *G;
    Graph(UBlueprint *B, const TCHAR *Name, bool Function = false) : BP(B)
    {
        G = FBlueprintEditorUtils::CreateNewGraph(BP, FName(Name), UEdGraph::StaticClass(),
                                                  UEdGraphSchema_K2::StaticClass());
        if (Function)
            FBlueprintEditorUtils::AddFunctionGraph<UClass>(BP, G, true, nullptr);
        else
            FBlueprintEditorUtils::AddUbergraphPage(BP, G);
    }
    template <class T> T *Node(int X, int Y)
    {
        auto *N = NewObject<T>(G);
        G->AddNode(N);
        N->CreateNewGuid();
        N->NodePosX = X;
        N->NodePosY = Y;
        return N;
    }
    void Comment(const TCHAR *Text, int X, int Y, int W, int H)
    {
        auto *N = Node<UEdGraphNode_Comment>(X, Y);
        N->NodeComment = Text;
        N->NodeWidth = W;
        N->NodeHeight = H;
        N->CommentColor = FLinearColor(.08f, .19f, .23f);
        N->FontSize = 20;
    }
    UK2Node_Event *Event(UClass *Owner, const TCHAR *Name, int X = 0, int Y = 0)
    {
        auto *N = Node<UK2Node_Event>(X, Y);
        N->EventReference.SetExternalMember(FName(Name), Owner);
        N->bOverrideFunction = true;
        N->AllocateDefaultPins();
        return N;
    }
    UK2Node_CustomEvent *Custom(const TCHAR *Name, int X = 0, int Y = 0, bool Index = false)
    {
        for (const auto &Page : BP->UbergraphPages)
            for (const auto &Existing : Page->Nodes)
                if (auto *Event = Cast<UK2Node_CustomEvent>(Existing))
                    if (Event->CustomFunctionName == FName(Name))
                    {
                        if (Page != G)
                        {
                            Page->Nodes.Remove(Event);
                            Event->Rename(nullptr, G, REN_DontCreateRedirectors);
                            G->AddNode(Event);
                        }
                        Event->NodePosX = X;
                        Event->NodePosY = Y;
                        return Event;
                    }
        auto *N = Node<UK2Node_CustomEvent>(X, Y);
        N->CustomFunctionName = FName(Name);
        N->AllocateDefaultPins();
        if (Index)
            N->CreateUserDefinedPin(TEXT("Index"), Type(UEdGraphSchema_K2::PC_Int), EGPD_Output);
        return N;
    }
    UK2Node_CallFunction *Call(UClass *Owner, const TCHAR *Name, int X, int Y, UEdGraphPin *Target = nullptr)
    {
        auto *F = Owner->FindFunctionByName(FName(Name));
        checkf(F, TEXT("Missing function: %s.%s"), *Owner->GetName(), Name);
        UK2Node_CallFunction *N = F->HasMetaData(TEXT("ArrayParm")) ? Node<UK2Node_CallArrayFunction>(X, Y)
                                                                    : Node<UK2Node_CallFunction>(X, Y);
        N->SetFromFunction(F);
        N->AllocateDefaultPins();
        if (Target)
            Link(Target, Pin(N, TEXT("self")));
        return N;
    }
    UK2Node_VariableGet *Get(const TCHAR *Name, int X, int Y, UClass *Owner = nullptr, UEdGraphPin *Target = nullptr)
    {
        auto *N = Node<UK2Node_VariableGet>(X, Y);
        if (Owner)
            N->VariableReference.SetExternalMember(FName(Name), Owner);
        else
            N->VariableReference.SetSelfMember(FName(Name));
        N->AllocateDefaultPins();
        if (Target)
            Link(Target, Pin(N, TEXT("self")));
        return N;
    }
    UK2Node_VariableSet *Set(const TCHAR *Name, int X, int Y, UEdGraphPin *Input = nullptr,
                             const FString &Default = TEXT(""), UClass *Owner = nullptr, UEdGraphPin *Target = nullptr)
    {
        auto *N = Node<UK2Node_VariableSet>(X, Y);
        if (Owner)
            N->VariableReference.SetExternalMember(FName(Name), Owner);
        else
            N->VariableReference.SetSelfMember(FName(Name));
        N->AllocateDefaultPins();
        if (Input)
            Link(Input, Pin(N, Name));
        else if (!Default.IsEmpty())
            Value(N, Name, Default);
        if (Target)
            Link(Target, Pin(N, TEXT("self")));
        return N;
    }
    UK2Node_CallFunction *LAM(const TCHAR *Name, int X, int Y)
    {
        return Call(ULAMAudio2ExpressionComponent::StaticClass(), Name, X, Y,
                    Get(TEXT("LAM"), X, Y + 180)->GetValuePin());
    }
    UK2Node_CallFunction *Math(const TCHAR *Name, int X, int Y)
    {
        return Call(UKismetMathLibrary::StaticClass(), Name, X, Y);
    }
    UK2Node_IfThenElse *Branch(UEdGraphPin *&Tail, UEdGraphPin *Condition, int X, int Y)
    {
        auto *N = Node<UK2Node_IfThenElse>(X, Y);
        N->AllocateDefaultPins();
        Link(Tail, N->GetExecPin());
        Link(Condition, N->GetConditionPin());
        Tail = N->GetThenPin();
        return N;
    }
    UK2Node_BreakStruct *Break(UScriptStruct *S, UEdGraphPin *Input, int X, int Y)
    {
        auto *N = Node<UK2Node_BreakStruct>(X, Y);
        N->StructType = S;
        N->AllocateDefaultPins();
        for (auto &P : N->ShowPinForProperties)
            P.bShowPin = true;
        N->ReconstructNode();
        Link(Input, N->FindPinChecked(S->GetFName()));
        return N;
    }
    UEdGraphPin *Item(const TCHAR *Array, UEdGraphPin *Index, int X, int Y)
    {
        auto *A = Get(Array, X - 220, Y);
        auto *N = Node<UK2Node_GetArrayItem>(X, Y);
        N->AllocateDefaultPins();
        Link(A->GetValuePin(), N->GetTargetArrayPin());
        Link(Index, N->GetIndexPin());
        return N->GetResultPin();
    }
    UK2Node_ComponentBoundEvent *Bound(const TCHAR *Name, int X, int Y)
    {
        auto *N = Node<UK2Node_ComponentBoundEvent>(X, Y);
        N->InitializeComponentBoundEventParams(
            FindFProperty<FObjectProperty>(BP->SkeletonGeneratedClass, TEXT("LAM")),
            FindFProperty<FMulticastDelegateProperty>(ULAMAudio2ExpressionComponent::StaticClass(), FName(Name)));
        N->AllocateDefaultPins();
        return N;
    }
    UEdGraphPin *Self(int X, int Y)
    {
        auto *N = Node<UK2Node_Self>(X, Y);
        N->AllocateDefaultPins();
        return Pin(N, TEXT("self"));
    }
    UK2Node_FunctionEntry *Entry()
    {
        for (const auto &N : G->Nodes)
            if (auto *E = Cast<UK2Node_FunctionEntry>(N))
                return E;
        checkNoEntry();
        return nullptr;
    }
};
void Then(UEdGraphPin *&Tail, UEdGraphNode *N)
{
    Link(Tail, Pin(N, TEXT("execute")));
    Tail = Pin(N, TEXT("then"));
}
UEdGraphPin *Result(UEdGraphNode *N)
{
    return Pin(N, TEXT("ReturnValue"));
}
UEdGraphPin *Exec(UEdGraphNode *N)
{
    return Pin(N, TEXT("then"));
}
UEdGraphPin *IsEnum(Graph &G, UEdGraphPin *Input, uint8 V, int X, int Y)
{
    auto *N = G.Math(TEXT("EqualEqual_ByteByte"), X, Y);
    Link(Input, Pin(N, TEXT("A")));
    Value(N, TEXT("B"), FString::FromInt(V));
    return Result(N);
}

void ActorGraphs(UBlueprint *BP)
{
    Graph Init(BP, TEXT("01_Setup"));
    auto *Start = Init.Event(AActor::StaticClass(), TEXT("ReceiveBeginPlay"));
    auto *Tail = Exec(Start);
    auto *PC = Init.Call(UGameplayStatics::StaticClass(), TEXT("GetPlayerController"), 0, 250);
    auto *View = Init.Call(APlayerController::StaticClass(), TEXT("SetViewTargetWithBlend"), 300, 0, Result(PC));
    Link(Init.Self(80, 420), Pin(View, TEXT("NewViewTarget")));
    Then(Tail, View);
    Then(Tail, Init.Set(TEXT("bShowMouseCursor"), 650, 0, nullptr, TEXT("true"), APlayerController::StaticClass(),
                        Result(PC)));
    Then(Tail, Init.Set(TEXT("bEnableClickEvents"), 650, 350, nullptr, TEXT("true"), APlayerController::StaticClass(),
                        Result(PC)));
    auto *Enable = Init.Call(AActor::StaticClass(), TEXT("EnableInput"), 950, 0);
    Link(Result(PC), Pin(Enable, TEXT("PlayerController")));
    Then(Tail, Enable);
    auto *Tick = Init.Call(UActorComponent::StaticClass(), TEXT("AddTickPrerequisiteComponent"), 1250, 0,
                           Init.Get(TEXT("Face"), 1250, 220)->GetValuePin());
    Link(Init.Get(TEXT("LAM"), 1450, 220)->GetValuePin(), Pin(Tick, TEXT("PrerequisiteComponent")));
    Then(Tail, Tick);
    auto *Out = Init.LAM(TEXT("SetOutputSubmix"), 1550, 0);
    Pin(Out, TEXT("Submix"))->DefaultObject = LoadObject<USoundSubmix>(nullptr, *(Base + TEXT("/Audio/SM_Dialogue")));
    Then(Tail, Out);
    auto *Select = Init.Call(BP->SkeletonGeneratedClass, TEXT("SelectSample"), 1900, 0);
    Value(Select, TEXT("Index"), TEXT("0"));
    Then(Tail, Select);
    Init.Comment(TEXT("START HERE: camera, input and component ordering. SelectSample starts the first voice."), -60,
                 -100, 2400, 720);

    // The actual async node and its completion/failure outputs are deliberately visible.
    Graph Analyze(BP, TEXT("02_Analyze_And_Play"));
    auto *SelectEvent = Analyze.Custom(TEXT("SelectSample"), 0, 0, true);
    Tail = Exec(SelectEvent);
    auto *ValidIndex = Analyze.Call(UKismetArrayLibrary::StaticClass(), TEXT("Array_IsValidIndex"), 0, 280);
    Link(Analyze.Get(TEXT("Samples"), -240, 240)->GetValuePin(), Pin(ValidIndex, TEXT("TargetArray")));
    Link(Pin(SelectEvent, TEXT("Index")), Pin(ValidIndex, TEXT("IndexToTest")));
    auto *ValidBranch = Analyze.Branch(Tail, Result(ValidIndex), 300, 0);
    auto *BadIndex = ValidBranch->GetElsePin();
    Then(BadIndex, Analyze.Set(TEXT("Status"), 600, -180, nullptr, TEXT("Invalid sample index")));
    Then(Tail, Analyze.LAM(TEXT("CancelAnalysis"), 600, 0));
    Then(Tail, Analyze.LAM(TEXT("StopMicrophone"), 900, 0));
    Then(Tail, Analyze.LAM(TEXT("Stop"), 1200, 0));
    Then(Tail, Analyze.Set(TEXT("Selected"), 1500, 0, Pin(SelectEvent, TEXT("Index"))));
    Then(Tail, Analyze.Set(TEXT("bAnalyzing"), 1750, 0, nullptr, TEXT("true")));
    Then(Tail, Analyze.Set(TEXT("Progress"), 2000, 0, nullptr, TEXT("0")));
    Then(Tail, Analyze.Set(TEXT("Status"), 2250, 0, nullptr, TEXT("Analyzing audio...")));
    auto *Async = Analyze.Node<UK2Node_AsyncAction>(2600, 0);
    Async->InitializeProxyFromFunction(
        ULAMAnalyzeAsync::StaticClass()->FindFunctionByName(TEXT("AnalyzeSoundWaveAsync")));
    Async->AllocateDefaultPins();
    Link(Tail, Pin(Async, TEXT("execute")));
    Link(Analyze.Get(TEXT("LAM"), 2300, 360)->GetValuePin(), Pin(Async, TEXT("Component")));
    Link(Analyze.Item(TEXT("Samples"), Analyze.Get(TEXT("Selected"), 1960, 540)->GetValuePin(), 2320, 520),
         Pin(Async, TEXT("SoundWave")));
    Tail = Pin(Async, TEXT("Completed"));
    Then(Tail, Analyze.Set(TEXT("LastClip"), 3040, 0, Pin(Async, TEXT("Clip"))));
    Then(Tail, Analyze.Set(TEXT("bAnalyzing"), 3340, 0, nullptr, TEXT("false")));
    Then(Tail, Analyze.Set(TEXT("Progress"), 3620, 0, nullptr, TEXT("1")));
    auto *Play = Analyze.LAM(TEXT("PlayExpressionClip"), 3900, 0);
    Link(Pin(Async, TEXT("Clip")), Pin(Play, TEXT("Clip")));
    Then(Tail, Play);
    auto *Played = Analyze.Branch(Tail, Result(Play), 4250, 0);
    Then(Tail, Analyze.Set(TEXT("Status"), 4550, 0, nullptr, TEXT("Playing | Space: pause")));
    auto *NotPlayed = Played->GetElsePin();
    Then(NotPlayed, Analyze.Set(TEXT("Status"), 4550, 200, nullptr, TEXT("Playback failed")));
    UEdGraphPin *ProgressValue = nullptr;
    for (auto *P : Async->Pins)
        if (P->PinName == TEXT("Progress") && P->PinType.PinCategory != UEdGraphSchema_K2::PC_Exec)
            ProgressValue = P;
    check(ProgressValue);
    Tail = Pin(Async, TEXT("Progress"));
    Then(Tail, Analyze.Set(TEXT("Progress"), 3040, 500, ProgressValue));
    Tail = Pin(Async, TEXT("Failed"));
    Then(Tail, Analyze.Set(TEXT("bAnalyzing"), 3040, 780, nullptr, TEXT("false")));
    Then(Tail, Analyze.Set(TEXT("Status"), 3340, 780, Pin(Async, TEXT("Error"))));
    Tail = Pin(Async, TEXT("Cancelled"));
    Then(Tail, Analyze.Set(TEXT("bAnalyzing"), 3040, 1040, nullptr, TEXT("false")));
    Then(Tail, Analyze.Set(TEXT("Status"), 3340, 1040, nullptr, TEXT("Analysis cancelled")));
    Analyze.Comment(TEXT("1. Select a SoundWave. Cancel the previous request before starting another."), -300, -320,
                    2850, 1120);
    Analyze.Comment(
        TEXT("2. Analyze SoundWave Async -> Completed -> Play Expression Clip. Error and progress are separate paths."),
        2560, -160, 2360, 1490);

    Graph Playback(BP, TEXT("03_Playback_Controls"));
    auto *Pause = Playback.Custom(TEXT("TogglePlayback"));
    Tail = Exec(Pause);
    auto *NotAnalyzing = Playback.Math(TEXT("Not_PreBool"), 0, 200);
    Link(Playback.Get(TEXT("bAnalyzing"), -220, 200)->GetValuePin(), Pin(NotAnalyzing, TEXT("A")));
    Playback.Branch(Tail, Result(NotAnalyzing), 300, 0);
    auto *Info = Playback.LAM(TEXT("GetPlaybackInfo"), 360, 280);
    auto *Fields = Playback.Break(FLAMPlaybackInfo::StaticStruct(), Result(Info), 700, 260);
    auto *IsPaused = Playback.Branch(
        Tail, IsEnum(Playback, Pin(Fields, TEXT("State")), uint8(ELAMPlaybackState::Paused), 960, 230), 1200, 0);
    Then(Tail, Playback.LAM(TEXT("Resume"), 1500, 0));
    Tail = IsPaused->GetElsePin();
    auto *IsStopped = Playback.Branch(
        Tail, IsEnum(Playback, Pin(Fields, TEXT("State")), uint8(ELAMPlaybackState::Stopped), 1200, 460), 1500, 360);
    Then(Tail, Playback.Call(BP->SkeletonGeneratedClass, TEXT("Replay"), 1800, 300));
    Tail = IsStopped->GetElsePin();
    Then(Tail, Playback.LAM(TEXT("Pause"), 1800, 600));
    Playback.Comment(TEXT("Space / Pause button: read the real playback state, then Pause, Resume or Replay."), -280,
                     -100, 2500, 980);
    auto *Replay = Playback.Custom(TEXT("Replay"), 0, 1100);
    Tail = Exec(Replay);
    auto *CanReplay = Playback.Math(TEXT("Not_PreBool"), -250, 1300);
    Link(Playback.Get(TEXT("bAnalyzing"), -500, 1300)->GetValuePin(), Pin(CanReplay, TEXT("A")));
    Playback.Branch(Tail, Result(CanReplay), 250, 1100);
    auto *HasClip = Playback.Call(UKismetSystemLibrary::StaticClass(), TEXT("IsValid"), 260, 1300);
    Link(Playback.Get(TEXT("LastClip"), 0, 1400)->GetValuePin(), Pin(HasClip, TEXT("Object")));
    Playback.Branch(Tail, Result(HasClip), 540, 1100);
    auto *ReplayPlay = Playback.LAM(TEXT("PlayExpressionClip"), 840, 1100);
    Link(Playback.Get(TEXT("LastClip"), 550, 1490)->GetValuePin(), Pin(ReplayPlay, TEXT("Clip")));
    Then(Tail, ReplayPlay);
    Tail = Exec(Playback.Custom(TEXT("FadeStop"), 0, 1760));
    auto *Fade = Playback.LAM(TEXT("FadeOutAndStop"), 360, 1760);
    Value(Fade, TEXT("Duration"), TEXT("0.3"));
    Then(Tail, Fade);
    Tail = Exec(Playback.Custom(TEXT("ToggleMute"), 0, 2300));
    auto *Settings = Playback.Get(TEXT("PlaybackSettings"), 0, 2500, ULAMAudio2ExpressionComponent::StaticClass(),
                                  Playback.Get(TEXT("LAM"), -230, 2530)->GetValuePin());
    auto *Audio = Playback.Break(FLAMAudioPlaybackSettings::StaticStruct(), Settings->GetValuePin(), 280, 2500);
    auto *NotMuted = Playback.Math(TEXT("Not_PreBool"), 620, 2560);
    Link(Pin(Audio, TEXT("bMuted")), Pin(NotMuted, TEXT("A")));
    auto *Mute = Playback.LAM(TEXT("SetMuted"), 920, 2300);
    Link(Result(NotMuted), Pin(Mute, TEXT("Muted")));
    Then(Tail, Mute);
    for (int I = 0; I < 2; ++I)
    {
        const int Y = 3300 + I * 900;
        Tail = Exec(Playback.Custom(I ? TEXT("VolumeUp") : TEXT("VolumeDown"), 0, Y));
        auto *S = Playback.Get(TEXT("PlaybackSettings"), 0, Y + 220, ULAMAudio2ExpressionComponent::StaticClass(),
                               Playback.Get(TEXT("LAM"), -230, Y + 240)->GetValuePin());
        auto *B = Playback.Break(FLAMAudioPlaybackSettings::StaticStruct(), S->GetValuePin(), 280, Y + 220);
        auto *Add = Playback.Math(TEXT("Add_DoubleDouble"), 620, Y + 200);
        Link(Pin(B, TEXT("Volume")), Pin(Add, TEXT("A")));
        Value(Add, TEXT("B"), I ? TEXT("0.1") : TEXT("-0.1"));
        auto *Clamp = Playback.Math(TEXT("FClamp"), 900, Y + 200);
        Link(Result(Add), Pin(Clamp, TEXT("Value")));
        Value(Clamp, TEXT("Min"), TEXT("0"));
        Value(Clamp, TEXT("Max"), TEXT("2"));
        auto *Set = Playback.LAM(TEXT("SetVolume"), 1200, Y);
        Link(Result(Clamp), Pin(Set, TEXT("Volume")));
        Then(Tail, Set);
    }
    Tail = Exec(Playback.Custom(TEXT("CycleOutput"), 0, 5150));
    auto *Plus = Playback.Math(TEXT("Add_IntInt"), 0, 5400);
    Link(Playback.Get(TEXT("OutputIndex"), -220, 5400)->GetValuePin(), Pin(Plus, TEXT("A")));
    Value(Plus, TEXT("B"), TEXT("1"));
    auto *Mod = Playback.Math(TEXT("Percent_IntInt"), 270, 5400);
    Link(Result(Plus), Pin(Mod, TEXT("A")));
    Value(Mod, TEXT("B"), TEXT("2"));
    Then(Tail, Playback.Set(TEXT("OutputIndex"), 550, 5150, Result(Mod)));
    auto *Mix = Playback.LAM(TEXT("SetOutputSubmix"), 900, 5150);
    Link(Playback.Item(TEXT("OutputSubmixes"), Playback.Get(TEXT("OutputIndex"), 350, 5700)->GetValuePin(), 660, 5620),
         Pin(Mix, TEXT("Submix")));
    Then(Tail, Mix);

    Graph Live(BP, TEXT("04_Microphone_And_Interval"));
    Tail = Exec(Live.Custom(TEXT("ToggleMicrophone")));
    auto *Metrics = Live.LAM(TEXT("GetLiveMetrics"), 0, 240);
    auto *L = Live.Break(FLAMLiveMetrics::StaticStruct(), Result(Metrics), 330, 240);
    auto *Or = Live.Math(TEXT("BooleanOR"), 930, 220);
    Link(IsEnum(Live, Pin(L, TEXT("State")), uint8(ELAMLiveState::Stopped), 630, 220), Pin(Or, TEXT("A")));
    Link(IsEnum(Live, Pin(L, TEXT("State")), uint8(ELAMLiveState::Failed), 630, 430), Pin(Or, TEXT("B")));
    auto *BeginLive = Live.Branch(Tail, Result(Or), 1230, 0);
    Then(Tail, Live.LAM(TEXT("CancelAnalysis"), 1550, 0));
    auto *Mic = Live.LAM(TEXT("StartMicrophone"), 1860, 0);
    Then(Tail, Mic);
    auto *Started = Live.Branch(Tail, Result(Mic), 2190, 0);
    Then(Tail, Live.Set(TEXT("Status"), 2500, 0, nullptr, TEXT("Microphone active")));
    auto *Failed = Started->GetElsePin();
    Then(Failed,
         Live.Set(TEXT("Status"), 2500, 250, nullptr, TEXT("Microphone failed - check Windows microphone access")));
    auto *Stop = BeginLive->GetElsePin();
    Then(Stop, Live.LAM(TEXT("StopMicrophone"), 1550, 500));
    Then(Stop, Live.Set(TEXT("Status"), 1860, 500, nullptr, TEXT("Microphone stopped")));
    Live.Comment(TEXT("Microphone uses the public plugin nodes. No audio is played back to the speakers."), -80, -120,
                 3100, 1000);
    Tail = Exec(Live.Custom(TEXT("ChangeLiveInterval"), 0, 1100));
    auto *Current = Live.LAM(TEXT("GetLiveInferenceInterval"), 0, 1340);
    auto *Small = Live.Math(TEXT("Less_DoubleDouble"), 350, 1350);
    Link(Result(Current), Pin(Small, TEXT("A")));
    Value(Small, TEXT("B"), TEXT("200"));
    auto *B = Live.Branch(Tail, Result(Small), 650, 1100);
    auto *Set = Live.LAM(TEXT("SetLiveInferenceInterval"), 960, 1100);
    Value(Set, TEXT("Milliseconds"), TEXT("333.333333"));
    Then(Tail, Set);
    Tail = B->GetElsePin();
    auto *Mid = Live.Math(TEXT("Less_DoubleDouble"), 650, 1590);
    Link(Result(Current), Pin(Mid, TEXT("A")));
    Value(Mid, TEXT("B"), TEXT("400"));
    B = Live.Branch(Tail, Result(Mid), 960, 1490);
    Set = Live.LAM(TEXT("SetLiveInferenceInterval"), 1280, 1490);
    Value(Set, TEXT("Milliseconds"), TEXT("1000"));
    Then(Tail, Set);
    Tail = B->GetElsePin();
    Set = Live.LAM(TEXT("SetLiveInferenceInterval"), 1280, 1880);
    Value(Set, TEXT("Milliseconds"), TEXT("100"));
    Then(Tail, Set);
    Live.Comment(TEXT("I / Interval button: 100 -> 333.3 -> 1000 ms. The current microphone session stays connected."),
                 -80, 980, 1800, 1260);

    Graph Events(BP, TEXT("05_Status_Events"));
    auto *Finished = Events.Bound(TEXT("OnPlaybackFinished"), 0, 0);
    Tail = Exec(Finished);
    Then(Tail, Events.Set(TEXT("Status"), 420, 0, nullptr, TEXT("Playback completed - R: replay")));
    auto *Ended = Events.Bound(TEXT("OnPlaybackEnded"), 0, 2200);
    Tail = Exec(Ended);
    auto *Completed = Events.Branch(
        Tail, IsEnum(Events, Pin(Ended, TEXT("Reason")), uint8(ELAMPlaybackEndReason::Completed), 350, 2400), 650,
        2200);
    Tail = Completed->GetElsePin();
    Then(Tail, Events.Set(TEXT("Status"), 1000, 2200, nullptr, TEXT("Playback ended | R: replay")));
    auto *FailedEvent = Events.Bound(TEXT("OnPlaybackFailed"), 0, 450);
    Tail = Exec(FailedEvent);
    auto *Error = Events.Break(FLAMPlaybackError::StaticStruct(), Pin(FailedEvent, TEXT("Error")), 320, 650);
    Then(Tail, Events.Set(TEXT("Status"), 700, 450, Pin(Error, TEXT("Message"))));
    auto *Status = Events.Bound(TEXT("OnStatus"), 0, 1060);
    Tail = Exec(Status);
    Then(Tail, Events.Set(TEXT("Status"), 420, 1060, Pin(Status, TEXT("Message"))));
    auto *State = Events.Bound(TEXT("OnPlaybackStateChanged"), 0, 1480);
    Tail = Exec(State);
    auto *StateInfo = Events.Break(FLAMPlaybackInfo::StaticStruct(), Pin(State, TEXT("Info")), 0, 1730);
    auto *PausedBranch = Events.Branch(
        Tail, IsEnum(Events, Pin(StateInfo, TEXT("State")), uint8(ELAMPlaybackState::Paused), 380, 1800), 690, 1480);
    Then(Tail, Events.Set(TEXT("Status"), 1040, 1480, nullptr, TEXT("Paused - Space: resume")));
    Tail = PausedBranch->GetElsePin();
    Events.Branch(Tail, IsEnum(Events, Pin(StateInfo, TEXT("State")), uint8(ELAMPlaybackState::Playing), 900, 1800),
                  1220, 1780);
    Then(Tail, Events.Set(TEXT("Status"), 1550, 1780, nullptr, TEXT("Playing | Space: pause")));
    Events.Comment(TEXT("Public component events update the HUD. OnPlaybackFinished means natural completion only."),
                   -80, -100, 2000, 2850);

    Graph Keys(BP, TEXT("06_Keyboard"));
    const TCHAR *KeyNames[] = {TEXT("One"), TEXT("Two"),      TEXT("Three"), TEXT("Four"),   TEXT("Five"),
                               TEXT("Six"), TEXT("SpaceBar"), TEXT("R"),     TEXT("V"),      TEXT("O"),
                               TEXT("M"),   TEXT("I"),        TEXT("F"),     TEXT("Hyphen"), TEXT("Equals")};
    const TCHAR *Commands[] = {TEXT("TogglePlayback"), TEXT("Replay"),           TEXT("ToggleMute"),
                               TEXT("CycleOutput"),    TEXT("ToggleMicrophone"), TEXT("ChangeLiveInterval"),
                               TEXT("FadeStop"),       TEXT("VolumeDown"),       TEXT("VolumeUp")};
    for (int I = 0; I < 15; ++I)
    {
        int X = (I / 5) * 800, Y = (I % 5) * 300;
        auto *K = Keys.Node<UK2Node_InputKey>(X, Y);
        K->InputKey = FKey(KeyNames[I]);
        K->AllocateDefaultPins();
        auto *C = Keys.Call(BP->SkeletonGeneratedClass, I < 6 ? TEXT("SelectSample") : Commands[I - 6], X + 300, Y);
        Link(Pin(K, TEXT("Pressed")), Pin(C, TEXT("execute")));
        if (I < 6)
            Value(C, TEXT("Index"), FString::FromInt(I));
    }
    Keys.Comment(TEXT("Keyboard shortcuts call the same Blueprint events as the HUD buttons."), -80, -100, 2450, 1650);
}

void HUDGraphs(UBlueprint *HUD, UBlueprint *Actor)
{
    // Reusable Blueprint functions keep Canvas mechanics out of the user-facing control graphs.
    for (int Kind = 0; Kind < 3; ++Kind)
    {
        Graph F(HUD, Kind == 0 ? TEXT("DrawPanel") : Kind == 1 ? TEXT("DrawLabel") : TEXT("DrawButton"), true);
        auto *E = F.Entry();
        auto Input = [&](const TCHAR *Name, FName Cat)
        { return E->CreateUserDefinedPin(FName(Name), Type(Cat), EGPD_Output); };
        auto *X = Input(TEXT("X"), UEdGraphSchema_K2::PC_Real);
        auto *Y = Input(TEXT("Y"), UEdGraphSchema_K2::PC_Real);
        auto *W = Input(Kind == 1 ? TEXT("Size") : TEXT("Width"), UEdGraphSchema_K2::PC_Real);
        UEdGraphPin *H = Kind == 0 ? Input(TEXT("Height"), UEdGraphSchema_K2::PC_Real) : nullptr;
        UEdGraphPin *TextPin = Kind > 0 ? Input(TEXT("Label"), UEdGraphSchema_K2::PC_String) : nullptr;
        UEdGraphPin *NamePin = Kind == 2 ? Input(TEXT("ButtonName"), UEdGraphSchema_K2::PC_Name) : nullptr;
        auto *Tail = Exec(E);
        auto Scale = [&](UEdGraphPin *P, int PX, int PY)
        {
            auto *M = F.Math(TEXT("Multiply_DoubleDouble"), PX, PY);
            Link(P, Pin(M, TEXT("A")));
            Link(F.Get(TEXT("UIScale"), PX - 240, PY + 90)->GetValuePin(), Pin(M, TEXT("B")));
            return Result(M);
        };
        if (Kind < 2)
        {
            auto *N = F.Call(AHUD::StaticClass(), Kind == 0 ? TEXT("DrawRect") : TEXT("DrawText"), 1000, 0);
            Then(Tail, N);
            Link(Scale(X, 330, 220), Pin(N, TEXT("ScreenX")));
            Link(Scale(Y, 330, 450), Pin(N, TEXT("ScreenY")));
            if (Kind == 0)
            {
                Link(Scale(W, 650, 680), Pin(N, TEXT("ScreenW")));
                Link(Scale(H, 1000, 680), Pin(N, TEXT("ScreenH")));
                Value(N, TEXT("RectColor"), TEXT("(R=0.025,G=0.05,B=0.065,A=0.96)"));
            }
            else
            {
                Link(TextPin, Pin(N, TEXT("Text")));
                Link(Scale(W, 850, 680), Pin(N, TEXT("Scale")));
                Value(N, TEXT("TextColor"), TEXT("(R=0.78,G=0.90,B=0.91,A=1)"));
            }
        }
        else
        {
            auto *Panel = F.Call(HUD->SkeletonGeneratedClass, TEXT("DrawPanel"), 400, 0);
            Then(Tail, Panel);
            Link(X, Pin(Panel, TEXT("X")));
            Link(Y, Pin(Panel, TEXT("Y")));
            Link(W, Pin(Panel, TEXT("Width")));
            Value(Panel, TEXT("Height"), TEXT("32"));
            auto *Label = F.Call(HUD->SkeletonGeneratedClass, TEXT("DrawLabel"), 850, 0);
            Then(Tail, Label);
            Link(TextPin, Pin(Label, TEXT("Label")));
            Value(Label, TEXT("Size"), TEXT("0.85"));
            auto *AX = F.Math(TEXT("Add_DoubleDouble"), 400, 370);
            Link(X, Pin(AX, TEXT("A")));
            Value(AX, TEXT("B"), TEXT("10"));
            Link(Result(AX), Pin(Label, TEXT("X")));
            auto *AY = F.Math(TEXT("Add_DoubleDouble"), 700, 500);
            Link(Y, Pin(AY, TEXT("A")));
            Value(AY, TEXT("B"), TEXT("8"));
            Link(Result(AY), Pin(Label, TEXT("Y")));
            auto *Hit = F.Call(AHUD::StaticClass(), TEXT("AddHitBox"), 1300, 0);
            Then(Tail, Hit);
            Link(NamePin, Pin(Hit, TEXT("InName")));
            Value(Hit, TEXT("bConsumesInput"), TEXT("true"));
            auto *Position = F.Math(TEXT("MakeVector2D"), 1300, 300);
            Link(Scale(X, 400, 850), Pin(Position, TEXT("X")));
            Link(Scale(Y, 750, 850), Pin(Position, TEXT("Y")));
            Link(Result(Position), Pin(Hit, TEXT("Position")));
            auto *Size = F.Math(TEXT("MakeVector2D"), 1650, 450);
            Link(Scale(W, 1050, 850), Pin(Size, TEXT("X")));
            auto *Height = F.Math(TEXT("Multiply_DoubleDouble"), 1400, 850);
            Link(F.Get(TEXT("UIScale"), 1150, 1070)->GetValuePin(), Pin(Height, TEXT("A")));
            Value(Height, TEXT("B"), TEXT("32"));
            Link(Result(Height), Pin(Size, TEXT("Y")));
            Link(Result(Size), Pin(Hit, TEXT("Size")));
        }
        F.Comment(TEXT("Reusable Blueprint drawing function. Coordinates are in an 800 px-high reference canvas."), -80,
                  -120, 2050, 1450);
        if (!Compile(HUD))
            return;
    }
    Graph DrawDeclarations(HUD, TEXT("DrawDeclarations"));
    for (const TCHAR *Name : {TEXT("DrawVoiceButtons"), TEXT("DrawControls"), TEXT("DrawReadouts")})
        DrawDeclarations.Custom(Name);
    if (!Compile(HUD))
        return;
    Graph Init(HUD, TEXT("01_Find_Demo"));
    auto *Tail = Exec(Init.Event(AActor::StaticClass(), TEXT("ReceiveBeginPlay")));
    auto *Find = Init.Call(UGameplayStatics::StaticClass(), TEXT("GetActorOfClass"), 0, 220);
    Pin(Find, TEXT("ActorClass"))->DefaultObject = Actor->GeneratedClass;
    Then(Tail, Find);
    auto *Cast = Init.Node<UK2Node_DynamicCast>(340, 200);
    Cast->TargetType = Actor->GeneratedClass;
    Cast->SetPurity(true);
    Cast->AllocateDefaultPins();
    Link(Result(Find), Cast->GetCastSourcePin());
    Then(Tail, Init.Set(TEXT("Demo"), 710, 0, Cast->GetCastResultPin()));
    Init.Comment(TEXT("The HUD keeps a reference to BP_FaceDemo. It does not perform inference or playback itself."),
                 -80, -100, 1230, 700);

    Graph Draw(HUD, TEXT("02_Draw_Interface"));
    auto *Event = Draw.Event(AHUD::StaticClass(), TEXT("ReceiveDrawHUD"));
    Tail = Exec(Event);
    auto *Demo = Draw.Get(TEXT("Demo"), 0, 280);
    auto *Valid = Draw.Call(UKismetSystemLibrary::StaticClass(), TEXT("IsValid"), 260, 240);
    Link(Demo->GetValuePin(), Pin(Valid, TEXT("Object")));
    Draw.Branch(Tail, Result(Valid), 300, 0);
    auto *Ratio = Draw.Math(TEXT("Divide_DoubleDouble"), 600, 280);
    Link(Pin(Event, TEXT("SizeY")), Pin(Ratio, TEXT("A")));
    Value(Ratio, TEXT("B"), TEXT("800"));
    auto *Clamp = Draw.Math(TEXT("FClamp"), 900, 280);
    Link(Result(Ratio), Pin(Clamp, TEXT("Value")));
    Value(Clamp, TEXT("Min"), TEXT("0.45"));
    Value(Clamp, TEXT("Max"), TEXT("1.5"));
    Then(Tail, Draw.Set(TEXT("UIScale"), 650, 0, Result(Clamp)));
    int DispatchX = 1000;
    for (const TCHAR *Name : {TEXT("DrawVoiceButtons"), TEXT("DrawControls"), TEXT("DrawReadouts")})
    {
        Then(Tail, Draw.Call(HUD->SkeletonGeneratedClass, Name, DispatchX, 0));
        DispatchX += 340;
    }
    Draw.Comment(TEXT("Draw HUD: scale once, then draw the voice list, controls and live readouts."), -80, -100, 2450,
                 700);
    // Drawing commands are grouped into readable rows instead of one long horizontal graph.
    int Row = 0;
    bool Compact = true;
    auto Text = [&](const FString &S, float X, float Y, float Size, UEdGraphPin *Content = nullptr)
    {
        int NY = 1000 + Row++ * (Compact ? 300 : 800);
        auto *N = Draw.Call(HUD->SkeletonGeneratedClass, TEXT("DrawLabel"), Compact ? 360 : 1400, NY);
        Then(Tail, N);
        if (Content)
            Link(Content, Pin(N, TEXT("Label")));
        else
            Value(N, TEXT("Label"), S);
        Value(N, TEXT("X"), FString::SanitizeFloat(X));
        Value(N, TEXT("Y"), FString::SanitizeFloat(Y));
        Value(N, TEXT("Size"), FString::SanitizeFloat(Size));
        return N;
    };
    auto Rect = [&](float X, float Y, float W, float H, const TCHAR *)
    {
        int NY = 1000 + Row++ * (Compact ? 300 : 800);
        auto *N = Draw.Call(HUD->SkeletonGeneratedClass, TEXT("DrawPanel"), Compact ? 360 : 1400, NY);
        Then(Tail, N);
        Value(N, TEXT("X"), FString::SanitizeFloat(X));
        Value(N, TEXT("Y"), FString::SanitizeFloat(Y));
        Value(N, TEXT("Width"), FString::SanitizeFloat(W));
        Value(N, TEXT("Height"), FString::SanitizeFloat(H));
    };
    auto Button = [&](const TCHAR *Name, const TCHAR *Label, float X, float Y, float W = 280)
    {
        int NY = 1000 + Row++ * (Compact ? 300 : 800);
        auto *N = Draw.Call(HUD->SkeletonGeneratedClass, TEXT("DrawButton"), Compact ? 360 : 1400, NY);
        Then(Tail, N);
        Value(N, TEXT("ButtonName"), Name);
        Value(N, TEXT("Label"), Label);
        Value(N, TEXT("X"), FString::SanitizeFloat(X));
        Value(N, TEXT("Y"), FString::SanitizeFloat(Y));
        Value(N, TEXT("Width"), FString::SanitizeFloat(W));
    };
    Draw = Graph(HUD, TEXT("04_Draw_Voice_Buttons"));
    Tail = Exec(Draw.Custom(TEXT("DrawVoiceButtons"), 360, 650));
    Rect(20, 20, 320, 755, TEXT("(R=0.025,G=0.034,B=0.05,A=0.96)"));
    Text(TEXT("LAM / FACE DEMO"), 40, 40, 1.4f);
    Text(TEXT("AUDIO TO ARKIT / BLUEPRINT DEMO"), 40, 75, .72f);
    const TCHAR *Labels[] = {TEXT("1  Anger"),     TEXT("2  Disgust"), TEXT("3  Fear"),
                             TEXT("4  Happiness"), TEXT("5  Sadness"), TEXT("6  Surprise")};
    const TCHAR *Boxes[] = {TEXT("Sample0"), TEXT("Sample1"), TEXT("Sample2"),
                            TEXT("Sample3"), TEXT("Sample4"), TEXT("Sample5")};
    for (int I = 0; I < 6; ++I)
        Button(Boxes[I], Labels[I], 40, 112 + I * 40);
    Draw.Comment(TEXT("Six voice buttons. ButtonName matches 03_Button_Clicks; edit Label to rename a voice."), 100,
                 470, 850, 3500);
    Draw = Graph(HUD, TEXT("05_Draw_Playback_Buttons"));
    Row = 0;
    Tail = Exec(Draw.Custom(TEXT("DrawControls"), 360, 650));
    Button(TEXT("Pause"), TEXT("Space  Play / Pause"), 40, 365, 280);
    Button(TEXT("Replay"), TEXT("R  Replay"), 40, 405, 135);
    Button(TEXT("Fade"), TEXT("F  Fade stop"), 185, 405, 135);
    Button(TEXT("Mute"), TEXT("V  Mute"), 40, 445, 135);
    Button(TEXT("Output"), TEXT("O  Output"), 185, 445, 135);
    Button(TEXT("Down"), TEXT("-  Volume"), 40, 485, 135);
    Button(TEXT("Up"), TEXT("+  Volume"), 185, 485, 135);
    Button(TEXT("Mic"), TEXT("M  Microphone"), 40, 525, 280);
    Button(TEXT("Interval"), TEXT("I  Interval: 100 / 333 / 1000 ms"), 40, 565, 280);
    Text(TEXT("Face: hinzka / VRoid"), 40, 662, .75);
    Text(TEXT("Voice: JVNV / litagin"), 40, 686, .75);
    Text(TEXT("Demo audiovisual: CC BY-SA 4.0"), 40, 712, .68f);
    Draw.Comment(
        TEXT("Playback and microphone buttons. Double-click DrawButton to inspect its Blueprint implementation."), 100,
        470, 850, 4600);
    Draw = Graph(HUD, TEXT("06_Draw_Status"));
    Row = 0;
    Compact = false;
    Tail = Exec(Draw.Custom(TEXT("DrawReadouts"), 1400, 650));
    Rect(360, 20, 320, 420, TEXT("(R=0.025,G=0.034,B=0.05,A=0.96)"));
    auto DemoValue = [&](const TCHAR *Name, int Y)
    {
        return Draw.Get(Name, 300, Y, Actor->GeneratedClass, Draw.Get(TEXT("Demo"), 0, Y + 150)->GetValuePin())
            ->GetValuePin();
    };
    Text(TEXT(""), 380, 40, .75, DemoValue(TEXT("Status"), 1000 + Row * 800));
    auto *Progress = Draw.Call(UKismetStringLibrary::StaticClass(), TEXT("BuildString_Double"), 650, 1000 + Row * 800);
    Link(DemoValue(TEXT("Progress"), Progress->NodePosY + 250), Pin(Progress, TEXT("InDouble")));
    Value(Progress, TEXT("Prefix"), TEXT("Analysis (0-1): "));
    Text(TEXT(""), 380, 74, .75, Result(Progress));
    auto *Component = DemoValue(TEXT("LAM"), 1000 + Row * 800);
    auto *Info = Draw.Call(ULAMAudio2ExpressionComponent::StaticClass(), TEXT("GetPlaybackInfo"), 300,
                           1000 + Row * 800 + 400, Component);
    auto *Fields = Draw.Break(FLAMPlaybackInfo::StaticStruct(), Result(Info), 600, Info->NodePosY + 160);
    auto Number = [&](const TCHAR *Prefix, UEdGraphPin *Data, float Y)
    {
        auto *S = Draw.Call(UKismetStringLibrary::StaticClass(), TEXT("BuildString_Double"), 650, 1000 + Row * 800);
        Link(Data, Pin(S, TEXT("InDouble")));
        Value(S, TEXT("Prefix"), Prefix);
        Text(TEXT(""), 380, Y, .75, Result(S));
    };
    Number(TEXT("Playback seconds: "), Pin(Fields, TEXT("Position")), 108);
    auto *Jaw = Draw.Call(ULAMAudio2ExpressionComponent::StaticClass(), TEXT("GetARKitCurveValue"), 200,
                          1000 + Row * 800 + 300, Component);
    Value(Jaw, TEXT("Name"), TEXT("jawOpen"));
    Number(TEXT("jawOpen: "), Result(Jaw), 142);
    auto *Interval = Draw.Call(ULAMAudio2ExpressionComponent::StaticClass(), TEXT("GetLiveInferenceInterval"), 200,
                               1000 + Row * 800 + 300, Component);
    Number(TEXT("Live interval (ms): "), Result(Interval), 176);
    auto *Settings = Draw.Get(TEXT("PlaybackSettings"), 200, 1000 + Row * 800 + 300,
                              ULAMAudio2ExpressionComponent::StaticClass(), Component);
    auto *Audio =
        Draw.Break(FLAMAudioPlaybackSettings::StaticStruct(), Settings->GetValuePin(), 450, Settings->NodePosY + 200);
    Number(TEXT("Volume: "), Pin(Audio, TEXT("Volume")), 210);
    auto *MixString = Draw.Call(UKismetSystemLibrary::StaticClass(), TEXT("GetDisplayName"), 650, 1000 + Row * 800);
    Link(Pin(Audio, TEXT("OutputSubmix")), Pin(MixString, TEXT("Object")));
    Text(TEXT(""), 380, 244, .75, Result(MixString));
    auto *Selected = Draw.Math(TEXT("Add_IntInt"), 350, 1000 + Row * 800 + 250);
    Link(DemoValue(TEXT("Selected"), Selected->NodePosY + 180), Pin(Selected, TEXT("A")));
    Value(Selected, TEXT("B"), TEXT("1"));
    auto *Selection = Draw.Call(UKismetStringLibrary::StaticClass(), TEXT("BuildString_Int"), 650, 1000 + Row * 800);
    Link(Result(Selected), Pin(Selection, TEXT("InInt")));
    Value(Selection, TEXT("Prefix"), TEXT("Selected voice: "));
    Text(TEXT(""), 380, 278, .75, Result(Selection));
    auto *Muted = Draw.Call(UKismetStringLibrary::StaticClass(), TEXT("BuildString_Bool"), 650, 1000 + Row * 800);
    Link(Pin(Audio, TEXT("bMuted")), Pin(Muted, TEXT("InBool")));
    Value(Muted, TEXT("Prefix"), TEXT("Muted: "));
    Text(TEXT(""), 380, 312, .75, Result(Muted));
    auto *Metrics = Draw.Call(ULAMAudio2ExpressionComponent::StaticClass(), TEXT("GetLiveMetrics"), 100,
                              1000 + Row * 800 + 220, Component);
    auto *Live = Draw.Break(FLAMLiveMetrics::StaticStruct(), Result(Metrics), 400, Metrics->NodePosY + 180);
    Number(TEXT("Delay (ms): "), Pin(Live, TEXT("EffectivePresentationDelayMilliseconds")), 346);
    Number(TEXT("Inference P95 (ms): "), Pin(Live, TEXT("InferenceP95Milliseconds")), 380);
    // Each row fits the graph viewport; comments describe the actual displayed element.
    for (int I = 0; I < Row; ++I)
        Draw.Comment(*FString::Printf(TEXT("HUD drawing step %02d - scaled coordinates; standard Canvas nodes"), I + 1),
                     -80, 900 + I * 800, 3050, 760);

    Graph Click(HUD, TEXT("03_Button_Clicks"));
    auto *E = Click.Event(AHUD::StaticClass(), TEXT("ReceiveHitBoxClick"));
    Tail = Exec(E);
    auto *Switch = Click.Node<UK2Node_SwitchName>(350, 0);
    for (auto *Box : Boxes)
        Switch->PinNames.Add(FName(Box));
    const TCHAR *Controls[] = {TEXT("Pause"),    TEXT("Replay"), TEXT("Mute"), TEXT("Output"), TEXT("Mic"),
                               TEXT("Interval"), TEXT("Fade"),   TEXT("Down"), TEXT("Up")};
    const TCHAR *Commands[] = {TEXT("TogglePlayback"), TEXT("Replay"),           TEXT("ToggleMute"),
                               TEXT("CycleOutput"),    TEXT("ToggleMicrophone"), TEXT("ChangeLiveInterval"),
                               TEXT("FadeStop"),       TEXT("VolumeDown"),       TEXT("VolumeUp")};
    for (auto *Name : Controls)
        Switch->PinNames.Add(FName(Name));
    Switch->AllocateDefaultPins();
    Link(Tail, Pin(Switch, TEXT("execute")));
    Link(Pin(E, TEXT("BoxName")), Pin(Switch, TEXT("Selection")));
    for (int I = 0; I < 15; ++I)
    {
        auto *C = Click.Call(Actor->GeneratedClass, I < 6 ? TEXT("SelectSample") : Commands[I - 6], 850, I * 220,
                             Click.Get(TEXT("Demo"), 600, I * 220 + 100)->GetValuePin());
        Link(Pin(Switch, I < 6 ? Boxes[I] : Controls[I - 6]), Pin(C, TEXT("execute")));
        if (I < 6)
            Value(C, TEXT("Index"), FString::FromInt(I));
    }
    Click.Comment(TEXT("Clickable buttons call exactly the same BP_FaceDemo events as the keyboard."), -80, -100, 1550,
                  3570);
    FBlueprintEditorUtils::RemoveGraph(HUD, DrawDeclarations.G);
}
} // namespace LAMDemoGraphs

bool ULAMDemoBlueprintLibrary::RebuildFaceDemoBlueprints()
{
    using namespace LAMDemoGraphs;
    Errors = 0;
    auto *BP = Blueprint(TEXT("BP_FaceDemo"), AActor::StaticClass());
    auto *Root = BP->SimpleConstructionScript->CreateNode(USceneComponent::StaticClass(), TEXT("Root"));
    BP->SimpleConstructionScript->AddNode(Root);
    auto *Face = BP->SimpleConstructionScript->CreateNode(USkeletalMeshComponent::StaticClass(), TEXT("Face"));
    Root->AddChildNode(Face);
    auto *Mesh = CastChecked<USkeletalMeshComponent>(Face->ComponentTemplate);
    Mesh->SetSkeletalMeshAsset(LoadObject<USkeletalMesh>(nullptr, *(Base + TEXT("/Character/Face52"))));
    Mesh->SetAnimInstanceClass(
        LoadObject<UAnimBlueprint>(nullptr, *(Base + TEXT("/Blueprints/ABP_Face52")))->GeneratedClass);
    Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Mesh->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
    auto *Camera = BP->SimpleConstructionScript->CreateNode(UCameraComponent::StaticClass(), TEXT("Camera"));
    Root->AddChildNode(Camera);
    auto *Cam = CastChecked<UCameraComponent>(Camera->ComponentTemplate);
    Cam->SetRelativeLocation(FVector(-18, 105, 145));
    Cam->SetRelativeRotation(FRotator(0, -90, 0));
    Cam->FieldOfView = 35;
    Cam->bConstrainAspectRatio = false;
    BP->SimpleConstructionScript->AddNode(
        BP->SimpleConstructionScript->CreateNode(ULAMAudio2ExpressionComponent::StaticClass(), TEXT("LAM")));
    Variable(BP, TEXT("Samples"), Type(UEdGraphSchema_K2::PC_Object, USoundWave::StaticClass(), true));
    Variable(BP, TEXT("OutputSubmixes"), Type(UEdGraphSchema_K2::PC_Object, USoundSubmix::StaticClass(), true));
    Variable(BP, TEXT("LastClip"), Type(UEdGraphSchema_K2::PC_Object, ULAMExpressionClip::StaticClass()));
    Variable(BP, TEXT("Selected"), Type(UEdGraphSchema_K2::PC_Int), TEXT("0"));
    Variable(BP, TEXT("OutputIndex"), Type(UEdGraphSchema_K2::PC_Int), TEXT("0"));
    Variable(BP, TEXT("Status"), Type(UEdGraphSchema_K2::PC_String), TEXT("Ready"));
    Variable(BP, TEXT("bAnalyzing"), Type(UEdGraphSchema_K2::PC_Boolean), TEXT("false"));
    Variable(BP, TEXT("Progress"), Type(UEdGraphSchema_K2::PC_Real), TEXT("0"));
    // First compile event declarations, so callers resolve proper Blueprint signatures.
    Graph Declarations(BP, TEXT("Declarations"));
    Declarations.Custom(TEXT("SelectSample"), 0, 0, true);
    for (const TCHAR *N :
         {TEXT("TogglePlayback"), TEXT("Replay"), TEXT("ToggleMute"), TEXT("CycleOutput"), TEXT("ToggleMicrophone"),
          TEXT("ChangeLiveInterval"), TEXT("FadeStop"), TEXT("VolumeDown"), TEXT("VolumeUp")})
        Declarations.Custom(N);
    if (!Compile(BP))
        return false;
    ActorGraphs(BP);
    FBlueprintEditorUtils::RemoveGraph(BP, Declarations.G);
    if (!Compile(BP))
        return false;
    for (const auto &G : BP->UbergraphPages)
        if (G->GetFName() == TEXT("02_Analyze_And_Play"))
            BP->LastEditedDocuments.Add(FEditedDocumentInfo(G, FVector2D(2450, -180), .65f));
    auto SetArray = [&](const TCHAR *Name, const TArray<UObject *> &Values)
    {
        auto *P = FindFProperty<FArrayProperty>(BP->GeneratedClass, FName(Name));
        FScriptArrayHelper A(P, P->ContainerPtrToValuePtr<void>(BP->GeneratedClass->GetDefaultObject()));
        A.Resize(Values.Num());
        for (int I = 0; I < Values.Num(); ++I)
            CastFieldChecked<FObjectPropertyBase>(P->Inner)->SetObjectPropertyValue(A.GetRawPtr(I), Values[I]);
    };
    TArray<UObject *> Waves;
    for (const TCHAR *Name : {TEXT("F1_anger_regular_31"), TEXT("F1_disgust_regular_38"), TEXT("F1_fear_regular_23"),
                              TEXT("F1_happy_regular_38"), TEXT("F1_sad_regular_10"), TEXT("F1_surprise_regular_11")})
        Waves.Add(LoadObject<USoundWave>(nullptr, *(Base + TEXT("/Audio/") + Name)));
    SetArray(TEXT("Samples"), Waves);
    SetArray(TEXT("OutputSubmixes"), {LoadObject<USoundSubmix>(nullptr, *(Base + TEXT("/Audio/SM_Dialogue"))),
                                      LoadObject<USoundSubmix>(nullptr, *(Base + TEXT("/Audio/SM_Alternate")))});
    if (!Save(BP))
        return false;
    auto *HUD = Blueprint(TEXT("BP_FaceDemoHUD"), AHUD::StaticClass());
    Variable(HUD, TEXT("Demo"), Type(UEdGraphSchema_K2::PC_Object, BP->GeneratedClass));
    Variable(HUD, TEXT("UIScale"), Type(UEdGraphSchema_K2::PC_Real), TEXT("1"));
    if (!Compile(HUD))
        return false;
    HUDGraphs(HUD, BP);
    if (!Compile(HUD) || !Save(HUD))
        return false;
    auto *Mode = Blueprint(TEXT("BP_FaceDemoGameMode"), AGameModeBase::StaticClass());
    if (!Compile(Mode))
        return false;
    auto *CDO = CastChecked<AGameModeBase>(Mode->GeneratedClass->GetDefaultObject());
    CDO->HUDClass = HUD->GeneratedClass;
    CDO->DefaultPawnClass = nullptr;
    return Save(Mode) && ValidateFaceDemoBlueprints();
}

bool ULAMDemoBlueprintLibrary::ValidateFaceDemoBlueprints()
{
    using namespace LAMDemoGraphs;
    int32 Nodes = 0, Async = 0, NativeDemoCalls = 0;
    for (const TCHAR *Name : {TEXT("BP_FaceDemo"), TEXT("BP_FaceDemoHUD"), TEXT("BP_FaceDemoGameMode")})
    {
        auto *BP = LoadObject<UBlueprint>(nullptr, *(Base + TEXT("/Blueprints/") + Name));
        if (!BP)
            return false;
        FKismetEditorUtilities::CompileBlueprint(BP);
        if (BP->Status == BS_Error)
            return false;
        if (BP->ParentClass->GetOutermost()->GetName() != TEXT("/Script/Engine"))
            return false;
        TArray<UEdGraph *> Graphs;
        BP->GetAllGraphs(Graphs);
        for (auto *G : Graphs)
            for (const auto &N : G->Nodes)
            {
                ++Nodes;
                if (Cast<UK2Node_AsyncAction>(N))
                    ++Async;
                if (auto *C = Cast<UK2Node_CallFunction>(N))
                    if (auto *F = C->GetTargetFunction())
                        if (F->GetOutermost()->GetName() == TEXT("/Script/LAMDemo"))
                            ++NativeDemoCalls;
            }
        if (!Save(BP))
            return false;
    }
    UE_LOG(LogTemp, Display, TEXT("LAM_BP_DEMO_GRAPHS nodes=%d async=%d native_demo_calls=%d"), Nodes, Async,
           NativeDemoCalls);
    return Async == 1 && NativeDemoCalls == 0 && Nodes > 100;
}
