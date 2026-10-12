#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "Ap5PlayerCharacter.generated.h"

class UCameraComponent;
class USpringArmComponent;
class UStaticMeshComponent;

UCLASS()
class AP5_API AAp5PlayerCharacter : public ACharacter
{
    GENERATED_BODY()

public:
    AAp5PlayerCharacter();

    UCameraComponent* GetFollowCamera() const { return FollowCamera; }
    void SetSprintEnabled(bool bEnabled);
    void AdjustCameraDistance(float DeltaCm);
    void ResetCameraDistance();

private:
    UPROPERTY()
    TObjectPtr<USpringArmComponent> CameraBoom;

    UPROPERTY()
    TObjectPtr<UCameraComponent> FollowCamera;

    UPROPERTY()
    TObjectPtr<UStaticMeshComponent> BodyVisual;

    UPROPERTY()
    TObjectPtr<UStaticMeshComponent> HeadVisual;

    float WalkSpeed = 450.0f;
    float SprintSpeed = 750.0f;
    float DefaultCameraDistance = 320.0f;
};
