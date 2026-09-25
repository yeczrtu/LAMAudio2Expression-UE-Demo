#include "LAMDemoActor.h"
#include "LAMAnalyzeAsync.h"
#include "LAMAudio2ExpressionComponent.h"
#include "LAMSettings.h"
#include "Engine/Canvas.h"
#include "EngineUtils.h"
#include "Engine/Engine.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/SpectatorPawn.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/Parse.h"
#include "HAL/PlatformMisc.h"
#include "Sound/SoundWave.h"
#include "UnrealClient.h"
#include "Components/SkeletalMeshComponent.h"
#include "Animation/AnimInstance.h"
#include "Engine/SkeletalMesh.h"
#include "UObject/ConstructorHelpers.h"
#include "AnimNode_LAMARKit.h"
#include "Animation/AnimClassInterface.h"
#include "UObject/UnrealType.h"
#include "LAMPlaybackTestActor.h"
ALAMDemoActor::ALAMDemoActor()
{
    PrimaryActorTick.bCanEverTick = true;
    Expression = CreateDefaultSubobject<ULAMAudio2ExpressionComponent>(TEXT("LAM"));
    static ConstructorHelpers::FObjectFinder<USkeletalMesh> Mesh(
        TEXT("/Engine/EngineMeshes/SkeletalCube.SkeletalCube"));
    TestMesh = Mesh.Object;
}
ALAMDemoGameMode::ALAMDemoGameMode()
{
    HUDClass = ALAMDemoHUD::StaticClass();
    DefaultPawnClass = ASpectatorPawn::StaticClass();
}
void ALAMDemoGameMode::BeginPlay()
{
    Super::BeginPlay();
    if (FParse::Param(FCommandLine::Get(), TEXT("LAMPlaybackTest")) || FParse::Param(FCommandLine::Get(), TEXT("LAMLiveIntervalTest")))
        GetWorld()->SpawnActor<ALAMPlaybackTestActor>();
    else if (FParse::Param(FCommandLine::Get(), TEXT("LAMTest")))
    {
        bool Found = false;
        for (TActorIterator<ALAMDemoActor> It(GetWorld()); It; ++It) Found = true;
        if (!Found)
        {
            auto* Test = GetWorld()->SpawnActorDeferred<ALAMDemoActor>(ALAMDemoActor::StaticClass(), FTransform::Identity);
            Test->Sound = LoadObject<USoundWave>(nullptr, TEXT("/Game/Audio/speech_stream.speech_stream"));
            Test->FinishSpawning(FTransform::Identity);
        }
    }
}
void ALAMDemoActor::BeginPlay()
{
    Super::BeginPlay();
    Start = FPlatformTime::Seconds();
    TestMode = FParse::Param(FCommandLine::Get(), TEXT("LAMTest"));
    if (Model)
        GetMutableDefault<ULAMSettings>()->Model = Model;
    if (FParse::Param(FCommandLine::Get(), TEXT("LAMCPU")))
        GetMutableDefault<ULAMSettings>()->bPreferGPU = false;
    FString Asset;
    if (FParse::Value(FCommandLine::Get(), TEXT("LAMSound="), Asset))
        Sound = LoadObject<USoundWave>(nullptr, *Asset);
    Expression->OnStatus.AddDynamic(this, &ALAMDemoActor::LiveStatus);
    if (FParse::Param(FCommandLine::Get(), TEXT("LAMBlueprintTest")))
    {
        UClass *BP = LoadClass<AActor>(nullptr, TEXT("/Game/Examples/BP_LAMPlayback.BP_LAMPlayback_C"));
        UClass *ABP = LoadClass<UAnimInstance>(nullptr, TEXT("/Game/Examples/ABP_LAMCurves.ABP_LAMCurves_C"));
        if (!BP || !ABP || !TestMesh)
        {
            Report(false, TEXT("Blueprint test assets missing"));
            return;
        }
        AActor *Actor = GetWorld()->SpawnActor<AActor>(BP);
        Expression = Actor->FindComponentByClass<ULAMAudio2ExpressionComponent>();
        BlueprintMesh = NewObject<USkeletalMeshComponent>(Actor);
        BlueprintMesh->SetSkeletalMesh(TestMesh);
        BlueprintMesh->SetAnimInstanceClass(ABP);
        BlueprintMesh->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
        BlueprintMesh->RegisterComponent();
        BlueprintMesh->AddTickPrerequisiteComponent(Expression);
        Stage = 20;
        return;
    }
    Action = ULAMAnalyzeAsync::AnalyzeSoundWaveAsync(Expression, Sound, FLAMAnalysisSettings());
    Action->Completed.AddDynamic(this, &ALAMDemoActor::Complete);
    Action->Failed.AddDynamic(this, &ALAMDemoActor::Failed);
    Action->Progress.AddDynamic(this, &ALAMDemoActor::Progressed);
    Action->Activate();
}
void ALAMDemoActor::Complete(ULAMExpressionClip *Clip, float, FString)
{
    if (!Clip || Clip->Curves.Num() != int((int64(Clip->NumSamples) * 30 + 15999) / 16000) * 52)
    {
        Report(false, Clip ? FString::Printf(TEXT("Invalid frame count samples=%d values=%d duration=%.9f"),
                                             Clip->NumSamples, Clip->Curves.Num(), Clip->Duration)
                           : TEXT("Null clip"));
        return;
    }
    for (float V : Clip->Curves)
        if (!FMath::IsFinite(V) || V < 0 || V > 1)
        {
            Report(false, TEXT("Invalid curve value"));
            return;
        }
    Progress = 1;
    if (FParse::Param(FCommandLine::Get(), TEXT("LAMAnalyzeOnly")))
    {
        Report(true, FString::Printf(TEXT("%s samples=%d frames=%d inference_seconds=%.3f total_seconds=%.3f"),
                                     *Clip->Backend, Clip->NumSamples, Clip->Curves.Num() / 52, Clip->AnalysisSeconds,
                                     FPlatformTime::Seconds() - Start));
        return;
    }
    Status = FString::Printf(TEXT("%s | %.2fs analyzed in %.2fs | Space: pause/resume | R: replay | M: microphone"),
                             *Clip->Backend, Clip->Duration, Clip->AnalysisSeconds);
    Expression->PlayExpressionClip(Clip);
    PlayStart = FPlatformTime::Seconds();
    Stage = 1;
    UE_LOG(LogTemp, Display, TEXT("LAM_ANALYSIS_SUCCESS backend=%s duration=%.6f frames=%d analysis_seconds=%.3f"),
           *Clip->Backend, Clip->Duration, Clip->Curves.Num() / 52, Clip->AnalysisSeconds);
    if (FParse::Param(FCommandLine::Get(), TEXT("LAMLiveTest")))
    {
        Expression->StartPCMStream(FLAMAnalysisSettings());
        Stage = 10;
        PlayStart = FPlatformTime::Seconds();
    }
}
void ALAMDemoActor::Failed(ULAMExpressionClip *, float, FString Error)
{
    Report(false, Error);
}
void ALAMDemoActor::Progressed(ULAMExpressionClip *, float P, FString)
{
    Progress = P;
    Status = FString::Printf(TEXT("Analyzing SoundWave: %.0f%%"), P * 100);
}
void ALAMDemoActor::LiveStatus(FString Message)
{
    Status = Message;
}
void ALAMDemoActor::Report(bool Success, const FString &Message)
{
    Status = Message;
    UE_LOG(LogTemp, Display, TEXT("LAM_TEST_%s %s"), Success ? TEXT("PASS") : TEXT("FAIL"), *Message);
    FString Path = FPaths::ProjectSavedDir() / TEXT("LAMSmoke.txt");
    FParse::Value(FCommandLine::Get(), TEXT("LAMReport="), Path);
    FFileHelper::SaveStringToFile((Success ? TEXT("PASS ") : TEXT("FAIL ")) + Message, *Path);
    if (TestMode)
        FPlatformMisc::RequestExitWithStatus(false, Success ? 0 : 1);
}
void ALAMDemoActor::Tick(float Delta)
{
    Super::Tick(Delta);
    const double Now = FPlatformTime::Seconds();
    if (TestMode)
    {
        if (Stage >= 20)
        {
            if (Now - Start > 60)
            {
                Report(false, TEXT("Blueprint playback timed out"));
                return;
            }
            const auto Current = Expression->GetCurrentExpressionFrame();
            UAnimInstance *Anim = BlueprintMesh->GetAnimInstance();
            FAnimNode_LAMARKit *Node = nullptr;
            if (Anim)
                for (const FStructProperty *Property :
                     IAnimClassInterface::GetFromClass(Anim->GetClass())->GetAnimNodeProperties())
                    if (Property->Struct == FAnimNode_LAMARKit::StaticStruct())
                        Node = Property->ContainerPtrToValuePtr<FAnimNode_LAMARKit>(Anim);
            if (!Node)
            {
                Report(false, TEXT("Compiled LAM AnimNode missing"));
                return;
            }
            if (Stage == 20 && Current.bValid && Current.TimeSeconds > 0.5f)
            {
                const float Actual = Anim->GetCurveValue(TEXT("jawOpen"));
                const float Expected = Expression->GetARKitCurveValue(TEXT("jawOpen"));
                if (!(Actual > 0 && FMath::Abs(Actual - Expected) < 0.01f))
                {
                    Report(false, TEXT("AnimGraph source mismatch"));
                    return;
                }
                auto *Profile = NewObject<ULAMCurveProfile>(Anim);
                FLAMCurveRule Rule;
                Rule.SourceName = TEXT("jawOpen");
                Rule.TargetName = TEXT("TestJaw");
                Rule.Scale = 0;
                Rule.Offset = .8f;
                Profile->Rules.Add(Rule);
                Rule.SourceName = TEXT("mouthClose");
                Rule.bEnabled = false;
                Profile->Rules.Add(Rule);
                Node->CurveProfile = Profile;
                Node->Alpha = .5f;
                Stage = 22;
                PlayStart = Now;
            }
            else if (Stage == 22 && Now - PlayStart > .1)
            {
                if (FMath::Abs(Anim->GetCurveValue(TEXT("TestJaw")) - .4f) > .001f ||
                    Anim->GetCurveValue(TEXT("mouthClose")) != 0)
                {
                    Report(false, TEXT("Curve profile remap/scale/offset/mask/alpha mismatch"));
                    return;
                }
                Expression->Stop();
                Stage = 23;
                PlayStart = Now;
            }
            else if (Stage == 23 && Now - PlayStart > .2)
            {
                Report(Anim->GetCurveValue(TEXT("TestJaw")) == 0,
                       TEXT("Blueprint async + AnimGraph source, remap, scale, offset, mask, alpha, stop fade"));
                Stage = 24;
            }
            return;
        }
        if (Now - Start > 600)
        {
            Report(false, TEXT("Timeout"));
            return;
        }
        if (Stage == 10)
        {
            const int64 Target = int64((Now - PlayStart) * 48000);
            const int Count = int(FMath::Min<int64>(Target - LiveSamples, 48000));
            if (Count > 0)
            {
                TArray<float> PCM;
                PCM.SetNumUninitialized(Count);
                for (int I = 0; I < Count; ++I)
                    PCM[I] = .2f * FMath::Sin(2 * PI * 145 * (LiveSamples + I) / 48000.0);
                if (!Expression->PushPCMAudio(PCM, 48000, 1))
                {
                    Report(false, TEXT("PCM input rejected"));
                    return;
                }
                LiveSamples += Count;
            }
            if (Now - PlayStart > 12)
            {
                const bool OK = Expression->GetCurrentExpressionFrame().bValid &&
                                Expression->InferenceP95Milliseconds > 0 &&
                                Expression->InferenceP95Milliseconds < 333.333f;
                Report(OK, FString::Printf(TEXT("PCM live input p95=%.2f ms valid=%d"),
                                           Expression->InferenceP95Milliseconds,
                                           Expression->GetCurrentExpressionFrame().bValid));
                Stage = 11;
                Expression->Stop();
            }
            return;
        }
        if (Stage == 1 && Now - PlayStart > .3)
        {
            if (!Expression->GetCurrentExpressionFrame().bValid)
            {
                Report(false, TEXT("No playback clock"));
                return;
            }
            FString CapturePath;
            if (FParse::Value(FCommandLine::Get(), TEXT("LAMCapture="), CapturePath))
                FScreenshotRequest::RequestScreenshot(CapturePath, false, false);
            Expression->Pause();
            PausedTime = Expression->GetCurrentExpressionFrame().TimeSeconds;
            Stage = 2;
            PlayStart = Now;
        }
        else if (Stage == 2 && Now - PlayStart > .1)
        {
            if (FMath::Abs(Expression->GetCurrentExpressionFrame().TimeSeconds - PausedTime) > 1e-5f)
            {
                Report(false, TEXT("Pause changed curve time"));
                return;
            }
            Expression->Resume();
            Expression->Seek(0);
            Stage = 3;
            PlayStart = Now;
        }
        else if (Stage == 3 && Now - PlayStart > .3)
        {
            Expression->Stop();
            Stage = 4;
            PlayStart = Now;
        }
        else if (Stage == 4 && Now - PlayStart > .2)
        {
            Report(Expression->GetCurrentExpressionFrame().Weight == 0,
                   TEXT("Analyze, playback, pause, seek, stop completed"));
            Stage = 5;
        }
        return;
    }
    if (auto *PC = GetWorld()->GetFirstPlayerController())
    {
        static const FKey Space(TEXT("SpaceBar")), R(TEXT("R")), M(TEXT("M"));
        if (PC->WasInputKeyJustPressed(Space))
        {
            if (Stage == 1)
            {
                Expression->Pause();
                Stage = 2;
            }
            else
            {
                Expression->Resume();
                Stage = 1;
            }
        }
        if (PC->WasInputKeyJustPressed(R))
            Expression->Seek(0);
        if (PC->WasInputKeyJustPressed(M))
            Expression->StartMicrophone(FLAMAnalysisSettings());
    }
}
void ALAMDemoHUD::DrawHUD()
{
    Super::DrawHUD();
    if (!Canvas)
        return;
    ALAMDemoActor *Demo = nullptr;
    for (TActorIterator<ALAMDemoActor> I(GetWorld()); I; ++I)
    {
        Demo = *I;
        break;
    }
    if (!Demo)
        return;
    DrawRect(FLinearColor(.018f, .024f, .037f, 1), 0, 0, Canvas->SizeX, Canvas->SizeY);
    DrawText(TEXT("LAM / AUDIO TO ARKIT"), FLinearColor(.3f, .9f, .8f), 28, 20, nullptr, 1.7f);
    DrawText(Demo->Status, FLinearColor::White, 28, 58, nullptr, 1.f);
    auto Frame = Demo->Expression->GetCurrentExpressionFrame();
    auto Names = ULAMAudio2ExpressionComponent::GetARKitCurveNames();
    const float Column = (Canvas->SizeX - 56.f) / 2, Row = FMath::Min(25.f, (Canvas->SizeY - 120.f) / 26);
    for (int I = 0; I < 52; ++I)
    {
        float X = 28 + (I / 26) * Column, Y = 100 + (I % 26) * Row,
              V = Frame.Values.IsValidIndex(I) ? Frame.Values[I] : 0;
        DrawText(Names[I].ToString(), FLinearColor(.7f, .76f, .83f), X, Y, nullptr, .9f);
        DrawRect(FLinearColor(.07f, .1f, .15f), X + 190, Y + 3, Column - 270, 10);
        DrawRect(FLinearColor(.25f, .82f, .67f), X + 190, Y + 3, (Column - 270) * V, 10);
        DrawText(FString::Printf(TEXT("%.3f"), V), FLinearColor::White, X + Column - 60, Y, nullptr, .85f);
    }
}
