#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "Ap5PlayerCharacter.generated.h"

class UCameraComponent;
class USceneComponent;
class USpringArmComponent;
class UStaticMeshComponent;

UCLASS()
class AP5_API AAp5PlayerCharacter : public ACharacter
{
    GENERATED_BODY()

public:
    AAp5PlayerCharacter();

    UCameraComponent* GetFollowCamera() const { return FollowCamera; }
    FVector GetProjectileMuzzleLocation() const;
    void SetAimPitch(float PitchDegrees);
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

    UPROPERTY()
    TObjectPtr<UStaticMeshComponent> PelvisVisual;

    UPROPERTY()
    TObjectPtr<UStaticMeshComponent> LeftUpperArmVisual;

    UPROPERTY()
    TObjectPtr<UStaticMeshComponent> LeftForearmVisual;

    UPROPERTY()
    TObjectPtr<UStaticMeshComponent> LeftHandVisual;

    UPROPERTY()
    TObjectPtr<UStaticMeshComponent> LeftThighVisual;

    UPROPERTY()
    TObjectPtr<UStaticMeshComponent> LeftShinVisual;

    UPROPERTY()
    TObjectPtr<UStaticMeshComponent> LeftFootVisual;

    UPROPERTY()
    TObjectPtr<UStaticMeshComponent> RightThighVisual;

    UPROPERTY()
    TObjectPtr<UStaticMeshComponent> RightShinVisual;

    UPROPERTY()
    TObjectPtr<UStaticMeshComponent> RightFootVisual;

    UPROPERTY()
    TObjectPtr<USceneComponent> WeaponPivot;

    UPROPERTY()
    TObjectPtr<UStaticMeshComponent> RightArmVisual;

    UPROPERTY()
    TObjectPtr<UStaticMeshComponent> RightForearmVisual;

    UPROPERTY()
    TObjectPtr<UStaticMeshComponent> RightHandVisual;

    UPROPERTY()
    TObjectPtr<UStaticMeshComponent> GunVisual;

    float WalkSpeed = 450.0f;
    float SprintSpeed = 750.0f;
    float DefaultCameraDistance = 320.0f;
};
