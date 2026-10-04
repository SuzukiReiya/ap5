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
    // 床の基本形状をCook対象に含めるための参照。
    UPROPERTY()
    TObjectPtr<UStaticMesh> FloorMesh;

};
