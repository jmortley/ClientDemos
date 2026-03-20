#include "ClientDemosModule.h"
#include "ClientDemoSubsystem.h"

IMPLEMENT_MODULE(FClientDemosModule, ClientDemos)

void FClientDemosModule::StartupModule()
{
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
