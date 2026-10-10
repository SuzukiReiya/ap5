#include "Ap5FrameProfiler.h"

#include "HAL/PlatformTime.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

namespace
{
const TCHAR* ProcessNames[] =
{
    TEXT("弾生成"),
    TEXT("弾補助判定"),
    TEXT("物理状態同期"),
    TEXT("体積編集・連結判定"),
    TEXT("表面生成"),
    TEXT("DynamicMesh構築"),
    TEXT("Mesh反映"),
    TEXT("衝突箱生成"),
    TEXT("衝突更新"),
    TEXT("物理ボディ設定"),
    TEXT("コンポーネント生成"),
    TEXT("その他（未計測）")
};
static_assert(UE_ARRAY_COUNT(ProcessNames) == static_cast<int32>(EAp5ProfileProcess::Count));
}

FAp5FrameProfiler& FAp5FrameProfiler::Get()
{
    static FAp5FrameProfiler Instance;
    return Instance;
}

void FAp5FrameProfiler::BeginFrame(float DeltaSeconds)
{
    MoveToFrame(GFrameCounter);
    CurrentFrameMilliseconds = FMath::Max(0.0, static_cast<double>(DeltaSeconds) * 1000.0);

    ++FramesSinceWrite;
    if (FramesSinceWrite >= FileWriteIntervalFrames)
    {
        WriteSummaryIfNeeded();
        FramesSinceWrite = 0;
    }
}

void FAp5FrameProfiler::AddMilliseconds(EAp5ProfileProcess Process, double Milliseconds)
{
    MoveToFrame(GFrameCounter);
    const int32 Index = static_cast<int32>(Process);
    if (Index < 0 || Index >= static_cast<int32>(EAp5ProfileProcess::Count)) return;
    CurrentMilliseconds[Index] += FMath::Max(0.0, Milliseconds);
}

void FAp5FrameProfiler::MoveToFrame(uint64 FrameNumber)
{
    if (CurrentFrame == TNumericLimits<uint64>::Max())
    {
        CurrentFrame = FrameNumber;
        return;
    }
    if (CurrentFrame == FrameNumber) return;

    FinalizeCurrentFrame();
    CurrentFrame = FrameNumber;
    CurrentFrameMilliseconds = 0.0;
    for (double& Value : CurrentMilliseconds) Value = 0.0;
}

void FAp5FrameProfiler::FinalizeCurrentFrame()
{
    if (CurrentFrameMilliseconds <= SlowFrameThresholdMilliseconds) return;

    double MeasuredMilliseconds = 0.0;
    const int32 UnmeasuredIndex = static_cast<int32>(EAp5ProfileProcess::Unmeasured);
    for (int32 I = 0; I < UnmeasuredIndex; ++I) MeasuredMilliseconds += CurrentMilliseconds[I];
    CurrentMilliseconds[UnmeasuredIndex] =
        FMath::Max(0.0, CurrentFrameMilliseconds - MeasuredMilliseconds);

    ++SlowFrameCount;
    for (int32 I = 0; I < static_cast<int32>(EAp5ProfileProcess::Count); ++I)
    {
        const double Value = CurrentMilliseconds[I];
        Summary[I].SumMilliseconds += Value;
        Summary[I].MinMilliseconds = FMath::Min(Summary[I].MinMilliseconds, Value);
        Summary[I].MaxMilliseconds = FMath::Max(Summary[I].MaxMilliseconds, Value);
    }
    bSummaryDirty = true;
}

void FAp5FrameProfiler::WriteSummaryIfNeeded()
{
    if (!bSummaryDirty || SlowFrameCount == 0) return;

    FString Text = TEXT("処理,平均_ms,最小_ms,最大_ms\n");
    for (int32 I = 0; I < static_cast<int32>(EAp5ProfileProcess::Count); ++I)
    {
        const double Average = Summary[I].SumMilliseconds / static_cast<double>(SlowFrameCount);
        const double Minimum = Summary[I].MinMilliseconds == TNumericLimits<double>::Max()
            ? 0.0 : Summary[I].MinMilliseconds;
        Text += FString::Printf(TEXT("%s,%.3f,%.3f,%.3f\n"),
            ProcessNames[I], Average, Minimum, Summary[I].MaxMilliseconds);
    }

    const FString Path = FPaths::Combine(FPaths::ProjectLogDir(), TEXT("Ap5SlowFrameProfile.csv"));
    if (FFileHelper::SaveStringToFile(Text, *Path))
        bSummaryDirty = false;
}

FAp5ProfileScope::FAp5ProfileScope(EAp5ProfileProcess InProcess)
    : Process(InProcess), StartedSeconds(FPlatformTime::Seconds())
{
}

FAp5ProfileScope::~FAp5ProfileScope()
{
    FAp5FrameProfiler::Get().AddMilliseconds(
        Process, (FPlatformTime::Seconds() - StartedSeconds) * 1000.0);
}
