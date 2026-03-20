// ClientDemoSubsystem.cpp
// Client-side demo recording for UT4

#include "ClientDemoSubsystem.h"
#include "Engine/GameEngine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Engine/DemoNetDriver.h"
#include "Misc/DateTime.h"
#include "Misc/Paths.h"
#include "HAL/IConsoleManager.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"

DEFINE_LOG_CATEGORY_STATIC(LogClientDemos, Log, All);

UWorld* UClientDemoRecorder::GetCurrentWorld() const
{
	if (GEngine)
	{
		for (const FWorldContext& Context : GEngine->GetWorldContexts())
		{
			if (Context.WorldType == EWorldType::Game || Context.WorldType == EWorldType::PIE)
			{
				return Context.World();
			}
		}
	}
	return nullptr;
}

void UClientDemoRecorder::Init()
{
	IConsoleManager::Get().RegisterConsoleCommand(
		TEXT("clientdemorec"),
		TEXT("Start recording a client-side demo. Usage: clientdemorec [name]"),
		FConsoleCommandWithArgsDelegate::CreateUObject(this, &UClientDemoRecorder::HandleDemoRec),
		ECVF_Default
	);

	IConsoleManager::Get().RegisterConsoleCommand(
		TEXT("clientdemostop"),
		TEXT("Stop recording the current client-side demo."),
		FConsoleCommandDelegate::CreateUObject(this, &UClientDemoRecorder::HandleDemoStop),
		ECVF_Default
	);

	IConsoleManager::Get().RegisterConsoleCommand(
		TEXT("clientdemostatus"),
		TEXT("Show current demo recording status."),
		FConsoleCommandDelegate::CreateUObject(this, &UClientDemoRecorder::HandleDemoStatus),
		ECVF_Default
	);

	IConsoleManager::Get().RegisterConsoleCommand(
		TEXT("clientdemoplay"),
		TEXT("Play back a local client-side demo. Usage: clientdemoplay [name]"),
		FConsoleCommandWithArgsDelegate::CreateUObject(this, &UClientDemoRecorder::HandleDemoPlay),
		ECVF_Default
	);

	UE_LOG(LogClientDemos, Log, TEXT("ClientDemos initialized. Use 'clientdemorec' to start recording."));
}

void UClientDemoRecorder::Shutdown()
{
	if (bIsRecordingClientDemo)
	{
		HandleDemoStop();
	}

	IConsoleObject* DemoRecCmd = IConsoleManager::Get().FindConsoleObject(TEXT("clientdemorec"));
	if (DemoRecCmd) { IConsoleManager::Get().UnregisterConsoleObject(DemoRecCmd, false); }
	IConsoleObject* DemoStopCmd = IConsoleManager::Get().FindConsoleObject(TEXT("clientdemostop"));
	if (DemoStopCmd) { IConsoleManager::Get().UnregisterConsoleObject(DemoStopCmd, false); }
	IConsoleObject* DemoStatusCmd = IConsoleManager::Get().FindConsoleObject(TEXT("clientdemostatus"));
	if (DemoStatusCmd) { IConsoleManager::Get().UnregisterConsoleObject(DemoStatusCmd, false); }
	IConsoleObject* DemoPlayCmd = IConsoleManager::Get().FindConsoleObject(TEXT("clientdemoplay"));
	if (DemoPlayCmd) { IConsoleManager::Get().UnregisterConsoleObject(DemoPlayCmd, false); }
}

void UClientDemoRecorder::HandleDemoRec(const TArray<FString>& Args)
{
	UWorld* World = GetCurrentWorld();
	if (!World)
	{
		UE_LOG(LogClientDemos, Warning, TEXT("demorec: No world available."));
		return;
	}

	if (bIsRecordingClientDemo)
	{
		UE_LOG(LogClientDemos, Warning, TEXT("demorec: Already recording '%s'. Use 'clientdemostop' first."), *CurrentDemoName);
		APlayerController* PC = World->GetFirstPlayerController();
		if (PC)
		{
			PC->ClientMessage(FString::Printf(TEXT("Already recording demo '%s'. Use 'clientdemostop' first."), *CurrentDemoName));
		}
		return;
	}

	// Check if a replay is currently playing
	if (World->DemoNetDriver && World->DemoNetDriver->IsPlaying())
	{
		UE_LOG(LogClientDemos, Warning, TEXT("demorec: A replay is currently playing."));
		return;
	}

	// Build demo name
	FString DemoName;
	if (Args.Num() > 0 && !Args[0].IsEmpty())
	{
		DemoName = Args[0];
	}
	else
	{
		DemoName = MakeDefaultDemoName();
	}

	// Get game instance and start recording
	UGameInstance* GI = World->GetGameInstance();
	if (!GI)
	{
		UE_LOG(LogClientDemos, Error, TEXT("demorec: No GameInstance."));
		return;
	}

	// Force local file streaming — UT4's default streamer is HTTP (Epic's dead Azure server).
	// The engine supports ReplayStreamerOverride= URL option (DemoNetDriver.cpp:373).
	TArray<FString> Options;
	Options.Add(TEXT("ReplayStreamerOverride=NullNetworkReplayStreaming"));

	UE_LOG(LogClientDemos, Warning, TEXT("demorec: Starting local recording '%s' World=%s GI=%s"),
		*DemoName, *World->GetName(), *GI->GetName());

	// Check pre-existing DemoNetDriver
	if (World->DemoNetDriver)
	{
		UE_LOG(LogClientDemos, Warning, TEXT("demorec: Pre-existing DemoNetDriver found, IsRecording=%d IsPlaying=%d"),
			World->DemoNetDriver->IsRecording(), World->DemoNetDriver->IsPlaying());
	}

	GI->StartRecordingReplay(DemoName, DemoName, Options);

	// Check result
	if (World->DemoNetDriver)
	{
		UE_LOG(LogClientDemos, Warning, TEXT("demorec: Post-call DemoNetDriver exists, IsRecording=%d IsPlaying=%d"),
			World->DemoNetDriver->IsRecording(), World->DemoNetDriver->IsPlaying());
	}
	else
	{
		UE_LOG(LogClientDemos, Warning, TEXT("demorec: Post-call DemoNetDriver is NULL (InitListen failed)"));
	}

	UE_LOG(LogClientDemos, Log, TEXT("demorec: After StartRecordingReplay - DemoNetDriver=%s IsRecording=%d"),
		World->DemoNetDriver ? TEXT("exists") : TEXT("null"),
		(World->DemoNetDriver && World->DemoNetDriver->IsRecording()) ? 1 : 0);

	// Verify
	if (World->DemoNetDriver && World->DemoNetDriver->IsRecording())
	{
		bIsRecordingClientDemo = true;
		CurrentDemoName = DemoName;

		FString DemoPath = FPaths::Combine(*FPaths::GameSavedDir(), TEXT("Demos"), *DemoName);
		UE_LOG(LogClientDemos, Log, TEXT("Recording demo '%s' to: %s"), *DemoName, *DemoPath);

		APlayerController* PC = World->GetFirstPlayerController();
		if (PC)
		{
			PC->ClientMessage(FString::Printf(TEXT("Recording demo: %s"), *DemoName));
		}
	}
	else
	{
		// Check if DemoNetDriver was created but InitListen failed
		FString FailReason = TEXT("Unknown");
		if (!World->DemoNetDriver)
		{
			FailReason = TEXT("DemoNetDriver is null after StartRecordingReplay - driver creation failed");
		}
		else if (!World->DemoNetDriver->IsRecording())
		{
			FailReason = TEXT("DemoNetDriver exists but IsRecording()=false - InitListen likely failed");
		}

		UE_LOG(LogClientDemos, Error, TEXT("demorec: Failed to start recording '%s'. Reason: %s"), *DemoName, *FailReason);
		APlayerController* PC = World->GetFirstPlayerController();
		if (PC)
		{
			PC->ClientMessage(FString::Printf(TEXT("Failed to record demo '%s': %s"), *DemoName, *FailReason));
		}
	}
}

void UClientDemoRecorder::HandleDemoStop()
{
	UWorld* World = GetCurrentWorld();

	if (!bIsRecordingClientDemo)
	{
		UE_LOG(LogClientDemos, Warning, TEXT("demostop: Not currently recording."));
		if (World)
		{
			APlayerController* PC = World->GetFirstPlayerController();
			if (PC)
			{
				PC->ClientMessage(TEXT("Not currently recording a demo."));
			}
		}
		return;
	}

	if (World)
	{
		UGameInstance* GI = World->GetGameInstance();
		if (GI)
		{
			GI->StopRecordingReplay();
		}
	}

	FString DemoPath = FPaths::Combine(*FPaths::GameSavedDir(), TEXT("Demos"), *CurrentDemoName);
	UE_LOG(LogClientDemos, Log, TEXT("Stopped recording demo '%s'. Saved to: %s"), *CurrentDemoName, *DemoPath);

	if (World)
	{
		APlayerController* PC = World->GetFirstPlayerController();
		if (PC)
		{
			PC->ClientMessage(FString::Printf(TEXT("Demo saved: %s"), *DemoPath));
		}
	}

	bIsRecordingClientDemo = false;
	CurrentDemoName.Empty();
}

void UClientDemoRecorder::HandleDemoStatus()
{
	UWorld* World = GetCurrentWorld();
	if (!World)
	{
		return;
	}

	APlayerController* PC = World->GetFirstPlayerController();

	if (bIsRecordingClientDemo)
	{
		FString DemoPath = FPaths::Combine(*FPaths::GameSavedDir(), TEXT("Demos"), *CurrentDemoName);

		float DemoSeconds = 0.0f;
		if (World->DemoNetDriver)
		{
			DemoSeconds = World->DemoNetDriver->DemoCurrentTime;
		}

		int32 Minutes = FMath::FloorToInt(DemoSeconds / 60.0f);
		int32 Seconds = FMath::FloorToInt(FMath::Fmod(DemoSeconds, 60.0f));

		FString StatusMsg = FString::Printf(TEXT("Recording: %s (%d:%02d) -> %s"),
			*CurrentDemoName, Minutes, Seconds, *DemoPath);

		UE_LOG(LogClientDemos, Log, TEXT("%s"), *StatusMsg);
		if (PC)
		{
			PC->ClientMessage(StatusMsg);
		}
	}
	else
	{
		if (PC)
		{
			PC->ClientMessage(TEXT("Not recording. Use 'clientdemorec [name]' to start."));
		}
	}
}

void UClientDemoRecorder::HandleDemoPlay(const TArray<FString>& Args)
{
	if (Args.Num() == 0 || Args[0].IsEmpty())
	{
		UE_LOG(LogClientDemos, Warning, TEXT("clientdemoplay: Usage: clientdemoplay <name>"));
		UWorld* World = GetCurrentWorld();
		if (World)
		{
			APlayerController* PC = World->GetFirstPlayerController();
			if (PC)
			{
				PC->ClientMessage(TEXT("Usage: clientdemoplay <name>"));
			}
		}
		return;
	}

	UWorld* World = GetCurrentWorld();
	if (!World)
	{
		UE_LOG(LogClientDemos, Warning, TEXT("clientdemoplay: No world available."));
		return;
	}

	UGameInstance* GI = World->GetGameInstance();
	if (!GI)
	{
		UE_LOG(LogClientDemos, Error, TEXT("clientdemoplay: No GameInstance."));
		return;
	}

	FString DemoName = Args[0];

	// Force local file streaming for playback too
	TArray<FString> Options;
	Options.Add(TEXT("ReplayStreamerOverride=NullNetworkReplayStreaming"));

	UE_LOG(LogClientDemos, Warning, TEXT("clientdemoplay: Playing back '%s' with local streamer"), *DemoName);

	GI->PlayReplay(DemoName, nullptr, Options);
}

FString UClientDemoRecorder::MakeDefaultDemoName() const
{
	UWorld* World = GetCurrentWorld();

	// Map name
	FString MapName = World ? World->GetMapName() : TEXT("Unknown");
	int32 LastSlash;
	if (MapName.FindLastChar('/', LastSlash))
	{
		MapName = MapName.RightChop(LastSlash + 1);
	}
	if (World)
	{
		MapName.RemoveFromStart(World->StreamingLevelsPrefix);
	}

	// Timestamp
	FDateTime Now = FDateTime::Now();
	FString Timestamp = Now.ToString(TEXT("%Y.%m.%d-%H.%M.%S"));

	// Player name
	FString PlayerName;
	if (World)
	{
		APlayerController* PC = World->GetFirstPlayerController();
		if (PC && PC->PlayerState)
		{
			PlayerName = PC->PlayerState->PlayerName;
			PlayerName.ReplaceInline(TEXT(" "), TEXT("_"));
			PlayerName.ReplaceInline(TEXT("/"), TEXT("_"));
			PlayerName.ReplaceInline(TEXT("\\"), TEXT("_"));
			PlayerName.ReplaceInline(TEXT("."), TEXT("_"));
		}
	}

	if (PlayerName.IsEmpty())
	{
		return FString::Printf(TEXT("%s-%s"), *MapName, *Timestamp);
	}

	return FString::Printf(TEXT("%s-%s-%s"), *MapName, *Timestamp, *PlayerName);
}
