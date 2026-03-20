#pragma once
#include "Modules/ModuleManager.h"

class UClientDemoRecorder;

class FClientDemosModule : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

private:
	UClientDemoRecorder* Recorder = nullptr;
};
