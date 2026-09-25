#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Tests/AutomationEditorCommon.h"
#include "Editor.h"
#include "EngineUtils.h"
#include "LAMAudio2ExpressionComponent.h"
#include "LAMAnalyzeAsync.h"
#include "LAMTestReceiver.h"
#include "UObject/StrongObjectPtr.h"
#include "Sound/SoundWave.h"
#include "Sound/SoundWaveProcedural.h"

class FLAMPIEPlayback : public IAutomationLatentCommand
{
    FAutomationTestBase *Test;
    double Started = FPlatformTime::Seconds(), Changed = 0;
    float PausedTime = 0;
    int Stage = 0;
    TStrongObjectPtr<ULAMTestReceiver> Receiver;
    TWeakObjectPtr<AActor> OtherActor;

  public:
    explicit FLAMPIEPlayback(FAutomationTestBase *InTest) : Test(InTest) {}
    virtual bool Update() override
    {
        if (FPlatformTime::Seconds() - Started > 90)
        {
            Test->AddError(TEXT("PIE expression playback timed out"));
            return true;
        }
        if (!GEditor->PlayWorld)
            return false;
        ULAMAudio2ExpressionComponent *Component = nullptr;
        for (TActorIterator<AActor> It(GEditor->PlayWorld); It; ++It)
            if ((Component = It->FindComponentByClass<ULAMAudio2ExpressionComponent>()))
                break;
        if (!Component)
            return false;
        const auto Frame = Component->GetCurrentExpressionFrame();
        const double Now = FPlatformTime::Seconds();
        if (Stage == 0 && Frame.bValid && Frame.Weight > 0 && Frame.TimeSeconds > .1f)
        {
            Component->Pause();
            PausedTime = Frame.TimeSeconds;
            Changed = Now;
            Stage = 1;
        }
        else if (Stage == 1 && Now - Changed > .15)
        {
            Test->TestEqual(TEXT("PIE pause holds expression"), Frame.TimeSeconds, PausedTime);
            Test->TestTrue(TEXT("PIE paused seek"), Component->Seek(.5f));
            Test->TestEqual(TEXT("PIE paused seek updates expression"),
                            Component->GetCurrentExpressionFrame().TimeSeconds, .5f);
            Component->Resume();
            Changed = Now;
            Stage = 2;
        }
        else if (Stage == 2 && Now - Changed > .2)
        {
            Test->TestTrue(TEXT("PIE resume advances"), Frame.TimeSeconds > .5f);
            Component->Stop();
            Changed = Now;
            Stage = 3;
        }
        else if (Stage == 3 && Now - Changed > .2)
        {
            Test->TestEqual(TEXT("PIE stop releases curves"), Frame.Weight, 0.f);
            Receiver.Reset(NewObject<ULAMTestReceiver>());
            auto Bind = [this](ULAMAnalyzeAsync *Action)
            {
                Action->Completed.AddDynamic(Receiver.Get(), &ULAMTestReceiver::Completed);
                Action->Failed.AddDynamic(Receiver.Get(), &ULAMTestReceiver::Failed);
                Action->Cancelled.AddDynamic(Receiver.Get(), &ULAMTestReceiver::Cancelled);
                Action->Progress.AddDynamic(Receiver.Get(), &ULAMTestReceiver::Progress);
            };
            for (int I = 0; I < 32; ++I)
            {
                auto *Action =
                    ULAMAnalyzeAsync::AnalyzeSoundWaveAsync(Component, Component->CurrentClip->SoundWave, {});
                Bind(Action);
                Action->Activate();
                Component->CancelAnalysis();
            }
            auto *Invalid = ULAMAnalyzeAsync::AnalyzeSoundWaveAsync(Component, NewObject<USoundWaveProcedural>(), {});
            Bind(Invalid);
            Invalid->Activate();
            OtherActor = GEditor->PlayWorld->SpawnActor<AActor>();
            auto *Other = NewObject<ULAMAudio2ExpressionComponent>(OtherActor.Get());
            Other->RegisterComponent();
            Test->TestTrue(TEXT("Shared clip plays on a second component"),
                           Other->PlayExpressionClip(Component->CurrentClip));
            auto *LongSound = LoadObject<USoundWave>(nullptr, TEXT("/Game/Audio/long_stream.long_stream"));
            auto *Work = ULAMAnalyzeAsync::AnalyzeSoundWaveAsync(Other, LongSound, {});
            Bind(Work);
            Work->Activate();
            Stage = 4;
            Changed = Now;
        }
        else if (Stage == 4 && Receiver->LatestProgress > 0)
        {
            OtherActor->Destroy();
            Stage = 5;
            Changed = Now;
        }
        else if (Stage == 5 && Now - Changed > 1)
        {
            Test->TestEqual(TEXT("No completion after cancellation or owner destruction"), Receiver->Completions, 0);
            Test->TestEqual(TEXT("Procedural sound rejected"), Receiver->Failures, 1);
            Test->TestEqual(TEXT("32 cancellations and destroyed owner's job"), Receiver->Cancellations, 33);
            return true;
        }
        return false;
    }
};
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLAMPIETest, "LAM.Editor.PIEPlayback",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FLAMPIETest::RunTest(const FString &)
{
    ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(TEXT("/Game/LAMDemo")));
    ADD_LATENT_AUTOMATION_COMMAND(FStartPIECommand(false));
    ADD_LATENT_AUTOMATION_COMMAND(FLAMPIEPlayback(this));
    ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
    return true;
}
#endif
