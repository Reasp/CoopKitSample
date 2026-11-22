// Copyright 2026 Ivan Iaitckikh. All Rights Reserved.

#include "CKSampleReplicationProbe.h"
#include "HAL/IConsoleManager.h"
#include "Net/UnrealNetwork.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(CKSampleReplicationProbe)

namespace CKSample::Tests
{
	static TAutoConsoleVariable<int32> CVarBreakReplication(
		TEXT("ck.sample.BreakReplication"),
		0,
		TEXT("Test only. 1 stops ACKSampleReplicationProbe from replicating its value and multicast event, so the replication tests must fail."),
		ECVF_Cheat);
}

ACKSampleReplicationProbe::ACKSampleReplicationProbe(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bReplicates = true;
	bAlwaysRelevant = true;
	PrimaryActorTick.bCanEverTick = false;
}

void ACKSampleReplicationProbe::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	// COND_Custom lets PreReplication switch the property off for the deliberate-break run.
	FDoRepLifetimeParams Params;
	Params.Condition = COND_Custom;
	DOREPLIFETIME_WITH_PARAMS_FAST(ACKSampleReplicationProbe, Value, Params);
}

void ACKSampleReplicationProbe::PreReplication(IRepChangedPropertyTracker& ChangedPropertyTracker)
{
	Super::PreReplication(ChangedPropertyTracker);

	DOREPLIFETIME_ACTIVE_OVERRIDE_FAST(ACKSampleReplicationProbe, Value, !IsReplicationBroken());
}

bool ACKSampleReplicationProbe::IsReplicationBroken()
{
	return CKSample::Tests::CVarBreakReplication.GetValueOnGameThread() != 0;
}

void ACKSampleReplicationProbe::SetValue(const int32 NewValue)
{
	check(HasAuthority());
	Value = NewValue;
}

void ACKSampleReplicationProbe::SendEvent(const int32 Payload)
{
	check(HasAuthority());
	if (IsReplicationBroken())
	{
		// Simulates a forgotten NetMulticast: the event only happens on the server.
		MulticastEvent_Implementation(Payload);
		return;
	}

	MulticastEvent(Payload);
}

void ACKSampleReplicationProbe::MulticastEvent_Implementation(const int32 Payload)
{
	++EventCount;
	LastEventPayload = Payload;
}
