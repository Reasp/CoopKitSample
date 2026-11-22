// Copyright 2026 Ivan Iaitckikh. All Rights Reserved.

#pragma once

#include "CKMatchmakingTypes.h"
#include "CKSessionsTypes.h"
#include "Containers/Ticker.h"
#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "UObject/SoftObjectPath.h"
#include "CKSampleMatchmakingSessionsBridge.generated.h"

class UCKMatchmakingSubsystem;
class UCKSessionsSubsystem;

/**
 * Bridge: CoopKit Matchmaking -> CoopKit Sessions (TASK-50, decision-51).
 *
 * A match found in a queue with Join Mode Manual is joined through a CoopKit Sessions session: the match host (the same
 * player on every machine) hosts a session that advertises MatchId = the match id on the queue's Host Map; every other
 * player searches for sessions with that MatchId (Find Sessions With Attributes), again every second until the queue's
 * Host Timeout, and joins the one found. The result goes back to CoopKit Matchmaking (Report Join Result), which sends
 * the ticket again when the host's session never showed up. Neither plugin knows the other; the transport (Null, Steam,
 * EOS) is the one CoopKit Sessions uses, so the online service, not the matchmaker, relays the connection.
 *
 * Only the queues listed in Queues are bridged; matches of other queues are left to their game or to the plugin's own
 * join. Configured in Config/DefaultGame.ini, section [/Script/CoopKitSampleBridges.CKSampleMatchmakingSessionsBridge].
 *
 * Console: ck.sample.FindMatch [queue asset path] [Key=Value ...] starts matchmaking with the sample's queue
 * DA_CKSampleMatchQueue. Test: CoopKit.Sample.Matchmaking.SessionBridge.
 */
UCLASS(Config = Game)
class UCKSampleMatchmakingSessionsBridge final : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

private:
	enum class EStep : uint8
	{
		Idle,
		Hosting,
		Searching,
		WaitingToSearch,
		Joining
	};

	UFUNCTION()
	void HandleMatchFound(const FCKMatchResult& Match);

	UFUNCTION()
	void HandleHostComplete(bool bSuccess, ECKSessionsFailure Failure);

	UFUNCTION()
	void HandleFindComplete(bool bSuccess, const TArray<FCKSessionsSearchResult>& Results);

	UFUNCTION()
	void HandleJoinComplete(bool bSuccess, ECKSessionsFailure Failure);

	bool Tick(float DeltaSeconds);
	void SearchForHost();
	void Finish(bool bSuccess, ECKMatchmakingFailure Failure);

	/** Queues whose matches (Join Mode Manual) the bridge joins through a CoopKit Sessions session. */
	UPROPERTY(Config)
	TArray<FSoftObjectPath> Queues;

	UPROPERTY(Transient)
	TObjectPtr<UCKMatchmakingSubsystem> Matchmaking;

	UPROPERTY(Transient)
	TObjectPtr<UCKSessionsSubsystem> Sessions;

	EStep Step = EStep::Idle;
	TMap<FName, FString> MatchFilter;
	double HostDeadlineSeconds = 0.0;
	double NextSearchSeconds = 0.0;
	FTSTicker::FDelegateHandle TickHandle;
};
