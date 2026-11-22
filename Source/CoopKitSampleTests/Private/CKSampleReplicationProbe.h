// Copyright 2026 Ivan Iaitckikh. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "CKSampleReplicationProbe.generated.h"

/**
 * Minimal replicated actor used by the replication automation tests.
 * Carries one replicated property and one reliable multicast event; the console variable
 * ck.sample.BreakReplication 1 disables both on the server to prove that the tests catch a broken replication.
 */
UCLASS(Transient, NotPlaceable)
class ACKSampleReplicationProbe : public AActor
{
	GENERATED_BODY()

public:
	ACKSampleReplicationProbe(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void PreReplication(IRepChangedPropertyTracker& ChangedPropertyTracker) override;

	/** Sets the replicated value. Server only. */
	void SetValue(int32 NewValue);

	/** Sends the multicast event to every machine. Server only. */
	void SendEvent(int32 Payload);

	int32 GetValue() const
	{
		return Value;
	}

	/** Number of multicast events this instance has received, including the server copy. */
	int32 GetEventCount() const
	{
		return EventCount;
	}

	int32 GetLastEventPayload() const
	{
		return LastEventPayload;
	}

	/** True while ck.sample.BreakReplication is set: the property stops replicating and the event stays on the server. */
	static bool IsReplicationBroken();

private:
	UFUNCTION(NetMulticast, Reliable)
	void MulticastEvent(int32 Payload);

	/** Value written on the server and checked on every client. */
	UPROPERTY(Replicated)
	int32 Value = 0;

	int32 EventCount = 0;
	int32 LastEventPayload = 0;
};
