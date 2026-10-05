#include "Ap5Observer.h"
#include "Ap5Monster.h"
#include "EngineUtils.h"

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
            Yaw += (MouseX - PreviousMouse.X) * 0.35f;
            Elevation -= (MouseY - PreviousMouse.Y) * 0.25f;
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
    if (IsInputKeyDown(EKeys::Left)) Yaw += Step;
    if (IsInputKeyDown(EKeys::Right)) Yaw -= Step;
    if (IsInputKeyDown(EKeys::Up)) Elevation -= Step;
    if (IsInputKeyDown(EKeys::Down)) Elevation += Step;
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
    if (!IsValid(TestMonster))
    {
        for (TActorIterator<AAp5Monster> It(GetWorld()); It; ++It)
        {
            TestMonster = *It;
            break;
        }
    }
    if (WasInputKeyJustPressed(EKeys::One)) bRepairMode = false;
    if (WasInputKeyJustPressed(EKeys::Two)) bRepairMode = true;
    if (WasInputKeyJustPressed(EKeys::Three)) BrushRadius = 12;
    if (WasInputKeyJustPressed(EKeys::Four)) BrushRadius = 20;
    if (WasInputKeyJustPressed(EKeys::Five)) BrushRadius = 30;
    if (!IsValid(TestMonster)) return;
    if (WasInputKeyJustPressed(EKeys::BackSpace))
    {
        TestMonster->ResetShape();
        EditStatus = TEXT("全形状を初期状態に戻しました（修復操作とは別）");
    }
    // 案内の上と視点ドラッグ中は加工しない。長押しによる連続加工も行わない。
    const bool bOverHUD = bMouseAvailable && MouseX >= 12 && MouseX <= 1012 && MouseY >= 12 && MouseY <= 146;
    if (WasInputKeyJustPressed(EKeys::LeftMouseButton) && bMouseAvailable && !bDragging && !bOverHUD)
    {
        FVector Start, Direction;
        if (DeprojectMousePositionToWorld(Start, Direction))
        {
            const int32 ChangedParts = TestMonster->ApplyBrush(Start, Direction, BrushRadius, bRepairMode);
            EditStatus = FString::Printf(TEXT("%s：更新 %d 部位 / CPU処理 %.1f ms（描画完了までの時間は含みません）"),
                bRepairMode ? TEXT("修復") : TEXT("穴あけ"), ChangedParts, TestMonster->LastEditMilliseconds);
        }
    }
}

void AAp5ObserverHUD::DrawHUD()
{
    Super::DrawHUD();
    DrawRect(FLinearColor(0, 0, 0, 0.75f), 12, 12, 1000, 134);
    const AAp5Observer* Observer = Cast<AAp5Observer>(GetOwningPlayerController());
    if (Observer == nullptr) return;
    DrawText(FString::Printf(TEXT("体積加工：%s　半径 %.0f cm　1：穴あけ　2：修復　左クリック：実行"),
        Observer->IsRepairMode() ? TEXT("修復") : TEXT("穴あけ"), Observer->GetBrushRadius()), FColor::White, 24, 20);
    DrawText(TEXT("3：細い（12 cm）　4：標準（20 cm）　5：太い（30 cm）　Backspace：形状を全リセット"), FColor::White, 24, 44);
    DrawText(TEXT("右ドラッグ／矢印：回転　ホイール／PageUp・Down：ズーム　R：視点を戻す　Esc：終了"), FColor::White, 24, 68);
    DrawText(Observer->GetEditStatus(), FColor::Yellow, 24, 92);
    DrawText(TEXT("視線方向に貫通加工。修復は指定範囲だけ元の体の内側へ材料を戻します。"), FColor::White, 24, 116);
}
