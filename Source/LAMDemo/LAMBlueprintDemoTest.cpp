// SPDX-License-Identifier: MIT
#include "LAMBlueprintDemoTest.h"
#include "LAMAudio2ExpressionComponent.h"
#include "Sound/SoundSubmix.h"
#include "Components/SkeletalMeshComponent.h"
#include "Animation/AnimInstance.h"
#include "Engine/DirectionalLight.h"
#include "Engine/Engine.h"
#include "EngineUtils.h"
#include "GameFramework/HUD.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "UObject/UnrealType.h"
#include "UnrealClient.h"

namespace
{
struct FDemoCheck
{
    double Started = FPlatformTime::Seconds(), StageTime = 0;
    int Stage = 0, Sample = 0;
    float Peak = 0, PausedAt = 0;
    TWeakObjectPtr<AActor> Actor;
    bool bLegacy = false, bTravelled = false;
    bool Finish(bool OK, const FString &Detail)
    {
        FString Path = FPaths::ProjectSavedDir() / TEXT("LAMPluginDemo.txt");
        FParse::Value(FCommandLine::Get(), TEXT("LAMReport="), Path);
        const FString Text = (OK ? TEXT("PASS ") : TEXT("FAIL ")) + Detail;
        FFileHelper::SaveStringToFile(Text, *Path);
        UE_LOG(LogTemp, Display, TEXT("LAM_BLUEPRINT_DEMO_TEST %s"), *Text);
        FPlatformMisc::RequestExitWithStatus(false, OK ? 0 : 1);
        return false;
    }
    bool Click(APlayerController *PC, FName Name)
    {
        auto *HUD = PC ? PC->GetHUD() : nullptr;
        if (!HUD)
            return false;
        const auto *Box = HUD->GetHitBoxWithName(Name);
        if (!Box)
            return false;
        int32 Width = 0, Height = 0;
        PC->GetViewportSize(Width, Height);
        for (int32 Y = 0; Y < Height; Y += 8)
            for (int32 X = 0; X < Width; X += 8)
            {
                const FVector2D Point(X, Y);
                if (Box->Contains(Point))
                    return HUD->UpdateAndDispatchHitBoxClickEvents(Point, IE_Pressed);
            }
        return false;
    }
    bool Tick(float)
    {
        if (!GEngine)
            return true;
        UWorld *World = nullptr;
        for (const auto &C : GEngine->GetWorldContexts())
            if (C.World() && C.World()->IsGameWorld() && C.World()->HasBegunPlay())
            {
                World = C.World();
                break;
            }
        if (!World)
            return true;
        if (bLegacy)
        {
            if (!bTravelled && !World->GetMapName().EndsWith(TEXT("LAMDemo")))
            {
                bTravelled = true;
                UGameplayStatics::OpenLevel(World, TEXT("/Game/LAMDemo"));
            }
            return !bTravelled;
        }
        const double Now = FPlatformTime::Seconds();
        if (Now - Started > 120)
            return Finish(false, TEXT("Blueprint demo timed out"));
        auto *PC = World->GetFirstPlayerController();
        if (Stage == 0)
        {
            for (TActorIterator<AActor> It(World); It; ++It)
                if (It->ActorHasTag(TEXT("LAMBlueprintDemo")))
                {
                    Actor = *It;
                    break;
                }
            if (!Actor.IsValid() || !PC || !PC->GetHUD())
                return true;
            if (!PC->GetHUD()->GetHitBoxWithName(TEXT("Sample0")))
                return true; // Wait for the first DrawHUD; test actual Blueprint hitboxes.
            if (!PC->bShowMouseCursor || !PC->bEnableClickEvents)
                return Finish(false, TEXT("Blueprint mouse/click setup is missing"));
            int Lights = 0;
            for (TActorIterator<ADirectionalLight> It(World); It; ++It)
                ++Lights;
            if (Lights != 1)
                return Finish(false, FString::Printf(TEXT("Expected one DirectionalLight, found %d"), Lights));
            if (Actor->GetClass()->GetSuperClass() != AActor::StaticClass() ||
                PC->GetHUD()->GetClass()->GetSuperClass() != AHUD::StaticClass())
                return Finish(false, TEXT("Demo still has a custom native parent"));
            if (!Click(PC, FName(*FString::Printf(TEXT("Sample%d"), Sample))))
                return Finish(false, TEXT("HUD click event missing"));
            Stage = 1;
            StageTime = Now;
            return true;
        }
        auto *Demo = Actor.Get();
        if (!Demo)
            return Finish(false, TEXT("Demo actor was destroyed"));
        auto *LAM = Demo->FindComponentByClass<ULAMAudio2ExpressionComponent>();
        auto *Mesh = Demo->FindComponentByClass<USkeletalMeshComponent>();
        if (!LAM || !Mesh)
            return Finish(false, TEXT("Demo components missing"));
        auto Info = LAM->GetPlaybackInfo();
        if (Info.State == ELAMPlaybackState::Failed)
            return Finish(false, TEXT("Playback failed"));
        switch (Stage)
        {
        case 1:
            if (Info.State == ELAMPlaybackState::Playing && Mesh->GetAnimInstance())
            {
                Peak = FMath::Max(Peak, Mesh->GetAnimInstance()->GetCurveValue(TEXT("jawOpen")));
                if (Info.Position > 2.5f)
                {
                    if (Peak < .01f)
                        return Finish(false, TEXT("Blueprint AnimGraph produced no jaw movement"));
                    FString Capture;
                    if (FParse::Value(FCommandLine::Get(), TEXT("LAMCapture="), Capture))
                        FScreenshotRequest::RequestScreenshot(Capture, false, false);
                    Stage = 2;
                    StageTime = Now;
                }
            }
            break;
        case 2:
            if (Now - StageTime < .6)
                break;
            if (Sample != 0)
                return Finish(true, FString::Printf(TEXT("sample=%d Blueprint HUD click -> async -> playback -> "
                                                         "AnimGraph, jaw_peak=%.4f, DirectionalLights=1, backend=%s"),
                                                    Sample, Peak, *LAM->CurrentClip->Backend));
            Click(PC, TEXT("Pause"));
            if (LAM->GetPlaybackInfo().State != ELAMPlaybackState::Paused)
                return Finish(false, TEXT("Pause button failed"));
            PausedAt = LAM->GetPlaybackInfo().Position;
            Stage = 3;
            StageTime = Now;
            break;
        case 3:
            if (Now - StageTime < .25)
                break;
            if (FMath::Abs(LAM->GetPlaybackInfo().Position - PausedAt) > .034f)
                return Finish(false, TEXT("Pause did not hold playback"));
            Click(PC, TEXT("Pause"));
            Click(PC, TEXT("Mute"));
            Click(PC, TEXT("Down"));
            Click(PC, TEXT("Output"));
            if (LAM->GetPlaybackInfo().State != ELAMPlaybackState::Playing || !LAM->PlaybackSettings.bMuted ||
                !FMath::IsNearlyEqual(LAM->PlaybackSettings.Volume, .9f) || !LAM->PlaybackSettings.OutputSubmix ||
                LAM->PlaybackSettings.OutputSubmix->GetName() != TEXT("SM_Alternate"))
                return Finish(false, TEXT("Resume/mute/volume/submix Blueprint controls failed"));
            for (float Expected : {1000.f, 100.f, 1000.f / 3})
            {
                Click(PC, TEXT("Interval"));
                if (!FMath::IsNearlyEqual(LAM->GetLiveInferenceInterval(), Expected, .01f))
                    return Finish(false, TEXT("Interval Blueprint control failed"));
            }
            Click(PC, TEXT("Fade"));
            Stage = 4;
            StageTime = Now;
            break;
        case 4:
            if (Now - StageTime < .6)
                break;
            if (Info.State != ELAMPlaybackState::Stopped)
                return Finish(false, TEXT("Fade stop failed"));
            Click(PC, TEXT("Replay"));
            Stage = 5;
            StageTime = Now;
            break;
        case 5:
            if (Now - StageTime < .4)
                break;
            if (Info.State != ELAMPlaybackState::Playing)
                return Finish(false, TEXT("Replay failed"));
            // Use the public Seek node to reach the end quickly, then verify the bound BP completion event.
            LAM->Seek(FMath::Max(0.f, Info.Duration - .3f));
            Stage = 6;
            StageTime = Now;
            break;
        case 6:
            if (Now - StageTime < 1.)
                break;
            if (Info.State != ELAMPlaybackState::Stopped)
                return Finish(false, TEXT("Natural completion failed"));
            if (auto *Status = FindFProperty<FStrProperty>(Demo->GetClass(), TEXT("Status")))
                if (!Status->GetPropertyValue_InContainer(Demo).StartsWith(TEXT("Playback completed")))
                    return Finish(false, TEXT("BP OnPlaybackFinished handler did not execute"));
            return Finish(
                true,
                FString::Printf(TEXT("sample=0 BP HUD and async playback, pause/resume, mute, volume, submix, "
                                     "intervals, fade, replay, completion event; jaw_peak=%.4f; DirectionalLights=1"),
                                Peak));
        }
        return true;
    }
};
} // namespace
FTSTicker::FDelegateHandle StartLAMBlueprintDemoTests()
{
    const TCHAR *Cmd = FCommandLine::Get();
    const bool Legacy = FParse::Param(Cmd, TEXT("LAMTest")) || FParse::Param(Cmd, TEXT("LAMPlaybackTest")) ||
                        FParse::Param(Cmd, TEXT("LAMLiveIntervalTest"));
    if (!Legacy && !FParse::Param(Cmd, TEXT("LAMPluginDemoTest")))
        return {};
    auto Check = MakeShared<FDemoCheck>();
    Check->bLegacy = Legacy;
    FParse::Value(Cmd, TEXT("LAMDemoSample="), Check->Sample);
    return FTSTicker::GetCoreTicker().AddTicker(
        FTickerDelegate::CreateLambda([Check](float Delta) { return Check->Tick(Delta); }));
}
