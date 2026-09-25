#include "LAMEditorLibrary.h"
#include "LAMAnalyzeAsync.h"
#include "LAMAudio2ExpressionComponent.h"
#include "AnimGraphNode_LAMARKit.h"
#include "AnimGraphNode_Root.h"
#include "AnimGraphNode_LocalRefPose.h"
#include "Animation/AnimBlueprint.h"
#include "Animation/AnimBlueprintGeneratedClass.h"
#include "Animation/Skeleton.h"
#include "Sound/SoundWave.h"
#include "AnimationGraphSchema.h"
#include "Factories/AnimBlueprintFactory.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "K2Node_Event.h"
#include "K2Node_AsyncAction.h"
#include "K2Node_CallFunction.h"
#include "K2Node_VariableGet.h"
#include "EdGraphSchema_K2.h"
#include "Engine/SimpleConstructionScript.h"
#include "Engine/SCS_Node.h"
#include "GameFramework/Actor.h"
#include "UObject/SavePackage.h"
#include "Misc/PackageName.h"
#include "NNEModelData.h"

static bool SaveLAMAsset(UObject *Object)
{
    FSavePackageArgs Args;
    Args.TopLevelFlags = RF_Public | RF_Standalone;
    Args.SaveFlags = SAVE_NoError;
    return UPackage::SavePackage(Object->GetOutermost(), Object,
                                 *FPackageName::LongPackageNameToFilename(Object->GetOutermost()->GetName(),
                                                                          FPackageName::GetAssetPackageExtension()),
                                 Args);
}
bool ULAMEditorLibrary::CreateExamples()
{
    if (auto *Model = LoadObject<UNNEModelData>(nullptr, TEXT("/LAMAudio2Expression/Models/LAM_A2E.LAM_A2E")))
    {
        TArray<FString> Runtimes = {TEXT("NNERuntimeORTCpu"), TEXT("NNERuntimeORTDml")};
        Model->SetTargetRuntimes(Runtimes);
        SaveLAMAsset(Model);
    }
    // These examples are created once. Never overwrite a user's edited graphs.
    if (!LoadObject<UBlueprint>(nullptr, TEXT("/Game/Examples/BP_LAMPlayback.BP_LAMPlayback"), nullptr, LOAD_NoWarn))
    {
        auto *Package = CreatePackage(TEXT("/Game/Examples/BP_LAMPlayback"));
        auto *BP = FKismetEditorUtilities::CreateBlueprint(AActor::StaticClass(), Package, TEXT("BP_LAMPlayback"),
                                                           BPTYPE_Normal, UBlueprint::StaticClass(),
                                                           UBlueprintGeneratedClass::StaticClass());
        auto *SCS = BP->SimpleConstructionScript.Get();
        SCS->AddNode(SCS->CreateNode(ULAMAudio2ExpressionComponent::StaticClass(), TEXT("LAM")));
        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BP);
        FKismetEditorUtilities::CompileBlueprint(BP);
        auto *Graph = FBlueprintEditorUtils::FindEventGraph(BP);
        const auto *Schema = GetDefault<UEdGraphSchema_K2>();
        FGraphNodeCreator<UK2Node_Event> E(*Graph);
        auto *Begin = E.CreateNode();
        Begin->EventReference.SetExternalMember(TEXT("ReceiveBeginPlay"), AActor::StaticClass());
        Begin->bOverrideFunction = true;
        E.Finalize();
        Begin->NodePosX = 0;
        FGraphNodeCreator<UK2Node_VariableGet> V(*Graph);
        auto *Component = V.CreateNode();
        Component->VariableReference.SetSelfMember(TEXT("LAM"));
        V.Finalize();
        Component->NodePosY = 160;
        FGraphNodeCreator<UK2Node_AsyncAction> A(*Graph);
        auto *Analyze = A.CreateNode();
        Analyze->InitializeProxyFromFunction(ULAMAnalyzeAsync::StaticClass()->FindFunctionByName(
            GET_FUNCTION_NAME_CHECKED(ULAMAnalyzeAsync, AnalyzeSoundWaveAsync)));
        A.Finalize();
        Analyze->NodePosX = 320;
        Analyze->FindPinChecked(TEXT("SoundWave"))->DefaultObject =
            LoadObject<USoundWave>(nullptr, TEXT("/Game/Audio/speech_stream.speech_stream"));
        FGraphNodeCreator<UK2Node_CallFunction> P(*Graph);
        auto *Play = P.CreateNode();
        Play->SetFromFunction(ULAMAudio2ExpressionComponent::StaticClass()->FindFunctionByName(
            GET_FUNCTION_NAME_CHECKED(ULAMAudio2ExpressionComponent, PlayExpressionClip)));
        P.Finalize();
        Play->NodePosX = 700;
        bool OK = true;
        if (!Component->GetValuePin())
        {
            UE_LOG(LogTemp, Error, TEXT("LAM example: component variable pin is missing"));
            return false;
        }
        OK &= Schema->TryCreateConnection(Begin->FindPinChecked(UEdGraphSchema_K2::PN_Then),
                                          Analyze->FindPinChecked(UEdGraphSchema_K2::PN_Execute));
        OK &= Schema->TryCreateConnection(Component->GetValuePin(), Analyze->FindPinChecked(TEXT("Component")));
        OK &= Schema->TryCreateConnection(Component->GetValuePin(), Play->FindPinChecked(UEdGraphSchema_K2::PN_Self));
        OK &= Schema->TryCreateConnection(Analyze->FindPinChecked(TEXT("Completed")), Play->GetExecPin());
        OK &= Schema->TryCreateConnection(Analyze->FindPinChecked(TEXT("Clip")), Play->FindPinChecked(TEXT("Clip")));
        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BP);
        FKismetEditorUtilities::CompileBlueprint(BP);
        if (!OK || BP->Status == BS_Error || !SaveLAMAsset(BP))
            return false;
    }
    if (!LoadObject<UAnimBlueprint>(nullptr, TEXT("/Game/Examples/ABP_LAMCurves.ABP_LAMCurves"), nullptr, LOAD_NoWarn))
    {
        auto *Skeleton =
            LoadObject<USkeleton>(nullptr, TEXT("/Engine/EngineMeshes/SkeletalCube_Skeleton.SkeletalCube_Skeleton"));
        if (!Skeleton)
            return false;
        auto *Factory = NewObject<UAnimBlueprintFactory>();
        Factory->TargetSkeleton = Skeleton;
        auto *Package = CreatePackage(TEXT("/Game/Examples/ABP_LAMCurves"));
        auto *BP = Cast<UAnimBlueprint>(Factory->FactoryCreateNew(
            UAnimBlueprint::StaticClass(), Package, TEXT("ABP_LAMCurves"), RF_Public | RF_Standalone, nullptr, GWarn));
        TArray<UEdGraph *> Graphs;
        BP->GetAllGraphs(Graphs);
        UEdGraph *Graph = nullptr;
        UAnimGraphNode_Root *Root = nullptr;
        for (auto *G : Graphs)
            for (const auto &N : G->Nodes)
                if (auto *R = Cast<UAnimGraphNode_Root>(N.Get()))
                {
                    Graph = G;
                    Root = R;
                }
        if (!Graph || !Root)
            return false;
        FGraphNodeCreator<UAnimGraphNode_LocalRefPose> R(*Graph);
        auto *Ref = R.CreateNode();
        R.Finalize();
        Ref->NodePosX = -500;
        FGraphNodeCreator<UAnimGraphNode_LAMARKit> N(*Graph);
        auto *Node = N.CreateNode();
        N.Finalize();
        Node->NodePosX = -240;
        auto Find = [](UEdGraphNode *O, EEdGraphPinDirection D) -> UEdGraphPin *
        {
            for (auto *Pin : O->Pins)
                if (Pin->Direction == D && Pin->PinType.PinCategory == UEdGraphSchema_K2::PC_Struct)
                    return Pin;
            return nullptr;
        };
        const auto *Schema = Graph->GetSchema();
        if (!Schema->TryCreateConnection(Find(Ref, EGPD_Output), Node->FindPinChecked(TEXT("SourcePose"))) ||
            !Schema->TryCreateConnection(Find(Node, EGPD_Output), Find(Root, EGPD_Input)))
            return false;
        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BP);
        FKismetEditorUtilities::CompileBlueprint(BP);
        if (BP->Status == BS_Error || !SaveLAMAsset(BP))
            return false;
    }
    return true;
}
