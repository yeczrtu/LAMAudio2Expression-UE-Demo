// SPDX-License-Identifier: Apache-2.0
// Adapted from aigc3d/LAM_Audio2Expression (02a703c3ea7d8e360eb43098eca85ee98a083529).
// Modified 2026: UE C++ runtime integration, deterministic timing and optional postprocessing.
// See ../../../Licenses/Apache-2.0.txt and ../../../THIRD_PARTY_NOTICES.md.
#include "LAMCore.h"
#include "NNE.h"
#include "NNEModelData.h"
#include "Misc/SecureHash.h"
#include "HAL/ThreadSafeCounter.h"
#include "Misc/ScopeLock.h"

LAM::FModels LAM::CreateModels(UNNEModelData *Data, bool PreferGPU)
{
    FModels M;
    if (!Data)
        return M;
    M.Key = Data->GetFileId().ToString();
    if (PreferGPU)
    {
        auto R = UE::NNE::GetRuntime<INNERuntimeGPU>(TEXT("NNERuntimeORTDml"));
        if (R.IsValid())
            M.GPU = R->CreateModelGPU(Data);
    }
    auto R = UE::NNE::GetRuntime<INNERuntimeCPU>(TEXT("NNERuntimeORTCpu"));
    if (R.IsValid())
        M.CPU = R->CreateModelCPU(Data);
    return M;
}
void LAM::MakeWindow(const TArray<float> &PCM, int64 End, TArray<float> &Out)
{
    Out.SetNumZeroed(Window);
    for (int32 I = 0; I < Window; ++I)
    {
        const int64 P = End - Window + I;
        Out[I] = P >= 0 && P < PCM.Num() ? PCM[int32(P)] : 0.f;
    }
}
TArray<float> LAM::Resample(const TArray<float> &In, int32 SourceRate)
{
    if (SourceRate == Rate)
        return In;
    TArray<float> Out;
    if (In.IsEmpty() || SourceRate <= 0)
        return Out;
    const int64 Count = (int64(In.Num()) * Rate + SourceRate - 1) / SourceRate;
    Out.SetNumUninitialized(int32(Count));
    // Windowed-sinc low-pass avoids aliasing when downsampling speech.
    const double Cutoff = FMath::Min(1.0, double(Rate) / SourceRate) * 0.94;
    constexpr int Radius = 32;
    for (int32 I = 0; I < Out.Num(); ++I)
    {
        const double P = double(I) * SourceRate / Rate;
        const int Center = int(P);
        double Sum = 0, Weight = 0;
        for (int J = Center - Radius + 1; J <= Center + Radius; ++J)
        {
            const double D = P - J;
            if (FMath::Abs(D) >= Radius)
                continue;
            const double X = PI * D * Cutoff;
            const double W =
                Cutoff * (FMath::Abs(X) < 1e-10 ? 1.0 : FMath::Sin(X) / X) * (0.5 + 0.5 * FMath::Cos(PI * D / Radius));
            Sum += In[FMath::Clamp(J, 0, In.Num() - 1)] * W;
            Weight += W;
        }
        Out[I] = float(Sum / Weight);
    }
    return Out;
}
bool LAM::InferWindow(const TArray<float> &Audio, int32 Style, const FModels &Models,
                      TSharedPtr<UE::NNE::IModelInstanceRunSync> &Instance, bool &UsingGPU, TArray<float> &Output)
{
    using namespace UE::NNE;
    auto Init = [&]()
    {
        if (!Instance)
        {
            if (UsingGPU)
            {
                if (!Models.GPU)
                    return false;
                Instance = Models.GPU->CreateModelInstanceGPU();
            }
            else if (Models.CPU)
                Instance = Models.CPU->CreateModelInstanceCPU();
        }
        if (!Instance)
            return false;
        const auto Inputs = Instance->GetInputTensorDescs(), Outputs = Instance->GetOutputTensorDescs();
        if (Inputs.Num() != 2 || Outputs.Num() != 1 || Inputs[0].GetName() != TEXT("audio") ||
            Inputs[1].GetName() != TEXT("identity") || Outputs[0].GetName() != TEXT("curves"))
            return false;
        for (const auto &Desc : Inputs)
            if (Desc.GetDataType() != ENNETensorDataType::Float)
                return false;
        if (Outputs[0].GetDataType() != ENNETensorDataType::Float)
            return false;
        const uint32 A[] = {1, 34133}, B[] = {1, 12};
        const FTensorShape Shapes[] = {FTensorShape::Make(A), FTensorShape::Make(B)};
        if (Instance->SetInputTensorShapes(Shapes) != EResultStatus::Ok)
            return false;
        const auto OutputShapes = Instance->GetOutputTensorShapes();
        return OutputShapes.Num() == 1 && OutputShapes[0].Volume() == 64 * 52;
    };
    auto Run = [&]()
    {
        if (!Instance && !Init())
        {
            Instance.Reset();
            return false;
        }
        float Identity[12] = {};
        Identity[FMath::Clamp(Style, 0, 11)] = 1;
        Output.SetNumZeroed(64 * 52);
        FTensorBindingCPU In[] = {{const_cast<float *>(Audio.GetData()), uint64(Audio.Num() * sizeof(float))},
                                  {Identity, sizeof(Identity)}};
        FTensorBindingCPU Out[] = {{Output.GetData(), uint64(Output.Num() * sizeof(float))}};
        if (Instance->RunSync(In, Out) != EResultStatus::Ok)
            return false;
        for (float V : Output)
            if (!FMath::IsFinite(V))
                return false;
        return true;
    };
    if (Run())
        return true;
    if (UsingGPU)
    {
        UsingGPU = false;
        Instance.Reset();
        if (Init() && Run())
            return true;
    }
    Instance.Reset();
    return false;
}
void LAM::Postprocess(TArray<float> &C, const TArray<float> &PCM, const FLAMAnalysisSettings &S, int32 Hop)
{
    const int N = C.Num() / 52;
    if (!N)
        return;
    if (S.bSuppressSilentMouth)
    {
        int Start = -1;
        for (int F = 0; F <= N; ++F)
        {
            double RMS = 0;
            int A = int(int64(F) * Rate / 30), B = FMath::Min(int(int64(F + 1) * Rate / 30), PCM.Num());
            for (int I = A; I < B; ++I)
                RMS += PCM[I] * PCM[I];
            const bool Silent = F < N && B > A && FMath::Sqrt(RMS / (B - A)) < 0.001;
            if (Silent && Start < 0)
                Start = F;
            if (!Silent && Start >= 0)
            {
                if (F - Start >= 7)
                    for (int J = Start; J < F; ++J)
                        for (int K = 0; K < 52; ++K)
                        {
                            if (K >= 22 || K == 5)
                                C[J * 52 + K] *= 0.1f;
                        }
                Start = -1;
            }
        }
    }
    if (S.bSmooth)
    {
        for (int Start = 0; Start < N; Start += Hop)
        {
            const int Len = FMath::Min(Start ? 5 : 3, N - Start);
            for (int K = 0; K < 52; ++K)
            {
                const float Previous = Start ? C[(Start - 1) * 52 + K] : 0;
                for (int J = 0; J < Len; ++J)
                    C[(Start + J) * 52 + K] = FMath::Lerp(Previous, C[(Start + J) * 52 + K], float(J + 1) / (Len + 1));
            }
        }
        TArray<float> Copy = C;
        const float Coeff[] = {-3.f / 35, 12.f / 35, 17.f / 35, 12.f / 35, -3.f / 35};
        for (int F = 0; F < N; ++F)
            for (int K = 0; K < 52; ++K)
            {
                float V = 0;
                for (int D = -2; D <= 2; ++D)
                {
                    int I = F + D;
                    if (N == 1)
                        I = 0;
                    else
                    {
                        while (I < 0 || I >= N)
                        {
                            if (I < 0)
                                I = -I;
                            if (I >= N)
                                I = 2 * N - 2 - I;
                        }
                    }
                    V += Copy[I * 52 + K] * Coeff[D + 2];
                }
                C[F * 52 + K] = FMath::Clamp(V, 0.f, 1.f);
            }
    }
    if (S.bSymmetrize)
        for (int K = 0; K < 52; ++K)
        {
            FString Name = CurveNames()[K].ToString();
            if (Name.EndsWith(TEXT("Left")))
            {
                const int R = CurveNames().IndexOfByKey(FName(Name.LeftChop(4) + TEXT("Right")));
                if (R != INDEX_NONE)
                    for (int F = 0; F < N; ++F)
                    {
                        float V = (C[F * 52 + K] + C[F * 52 + R]) * 0.5f;
                        C[F * 52 + K] = C[F * 52 + R] = V;
                    }
            }
        }
    if (S.bAutoBlink)
    {
        FRandomStream Random(S.BlinkSeed);
        const float Blink[] = {0.0f, 0.557f, 0.953f, 0.942f, 0.426f, 0.148f, 0.018f};
        for (int F = Random.RandRange(60, 150); F < N; F += Random.RandRange(60, 150))
            for (int J = 0; J < 7 && F + J < N; ++J)
                C[(F + J) * 52 + 8] = C[(F + J) * 52 + 9] = Blink[J];
    }
}
void LAM::Analyze(const TArray<float> &PCM, const FModels &Models, const FLAMAnalysisSettings &S, FLAMJob &Job)
{
    const double Start = FPlatformTime::Seconds();
    if (PCM.IsEmpty() || PCM.Num() > Rate * 300)
    {
        Job.Error = TEXT("Audio must be nonempty and at most 300 seconds.");
        return;
    }
    Job.NumSamples = PCM.Num();
    Job.Duration = float(PCM.Num()) / Rate;
    const int Frames = int((int64(PCM.Num()) * 30 + Rate - 1) / Rate);
    Job.Curves.Reserve(Frames * 52);
    TSharedPtr<UE::NNE::IModelInstanceRunSync> Instance;
    bool GPU = Models.GPU.IsValid();
    TArray<float> WindowData, Output;
    for (int64 Offset = 0; Offset < PCM.Num(); Offset += Rate)
    {
        if (Job.Cancelled)
            return;
        MakeWindow(PCM, Offset + Rate, WindowData);
        if (!InferWindow(WindowData, S.Style, Models, Instance, GPU, Output))
        {
            Job.Error = TEXT("ONNX inference failed on GPU and CPU. Verify the fixed-shape model and cooked runtimes.");
            return;
        }
        const int Remaining = Frames - Job.Curves.Num() / 52, Count = FMath::Min(30, Remaining);
        Job.Curves.Append(Output.GetData() + 34 * 52, Count * 52);
        Job.Progress = 0.2f + 0.8f * float(Offset + Rate) / PCM.Num();
    }
    Postprocess(Job.Curves, PCM, S);
    Job.Backend = GPU ? TEXT("DirectML") : TEXT("CPU");
    Job.Seconds = float(FPlatformTime::Seconds() - Start);
}
