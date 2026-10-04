#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/HUD.h"
#include "Ap5Observer.generated.h"

class ACameraActor;

UCLASS()
class AP5_API AAp5Observer : public APlayerController
{
    GENERATED_BODY()
public:
    AAp5Observer();
    virtual void BeginPlay() override;
    virtual void PlayerTick(float DeltaTime) override;
private:
    void ResetView();
    void UpdateView();
    UPROPERTY()
    TObjectPtr<ACameraActor> Camera;
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
