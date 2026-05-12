// ClientDemoNetDriver.h
// Throttled DemoNetDriver that skips expensive actor prioritization on non-record frames.
// At 480fps with 20Hz recording, this eliminates ~460 wasted iteration passes per second.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DemoNetDriver.h"
#include "ClientDemoNetDriver.generated.h"

UCLASS()
class CLIENTDEMOS_API UClientDemoNetDriver : public UDemoNetDriver
{
	GENERATED_BODY()

public:
	virtual void TickFlush(float DeltaSeconds) override;

private:
	/** Accumulates time between record frames */
	float RecordAccumulator = 0.0f;
};
