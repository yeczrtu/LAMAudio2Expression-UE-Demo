#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "LAMCore.h"
#include "LAMSettings.h"
#include "Sound/SoundWave.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Async/Async.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLAMMathTest, "LAM.Core.BoundariesAndCurves",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FLAMMathTest::RunTest(const FString &)
{
    TestEqual(TEXT("52 names"), LAM::CurveNames().Num(), 52);
    TSet<FName> Unique(LAM::CurveNames());
    TestEqual(TEXT("Unique names"), Unique.Num(), 52);
    TestEqual(TEXT("jawOpen index"), LAM::CurveNames()[24], FName(TEXT("jawOpen")));
    for (int Samples : {1, 399, 16000, 16001, 34192, 4800000})
    {
        TArray<float> PCM;
        PCM.Init(0.2f, Samples);
        TArray<float> Window;
        int Frames = 0;
        for (int64 Offset = 0; Offset < Samples; Offset += 16000)
        {
            LAM::MakeWindow(PCM, Offset + 16000, Window);
            TestEqual(TEXT("Window length"), Window.Num(), 34133);
            Frames += FMath::Min(30, int((int64(Samples) * 30 + 15999) / 16000) - Frames);
        }
        TestEqual(TEXT("Frame count"), Frames, int((int64(Samples) * 30 + 15999) / 16000));
    }
    auto *Clip = NewObject<ULAMExpressionClip>();
    Clip->Duration = 1;
    Clip->Curves.Init(0, 104);
    for (int C = 0; C < 52; ++C)
        Clip->Curves[52 + C] = 1;
    TestEqual(TEXT("Midframe interpolation"), Clip->Sample(1.f / 60).Values[24], 0.5f);
    TestEqual(TEXT("Negative clamp"), Clip->Sample(-1).Values[24], 0.f);
    TestEqual(TEXT("End clamp"), Clip->Sample(10).Values[24], 1.f);
    TArray<float> Silence;
    Silence.Init(0, 16000);
    TArray<float> Curves;
    Curves.Init(.5f, 30 * 52);
    FLAMAnalysisSettings Settings;
    Settings.bSmooth = false;
    LAM::Postprocess(Curves, Silence, Settings);
    TestEqual(TEXT("Silence suppresses jaw"), Curves[24], .05f);
    TestEqual(TEXT("Silence preserves brow"), Curves[0], .5f);
    TArray<float> Constant;
    Constant.Init(.25f, 44100);
    auto Converted = LAM::Resample(Constant, 44100);
    TestEqual(TEXT("Resampled sample count"), Converted.Num(), 16000);
    TestTrue(TEXT("DC preservation"), FMath::Abs(Converted[500] - .25f) < 1e-5);
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLAMModelTest, "LAM.Model.Parity",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FLAMModelTest::RunTest(const FString &)
{
    auto *Model = GetDefault<ULAMSettings>()->Model.LoadSynchronous();
    if (!TestNotNull(TEXT("Imported model"), Model))
        return false;
    FString Dir = FPaths::ProjectDir() / TEXT(".work/export");
    FParse::Value(FCommandLine::Get(), TEXT("LAMFixtureDir="), Dir);
    FString Report;
    for (bool WantGPU : {false, true})
    {
        auto Models = LAM::CreateModels(Model, WantGPU);
        bool GPU = WantGPU;
        if (WantGPU && !Models.GPU)
        {
            AddError(TEXT("DirectML unavailable"));
            continue;
        }
        TSharedPtr<UE::NNE::IModelInstanceRunSync> Instance;
        for (const FString Name : {TEXT("noise"), TEXT("silence"), TEXT("style11")})
        {
            TArray<uint8> Input, Reference;
            FFileHelper::LoadFileToArray(Input, *(Dir / (Name + TEXT(".audio.f32"))));
            FFileHelper::LoadFileToArray(Reference, *(Dir / (Name + TEXT(".expected.f32"))));
            if (!TestEqual(TEXT("Fixture input bytes"), Input.Num(), 34133 * 4) ||
                !TestEqual(TEXT("Fixture output bytes"), Reference.Num(), 64 * 52 * 4))
                return false;
            TArray<float> Audio;
            Audio.SetNumUninitialized(34133);
            FMemory::Memcpy(Audio.GetData(), Input.GetData(), Input.Num());
            TArray<float> Output;
            const double Start = FPlatformTime::Seconds();
            const bool OK = LAM::InferWindow(Audio, Name == TEXT("style11") ? 11 : 0, Models, Instance, GPU, Output);
            TestTrue(TEXT("Inference succeeds"), OK);
            TestEqual(TEXT("Requested backend used"), GPU, WantGPU);
            if (!OK)
                continue;
            float Error = 0;
            for (int I = 0; I < Output.Num(); ++I)
            {
                float Expected;
                FMemory::Memcpy(&Expected, Reference.GetData() + 4 * I, 4);
                Error = FMath::Max(Error, FMath::Abs(Expected - Output[I]));
            }
            const FString Line = FString::Printf(TEXT("%s %s max_error=%.9f elapsed_ms=%.2f\n"),
                                                 WantGPU ? TEXT("DirectML") : TEXT("CPU"), *Name, Error,
                                                 (FPlatformTime::Seconds() - Start) * 1000);
            AddInfo(Line);
            Report += Line;
            TestTrue(TEXT("Absolute error <= 1e-3"), Error <= 1e-3);
        }
    }
    FFileHelper::SaveStringToFile(Report, *(FPaths::ProjectSavedDir() / TEXT("LAMParity.txt")));
    // Simulate an unavailable GPU, then loss of both backends, without relying on driver faults.
    TArray<float> Silence, Output;
    Silence.Init(0, LAM::Window);
    auto CPUOnly = LAM::CreateModels(Model, false);
    TSharedPtr<UE::NNE::IModelInstanceRunSync> Instance;
    bool GPU = true;
    TestTrue(TEXT("Missing GPU falls back to CPU"), LAM::InferWindow(Silence, 0, CPUOnly, Instance, GPU, Output));
    TestFalse(TEXT("Fallback reports CPU"), GPU);
    Instance.Reset();
    GPU = true;
    TestFalse(TEXT("Both backends missing returns failure"), LAM::InferWindow(Silence, 0, {}, Instance, GPU, Output));
    for (int I = 0; I < 32; ++I)
    {
        FLAMJob Job;
        Job.Cancelled = true;
        LAM::Analyze(Silence, CPUOnly, FLAMAnalysisSettings(), Job);
        TestTrue(TEXT("Cancelled work publishes no curves"), Job.Curves.IsEmpty());
    }
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLAMDecodeTest, "LAM.Audio.CookedDecoder",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FLAMDecodeTest::RunTest(const FString &)
{
    for (const FString Name : {TEXT("short_inline"), TEXT("one_inline"), TEXT("fraction_stream"), TEXT("speech_stream"),
                               TEXT("silence_inline")})
    {
        auto *Sound = LoadObject<USoundWave>(nullptr, *(TEXT("/Game/Audio/") + Name + TEXT(".") + Name));
        if (!TestNotNull(*Name, Sound))
            continue;
        if (!Sound->IsStreaming())
            Sound->InitAudioResource(Sound->GetRuntimeFormat());
        auto Data = Sound->GetSoundWaveProxy()->GetSoundWaveDataRef();
        auto Result = Async(EAsyncExecution::ThreadPool,
                            [Data]()
                            {
                                FLAMJob Job;
                                TArray<float> PCM;
                                LAM::Decode(Data, PCM, Job);
                                return TPair<FString, int32>(Job.Error, PCM.Num());
                            })
                          .Get();
        TestTrue(*Result.Key, Result.Key.IsEmpty());
        TestTrue(TEXT("Duration preserved within 2 samples"),
                 FMath::Abs(Result.Value - FMath::RoundToInt(Sound->Duration * 16000)) <= 2);
    }
    return true;
}
#endif
