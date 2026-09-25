#include "LAMAudio2ExpressionComponent.h"
#include "LAMAnalyzeAsync.h"
#include "Components/AudioComponent.h"
#include "GameFramework/Actor.h"
#include "Sound/SoundWave.h"
#include "LAMLive.h"

ULAMAudio2ExpressionComponent::ULAMAudio2ExpressionComponent()
{
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.TickGroup = TG_PrePhysics;
}
ULAMAudio2ExpressionComponent::~ULAMAudio2ExpressionComponent() = default;
void ULAMAudio2ExpressionComponent::SetAnalysis(ULAMAnalyzeAsync *A)
{
    CancelAnalysis();
    Analysis = A;
}
void ULAMAudio2ExpressionComponent::CancelAnalysis()
{
    if (Analysis)
        Analysis->Cancel();
    Analysis = nullptr;
}
bool ULAMAudio2ExpressionComponent::PlayExpressionClip(ULAMExpressionClip *Clip, float Start)
{
    if (!Clip || !Clip->SoundWave || Clip->Curves.IsEmpty() || !GetWorld() || !FMath::IsFinite(Start))
        return false;
    StopMicrophone();
    // A fresh component makes callbacks from an old playback/seek distinguishable.
    if (Audio)
    {
        Audio->OnAudioPlaybackPercentNative.RemoveAll(this);
        Audio->OnAudioFinishedNative.RemoveAll(this);
        Audio->Stop();
        Audio->DestroyComponent();
    }
    CurrentClip = Clip;
    Audio = NewObject<UAudioComponent>(GetOwner());
    Audio->bAutoActivate = false;
    Audio->bAutoDestroy = false;
    Audio->SetSound(Clip->SoundWave);
    Audio->SetPitchMultiplier(1);
    Audio->bIsUISound = false;
    Audio->OnAudioPlaybackPercentNative.AddUObject(this, &ULAMAudio2ExpressionComponent::PlaybackPercent);
    Audio->OnAudioFinishedNative.AddUObject(this, &ULAMAudio2ExpressionComponent::PlaybackFinished);
    Audio->RegisterComponent();
    AudioPosition = FMath::Clamp(Start, 0.f, FMath::Max(0.f, Clip->Duration - KINDA_SMALL_NUMBER));
    bPlaying = true;
    bPaused = false;
    bHaveClock = false;
    Frame = Clip->Sample(AudioPosition);
    Frame.Weight = 0;
    Audio->Play(AudioPosition);
    return true;
}
void ULAMAudio2ExpressionComponent::PlaybackPercent(const UAudioComponent *C, const USoundWave *S, float Percent)
{
    if (C != Audio || !bPlaying || !CurrentClip || S != CurrentClip->SoundWave || bPaused)
        return;
    AudioPosition = FMath::Clamp(Percent * CurrentClip->Duration, 0.f, CurrentClip->Duration);
    LastUpdate = FPlatformTime::Seconds();
    bHaveClock = true;
}
void ULAMAudio2ExpressionComponent::PlaybackFinished(UAudioComponent *C)
{
    if (C == Audio)
    {
        bPlaying = false;
        bPaused = false;
    }
}
void ULAMAudio2ExpressionComponent::Pause()
{
    if (bPlaying && Audio)
    {
        bPaused = true;
        Audio->SetPaused(true);
        AudioPosition = Frame.TimeSeconds;
    }
}
void ULAMAudio2ExpressionComponent::Resume()
{
    if (bPlaying && Audio)
    {
        bPaused = false;
        LastUpdate = FPlatformTime::Seconds();
        Audio->SetPaused(false);
    }
}
void ULAMAudio2ExpressionComponent::Stop()
{
    bPlaying = false;
    bPaused = false;
    if (Audio)
        Audio->Stop();
    StopMicrophone();
}
bool ULAMAudio2ExpressionComponent::Seek(float Time)
{
    if (!CurrentClip)
        return false;
    const bool Paused = bPaused;
    bool OK = PlayExpressionClip(CurrentClip, Time);
    if (OK && Paused)
    {
        Pause();
        bHaveClock = true;
        Frame = CurrentClip->Sample(AudioPosition);
    }
    return OK;
}
float ULAMAudio2ExpressionComponent::GetARKitCurveValue(FName Name) const
{
    const int I = LAM::CurveNames().IndexOfByKey(Name);
    return Frame.Values.IsValidIndex(I) ? Frame.Values[I] : 0;
}
TArray<FName> ULAMAudio2ExpressionComponent::GetARKitCurveNames()
{
    return LAM::CurveNames();
}
void ULAMAudio2ExpressionComponent::TickComponent(float Delta, ELevelTick Type, FActorComponentTickFunction *Function)
{
    Super::TickComponent(Delta, Type, Function);
    if (Live)
    {
        UpdateLive(Delta);
        return;
    }
    if (bPlaying && CurrentClip && bHaveClock)
    {
        float Time = AudioPosition;
        if (!bPaused)
            Time += float(FMath::Min(FPlatformTime::Seconds() - LastUpdate, 1.0 / 30.0));
        Frame = CurrentClip->Sample(Time);
    }
    else if (!bPlaying)
    {
        Frame.Weight = FMath::Max(0.f, Frame.Weight - Delta / 0.1f);
        if (Frame.Weight == 0)
            Frame.bValid = false;
    }
}
void ULAMAudio2ExpressionComponent::EndPlay(const EEndPlayReason::Type Reason)
{
    CancelAnalysis();
    Stop();
    if (Audio)
        Audio->DestroyComponent();
    Super::EndPlay(Reason);
}
