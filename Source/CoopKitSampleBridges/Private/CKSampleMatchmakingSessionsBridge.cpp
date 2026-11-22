// Copyright 2026 Ivan Iaitckikh. All Rights Reserved.

#include "CKSampleMatchmakingSessionsBridge.h"
#include "CKMatchmakingQueueData.h"
#include "CKMatchmakingSubsystem.h"
#include "CKSessionsSubsystem.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
#include "HAL/PlatformTime.h"
#include "UObject/SoftObjectPath.h"

DEFINE_LOG_CATEGORY_STATIC(LogCKSampleMatchmaking, Log, All);

namespace CKSample::MatchmakingBridge
{
	/** Session attribute with the match id: the host advertises it, the other players search for it. */
	const FName MatchIdAttribute(TEXT("MatchId"));

	/** Pause between two searches for the host's session, in seconds. */
	constexpr double SearchPauseSeconds = 1.0;

	/** Queue of ck.sample.FindMatch without a queue argument: Local provider, Join Mode Manual, 3 players, host map L_Lobby. */
	const TCHAR* const SampleQueuePath = TEXT("/Game/CoopKitSample/Data/DA_CKSampleMatchQueue.DA_CKSampleMatchQueue");

	void FindMatch(const TArray<FString>& Args, UWorld* World)
	{
		UCKMatchmakingSubsystem* Matchmaking = UCKMatchmakingSubsystem::Get(World);
		if (Matchmaking == nullptr)
		{
			UE_LOG(LogCKSampleMatchmaking, Warning, TEXT("ck.sample.FindMatch: run it in a game (PIE or standalone)."));
			return;
		}
		FString QueuePath = SampleQueuePath;
		TMap<FName, FString> Attributes;
		for (const FString& Arg : Args)
		{
			FString Key;
			FString Value;
			if (Arg.Split(TEXT("="), &Key, &Value))
			{
				Attributes.Add(FName(*Key), Value);
			}
			else if (Arg.StartsWith(TEXT("/")))
			{
				QueuePath = Arg;
			}
		}
		const UCKMatchmakingQueueData* Queue = Cast<UCKMatchmakingQueueData>(FSoftObjectPath(QueuePath).TryLoad());
		if (Queue == nullptr)
		{
			UE_LOG(LogCKSampleMatchmaking, Warning, TEXT("ck.sample.FindMatch: %s is not a matchmaking queue."), *QueuePath);
			return;
		}
		Matchmaking->StartMatchmaking(Queue, Attributes);
	}

	FAutoConsoleCommandWithWorldAndArgs FindMatchCommand(TEXT("ck.sample.FindMatch"),
		TEXT("Starts matchmaking; a match of a Join Mode Manual queue is joined through a CoopKit Sessions session. Arguments (optional): a queue asset path (default: the sample's DA_CKSampleMatchQueue) and Key=Value ticket attributes."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&FindMatch));
}

void UCKSampleMatchmakingSessionsBridge::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	Matchmaking = Collection.InitializeDependency<UCKMatchmakingSubsystem>();
	Sessions = Collection.InitializeDependency<UCKSessionsSubsystem>();
	if (Matchmaking == nullptr || Sessions == nullptr)
	{
		return;
	}
	Matchmaking->OnMatchFound.AddDynamic(this, &UCKSampleMatchmakingSessionsBridge::HandleMatchFound);
	Sessions->OnHostComplete.AddDynamic(this, &UCKSampleMatchmakingSessionsBridge::HandleHostComplete);
	Sessions->OnFindComplete.AddDynamic(this, &UCKSampleMatchmakingSessionsBridge::HandleFindComplete);
	Sessions->OnJoinComplete.AddDynamic(this, &UCKSampleMatchmakingSessionsBridge::HandleJoinComplete);
	TickHandle = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateUObject(this, &UCKSampleMatchmakingSessionsBridge::Tick));
}

void UCKSampleMatchmakingSessionsBridge::Deinitialize()
{
	FTSTicker::GetCoreTicker().RemoveTicker(TickHandle);
	if (Matchmaking != nullptr)
	{
		Matchmaking->OnMatchFound.RemoveAll(this);
	}
	if (Sessions != nullptr)
	{
		Sessions->OnHostComplete.RemoveAll(this);
		Sessions->OnFindComplete.RemoveAll(this);
		Sessions->OnJoinComplete.RemoveAll(this);
	}
	Super::Deinitialize();
}

void UCKSampleMatchmakingSessionsBridge::HandleMatchFound(const FCKMatchResult& Match)
{
	// Join Mode Automatic: the plugin joins by itself; a dedicated server address needs no session either.
	const UCKMatchmakingQueueData* Queue = Matchmaking->GetActiveQueue();
	// Queues outside the list belong to their game, or to the plugin's own join.
	if (Queue == nullptr || Queue->GetJoinMode() != ECKMatchmakingJoinMode::Manual || !Match.ConnectionInfo.IsEmpty()
		|| !Queues.Contains(FSoftObjectPath(Queue)))
	{
		return;
	}

	MatchFilter.Reset();
	MatchFilter.Add(CKSample::MatchmakingBridge::MatchIdAttribute, Match.MatchId);
	HostDeadlineSeconds = FPlatformTime::Seconds() + Queue->GetHostTimeoutSeconds();

	if (Match.IsLocalPlayerHost())
	{
		UE_LOG(LogCKSampleMatchmaking, Log, TEXT("%s: hosting the session of match %s."), *GetNameSafe(GetGameInstance()), *Match.MatchId);
		Step = EStep::Hosting;
		if (!Sessions->HostSessionWithAttributes(nullptr, Queue->GetHostMap(), MatchFilter) && Step == EStep::Hosting)
		{
			Finish(false, ECKMatchmakingFailure::JoinFailed);
		}
		return;
	}

	UE_LOG(LogCKSampleMatchmaking, Log, TEXT("%s: looking for the session of match %s."), *GetNameSafe(GetGameInstance()), *Match.MatchId);
	SearchForHost();
}

void UCKSampleMatchmakingSessionsBridge::SearchForHost()
{
	Step = EStep::Searching;
	if (!Sessions->FindSessionsWithAttributes(nullptr, MatchFilter) && Step == EStep::Searching)
	{
		// Refused (busy): try again after the pause, until the host timeout.
		Step = EStep::WaitingToSearch;
		NextSearchSeconds = FPlatformTime::Seconds() + CKSample::MatchmakingBridge::SearchPauseSeconds;
	}
}

void UCKSampleMatchmakingSessionsBridge::HandleHostComplete(const bool bSuccess, const ECKSessionsFailure Failure)
{
	if (Step == EStep::Hosting)
	{
		Finish(bSuccess, bSuccess ? ECKMatchmakingFailure::None : ECKMatchmakingFailure::JoinFailed);
	}
}

void UCKSampleMatchmakingSessionsBridge::HandleFindComplete(const bool bSuccess, const TArray<FCKSessionsSearchResult>& Results)
{
	if (Step != EStep::Searching)
	{
		return;
	}
	if (bSuccess && !Results.IsEmpty())
	{
		Step = EStep::Joining;
		if (!Sessions->JoinSession(Results[0]) && Step == EStep::Joining)
		{
			Finish(false, ECKMatchmakingFailure::JoinFailed);
		}
		return;
	}
	// The host may still be opening its map: search again until the host timeout.
	Step = EStep::WaitingToSearch;
	NextSearchSeconds = FPlatformTime::Seconds() + CKSample::MatchmakingBridge::SearchPauseSeconds;
}

void UCKSampleMatchmakingSessionsBridge::HandleJoinComplete(const bool bSuccess, const ECKSessionsFailure Failure)
{
	if (Step == EStep::Joining)
	{
		Finish(bSuccess, bSuccess ? ECKMatchmakingFailure::None : ECKMatchmakingFailure::JoinFailed);
	}
}

bool UCKSampleMatchmakingSessionsBridge::Tick(const float DeltaSeconds)
{
	if (Step != EStep::WaitingToSearch)
	{
		return true;
	}
	const double Now = FPlatformTime::Seconds();
	if (Now > HostDeadlineSeconds)
	{
		UE_LOG(LogCKSampleMatchmaking, Warning, TEXT("%s: no session of the match within the host timeout."), *GetNameSafe(GetGameInstance()));
		Finish(false, ECKMatchmakingFailure::HostTimeout);
	}
	else if (Now >= NextSearchSeconds)
	{
		SearchForHost();
	}
	return true;
}

void UCKSampleMatchmakingSessionsBridge::Finish(const bool bSuccess, const ECKMatchmakingFailure Failure)
{
	Step = EStep::Idle;
	MatchFilter.Reset();
	Matchmaking->ReportJoinResult(bSuccess, Failure);
}
