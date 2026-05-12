// ClientDemoNetDriver.cpp
// Throttled DemoNetDriver — only runs the expensive TickDemoRecord path at RecordHz.
// On all other frames, just advances DemoCurrentTime so playback timing stays correct.

#include "ClientDemoNetDriver.h"
#include "Engine/World.h"
#include "GameFramework/WorldSettings.h"

static TAutoConsoleVariable<float> CVarClientDemoRecordHz(
	TEXT("demo.ClientRecordHz"),
	20.0f,
	TEXT("Record Hz used by ClientDemoNetDriver throttle. Controls how often the expensive actor serialization runs. Set to 0 to disable throttle and use stock behavior.")
);

void UClientDemoNetDriver::TickFlush(float DeltaSeconds)
{
	const float TargetHz = CVarClientDemoRecordHz.GetValueOnAnyThread();

	// If throttle disabled, not recording, paused, or playing back — use stock behavior
	if (TargetHz <= 0.0f || !IsRecording() || IsRecordingPaused() || IsPlaying())
	{
		Super::TickFlush(DeltaSeconds);
		return;
	}

	static bool bLoggedOnce = false;
	if (!bLoggedOnce)
	{
		UE_LOG(LogTemp, Warning, TEXT("ClientDemoNetDriver: Throttle ACTIVE at %.0fHz"), TargetHz);
		bLoggedOnce = true;
	}

	const float RecordInterval = 1.0f / FMath::Clamp(TargetHz, 1.0f, 120.0f);

	RecordAccumulator += DeltaSeconds;

	if (RecordAccumulator >= RecordInterval)
	{
		// Time to record — pre-advance DemoCurrentTime by the skipped frames' worth of time.
		// Super::TickFlush will advance it by this frame's DeltaSeconds via TickDemoRecord.
		// We need to add the accumulated skipped time so the demo clock stays accurate.
		const float SkippedTime = RecordAccumulator - DeltaSeconds;
		if (SkippedTime > 0.0f && World != nullptr)
		{
			const float TimeDilation = World->GetWorldSettings()->GetEffectiveTimeDilation();
			const float ClampedSkipped = World->GetWorldSettings()->FixupDeltaSeconds(
				SkippedTime * TimeDilation, SkippedTime);
			DemoCurrentTime += ClampedSkipped;

			if (ReplayStreamer.IsValid())
			{
				ReplayStreamer->UpdateTotalDemoTime(GetDemoCurrentTimeInMS());
			}
		}

		RecordAccumulator = 0.0f;

		// Full tick — actor prioritization, serialization, the works
		Super::TickFlush(DeltaSeconds);
	}
	else
	{
		// Non-record frame — skip the expensive TickDemoRecord entirely.
		// At 480fps with 20Hz this skips ~460 actor iteration passes per second.
		// DemoCurrentTime is NOT advanced here — it's batched and applied on the next record frame.
		PostTickFlush();
	}
}
