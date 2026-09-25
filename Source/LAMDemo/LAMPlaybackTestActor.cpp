#include "LAMPlaybackTestActor.h"
#include "LAMAudio2ExpressionComponent.h"
#include "LAMSettings.h"
#include "LAMAnalyzeAsync.h"
#include "Components/SceneComponent.h"
#include "Components/AudioComponent.h"
#include "Sound/SoundWave.h"
#include "Sound/SoundSubmix.h"
#include "Sound/SoundAttenuation.h"
#include "Sound/SoundConcurrency.h"
#include "Sound/SoundClass.h"
#include "AudioDevice.h"
#include "ActiveSound.h"
#include "Audio.h"
#include "ISubmixBufferListener.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/App.h"

class FLAMSubmixMeter : public ISubmixBufferListener
{
public:
    FCriticalSection Mutex;
    TAtomic<float> Peak{0};
    TAtomic<int32> QuietBuffers{0};
    void Reset() { FScopeLock Lock(&Mutex); Peak = 0; }
    virtual bool IsRenderingAudio() const override { return true; }
    virtual void OnNewSubmixBuffer(const USoundSubmix*, float* Data, int32 Num, int32, int32, double) override
    {
        FScopeLock Lock(&Mutex);
        float BlockPeak = 0;
        for (int I=0; I<Num; ++I) BlockPeak = FMath::Max(BlockPeak, FMath::Abs(Data[I]));
        Peak = FMath::Max(Peak.Load(), BlockPeak);
        QuietBuffers = BlockPeak < .00001f ? QuietBuffers.Load() + 1 : 0;
    }
};
ALAMPlaybackTestActor::ALAMPlaybackTestActor()
{
    PrimaryActorTick.bCanEverTick = true; PrimaryActorTick.bTickEvenWhenPaused = true;
    RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
    A = CreateDefaultSubobject<ULAMAudio2ExpressionComponent>(TEXT("A"));
    B = CreateDefaultSubobject<ULAMAudio2ExpressionComponent>(TEXT("B"));
    PrimaryActorTick.TickGroup = TG_PostUpdateWork;
}
void ALAMPlaybackTestActor::BeginPlay()
{
    Super::BeginPlay(); Began = At = FPlatformTime::Seconds();
    FApp::SetUnfocusedVolumeMultiplier(1);
    FApp::SetVolumeMultiplier(1);
    LiveTest = FParse::Param(FCommandLine::Get(), TEXT("LAMLiveIntervalTest"));
    if (LiveTest)
    {
        if (FParse::Param(FCommandLine::Get(), TEXT("LAMCPU"))) GetMutableDefault<ULAMSettings>()->bPreferGPU = false;
        FParse::Value(FCommandLine::Get(), TEXT("LAMRate="), LiveRate);
        FParse::Value(FCommandLine::Get(), TEXT("LAMChannels="), LiveChannels);
        if (!Check(A->StartPCMStream({}), TEXT("start PCM"))) return;
        Began = At = FPlatformTime::Seconds();
        return;
    }
    FString Asset = TEXT("/Game/Audio/speech_stream.speech_stream");
    FParse::Value(FCommandLine::Get(), TEXT("LAMSound="), Asset);
    auto* Wave = LoadObject<USoundWave>(nullptr, *Asset);
    if (!Check(Wave != nullptr, TEXT("load cooked test sound"))) return;
    Clip = NewObject<ULAMExpressionClip>(this); Clip->SoundWave = Wave; Clip->Duration = Wave->Duration;
    Clip->Curves.Init(.5f, FMath::CeilToInt(Clip->Duration * 30) * 52);
    A->OnPlaybackEnded.AddDynamic(this, &ALAMPlaybackTestActor::Ended);
    A->OnPlaybackFinished.AddDynamic(this, &ALAMPlaybackTestActor::Finished);
    A->OnPlaybackStarted.AddDynamic(this, &ALAMPlaybackTestActor::Started);
    A->OnPlaybackStateChanged.AddDynamic(this, &ALAMPlaybackTestActor::StateChanged);
    auto Device = GetWorld()->GetAudioDevice();
    if (!Check(Device.IsValid(), TEXT("audio device"))) return;
    for (int I=0; I<3; ++I)
    {
        auto* Mix = NewObject<USoundSubmix>(); Mix->bAutoDisable = false;
        Submixes.Add(Mix);
        Device->RegisterSoundSubmix(Mix, true);
        auto Meter = MakeShared<FLAMSubmixMeter, ESPMode::ThreadSafe>();
        Meters.Add(Meter); Device->RegisterSubmixBufferListener(Meter, *Mix);
    }
    Settings.OutputSubmix = Submixes[0]; Settings.bInheritSoundWaveSends = false;
    FLAMSubmixSend Send; Send.Submix = Submixes[2]; Send.Level = .25f;
    Settings.AdditionalSubmixSends.Add(Send);
    if (!Check(A->PlayExpressionClipWithSettings(Clip, Settings), TEXT("play A"))) return;
    auto Other = Settings; Other.OutputSubmix = Submixes[1]; Other.AdditionalSubmixSends.Reset(); Other.Volume = .5f;
    Check(B->PlayExpressionClipWithSettings(Clip, Other), TEXT("play B"));
}
void ALAMPlaybackTestActor::ResetMeters() { for (auto M : Meters) M->Reset(); }
void ALAMPlaybackTestActor::Advance() { ++Stage; At = FPlatformTime::Seconds(); UE_LOG(LogTemp, Display, TEXT("LAM_PLAYBACK_STAGE %d"), Stage); }
bool ALAMPlaybackTestActor::Check(bool OK, const FString& What)
{
    if (!OK)
    {
        if (auto* C = FindComponentByClass<UAudioComponent>())
            GetWorld()->GetAudioDevice()->SendCommandToActiveSounds(C->GetAudioComponentID(), [](FActiveSound& S)
            {
                auto* W = S.FindWaveInstance(0);
                auto* V = W ? S.AudioDevice->GetSoundSource(W) : nullptr;
                UE_LOG(LogTemp, Display, TEXT("LAM_SOURCE_DIAGNOSTIC silent=%d ui=%d activePaused=%d wave=%d voice=%d voicePaused=%d gamePaused=%d percent=%.6f frames=%lld"),
                    S.IsPlayWhenSilent(), S.bIsUISound, S.bIsPaused, W!=nullptr, V!=nullptr, V ? V->IsPaused() : -1,
                    V ? V->IsPausedByGame() : -1, V ? V->GetPlaybackPercent() : -1, V ? V->GetNumFramesPlayed() : -1);
            });
        Finish(false, FString::Printf(TEXT("stage=%d %s"), Stage, *What));
    }
    return OK;
}
void ALAMPlaybackTestActor::Finish(bool OK, const FString& What)
{
    if (Done) return; Done = true;
    FString Path = FPaths::ProjectSavedDir() / TEXT("LAMPlaybackTest.txt");
    FParse::Value(FCommandLine::Get(), TEXT("LAMReport="), Path);
    const FString Text = (OK ? TEXT("PASS ") : TEXT("FAIL ")) + What + Details;
    FFileHelper::SaveStringToFile(Text, *Path);
    UE_LOG(LogTemp, Display, TEXT("LAM_EXTENDED_TEST %s"), *Text);
    FPlatformMisc::RequestExitWithStatus(false, OK ? 0 : 1);
}
void ALAMPlaybackTestActor::Ended(FLAMPlaybackInfo Info, ELAMPlaybackEndReason Reason)
{
    if (!Check(!EndIds.Contains(Info.PlaybackId), TEXT("duplicate ended event"))) return;
    EndIds.Add(Info.PlaybackId); Reasons.Add(Reason);
    UE_LOG(LogTemp, Display, TEXT("LAM_ENDED id=%lld reason=%d pos=%.3f"), Info.PlaybackId, int(Reason), Info.Position);
    if (Reentrant) { Reentrant = false; A->PlayExpressionClipWithSettings(Clip, Settings); }
}
void ALAMPlaybackTestActor::Finished(FLAMPlaybackInfo Info)
{
    ++FinishedCount;
    Check(Info.Progress == 1 && Info.Position == Info.Duration, TEXT("natural completion info"));
}
void ALAMPlaybackTestActor::Started(FLAMPlaybackInfo Info)
{
    ++StartedCount;
    if (Stage == 0 && StartedCount == 1) At = FPlatformTime::Seconds();
}
void ALAMPlaybackTestActor::StateChanged(FLAMPlaybackInfo Info)
{
    if (Stage == 0 && !PausedFromState && Info.State == ELAMPlaybackState::Playing)
    {
        PausedFromState = true;
        A->Pause();
    }
}
void ALAMPlaybackTestActor::Tick(float Delta)
{
    Super::Tick(Delta); if (Done) return;
    if (LiveTest) { TickLive(Delta); return; }
    const double Now = FPlatformTime::Seconds(), T = Now - At;
    if (Now - Began > 45) { Finish(false, TEXT("playback test timeout")); return; }
    if (Stage == 0 && PausedFromState && A->GetPlaybackInfo().State == ELAMPlaybackState::Paused)
    {
        if (!Check(StartedCount == 1, TEXT("state-listener pause preserves started event"))) return;
        A->Resume();
    }
    // The packaged audio device fades in during startup; source clocks can advance before
    // its first audible submix buffer. Allow that initialization before measuring routes.
    if (Stage == 0 && (T < 1 || A->GetPlaybackInfo().Position < .2f || B->GetPlaybackInfo().Position < .2f) && Now - Began < 10) return;
    if (T < .25) return;
    switch (Stage)
    {
    case 0:
        if (!Check(Meters[0]->Peak > .0001f && Meters[1]->Peak > .0001f && Meters[2]->Peak > .00001f,
            FString::Printf(TEXT("initial routed audio %.6f %.6f %.6f"), Meters[0]->Peak.Load(), Meters[1]->Peak.Load(), Meters[2]->Peak.Load()))) return;
        Details += FString::Printf(TEXT("\ninitial submix peaks: %.6f %.6f %.6f"), Meters[0]->Peak.Load(), Meters[1]->Peak.Load(), Meters[2]->Peak.Load());
        B->Stop(); A->SetOutputSubmix(Submixes[2]); A->RemoveSubmixSend(Submixes[2]); A->Seek(0); Advance(); break;
    case 1:
        // Stop/reroute are audio-thread commands. Wait for rendered buffers to settle,
        // rather than treating a game-thread delay as acknowledgement under model load.
        if ((Meters[0]->QuietBuffers < 8 || Meters[1]->QuietBuffers < 8) && T < 3) return;
        if (!Check(Meters[0]->QuietBuffers >= 8 && Meters[1]->QuietBuffers >= 8, TEXT("old routes drain"))) return;
        ResetMeters(); Advance(); break;
    case 2:
        if (!Check(Meters[0]->Peak < .00001f && Meters[1]->Peak < .00001f && Meters[2]->Peak > .00001f,
            FString::Printf(TEXT("reroute removes old outputs %.6f %.6f %.6f"), Meters[0]->Peak.Load(), Meters[1]->Peak.Load(), Meters[2]->Peak.Load()))) return;
        A->SetMuted(true); A->Seek(0); Advance(); break;
    case 3:
        if (A->GetPlaybackInfo().Position < .1f) return; // Wait for the seek's new renderer, not its request.
        ResetMeters(); HeldTime = A->GetPlaybackInfo().Position; Advance(); break;
    case 4:
        if (!Check(Meters[2]->Peak < .00001f && A->GetPlaybackInfo().Position > HeldTime, FString::Printf(TEXT("mute peak=%.5f position=%.4f held=%.4f state=%d"), Meters[2]->Peak.Load(), A->GetPlaybackInfo().Position, HeldTime, int(A->GetPlaybackInfo().State)))) return;
        A->SetMuted(false); A->Pause(); A->Seek(.1f); HeldTime = A->GetPlaybackInfo().Position;
        A->FadeOutAndStop(.15f); Advance(); break;
    case 5:
        if (!Check(A->GetPlaybackInfo().State == ELAMPlaybackState::Paused && FMath::Abs(A->GetPlaybackInfo().Position-HeldTime)<.0001 && Reasons.IsEmpty(), TEXT("paused seek/fade holds, seek emits no end"))) return;
        A->Resume(); Advance(); break;
    case 6:
        if (!Check(Reasons.Num()==1 && Reasons.Last()==ELAMPlaybackEndReason::Stopped && FinishedCount==0, TEXT("fade-out ends as stopped"))) return;
        A->PlayExpressionClipWithSettings(Clip, Settings, FMath::Max(0.f, Clip->Duration-.2f)); Advance(); break;
    case 7:
        if (T < .55) return;
        if (!Check(Reasons.Num()==2 && Reasons.Last()==ELAMPlaybackEndReason::Completed && FinishedCount==1, TEXT("natural EOF callback"))) return;
        A->PlayExpressionClipWithSettings(Clip, Settings); A->PlayExpressionClipWithSettings(Clip, Settings); Advance(); break;
    case 8:
        if (!Check(Reasons.Last()==ELAMPlaybackEndReason::Replaced && FinishedCount==1, TEXT("replacement reason"))) return;
        Reentrant=true; A->Stop(); Advance(); break;
    case 9:
        if (!Check(A->GetPlaybackInfo().State==ELAMPlaybackState::Playing && FinishedCount==1, TEXT("callback starts next playback safely"))) return;
        A->Stop(); Settings.AdditionalSubmixSends.Reset(); Settings.PlaybackMode=ELAMPlaybackMode::ThreeDimensional;
        Settings.Attachment=RootComponent;
        {
            auto* Attenuation=NewObject<USoundAttenuation>(this);
            Attenuation->Attenuation.bSpatialize=true; Attenuation->Attenuation.bAttenuate=true;
            Attenuation->Attenuation.AttenuationShapeExtents=FVector(1000); Attenuation->Attenuation.FalloffDistance=1000;
            Settings.AttenuationSettings=Attenuation;
        }
        A->PlayExpressionClipWithSettings(Clip, Settings); Advance(); break;
    case 10:
        if (!Check(A->GetPlaybackInfo().State==ELAMPlaybackState::Playing, TEXT("3D playback"))) return;
        {
            auto* Audio=Cast<UAudioComponent>(RootComponent->GetAttachChildren().Last());
            if (!Check(Audio && Audio->bAllowSpatialization && Audio->GetAttachParent()==RootComponent, TEXT("3D attachment"))) return;
        }
        A->Stop(); Settings.PlaybackMode=ELAMPlaybackMode::TwoDimensional;
        Concurrency=NewObject<USoundConcurrency>(this); Concurrency->Concurrency.MaxCount=1;
        Concurrency->Concurrency.ResolutionRule=EMaxConcurrentResolutionRule::PreventNew;
        Settings.ConcurrencySettings.Add(Concurrency); A->PlayExpressionClipWithSettings(Clip,Settings); Advance(); break;
    case 11: B->PlayExpressionClipWithSettings(Clip,Settings); Advance(); break;
    case 12:
        if (!Check(B->GetPlaybackInfo().State==ELAMPlaybackState::Failed, TEXT("concurrency rejection becomes failed"))) return;
        A->Stop(); B->Stop(); Advance(); break;
    case 13:
        Concurrency->Concurrency.ResolutionRule=EMaxConcurrentResolutionRule::StopOldest;
        A->PlayExpressionClipWithSettings(Clip,Settings); Advance(); break;
    case 14: B->PlayExpressionClipWithSettings(Clip,Settings); Advance(); break;
    case 15:
        if (!Check(Reasons.Last()==ELAMPlaybackEndReason::Interrupted && FinishedCount==1, TEXT("concurrency eviction is not natural EOF"))) return;
        B->Stop(); Settings.ConcurrencySettings.Reset();
        Settings.SoundClassOverride = NewObject<USoundClass>(this);
        Settings.SoundClassOverride->Properties.bIsUISound = true;
        A->PlayExpressionClipWithSettings(Clip,Settings);
        UGameplayStatics::SetGamePaused(this,true); HeldTime=A->GetPlaybackInfo().Position; Advance(); break;
    case 16:
        if (T < .75) return;
        if (!Check(FMath::Abs(A->GetPlaybackInfo().Position-HeldTime)<.034f, TEXT("game pause holds face"))) return;
        if (!Check(Meters[0]->QuietBuffers >= 8, TEXT("pause setting overrides UI SoundClass audio"))) return;
        UGameplayStatics::SetGamePaused(this,false); A->Stop(); Settings.bPlayWhenGamePaused=true;
        A->PlayExpressionClipWithSettings(Clip,Settings); UGameplayStatics::SetGamePaused(this,true); Advance(); break;
    case 17:
        if (T < .75) return;
        if (!Check(A->GetPlaybackInfo().Position>.1f, FString::Printf(TEXT("UI playback/face paused: pos=%.4f state=%d valid=%d tickpaused=%d settings=%d"), A->GetPlaybackInfo().Position, int(A->GetPlaybackInfo().State), A->GetCurrentExpressionFrame().bValid, A->PrimaryComponentTick.bTickEvenWhenPaused, Settings.bPlayWhenGamePaused))) return;
        UGameplayStatics::SetGamePaused(this,false); A->Stop();
        Settings.bPlayWhenGamePaused=false;
        Settings.OutputSubmix=Submixes[0];
        A->PlayExpressionClipWithSettings(Clip,Settings); A->SetOutputSubmix(nullptr);
        Advance(); break;
    case 18: ResetMeters(); Advance(); break;
    case 19:
        if (!Check(Meters[0]->Peak<.00001f && Meters[1]->Peak<.00001f && Meters[2]->Peak<.00001f && A->GetPlaybackInfo().Position>.1f,
            TEXT("null main output restores inherited route without stale sends"))) return;
        if (!Check(Clip->SoundWave->GetSoundSubmix()!=Submixes[0] && Clip->SoundWave->GetSoundSubmix()!=Submixes[2], TEXT("shared sound routing unchanged"))) return;
        A->Stop();
        if (!Check(!A->PlayExpressionClip(nullptr) && FinishedCount==1, TEXT("invalid input rejected"))) return;
        {
            auto* InlineWave = LoadObject<USoundWave>(nullptr, TEXT("/Game/Audio/inline_concurrency.inline_concurrency"));
            if (!Check(InlineWave && InlineWave->bOverrideConcurrency, TEXT("load inline-concurrency fixture"))) return;
            InlineClip = NewObject<ULAMExpressionClip>(this); InlineClip->SoundWave = InlineWave;
            InlineClip->Duration = InlineWave->Duration; InlineClip->Curves.Init(.5f, 30 * 52);
            A->PlayExpressionClipWithSettings(InlineClip, Settings);
        }
        Advance(); break;
    case 20:
        if (A->GetPlaybackInfo().Position < .05f) return;
        B->PlayExpressionClipWithSettings(InlineClip, Settings); Advance(); break;
    case 21:
        if (!Check(B->GetPlaybackInfo().State == ELAMPlaybackState::Failed, TEXT("inherited inline concurrency shared between components"))) return;
        A->Stop();
        Finish(true,FString::Printf(TEXT("routing, mute, pause, seek, fades, completion, replacement/reentrancy, 3D, explicit/inline concurrency, world pause; started=%d ended=%d"),StartedCount,Reasons.Num())); break;
    }
}
void ALAMPlaybackTestActor::TickLive(float Delta)
{
    const double Now=FPlatformTime::Seconds(), Elapsed=Now-Began;
    if (Elapsed>45) { Finish(false,TEXT("live timeout")); return; }
    const int64 Target=int64(Elapsed*LiveRate);
    const int Count=int(FMath::Clamp<int64>(Target-Samples,0,LiveRate*2));
    if (Count)
    {
        TArray<float> PCM; PCM.SetNumUninitialized(Count*LiveChannels);
        for (int I=0;I<Count;++I)
            for (int C=0;C<LiveChannels;++C) PCM[I*LiveChannels+C]=.2f*FMath::Sin(2*PI*145*(Samples+I)/LiveRate);
        if (!Check(A->PushPCMAudio(PCM,LiveRate,LiveChannels),TEXT("push PCM"))) return;
        Samples+=Count;
    }
    const auto Frame=A->GetCurrentExpressionFrame();
    const auto Metrics=A->GetLiveMetrics();
    SawLag |= Metrics.State == ELAMLiveState::Lagging;
    if (!Check(Metrics.State!=ELAMLiveState::Failed && Metrics.EffectivePresentationDelayMilliseconds<=2000,TEXT("live state/delay cap"))) return;
    if (Frame.bValid)
    {
        if (!Check(Frame.TimeSeconds>=LastLiveTime,TEXT("live time is monotonic"))) return;
        LastLiveTime=Frame.TimeSeconds; ++LiveFrames;
    }
    const float Intervals[]={1000.f/3,100,1000,1000.f/30,500,1000.f/3};
    if (Now-At>3.5)
    {
        if (!Check(FMath::Abs(Metrics.ActualIntervalMilliseconds-Intervals[Stage])<.01f,TEXT("interval applied while running"))) return;
        Details+=FString::Printf(TEXT("\nhop_ms=%.3f inference_p95=%.3f latency_p95=%.3f delay=%.3f dropped=%lld backend=%s"),Metrics.ActualIntervalMilliseconds,
            Metrics.InferenceP95Milliseconds,Metrics.ResultLatencyP95Milliseconds,Metrics.EffectivePresentationDelayMilliseconds,Metrics.DroppedIntervals,*Metrics.Backend);
        Advance();
        if (FParse::Param(FCommandLine::Get(), TEXT("LAMLateAnalysisTest")))
        {
            if (Stage == 1)
            {
                auto* LongSound = LoadObject<USoundWave>(nullptr, TEXT("/Game/Audio/long_stream.long_stream"));
                CompetingAnalysis = ULAMAnalyzeAsync::AnalyzeSoundWaveAsync(A, LongSound, {});
                CompetingAnalysis->Activate();
            }
            if (Stage == 2) A->CancelAnalysis();
            if (Stage == 6 && !Check(SawLag && Metrics.DroppedIntervals > 0, TEXT("queue contention reports lag/drops"))) return;
        }
        if (Stage==6) { Finish(LiveFrames>30,FString::Printf(TEXT("live interval changes rate=%d channels=%d valid_ticks=%d"),LiveRate,LiveChannels,LiveFrames)); A->Stop(); return; }
        A->SetLiveInferenceInterval(Intervals[Stage]);
    }
}
void ALAMPlaybackTestActor::EndPlay(const EEndPlayReason::Type Reason)
{
    auto Device=GetWorld()->GetAudioDevice();
    if (Device.IsValid()) for (int I=0;I<Meters.Num();++I)
    {
        Device->UnregisterSubmixBufferListener(Meters[I].ToSharedRef(),*Submixes[I]);
        Device->UnregisterSoundSubmix(Submixes[I], true);
    }
    Super::EndPlay(Reason);
}
