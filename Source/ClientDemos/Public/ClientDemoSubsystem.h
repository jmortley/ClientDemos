#pragma once
#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "ClientDemoSubsystem.generated.h"

/**
 * Client-side demo recording manager.
 *
 * Adds console commands for clients to record demos locally:
 *   clientdemorec [name]  - Start recording (auto-names if no name given)
 *   clientdemostop        - Stop recording
 *   clientdemostatus      - Show recording state and file path
 *   clientdemoplay [name] - Play back a local demo
 *
 * Demos are saved to: [GameSavedDir]/Demos/[DemoName]/
 * Typically: %LOCALAPPDATA%/UnrealTournament/Saved/Demos/
 *
 * Created by the module startup — no mutator or controller dependency.
 */
UCLASS()
class CLIENTDEMOS_API UClientDemoRecorder : public UObject
{
	GENERATED_BODY()

public:
	/** Register console commands. Call from module startup. */
	void Init();

	/** Unregister and stop recording. Call from module shutdown. */
	void Shutdown();

private:
	void HandleDemoRec(const TArray<FString>& Args);
	void HandleDemoStop();
	void HandleDemoStatus();
	void HandleDemoPlay(const TArray<FString>& Args);

	/** Build a default demo name: MapName-Date-Time-PlayerName */
	FString MakeDefaultDemoName() const;

	/** Get the current world (first game world found) */
	UWorld* GetCurrentWorld() const;

	bool bIsRecordingClientDemo = false;
	FString CurrentDemoName;

	// Tick throttle — prevent DemoNetDriver from running TickDemoRecord on every game frame.
	// At 480fps, the driver wastes ~120fps worth of budget iterating actors on frames it won't record.
	// We pause recording most frames and only unpause at the target record rate.
	float ThrottleAccumulator = 0.0f;
	float ThrottleInterval = 0.0f; // 1.0 / RecordHz, computed at record start
	bool bThrottleActive = false;

	/** Called every game frame via FTickerDelegate to gate DemoNetDriver ticks. */
	bool TickThrottle(float DeltaSeconds);

	FDelegateHandle ThrottleTickHandle;

	// Playback HUD fix — polls for spectator PC after replay loads, then force-creates AUTHUD.
	FDelegateHandle PlaybackHUDTickHandle;
	float PlaybackHUDWaitTime = 0.0f;
	bool TickPlaybackHUD(float DeltaSeconds);
};
