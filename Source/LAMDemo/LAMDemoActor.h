#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GameFramework/HUD.h"
#include "GameFramework/GameModeBase.h"
#include "LAMTypes.h"
#include "LAMDemoActor.generated.h"
class ULAMAudio2ExpressionComponent;
class ULAMAnalyzeAsync;
UCLASS()
class ALAMDemoActor : public AActor
{
    GENERATED_BODY()
  public:
    ALAMDemoActor();
    UPROPERTY(VisibleAnywhere) TObjectPtr<ULAMAudio2ExpressionComponent> Expression;
    UPROPERTY(EditAnywhere) TObjectPtr<class USoundWave> Sound;
    UPROPERTY(EditAnywhere) TObjectPtr<class UNNEModelData> Model;
    UPROPERTY() TObjectPtr<class USkeletalMesh> TestMesh;
    UPROPERTY() TObjectPtr<class USkeletalMeshComponent> BlueprintMesh;
    UPROPERTY(BlueprintReadOnly) FString Status = TEXT("Loading model...");
    UPROPERTY(BlueprintReadOnly) float Progress = 0;
    void BeginPlay() override;
    void Tick(float Delta) override;

  private:
    UFUNCTION() void Complete(ULAMExpressionClip *Clip, float P, FString Error);
    UFUNCTION() void Failed(ULAMExpressionClip *Clip, float P, FString Error);
    UFUNCTION() void Progressed(ULAMExpressionClip *Clip, float P, FString Error);
    UFUNCTION() void LiveStatus(FString Message);
    void Report(bool Success, const FString &Message);
    UPROPERTY() TObjectPtr<ULAMAnalyzeAsync> Action;
    bool TestMode = false;
    double Start = 0, PlayStart = 0;
    float PausedTime = 0;
    int64 LiveSamples = 0;
    int32 Stage = 0;
};
UCLASS()
class ALAMDemoHUD : public AHUD
{
    GENERATED_BODY()
  public:
    void DrawHUD() override;
};
UCLASS()
class ALAMDemoGameMode : public AGameModeBase
{
    GENERATED_BODY()
  public:
    ALAMDemoGameMode();
    void BeginPlay() override;
};
