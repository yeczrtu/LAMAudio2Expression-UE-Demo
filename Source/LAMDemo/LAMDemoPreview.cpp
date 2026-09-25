// SPDX-License-Identifier: MIT
#include "LAMDemoPreview.h"
#include "LAMAudio2ExpressionComponent.h"
#include "LAMAnalyzeAsync.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/SceneComponent.h"
#include "Camera/CameraComponent.h"
#include "Animation/AnimInstance.h"
#include "Engine/Canvas.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "Sound/SoundWave.h"
#include "Sound/SoundSubmix.h"
#include "AudioDevice.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "UnrealClient.h"

ALAMDemoPreview::ALAMDemoPreview()
{
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.TickGroup = TG_PostUpdateWork;
    RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
    Face = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("Face"));
    Face->SetupAttachment(RootComponent);
    Face->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Face->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
    Expression = CreateDefaultSubobject<ULAMAudio2ExpressionComponent>(TEXT("LAM"));
    Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
    Camera->SetupAttachment(RootComponent);
    Camera->SetRelativeLocation(FVector(-18, 105, 145));
    Camera->SetRelativeRotation(FRotator(0, -90, 0));
    Camera->FieldOfView = 35;
    Camera->bConstrainAspectRatio = false;
}
ALAMDemoPreviewGameMode::ALAMDemoPreviewGameMode()
{
    HUDClass = ALAMDemoPreviewHUD::StaticClass();
    DefaultPawnClass = nullptr;
}
void ALAMDemoPreviewGameMode::BeginPlay()
{
    Super::BeginPlay();
    // Installed Shipping engines may ignore a positional command-line map override.
    // Explicit test flags still reach the harness without changing engine build settings.
    if (FParse::Param(FCommandLine::Get(), TEXT("LAMTest")) ||
        FParse::Param(FCommandLine::Get(), TEXT("LAMPlaybackTest")) ||
        FParse::Param(FCommandLine::Get(), TEXT("LAMLiveIntervalTest")))
        UGameplayStatics::OpenLevel(this, TEXT("/Game/LAMDemo"));
}
void ALAMDemoPreview::BeginPlay()
{
    Super::BeginPlay();
    if (FParse::Param(FCommandLine::Get(), TEXT("LAMTest")) ||
        FParse::Param(FCommandLine::Get(), TEXT("LAMPlaybackTest")) ||
        FParse::Param(FCommandLine::Get(), TEXT("LAMLiveIntervalTest"))) return;
    Face->SetTickableWhenPaused(true);
    Started = FPlatformTime::Seconds();
    Expression->OnPlaybackEnded.AddDynamic(this, &ALAMDemoPreview::PlaybackEnded);
    if (OutputSubmixes.IsEmpty())
    {
        bOwnsOutputSubmixes = true;
        for (FName Name : {FName(TEXT("Dialogue")), FName(TEXT("Alternate"))})
            OutputSubmixes.Add(NewObject<USoundSubmix>(GetTransientPackage(), MakeUniqueObjectName(GetTransientPackage(), USoundSubmix::StaticClass(), Name)));
        if (auto Device = GetWorld()->GetAudioDevice(); Device.IsValid())
            for (auto Mix : OutputSubmixes) Device->RegisterSoundSubmix(Mix, true);
    }
    Expression->SetOutputSubmix(OutputSubmixes[0]);
    Face->AddTickPrerequisiteComponent(Expression);
    AddTickPrerequisiteComponent(Face);
    if (auto *PC = GetWorld()->GetFirstPlayerController())
    {
        PC->SetViewTarget(this);
        PC->bShowMouseCursor = true;
        PC->bEnableClickEvents = true;
        FInputModeGameAndUI Mode;
        Mode.SetHideCursorDuringCapture(false);
        PC->SetInputMode(Mode);
    }
    bTest = FParse::Param(FCommandLine::Get(), TEXT("LAMPluginDemoTest"));
    int Index = 0;
    FParse::Value(FCommandLine::Get(), TEXT("LAMDemoSample="), Index);
    SelectSample(Index);
}
void ALAMDemoPreview::EndPlay(const EEndPlayReason::Type Reason)
{
    if (bOwnsOutputSubmixes)
        if (auto Device = GetWorld()->GetAudioDevice(); Device.IsValid())
            for (auto Mix : OutputSubmixes) Device->UnregisterSoundSubmix(Mix, true);
    Super::EndPlay(Reason);
}
void ALAMDemoPreview::SelectSample(int32 Index)
{
    if (!Samples.IsValidIndex(Index) || !Samples[Index])
    {
        Status = TEXT("Demo audio missing");
        if (bTest)
            Report(false, Status);
        return;
    }
    Expression->CancelAnalysis();
    Expression->Stop();
    Selected = Index;
    bAnalyzing = true;
    bPaused = false;
    Progress = 0;
    Status = TEXT("Analyzing audio...");
    Action = ULAMAnalyzeAsync::AnalyzeSoundWaveAsync(Expression, Samples[Index], {});
    Action->Completed.AddDynamic(this, &ALAMDemoPreview::Completed);
    Action->Failed.AddDynamic(this, &ALAMDemoPreview::Failed);
    Action->Progress.AddDynamic(this, &ALAMDemoPreview::Updated);
    Action->Activate();
}
void ALAMDemoPreview::Completed(ULAMExpressionClip *Clip, float, FString)
{
    bAnalyzing = false;
    LastClip = Clip;
    Progress = 1;
    if (!Expression->PlayExpressionClip(Clip))
    {
        Failed(Clip, 0, TEXT("Playback could not start"));
        return;
    }
    PlaybackStarted = FPlatformTime::Seconds();
    Status = FString::Printf(TEXT("%s  |  %.1f s audio  |  Ready"), *Clip->Backend, Clip->Duration);
}
void ALAMDemoPreview::Failed(ULAMExpressionClip *, float, FString Error)
{
    bAnalyzing = false;
    Status = Error;
    if (bTest)
        Report(false, Error);
}
void ALAMDemoPreview::Updated(ULAMExpressionClip *, float Value, FString)
{
    Progress = Value;
}
void ALAMDemoPreview::TogglePlayback()
{
    if (bAnalyzing)
        return;
    const auto LiveState = Expression->GetLiveMetrics().State;
    if (LiveState != ELAMLiveState::Stopped && LiveState != ELAMLiveState::Failed)
    {
        Expression->StopMicrophone();
        bPaused = false;
        return;
    }
    if (bPaused)
    {
        Expression->Resume();
        bPaused = false;
    }
    else if (Expression->GetCurrentExpressionFrame().Weight > 0)
    {
        Expression->Pause();
        bPaused = true;
    }
    else
        Replay();
}
void ALAMDemoPreview::Replay()
{
    if (bAnalyzing)
        return;
    if (LastClip)
    {
        Expression->PlayExpressionClip(LastClip);
        bPaused = false;
    }
}
void ALAMDemoPreview::ToggleMute() { Expression->SetMuted(!Expression->PlaybackSettings.bMuted); }
void ALAMDemoPreview::CycleOutput()
{
    if (OutputSubmixes.IsEmpty()) return;
    OutputIndex = (OutputIndex + 1) % OutputSubmixes.Num();
    Expression->SetOutputSubmix(OutputSubmixes[OutputIndex]);
}
void ALAMDemoPreview::ToggleMicrophone()
{
    const auto State = Expression->GetLiveMetrics().State;
    if (State != ELAMLiveState::Stopped && State != ELAMLiveState::Failed)
        Expression->StopMicrophone();
    else
    {
        if (!Expression->StartMicrophone({})) Status = TEXT("Microphone could not start. Check the device and Windows microphone access.");
        bPaused = false;
    }
}
void ALAMDemoPreview::ChangeLiveInterval()
{
    const float Current = Expression->GetLiveInferenceInterval();
    Expression->SetLiveInferenceInterval(Current < 200 ? 1000.f/3 : Current < 400 ? 1000 : 100);
}
void ALAMDemoPreview::PlaybackEnded(FLAMPlaybackInfo Info, ELAMPlaybackEndReason Reason)
{
    bPaused = false;
    Status = FString::Printf(TEXT("Playback %lld: %s"), Info.PlaybackId, *StaticEnum<ELAMPlaybackEndReason>()->GetNameStringByValue(int64(Reason)));
}
void ALAMDemoPreview::Report(bool Success, const FString &Detail)
{
    if (bReported)
        return;
    bReported = true;
    FString Path = FPaths::ProjectSavedDir() / TEXT("LAMPluginDemo.txt");
    FParse::Value(FCommandLine::Get(), TEXT("LAMReport="), Path);
    FFileHelper::SaveStringToFile((Success ? TEXT("PASS ") : TEXT("FAIL ")) + Detail, *Path);
    UE_LOG(LogTemp, Display, TEXT("LAM_PLUGIN_DEMO_%s %s"), Success ? TEXT("PASS") : TEXT("FAIL"), *Detail);
    if (bTest)
        FPlatformMisc::RequestExitWithStatus(false, Success ? 0 : 1);
}
void ALAMDemoPreview::Tick(float Delta)
{
    Super::Tick(Delta);
    if (auto* PC = GetWorld()->GetFirstPlayerController())
    {
        if (PC->WasInputKeyJustPressed(FKey(TEXT("V")))) ToggleMute();
        if (PC->WasInputKeyJustPressed(FKey(TEXT("O")))) CycleOutput();
        if (PC->WasInputKeyJustPressed(FKey(TEXT("M")))) ToggleMicrophone();
        if (PC->WasInputKeyJustPressed(FKey(TEXT("I")))) ChangeLiveInterval();
        if (PC->WasInputKeyJustPressed(FKey(TEXT("F")))) Expression->FadeOutAndStop(.3f);
        if (PC->WasInputKeyJustPressed(FKey(TEXT("Hyphen")))) Expression->SetVolume(FMath::Max(0.f, Expression->PlaybackSettings.Volume-.1f));
        if (PC->WasInputKeyJustPressed(FKey(TEXT("Equals")))) Expression->SetVolume(FMath::Min(2.f, Expression->PlaybackSettings.Volume+.1f));
    }
    const double Now = FPlatformTime::Seconds();
    if (bTest && !bReported)
    {
        if (Now - Started > 120)
        {
            Report(false, TEXT("Demo timed out"));
            return;
        }
        if (!bAnalyzing && Expression->CurrentClip && Face->GetAnimInstance())
        {
            PeakJaw = FMath::Max(PeakJaw, Face->GetAnimInstance()->GetCurveValue(TEXT("jawOpen")));
            if (Now - PlaybackStarted > 2.5 && !bCaptured)
            {
                FString Path;
                if (FParse::Value(FCommandLine::Get(), TEXT("LAMCapture="), Path))
                    FScreenshotRequest::RequestScreenshot(Path, false, false);
                bCaptured = true;
            }
            if (Now - PlaybackStarted > 3.1)
            {
                const auto Frame = Expression->GetCurrentExpressionFrame();
                const float Error = FMath::Abs(Face->GetAnimInstance()->GetCurveValue(TEXT("jawOpen")) -
                                               Expression->GetARKitCurveValue(TEXT("jawOpen")));
                Report(PeakJaw > .01f && Error < .05f,
                       FString::Printf(TEXT("sample=%d frames=%d jaw_peak=%.4f curve_error=%.6f backend=%s"), Selected,
                                       Expression->CurrentClip->Curves.Num() / 52, PeakJaw, Error,
                                       *Expression->CurrentClip->Backend));
            }
        }
    }
    if (auto *PC = GetWorld()->GetFirstPlayerController())
    {
        if (PC->WasInputKeyJustPressed(FKey(TEXT("SpaceBar"))))
            TogglePlayback();
        if (PC->WasInputKeyJustPressed(FKey(TEXT("R"))))
            Replay();
        const TCHAR *Keys[] = {TEXT("One"), TEXT("Two"), TEXT("Three"), TEXT("Four"), TEXT("Five"), TEXT("Six")};
        for (int I = 0; I < 6; ++I)
            if (PC->WasInputKeyJustPressed(FKey(Keys[I])))
                SelectSample(I);
    }
}
static ALAMDemoPreview *FindLAMDemo(UWorld *World)
{
    for (TActorIterator<ALAMDemoPreview> I(World); I; ++I)
        return *I;
    return nullptr;
}
void ALAMDemoPreviewHUD::DrawHUD()
{
    Super::DrawHUD();
    auto *Demo = FindLAMDemo(GetWorld());
    if (!Canvas || !Demo)
        return;
    const float S = FMath::Clamp(Canvas->SizeY / 800.f, .65f, 1.5f);
    const FLinearColor Ink(.78f, .85f, .92f), Accent(.25f, .95f, .77f), Panel(.025f, .034f, .05f, .96f);
    DrawRect(Panel, 20 * S, 20 * S, 320 * S, 690 * S);
    DrawText(TEXT("LAM / FACE DEMO"), Accent, 40 * S, 40 * S, nullptr, 1.4f * S);
    DrawText(TEXT("AUDIO TO ARKIT  /  UE 5.8"), Ink, 40 * S, 72 * S, nullptr, .8f * S);
    DrawText(TEXT("Choose a voice sample"), FLinearColor::White, 40 * S, 119 * S, nullptr, S);
    for (int I = 0; I < Demo->Samples.Num(); ++I)
    {
        const float Y = (153 + I * 48) * S;
        DrawRect(I == Demo->Selected ? FLinearColor(.07f, .23f, .23f) : FLinearColor(.065f, .082f, .11f), 40 * S, Y,
                 280 * S, 38 * S);
        const FString Label =
            Demo->SampleLabels.IsValidIndex(I) ? Demo->SampleLabels[I] : FString::Printf(TEXT("Sample %d"), I + 1);
        DrawText(FString::Printf(TEXT("%d   %s"), I + 1, *Label), I == Demo->Selected ? Accent : Ink, 53 * S,
                 Y + 10 * S, nullptr, S);
        AddHitBox(FVector2D(40 * S, Y), FVector2D(280 * S, 38 * S), FName(*FString::Printf(TEXT("Sample%d"), I)), true);
    }
    const FString State = Demo->IsAnalyzing() ? FString::Printf(TEXT("Analyzing %.0f%%"), Demo->Progress * 100)
                                              : (Demo->IsPaused() ? TEXT("Paused") : TEXT("Space: play / pause"));
    DrawText(State, Accent, 40 * S, 463 * S, nullptr, S);
    DrawText(TEXT("R: replay    1-6: select"), Ink, 40 * S, 492 * S, nullptr, .85f * S);
    const auto Frame = Demo->Expression->GetCurrentExpressionFrame();
    const float Duration = Demo->Expression->CurrentClip ? Demo->Expression->CurrentClip->Duration : 0;
    const auto LiveState = Demo->Expression->GetLiveMetrics().State;
    const bool LiveActive = LiveState != ELAMLiveState::Stopped && LiveState != ELAMLiveState::Failed;
    DrawText(LiveActive ? FString::Printf(TEXT("Live input %.2f seconds"), Frame.TimeSeconds) : FString::Printf(TEXT("%.2f / %.2f seconds"), Frame.TimeSeconds, Duration), Ink, 40 * S, 528 * S, nullptr,
             .9f * S);
    DrawRect(FLinearColor(.08f, .12f, .16f), 40 * S, 553 * S, 280 * S, 5 * S);
    DrawRect(Accent, 40 * S, 553 * S, 280 * S * (LiveActive ? 1.f : FMath::Clamp(Duration > 0 ? Frame.TimeSeconds / Duration : Demo->Progress, 0.f, 1.f)), 5 * S);
    DrawText(FString::Printf(TEXT("jawOpen    %.3f"), Demo->Expression->GetARKitCurveValue(TEXT("jawOpen"))), Ink,
             40 * S, 585 * S, nullptr, S);
    DrawText(TEXT("Face: hinzka / VRoid"), Ink, 40 * S, 638 * S, nullptr, .75f * S);
    DrawText(TEXT("Voice: JVNV / litagin"), Ink, 40 * S, 661 * S, nullptr, .75f * S);
    DrawText(TEXT("Demo audiovisual content: CC BY-SA 4.0"), Ink, 40 * S, 683 * S, nullptr, .60f * S);
    const float X = 360*S;
    DrawRect(Panel, X, 20*S, 305*S, 300*S);
    DrawText(TEXT("PLAYBACK CONTROLS"), Accent, X+16*S, 38*S, nullptr, S);
    DrawText(FString::Printf(TEXT("V: mute [%s]  -/+: volume %.1f"), Demo->Expression->PlaybackSettings.bMuted ? TEXT("ON") : TEXT("OFF"), Demo->Expression->PlaybackSettings.Volume), Ink, X+16*S, 70*S, nullptr, .8f*S);
    DrawText(TEXT("F: fade out    O: output submix"), Ink, X+16*S, 97*S, nullptr, .8f*S);
    const FString Output = Demo->OutputSubmixes.IsValidIndex(Demo->OutputIndex) ? Demo->OutputSubmixes[Demo->OutputIndex]->GetFName().GetPlainNameString() : TEXT("Inherited");
    DrawText(Output, Accent, X+16*S, 124*S, nullptr, .8f*S);
    const auto Metrics=Demo->Expression->GetLiveMetrics();
    DrawText(TEXT("M: microphone    I: live interval"), Ink, X+16*S, 168*S, nullptr, .8f*S);
    DrawText(FString::Printf(TEXT("Interval %.0f ms | Delay %.0f ms"), Demo->Expression->GetLiveInferenceInterval(), Metrics.EffectivePresentationDelayMilliseconds), Ink, X+16*S, 195*S, nullptr, .8f*S);
    DrawText(FString::Printf(TEXT("Inference P95 %.1f ms | %s"), Metrics.InferenceP95Milliseconds, *Metrics.Backend), Ink, X+16*S, 222*S, nullptr, .8f*S);
    DrawText(StaticEnum<ELAMLiveState>()->GetNameStringByValue(int64(Metrics.State)), Accent, X+16*S, 250*S, nullptr, .8f*S);
    DrawText(TEXT("Mic speaker monitoring is off"), Ink, X+16*S, 285*S, nullptr, .7f*S);
    DrawText(Demo->Status, FLinearColor::White, 24 * S, Canvas->SizeY - 28 * S, nullptr, .8f * S);
}
void ALAMDemoPreviewHUD::NotifyHitBoxClick(FName Name)
{
    Super::NotifyHitBoxClick(Name);
    if (auto *Demo = FindLAMDemo(GetWorld()))
    {
        const FString Value = Name.ToString();
        if (Value.StartsWith(TEXT("Sample")))
            Demo->SelectSample(FCString::Atoi(*Value.Mid(6)));
    }
}
