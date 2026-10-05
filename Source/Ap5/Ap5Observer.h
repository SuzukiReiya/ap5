#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/HUD.h"
#include "Ap5Observer.generated.h"

class ACameraActor;
class AAp5Monster;

UCLASS()
class AP5_API AAp5Observer : public APlayerController
{
    GENERATED_BODY()
public:
    AAp5Observer();
    virtual void BeginPlay() override;
    virtual void PlayerTick(float DeltaTime) override;
    bool IsRepairMode() const { return bRepairMode; }
    float GetBrushRadius() const { return BrushRadius; }
    const FString& GetEditStatus() const { return EditStatus; }
private:
    void ResetView();
    void UpdateView();
    UPROPERTY()
    TObjectPtr<ACameraActor> ObservationCamera;
    UPROPERTY()
    TObjectPtr<AAp5Monster> TestMonster;
    bool bRepairMode = false;
    float BrushRadius = 20.0f;
    FString EditStatus = TEXT("体にカーソルを合わせて左クリックしてください");
    float Yaw = 210.0f;
    float Elevation = 12.0f;
    float Distance = 750.0f;
    FVector2D PreviousMouse = FVector2D::ZeroVector;
    bool bDragging = false;
};

UCLASS()
class AP5_API AAp5ObserverHUD : public AHUD
{
    GENERATED_BODY()
public:
    virtual void DrawHUD() override;
};
