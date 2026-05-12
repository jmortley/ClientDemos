#include "ClientDemosModule.h"
#include "ClientDemoSubsystem.h"
#include "Misc/ConfigCacheIni.h"

IMPLEMENT_MODULE(FClientDemosModule, ClientDemos)

void FClientDemosModule::StartupModule()
{
	// Override DemoNetDriver class to use our throttled version.
	// Must happen before any DemoNetDriver is created.
	// The engine reads NetDriverDefinitions from [/Script/Engine.Engine] in GEngineIni.
	FString EngineSection = TEXT("/Script/Engine.Engine");

	// Remove existing DemoNetDriver definition and add ours
	TArray<FString> NetDriverDefs;
	GConfig->GetArray(*EngineSection, TEXT("NetDriverDefinitions"), NetDriverDefs, GEngineIni);

	// Filter out existing DemoNetDriver entries
	TArray<FString> FilteredDefs;
	for (const FString& Def : NetDriverDefs)
	{
		if (!Def.Contains(TEXT("DemoNetDriver")))
		{
			FilteredDefs.Add(Def);
		}
	}

	// Add our throttled driver with UTDemoNetDriver as fallback
	FilteredDefs.Add(TEXT("(DefName=\"DemoNetDriver\",DriverClassName=\"/Script/ClientDemos.ClientDemoNetDriver\",DriverClassNameFallback=\"/Script/UnrealTournament.UTDemoNetDriver\")"));

	GConfig->SetArray(*EngineSection, TEXT("NetDriverDefinitions"), FilteredDefs, GEngineIni);

	UE_LOG(LogTemp, Warning, TEXT("ClientDemos: Registered ClientDemoNetDriver as DemoNetDriver"));

	Recorder = NewObject<UClientDemoRecorder>();
	Recorder->AddToRoot(); // Prevent GC
	Recorder->Init();
}

void FClientDemosModule::ShutdownModule()
{
	if (Recorder)
	{
		Recorder->Shutdown();
		Recorder->RemoveFromRoot();
		Recorder = nullptr;
	}
}
