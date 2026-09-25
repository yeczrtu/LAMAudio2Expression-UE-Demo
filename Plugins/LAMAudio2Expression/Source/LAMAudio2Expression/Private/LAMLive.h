#pragma once
#include "LAMCore.h"
#include "AudioCaptureCore.h"
#include "Misc/ScopeLock.h"
struct FLAMLiveSession
{
    FCriticalSection Mutex;
    TAtomic<bool> Cancelled{false};
    bool Busy = false, Ready = false, GPU = false;
    bool CaptureOverflow = false;
    int32 CaptureRate = 0, CaptureChannels = 0;
    TArray<float> CapturePCM;
    TArray<float> History, Native;
    int64 HistoryStart = 0, TotalSamples = 0, NativeStart = 0, NativeTotal = 0, ResampledTotal = 0, Step = 0;
    int32 NativeRate = 0;
    uint64 Generation = 0;
    double StartClock = 0;
    LAM::FModels Models;
    FLAMAnalysisSettings Options;
    TSharedPtr<UE::NNE::IModelInstanceRunSync> Instance;
    TArray<float> Result, Timings;
    double ResultStart = 0;
    FString Error;
};
class FLAMCapture
{
  public:
    Audio::FAudioCapture Device;
    ~FLAMCapture()
    {
        Device.AbortStream();
    }
};
