#include "Ap5Observer.h"

#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Engine/World.h"
#include "InputCoreTypes.h"
#include "Kismet/KismetSystemLibrary.h"

AAp5Observer::AAp5Observer()
{
    bAutoManageActiveCameraTarget = false;
    bShowMouseCursor = true;
}

void AAp5Observer::BeginPlay()
{
    Super::BeginPlay();
    if (!IsLocalController())
    {
        return;
    }
    ObservationCamera = GetWorld()->SpawnActor<ACameraActor>();
    if (ObservationCamera == nullptr)
    {
        UE_LOG(LogTemp, Error, TEXT("AP5_CAMERA_FAILED"));
        return;
    }
    ObservationCamera->GetCameraComponent()->SetFieldOfView(50.0f);
    SetViewTarget(ObservationCamera);
    FInputModeGameAndUI Mode;
    Mode.SetHideCursorDuringCapture(false);
    Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
    SetInputMode(Mode);
    ResetView();
}

void AAp5Observer::ResetView()
{
    Yaw = 210.0f;
    Elevation = 12.0f;
    Distance = 750.0f;
    bDragging = false;
    UpdateView();
}

void AAp5Observer::UpdateView()
{
    if (ObservationCamera == nullptr)
    {
        return;
    }
    const FVector Target(0, 0, 170);
    const FVector Offset = FRotator(Elevation, Yaw, 0).Vector() * Distance;
    ObservationCamera->SetActorLocation(Target + Offset);
    ObservationCamera->SetActorRotation((-Offset).Rotation());
}

void AAp5Observer::PlayerTick(float DeltaTime)
{
    Super::PlayerTick(DeltaTime);
    if (!IsLocalController() || ObservationCamera == nullptr)
    {
        return;
    }
    float MouseX = 0;
    float MouseY = 0;
    const bool bMouseAvailable = GetMousePosition(MouseX, MouseY);
    if (IsInputKeyDown(EKeys::RightMouseButton) && bMouseAvailable)
    {
        if (bDragging)
        {
            Yaw -= (MouseX - PreviousMouse.X) * 0.35f;
            Elevation += (MouseY - PreviousMouse.Y) * 0.25f;
        }
        PreviousMouse = FVector2D(MouseX, MouseY);
        bDragging = true;
    }
    else
    {
        bDragging = false;
    }
    // マウス操作が難しい環境でも同じ確認ができるようにする。
    const float Step = 60.0f * FMath::Min(DeltaTime, 0.1f);
    if (IsInputKeyDown(EKeys::Left)) Yaw -= Step;
    if (IsInputKeyDown(EKeys::Right)) Yaw += Step;
    if (IsInputKeyDown(EKeys::Up)) Elevation += Step;
    if (IsInputKeyDown(EKeys::Down)) Elevation -= Step;
    if (WasInputKeyJustPressed(EKeys::MouseScrollUp)) Distance -= 50;
    if (WasInputKeyJustPressed(EKeys::MouseScrollDown)) Distance += 50;
    if (IsInputKeyDown(EKeys::PageUp)) Distance -= Step * 5;
    if (IsInputKeyDown(EKeys::PageDown)) Distance += Step * 5;
    Elevation = FMath::Clamp(Elevation, 0.0f, 75.0f);
    Distance = FMath::Clamp(Distance, 400.0f, 1400.0f);
    Yaw = FMath::UnwindDegrees(Yaw);
    if (WasInputKeyJustPressed(EKeys::R)) ResetView();
    if (WasInputKeyJustPressed(EKeys::Escape))
    {
        UKismetSystemLibrary::QuitGame(this, this, EQuitPreference::Quit, false);
    }
    UpdateView();
}

void AAp5ObserverHUD::DrawHUD()
{
    Super::DrawHUD();
    DrawRect(FLinearColor(0, 0, 0, 0.75f), 12, 12, 760, 86);
    DrawText(TEXT("モンスター形状の検証（静止・19部位）"), FColor::White, 24, 20);
    DrawText(TEXT("右ドラッグ／矢印キー：回転　ホイール／PageUp・Down：ズーム"), FColor::White, 24, 44);
    DrawText(TEXT("R：視点を戻す　Esc：終了　※切断・穴あけ・修復は次の段階"), FColor::White, 24, 68);
}
