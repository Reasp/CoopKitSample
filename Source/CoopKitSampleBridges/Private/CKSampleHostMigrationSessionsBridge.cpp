// Copyright 2026 Ivan Iaitckikh. All Rights Reserved.

#include "CKSampleHostMigrationSessionsBridge.h"
#include "CKHostMigrationProtocol.h"
#include "CKSessionsLibrary.h"
#include "CKSessionsSubsystem.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "HAL/PlatformTime.h"
#include "Misc/PackageName.h"
#include "UObject/SoftObjectPath.h"

DEFINE_LOG_CATEGORY_STATIC(LogCKSampleMigrationSessions, Log, All);

namespace CKSample::MigrationSessionsBridge
{
	/** Session attribute with the migration's session type, as the interface documents it (ICKHostMigrationSessionDriver). */
	const FName SessionTypeAttribute(TEXT("CKHM_TYPE"));

	/**
	 * Names of the attributes the migration advertises (type, group, epoch, rank, host), taken from the migration's own
	 * function instead of repeating the strings.
	 */
	const TArray<FName>& GetGroupAttributeKeys()
	{
		static const TArray<FName> Keys = []()
		{
			const TMap<FName, FString> Probe = CKHostMigration::Protocol::MakeSessionAttributes(FName(TEXT("ProbeType")), TEXT("ProbeGroup"), 7, 8, TEXT("ProbeHost"));
			TArray<FName> Found;
			Probe.GenerateKeyArray(Found);
			return Found;
		}();
		return Keys;
	}

	/** A map soft reference from a long package name; empty for no map (the current map). */
	TSoftObjectPtr<UWorld> MakeMapReference(const FString& MapPackageName)
	{
		if (MapPackageName.IsEmpty())
		{
			return TSoftObjectPtr<UWorld>();
		}
		return TSoftObjectPtr<UWorld>(FSoftObjectPath(FString::Printf(TEXT("%s.%s"), *MapPackageName, *FPackageName::GetShortName(MapPackageName))));
	}

	/** Seconds a leave waits when the world does not change (a player who was not travelling anywhere). */
	constexpr double LeaveSettleSeconds = 3.0;
}

void UCKSampleHostMigrationSessionDriver::BeginDestroy()
{
	Unbind();
	FTSTicker::GetCoreTicker().RemoveTicker(TickerHandle);
	TickerHandle.Reset();
	Super::BeginDestroy();
}

UCKSessionsSubsystem* UCKSampleHostMigrationSessionDriver::Begin(UGameInstance& GameInstance, const EOperation NewOperation)
{
	UCKSessionsSubsystem* Sessions = GameInstance.GetSubsystem<UCKSessionsSubsystem>();
	if (Sessions == nullptr)
	{
		return nullptr;
	}
	Operation = NewOperation;
	ActiveInstance = &GameInstance;
	StartWorld = GameInstance.GetWorld();
	StartedSeconds = FPlatformTime::Seconds();
	BoundSessions = Sessions;
	TickerHandle = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateUObject(this, &UCKSampleHostMigrationSessionDriver::Tick));
	return Sessions;
}

void UCKSampleHostMigrationSessionDriver::HostSession(UGameInstance& GameInstance, const FCKHostMigrationHostParams& Params, FCKHostMigrationDriverDone Done)
{
	using namespace CKSample::MigrationSessionsBridge;

	if (Operation != EOperation::None)
	{
		Finish(false);
	}
	PendingDone = MoveTemp(Done);
	UCKSessionsSubsystem* Sessions = Begin(GameInstance, EOperation::Host);
	if (Sessions == nullptr)
	{
		Finish(false);
		return;
	}
	// The session plugin gives the session its own profile's slots: Params.MaxPlayers has no counterpart in its API.
	Sessions->OnHostComplete.AddDynamic(this, &UCKSampleHostMigrationSessionDriver::HandleHostComplete);
	if (!Sessions->HostSessionWithAttributes(nullptr, MakeMapReference(Params.MapPackageName), Params.Attributes) && Operation == EOperation::Host)
	{
		Finish(false);
	}
}

void UCKSampleHostMigrationSessionDriver::FindSessions(UGameInstance& GameInstance, const FCKHostMigrationSearchParams& Params, FCKHostMigrationSearchDone Done)
{
	using namespace CKSample::MigrationSessionsBridge;

	if (Operation != EOperation::None)
	{
		Finish(false);
	}
	PendingSearch = MoveTemp(Done);
	SearchType = Params.SessionType;
	UCKSessionsSubsystem* Sessions = Begin(GameInstance, EOperation::Find);
	if (Sessions == nullptr)
	{
		Finish(false);
		return;
	}
	Sessions->OnBackgroundFindComplete.AddDynamic(this, &UCKSampleHostMigrationSessionDriver::HandleFindComplete);
	// A background search works in any state: a host looks for a second host of its group (the migration's conflict watch)
	// with the same call a player uses to find the new host after a loss.
	TMap<FName, FString> Filter;
	Filter.Add(SessionTypeAttribute, Params.SessionType.ToString());
	if (!Sessions->FindSessionsInBackground(nullptr, Filter) && Operation == EOperation::Find)
	{
		Finish(false);
	}
}

void UCKSampleHostMigrationSessionDriver::JoinSession(UGameInstance& GameInstance, const FCKHostMigrationSessionInfo& Session, FCKHostMigrationDriverDone Done)
{
	if (Operation != EOperation::None)
	{
		Finish(false);
	}
	PendingDone = MoveTemp(Done);
	const bool bKnown = LastResults.IsValidIndex(Session.DriverIndex) && LastResults[Session.DriverIndex].GetSessionId() == Session.SessionId;
	UCKSessionsSubsystem* Sessions = bKnown ? Begin(GameInstance, EOperation::Join) : nullptr;
	if (Sessions == nullptr)
	{
		Finish(false);
		return;
	}
	Sessions->OnJoinComplete.AddDynamic(this, &UCKSampleHostMigrationSessionDriver::HandleJoinComplete);
	if (!Sessions->JoinSession(LastResults[Session.DriverIndex]) && Operation == EOperation::Join)
	{
		Finish(false);
	}
}

void UCKSampleHostMigrationSessionDriver::LeaveSession(UGameInstance& GameInstance, const FString& MapPackageName, FCKHostMigrationDriverDone Done)
{
	using namespace CKSample::MigrationSessionsBridge;

	if (Operation != EOperation::None)
	{
		Finish(false);
	}
	PendingDone = MoveTemp(Done);
	UCKSessionsSubsystem* Sessions = Begin(GameInstance, EOperation::Leave);
	if (Sessions == nullptr)
	{
		Finish(false);
		return;
	}
	// An empty map opens CoopKit Sessions' menu map (Project Settings > CoopKit Sessions), the sample's main menu.
	Sessions->LeaveSessionToMap(MakeMapReference(MapPackageName));
}

void UCKSampleHostMigrationSessionDriver::PrepareAfterHostLoss(UGameInstance& GameInstance, FCKHostMigrationDriverDone Done)
{
	if (Operation != EOperation::None)
	{
		Finish(false);
	}
	PendingDone = MoveTemp(Done);
	// The session plugin hands the lost host over (On Host Lost) once the engine is on its default map and is idle after
	// that: only then Host Session and Find Sessions work. The tick waits for it.
	if (Begin(GameInstance, EOperation::Prepare) == nullptr)
	{
		Finish(false);
	}
}

void UCKSampleHostMigrationSessionDriver::HandleHostComplete(const bool bSuccess, const ECKSessionsFailure Failure)
{
	if (Operation == EOperation::Host)
	{
		if (!bSuccess)
		{
			UE_LOG(LogCKSampleMigrationSessions, Warning, TEXT("CoopKit Sessions could not host the group's session: %s."), *UEnum::GetValueAsString(Failure));
		}
		Finish(bSuccess);
	}
}

void UCKSampleHostMigrationSessionDriver::HandleFindComplete(const bool bSuccess, const ECKSessionsFailure Failure, const TArray<FCKSessionsSearchResult>& Results)
{
	if (Operation != EOperation::Find)
	{
		return;
	}
	LastResults = Results;
	TArray<FCKHostMigrationSessionInfo> Found;
	if (bSuccess)
	{
		for (int32 Index = 0; Index < Results.Num(); ++Index)
		{
			const FCKSessionsSearchResult& Result = Results[Index];
			if (!UCKSessionsLibrary::IsValidResult(Result))
			{
				continue;
			}
			TMap<FName, FString> Attributes;
			for (const FName& Key : CKSample::MigrationSessionsBridge::GetGroupAttributeKeys())
			{
				FString Value;
				if (UCKSessionsLibrary::GetSessionAttribute(Result, Key, Value))
				{
					Attributes.Add(Key, Value);
				}
			}
			FCKHostMigrationSessionInfo Info;
			if (!CKHostMigration::Protocol::ReadSessionAttributes(Attributes, SearchType, Info))
			{
				continue;
			}
			int32 Players = 0;
			int32 MaxPlayers = 0;
			UCKSessionsLibrary::GetPlayerCounts(Result, Players, MaxPlayers);
			Info.SessionId = Result.GetSessionId();
			Info.PingMs = UCKSessionsLibrary::GetPingMs(Result);
			Info.OpenSlots = FMath::Max(0, MaxPlayers - Players);
			Info.DriverIndex = Index;
			Found.Add(MoveTemp(Info));
		}
	}
	UE_LOG(LogCKSampleMigrationSessions, Verbose, TEXT("Search %s (%s): %d sessions, %d of type %s."), bSuccess ? TEXT("done") : TEXT("failed"),
		*UEnum::GetValueAsString(Failure), Results.Num(), Found.Num(), *SearchType.ToString());

	Unbind();
	FTSTicker::GetCoreTicker().RemoveTicker(TickerHandle);
	TickerHandle.Reset();
	Operation = EOperation::None;
	FCKHostMigrationSearchDone Done = MoveTemp(PendingSearch);
	PendingSearch.Unbind();
	Done.ExecuteIfBound(bSuccess, Found);
}

void UCKSampleHostMigrationSessionDriver::HandleJoinComplete(const bool bSuccess, const ECKSessionsFailure Failure)
{
	if (Operation != EOperation::Join)
	{
		return;
	}
	if (bSuccess)
	{
		// Connected to the host: the migration wants the player ready, so wait for the player state on the new map.
		Operation = EOperation::JoinPlayer;
		return;
	}
	UE_LOG(LogCKSampleMigrationSessions, Warning, TEXT("CoopKit Sessions could not join the group's session: %s."), *UEnum::GetValueAsString(Failure));
	Finish(false);
}

bool UCKSampleHostMigrationSessionDriver::Tick(const float DeltaSeconds)
{
	if (Operation == EOperation::None)
	{
		TickerHandle.Reset();
		return false;
	}
	const double Elapsed = FPlatformTime::Seconds() - StartedSeconds;
	const UGameInstance* GameInstance = ActiveInstance.Get();
	const UWorld* World = GameInstance != nullptr ? GameInstance->GetWorld() : nullptr;
	const UCKSessionsSubsystem* Sessions = BoundSessions.Get();
	if (GameInstance == nullptr || Sessions == nullptr)
	{
		Finish(false);
		return false;
	}

	switch (Operation)
	{
	case EOperation::JoinPlayer:
		{
			// A client world other than the one the join started in, with the local player's player state replicated.
			const APlayerController* Controller = World != nullptr ? World->GetFirstPlayerController() : nullptr;
			if (World != nullptr && World != StartWorld.Get() && World->GetNetMode() == NM_Client && Controller != nullptr && Controller->PlayerState != nullptr)
			{
				Finish(true);
				return false;
			}
			break;
		}
	case EOperation::Leave:
		if (Sessions->GetState() == ECKSessionsState::Idle && IsSettledStandalone()
			&& (World != StartWorld.Get() || Elapsed >= CKSample::MigrationSessionsBridge::LeaveSettleSeconds))
		{
			Finish(true);
			return false;
		}
		break;
	case EOperation::Prepare:
		if (Sessions->GetState() == ECKSessionsState::Idle && IsSettledStandalone())
		{
			Finish(true);
			return false;
		}
		break;
	default:
		break;
	}

	if (Elapsed > OperationTimeoutSeconds)
	{
		UE_LOG(LogCKSampleMigrationSessions, Warning, TEXT("A session operation (%d) did not end in %.0f s; CoopKit Sessions is %s."),
			static_cast<int32>(Operation), OperationTimeoutSeconds, *UEnum::GetValueAsString(Sessions->GetState()));
		Finish(false);
		return false;
	}
	return true;
}

void UCKSampleHostMigrationSessionDriver::Finish(const bool bSuccess)
{
	Unbind();
	FTSTicker::GetCoreTicker().RemoveTicker(TickerHandle);
	TickerHandle.Reset();
	Operation = EOperation::None;
	// At most one callback is set: a search answers with its list, everything else with success.
	FCKHostMigrationDriverDone Done = MoveTemp(PendingDone);
	PendingDone.Unbind();
	FCKHostMigrationSearchDone Search = MoveTemp(PendingSearch);
	PendingSearch.Unbind();
	Done.ExecuteIfBound(bSuccess);
	Search.ExecuteIfBound(bSuccess, TArray<FCKHostMigrationSessionInfo>());
}

void UCKSampleHostMigrationSessionDriver::Unbind()
{
	if (UCKSessionsSubsystem* Sessions = BoundSessions.Get())
	{
		Sessions->OnHostComplete.RemoveAll(this);
		Sessions->OnBackgroundFindComplete.RemoveAll(this);
		Sessions->OnJoinComplete.RemoveAll(this);
	}
	BoundSessions.Reset();
}

bool UCKSampleHostMigrationSessionDriver::IsSettledStandalone() const
{
	const UGameInstance* GameInstance = ActiveInstance.Get();
	const UWorld* World = GameInstance != nullptr ? GameInstance->GetWorld() : nullptr;
	const FWorldContext* Context = World != nullptr && GEngine != nullptr ? GEngine->GetWorldContextFromWorld(World) : nullptr;
	return Context != nullptr && World->GetNetMode() == NM_Standalone && World->HasBegunPlay() && Context->TravelURL.IsEmpty() && Context->PendingNetGame == nullptr;
}
