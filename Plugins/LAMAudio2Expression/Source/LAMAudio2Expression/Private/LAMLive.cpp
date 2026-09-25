// SPDX-License-Identifier: Apache-2.0
// Adapted from aigc3d/LAM_Audio2Expression (02a703c3ea7d8e360eb43098eca85ee98a083529).
// Modified 2026: UE C++ runtime integration, deterministic timing and optional postprocessing.
// See ../../../Licenses/Apache-2.0.txt and ../../../THIRD_PARTY_NOTICES.md.
#include "LAMLive.h"
#include "LAMAudio2ExpressionComponent.h"
#include "LAMSettings.h"
#include "Components/AudioComponent.h"
#include "Algo/Sort.h"

bool ULAMAudio2ExpressionComponent::StartPCMStream(FLAMAnalysisSettings Settings)
{
    Stop();
    if (Settings.Style < 0 || Settings.Style > 11)
        return false;
    LiveModel = GetDefault<ULAMSettings>()->Model.LoadSynchronous();
    auto Models = LAM::CreateModels(LiveModel, GetDefault<ULAMSettings>()->bPreferGPU);
    if (!Models.CPU && !Models.GPU)
    {
        OnStatus.Broadcast(TEXT("Live input requires an imported and cooked LAM model."));
        return false;
    }
    Live = MakeShared<FLAMLiveSession, ESPMode::ThreadSafe>();
    Live->Models = Models;
    Live->GPU = Models.GPU.IsValid();
    Live->Options = Settings;
    Live->StartClock = FPlatformTime::Seconds();
    CurrentClip = nullptr;
    Frame = FLAMExpressionFrame();
    InferenceP95Milliseconds = 0;
    OnStatus.Broadcast(TEXT("Live input started; microphone monitoring is disabled."));
    return true;
}
bool ULAMAudio2ExpressionComponent::StartMicrophone(FLAMAnalysisSettings Settings, int32 Index)
{
    if (!StartPCMStream(Settings))
        return false;
    Capture = MakeShared<FLAMCapture>();
    Audio::FAudioCaptureDeviceParams Params;
    Params.DeviceIndex = Index;
    auto Session = Live;
    if (!Capture->Device.OpenAudioCaptureStream(
            Params,
            [Session](const void *Data, int32 Frames, int32 Channels, int32 Rate, double, bool Overflow)
            {
                if (Session->Cancelled || Channels < 1 || Channels > 2 || Rate <= 0)
                    return;
                FScopeLock Lock(&Session->Mutex);
                Session->CaptureRate = Rate;
                Session->CaptureChannels = Channels;
                const int Count = Frames * Channels, Limit = Rate * Channels * 2;
                if (Count <= 0 || Count > Limit)
                {
                    Session->CaptureOverflow = true;
                    return;
                }
                if (Session->CapturePCM.Num() + Count > Limit)
                {
                    Session->CapturePCM.RemoveAt(0, Session->CapturePCM.Num() + Count - Limit, EAllowShrinking::No);
                    Session->CaptureOverflow = true;
                }
                Session->CapturePCM.Append(static_cast<const float *>(Data), Count);
                Session->CaptureOverflow |= Overflow;
            },
            1024) ||
        !Capture->Device.StartStream())
    {
        StopMicrophone();
        OnStatus.Broadcast(TEXT("Microphone unavailable or access denied."));
        return false;
    }
    return true;
}
void ULAMAudio2ExpressionComponent::StopMicrophone()
{
    if (Live)
        Live->Cancelled = true;
    Capture.Reset();
    Live.Reset();
    LiveModel = nullptr;
}
bool ULAMAudio2ExpressionComponent::PushPCMAudio(const TArray<float> &PCM, int32 Rate, int32 Channels)
{
    if (!Live || Rate < 8000 || Rate > 192000 || Channels < 1 || Channels > 2 || PCM.IsEmpty() ||
        PCM.Num() % Channels || PCM.Num() > Rate * Channels * 2)
        return false;
    auto &S = *Live;
    if (S.NativeRate && S.NativeRate != Rate)
    {
        OnStatus.Broadcast(TEXT("Sample rate changed: restart the PCM stream."));
        return false;
    }
    for (float V : PCM)
        if (!FMath::IsFinite(V))
            return false;
    S.NativeRate = Rate;
    for (int I = 0; I < PCM.Num(); I += Channels)
    {
        float V = 0;
        for (int C = 0; C < Channels; ++C)
            V += PCM[I + C];
        S.Native.Add(FMath::Clamp(V / Channels, -1.f, 1.f));
    }
    S.NativeTotal += PCM.Num() / Channels;
    // Continuous sample counter + symmetric sinc kernel across callback boundaries.
    const double Cutoff = FMath::Min(1.0, 16000.0 / Rate) * 0.94;
    constexpr int Radius = 32;
    while (double(S.ResampledTotal) * Rate / 16000 + Radius < S.NativeTotal)
    {
        const double Pos = double(S.ResampledTotal) * Rate / 16000;
        const int64 Center = int64(Pos);
        double Sum = 0, Weight = 0;
        for (int J = -Radius + 1; J <= Radius; ++J)
        {
            const double D = Pos - (Center + J);
            if (FMath::Abs(D) >= Radius)
                continue;
            const double X = PI * D * Cutoff;
            const double W =
                Cutoff * (FMath::Abs(X) < 1e-10 ? 1 : FMath::Sin(X) / X) * (0.5 + 0.5 * FMath::Cos(PI * D / Radius));
            const int Idx = int(FMath::Clamp<int64>(Center + J - S.NativeStart, 0, S.Native.Num() - 1));
            Sum += S.Native[Idx] * W;
            Weight += W;
        }
        S.History.Add(float(Sum / Weight));
        ++S.ResampledTotal;
        ++S.TotalSamples;
    }
    const int64 RetainFrom = FMath::Max<int64>(0, int64(double(S.ResampledTotal) * Rate / 16000) - Radius);
    const int Drop = int(RetainFrom - S.NativeStart);
    if (Drop > 0)
    {
        S.Native.RemoveAt(0, Drop, EAllowShrinking::No);
        S.NativeStart = RetainFrom;
    }
    const int MaxHistory = LAM::Window + LAM::Rate * 2;
    if (S.History.Num() > MaxHistory)
    {
        const int N = S.History.Num() - MaxHistory;
        S.History.RemoveAt(0, N, EAllowShrinking::No);
        S.HistoryStart += N;
    }
    return true;
}
void ULAMAudio2ExpressionComponent::UpdateLive(float Delta)
{
    auto S = Live;
    if (!S)
        return;
    TArray<float> Input;
    int Rate = 0, Channels = 0;
    bool Overflow = false;
    {
        FScopeLock Lock(&S->Mutex);
        Swap(Input, S->CapturePCM);
        Rate = S->CaptureRate;
        Channels = S->CaptureChannels;
        Overflow = S->CaptureOverflow;
        S->CaptureOverflow = false;
    }
    if (Overflow)
    {
        // Reset the timeline to the retained, newest audio. In-flight old work must not publish.
        {
            FScopeLock Lock(&S->Mutex);
            ++S->Generation;
            S->Result.Reset();
            S->Ready = false;
        }
        S->History.Reset();
        S->Native.Reset();
        S->HistoryStart = S->TotalSamples = S->NativeStart = S->NativeTotal = S->ResampledTotal = S->Step = 0;
        S->NativeRate = 0;
        S->StartClock = FPlatformTime::Seconds() - double(Input.Num()) / FMath::Max(1, Rate * Channels);
        CurrentClip = nullptr;
        Frame = FLAMExpressionFrame();
        OnStatus.Broadcast(TEXT("Microphone overflow: oldest buffered input dropped."));
    }
    if (Live != S)
        return;
    if (!Input.IsEmpty())
        PushPCMAudio(Input, Rate, Channels);
    if (!Live)
        return;
    FString Status;
    {
        FScopeLock Lock(&S->Mutex);
        Status = MoveTemp(S->Error);
        S->Error.Reset();
        if (S->Ready)
        {
            // Keep two seconds of timestamped frames in the clip, used only on the game thread.
            if (!CurrentClip || CurrentClip->SoundWave)
            {
                CurrentClip = NewObject<ULAMExpressionClip>(this);
                CurrentClip->Duration = 2;
            }
            CurrentClip->Curves = S->Result;
            AudioPosition = float(S->ResultStart);
            S->Ready = false;
            if (!S->Timings.IsEmpty())
            {
                auto Sorted = S->Timings;
                Sorted.Sort();
                InferenceP95Milliseconds =
                    Sorted[FMath::Min(Sorted.Num() - 1, FMath::CeilToInt(Sorted.Num() * 0.95f) - 1)];
            }
        }
    }
    if (!Status.IsEmpty())
    {
        OnStatus.Broadcast(Status);
        if (Live != S)
            return;
    }
    if (CurrentClip && !CurrentClip->SoundWave && !CurrentClip->Curves.IsEmpty())
    {
        const double T = FPlatformTime::Seconds() - S->StartClock - FMath::Clamp(PresentationDelay, 0.4f, 2.f);
        const double Relative = T - AudioPosition;
        if (InferenceP95Milliseconds > 0 && InferenceP95Milliseconds < 333.333f && Relative >= 0 &&
            Relative < CurrentClip->Curves.Num() / 52.0 / 30.0)
        {
            Frame = CurrentClip->Sample(float(Relative));
            Frame.TimeSeconds += AudioPosition;
        }
        else if (Relative >= 0)
            Frame.Weight = FMath::Max(0.f, Frame.Weight - Delta / 0.1f);
    }
    const int64 AvailableStep = S->TotalSamples * 3 / LAM::Rate;
    {
        FScopeLock Lock(&S->Mutex);
        if (S->Busy || AvailableStep <= S->Step)
            return;
        S->Busy = true;
    }
    // Catch up to the newest complete hop instead of building an unbounded queue.
    const int64 Next = AvailableStep;
    const int64 End = Next * LAM::Rate / 3;
    S->Step = Next;
    const uint64 Generation = S->Generation;
    TArray<float> WindowData;
    WindowData.SetNumZeroed(LAM::Window);
    for (int I = 0; I < LAM::Window; ++I)
    {
        const int64 P = End - LAM::Window + I - S->HistoryStart;
        if (P >= 0 && P < S->History.Num())
            WindowData[I] = S->History[int(P)];
    }
    LAM::Queue(
        [S, WindowData = MoveTemp(WindowData), End, Next, Generation]()
        {
            if (S->Cancelled)
                return;
            const double Start = FPlatformTime::Seconds();
            TArray<float> Output;
            const bool OK = LAM::InferWindow(WindowData, S->Options.Style, S->Models, S->Instance, S->GPU, Output);
            if (OK)
            {
                auto Options = S->Options;
                Options.bAutoBlink = false;
                LAM::Postprocess(Output, WindowData, Options, 64);
                if (S->Options.bAutoBlink)
                {
                    FRandomStream Random(S->Options.BlinkSeed);
                    const float Blink[] = {0, .557f, .953f, .942f, .426f, .148f, .018f};
                    for (int64 F = Random.RandRange(60, 150); F < Next * 10; F += Random.RandRange(60, 150))
                        for (int J = 0; J < 7; ++J)
                        {
                            const int64 Local = F + J - (Next * 10 - 10);
                            if (Local >= 0 && Local < 10)
                                Output[(54 + int(Local)) * 52 + 8] = Output[(54 + int(Local)) * 52 + 9] = Blink[J];
                        }
                }
            }
            const float MS = float((FPlatformTime::Seconds() - Start) * 1000);
            FScopeLock Lock(&S->Mutex);
            if (S->Cancelled)
                return;
            if (S->Generation != Generation)
            {
                S->Busy = false;
                return;
            }
            if (OK)
            {
                const double Begin = double(End) / 16000 - 10.0 / 30;
                // Preserve a timeline longer than presentation delay; discard obsolete frames.
                const int Existing = S->Result.Num() / 52;
                if (Existing && FMath::Abs(S->ResultStart + Existing / 30.0 - Begin) > 0.001)
                    S->Result.Reset();
                if (S->Result.IsEmpty())
                    S->ResultStart = Begin;
                S->Result.Append(Output.GetData() + 54 * 52, 10 * 52);
                if (S->Result.Num() > 60 * 52)
                {
                    const int Remove = S->Result.Num() - 60 * 52;
                    S->Result.RemoveAt(0, Remove, EAllowShrinking::No);
                    S->ResultStart += double(Remove / 52) / 30;
                }
                S->Ready = true;
                S->Timings.Add(MS);
                if (S->Timings.Num() > 60)
                    S->Timings.RemoveAt(0);
                auto Sorted = S->Timings;
                Sorted.Sort();
                if (Sorted.Num() >= 5 &&
                    Sorted[FMath::Min(Sorted.Num() - 1, FMath::CeilToInt(Sorted.Num() * 0.95f) - 1)] >= 333.333f)
                    S->Error = TEXT("Live inference p95 exceeds 333 ms; real-time performance is not met.");
            }
            else
                S->Error = TEXT("Live inference failed on GPU and CPU.");
            S->Busy = false;
        });
}
