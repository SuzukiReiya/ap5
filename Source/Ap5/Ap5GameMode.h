#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "Ap5GameMode.generated.h"

class UStaticMesh;

UCLASS()
class AP5_API AAp5GameMode : public AGameModeBase
{
    GENERATED_BODY()

public:
    AAp5GameMode();
    virtual void StartPlay() override;

private:
    // Hard references ensure the engine's basic shapes are included during cooking.
    UPROPERTY()
    TObjectPtr<UStaticMesh> FloorMesh;

    UPROPERTY()
    TObjectPtr<UStaticMesh> BallMesh;
};
