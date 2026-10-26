#include "Ap5FrameProfiler.h"

#include "HAL/PlatformTime.h"
#include "Misc/App.h"
#include "Misc/CoreDelegates.h"
#include "RenderTimer.h"
#include "UnrealClient.h"
#include "DynamicRHI.h"
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
    TEXT("DynamicMesh頂点法線初期化"),
    TEXT("DynamicMesh頂点追加・法線設定"),
    TEXT("DynamicMesh三角形追加"),
    TEXT("描画チャンク管理"),
    TEXT("Mesh反映"),
    TEXT("衝突箱生成"),
    TEXT("衝突更新"),
    TEXT("物理衝突状態設定"),
    TEXT("物理Simulation切替"),
    TEXT("物理速度復元"),
    TEXT("駆動部位Sweep"),
    TEXT("コンポーネント生成"),
    TEXT("プロファイラI/O"),
    TEXT("FPS制限待機"),
    TEXT("FPS制限待機超過"),
    TEXT("その他（未計測）"),
    TEXT("参考_GameThread CPU"),
    TEXT("参考_GameThread Wait"),
    TEXT("参考_RenderThread CPU"),
    TEXT("参考_RenderThread Wait"),
    TEXT("参考_RHIThread CPU"),
    TEXT("参考_GPU")
};
static_assert(UE_ARRAY_COUNT(ProcessNames) == static_cast<int32>(EAp5ProfileProcess::Count));
}

FAp5FrameProfiler& FAp5FrameProfiler::Get()
{
    static FAp5FrameProfiler Instance;
    return Instance;
}

FAp5FrameProfiler::FAp5FrameProfiler()
{
    BeginFrameHandle=FCoreDelegates::OnBeginFrame.AddRaw(
        this,&FAp5FrameProfiler::HandleBeginFrame);
    EndFrameHandle=FCoreDelegates::OnEndFrame.AddRaw(
        this,&FAp5FrameProfiler::HandleEndFrame);
}

FAp5FrameProfiler::~FAp5FrameProfiler()
{
    FCoreDelegates::OnBeginFrame.Remove(BeginFrameHandle);
    FCoreDelegates::OnEndFrame.Remove(EndFrameHandle);
}

void FAp5FrameProfiler::HandleBeginFrame()
{
    for (double& Value : CurrentMilliseconds) Value=0.0;
    FrameStartedSeconds=FPlatformTime::Seconds();
    bFrameOpen=true;
}

void FAp5FrameProfiler::HandleEndFrame()
{
    if (!bFrameOpen) return;

    ++FramesSinceWrite;
    if (FramesSinceWrite>=FileWriteIntervalFrames)
    {
        // I/O時間も現在フレームへ含めてからフレームを確定する。
        WriteSummaryIfNeeded();
        FramesSinceWrite=0;
    }

    // t.MaxFPS等による意図的なSleepは処理負荷ではないため、その他から分離する。
    // IdleTimeOvershootはIdleTimeの内数なので、要求待機と超過分に分けて合計がIdleTimeになるようにする。
    const double IdleMilliseconds=FMath::Max(0.0,FApp::GetIdleTime()*1000.0);
    const double OvershootMilliseconds=FMath::Clamp(
        FApp::GetIdleTimeOvershoot()*1000.0,0.0,IdleMilliseconds);
    CurrentMilliseconds[static_cast<int32>(EAp5ProfileProcess::FrameLimiterWait)]+=
        FMath::Max(0.0,IdleMilliseconds-OvershootMilliseconds);
    CurrentMilliseconds[static_cast<int32>(EAp5ProfileProcess::FrameLimiterOvershoot)]+=
        OvershootMilliseconds;

    // UEのフレーム統計は自前処理カテゴリと並列・重複するため「参考値」として記録し、
    // その他（未計測）の差し引きには使わない。
    CurrentMilliseconds[static_cast<int32>(EAp5ProfileProcess::EngineGameThreadCPU)]=
        FPlatformTime::ToMilliseconds(GGameThreadTime);
    CurrentMilliseconds[static_cast<int32>(EAp5ProfileProcess::EngineGameThreadWait)]=
        FPlatformTime::ToMilliseconds(GGameThreadWaitTime);
    CurrentMilliseconds[static_cast<int32>(EAp5ProfileProcess::EngineRenderThreadCPU)]=
        FPlatformTime::ToMilliseconds(GRenderThreadTime);
    CurrentMilliseconds[static_cast<int32>(EAp5ProfileProcess::EngineRenderThreadWait)]=
        FPlatformTime::ToMilliseconds(GRenderThreadWaitTime);
    CurrentMilliseconds[static_cast<int32>(EAp5ProfileProcess::EngineRHIThreadCPU)]=
        FPlatformTime::ToMilliseconds(GRHIThreadTime);
    CurrentMilliseconds[static_cast<int32>(EAp5ProfileProcess::EngineGPU)]=
        FPlatformTime::ToMilliseconds(RHIGetGPUFrameCycles(0));

    const double FrameMilliseconds=
        (FPlatformTime::Seconds()-FrameStartedSeconds)*1000.0;

    if (WarmupFramesRemaining>0)
    {
        --WarmupFramesRemaining;
        bFrameOpen=false;
        return;
    }

    FinalizeCurrentFrame(FrameMilliseconds);
    bFrameOpen=false;
}

void FAp5FrameProfiler::AddMilliseconds(EAp5ProfileProcess Process, double Milliseconds)
{
    if (!bFrameOpen) return;
    const int32 Index=static_cast<int32>(Process);
    if (Index<0 || Index>=static_cast<int32>(EAp5ProfileProcess::Count)) return;
    CurrentMilliseconds[Index]+=FMath::Max(0.0,Milliseconds);
}

void FAp5FrameProfiler::FinalizeCurrentFrame(double FrameMilliseconds)
{
    ++MeasuredFrameCount;
    TotalFrameMilliseconds+=FrameMilliseconds;
    MinFrameMilliseconds=FMath::Min(MinFrameMilliseconds,FrameMilliseconds);
    MaxFrameMilliseconds=FMath::Max(MaxFrameMilliseconds,FrameMilliseconds);

    if (FrameMilliseconds <= SlowFrameThresholdMilliseconds)
    {
        bSummaryDirty=true;
        return;
    }

    TotalSlowFrameMilliseconds+=FrameMilliseconds;
    MinSlowFrameMilliseconds=FMath::Min(MinSlowFrameMilliseconds,FrameMilliseconds);
    MaxSlowFrameMilliseconds=FMath::Max(MaxSlowFrameMilliseconds,FrameMilliseconds);

    double MeasuredMilliseconds = 0.0;
    const int32 UnmeasuredIndex = static_cast<int32>(EAp5ProfileProcess::Unmeasured);
    // Unmeasuredより後ろはGame/Render/GPUの参考値で、並列実行されるため加算しない。
    for (int32 I = 0; I < UnmeasuredIndex; ++I) MeasuredMilliseconds += CurrentMilliseconds[I];
    CurrentMilliseconds[UnmeasuredIndex] =
        FMath::Max(0.0,FrameMilliseconds-MeasuredMilliseconds);

    ++SlowFrameCount;
    for (int32 I = 0; I < static_cast<int32>(EAp5ProfileProcess::Count); ++I)
    {
        const double Value=CurrentMilliseconds[I];
        Summary[I].AverageMilliseconds +=
            (Value-Summary[I].AverageMilliseconds)/static_cast<double>(SlowFrameCount);
        Summary[I].MinMilliseconds=FMath::Min(Summary[I].MinMilliseconds,Value);
        Summary[I].MaxMilliseconds=FMath::Max(Summary[I].MaxMilliseconds,Value);
    }
    bSummaryDirty = true;
}

void FAp5FrameProfiler::WriteSummaryIfNeeded()
{
    if (!bSummaryDirty || MeasuredFrameCount == 0) return;

    FString Text = TEXT("処理,平均_ms,最小_ms,最大_ms\n");
    for (int32 I = 0; I < static_cast<int32>(EAp5ProfileProcess::Count); ++I)
    {
        const double Minimum=Summary[I].MinMilliseconds==TNumericLimits<double>::Max()
            ? 0.0 : Summary[I].MinMilliseconds;
        Text+=FString::Printf(TEXT("%s,%.3f,%.3f,%.3f\n"),
            ProcessNames[I],Summary[I].AverageMilliseconds,Minimum,Summary[I].MaxMilliseconds);
    }

    const FString Path=FPaths::Combine(FPaths::ProjectLogDir(),TEXT("Ap5SlowFrameProfile.csv"));
    const FString SummaryPath=FPaths::Combine(FPaths::ProjectLogDir(),TEXT("Ap5FrameSummary.csv"));
    const double IoStarted=FPlatformTime::Seconds();
    const bool bSaved=FFileHelper::SaveStringToFile(Text,*Path);

    const double AllAverage=MeasuredFrameCount>0
        ? TotalFrameMilliseconds/static_cast<double>(MeasuredFrameCount) : 0.0;
    const double SlowAverage=SlowFrameCount>0
        ? TotalSlowFrameMilliseconds/static_cast<double>(SlowFrameCount) : 0.0;
    const double SlowRate=MeasuredFrameCount>0
        ? 100.0*static_cast<double>(SlowFrameCount)/static_cast<double>(MeasuredFrameCount) : 0.0;
    const double AllMinimum=MinFrameMilliseconds==TNumericLimits<double>::Max()
        ? 0.0 : MinFrameMilliseconds;
    const double SlowMinimum=MinSlowFrameMilliseconds==TNumericLimits<double>::Max()
        ? 0.0 : MinSlowFrameMilliseconds;

    FString FrameSummary=TEXT("項目,値\n");
    FrameSummary+=FString::Printf(TEXT("計測フレーム数,%llu\n"),
        static_cast<unsigned long long>(MeasuredFrameCount));
    FrameSummary+=FString::Printf(TEXT("低FPSフレーム数,%llu\n"),
        static_cast<unsigned long long>(SlowFrameCount));
    FrameSummary+=FString::Printf(TEXT("低FPS率_pct,%.3f\n"),SlowRate);
    FrameSummary+=FString::Printf(TEXT("全フレーム平均_ms,%.3f\n"),AllAverage);
    FrameSummary+=FString::Printf(TEXT("全フレーム最小_ms,%.3f\n"),AllMinimum);
    FrameSummary+=FString::Printf(TEXT("全フレーム最大_ms,%.3f\n"),MaxFrameMilliseconds);
    FrameSummary+=FString::Printf(TEXT("低FPS平均_ms,%.3f\n"),SlowAverage);
    FrameSummary+=FString::Printf(TEXT("低FPS最小_ms,%.3f\n"),SlowMinimum);
    FrameSummary+=FString::Printf(TEXT("低FPS最大_ms,%.3f\n"),MaxSlowFrameMilliseconds);
    const bool bSummarySaved=FFileHelper::SaveStringToFile(FrameSummary,*SummaryPath);

    const double IoMilliseconds=(FPlatformTime::Seconds()-IoStarted)*1000.0;
    CurrentMilliseconds[static_cast<int32>(EAp5ProfileProcess::ProfilerIO)]+=IoMilliseconds;
    if (bSaved && bSummarySaved) bSummaryDirty=false;
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
