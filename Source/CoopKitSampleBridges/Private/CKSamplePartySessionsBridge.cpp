// Copyright 2026 Ivan Iaitckikh. All Rights Reserved.

#include "CKSamplePartySessionsBridge.h"
#include "CKPartyProtocol.h"
#include "CKPartySubsystem.h"
#include "CKSessionsLibrary.h"
#include "CKSessionsOnlineSettings.h"
#include "CKSessionsSubsystem.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "HAL/PlatformTime.h"
#include "Misc/PackageName.h"
#include "OnlineSubsystem.h"
#include "OnlineSubsystemUtils.h"
#include "UObject/SoftObjectPath.h"

DEFINE_LOG_CATEGORY_STATIC(LogCKSamplePartySessions, Log, All);

namespace CKSample::PartySessionsBridge
{
	/** A map soft reference from a long package name; empty for no map (the session plugin's default). */
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

	/** Seconds between two rounds of a member looking for its leader's game. */
	constexpr double FollowRetrySeconds = 1.0;

	/** Seconds a member's own travel to a game address may take before it looks again. */
	constexpr double RawTravelSeconds = 20.0;

	/** The first local player of the Online Subsystem calls. */
	constexpr int32 LocalUserNum = 0;

	/** The online subsystem that has no service: its sessions are only visible on the LAN. */
	const FName NullSubsystemName(TEXT("NULL"));

	/** What the party panel's Play button hands the party. */
	void Play(const TArray<FString>& Args, UWorld* World)
	{
		const UGameInstance* GameInstance = World != nullptr ? World->GetGameInstance() : nullptr;
		UCKSamplePartySessionsBridge* Bridge = GameInstance != nullptr ? GameInstance->GetSubsystem<UCKSamplePartySessionsBridge>() : nullptr;
		if (Bridge == nullptr)
		{
			UE_LOG(LogCKSamplePartySessions, Warning, TEXT("ck.sample.PartyPlay: run it in a game (PIE or standalone)."));
			return;
		}
		Bridge->PlayWithParty();
	}

	FAutoConsoleCommandWithWorldAndArgs PlayCommand(TEXT("ck.sample.PartyPlay"),
		TEXT("Party leader: takes the whole party into the sample's lobby through the party's game driver."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&Play));
}

// ---------- the driver ----------

void UCKSamplePartySessionsDriver::BeginDestroy()
{
	Unbind();
	FTSTicker::GetCoreTicker().RemoveTicker(TickerHandle);
	TickerHandle.Reset();
	Super::BeginDestroy();
}

UCKSessionsSubsystem* UCKSamplePartySessionsDriver::Begin(UGameInstance& GameInstance, const EOperation NewOperation)
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
	TickerHandle = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateUObject(this, &UCKSamplePartySessionsDriver::Tick));
	return Sessions;
}

void UCKSamplePartySessionsDriver::HostGame(UGameInstance& GameInstance, const FCKPartyGameHostParams& Params, FCKPartyGameDone Done)
{
	using namespace CKSample::PartySessionsBridge;

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
	// The session plugin gives the session its own profile's slots and its own travel options: Params.MaxPlayers and
	// Params.Options have no counterpart in its API. The party has already checked its places against MaxPlayers.
	Sessions->OnHostComplete.AddDynamic(this, &UCKSamplePartySessionsDriver::HandleHostComplete);
	if (!Sessions->HostSessionWithAttributes(nullptr, MakeMapReference(Params.MapPackageName), Params.Attributes) && Operation == EOperation::Host)
	{
		Finish(false);
	}
}

void UCKSamplePartySessionsDriver::FindGames(UGameInstance& GameInstance, const FCKPartyGameSearchParams& Params, FCKPartyGameSearchDone Done)
{
	if (Operation != EOperation::None)
	{
		Finish(false);
	}
	PendingSearch = MoveTemp(Done);
	UCKSessionsSubsystem* Sessions = Begin(GameInstance, EOperation::Find);
	if (Sessions == nullptr)
	{
		Finish(false);
		return;
	}
	Sessions->OnFindComplete.AddDynamic(this, &UCKSamplePartySessionsDriver::HandleFindComplete);
	if (!Sessions->FindSessionsWithAttributes(nullptr, Params.RequiredAttributes) && Operation == EOperation::Find)
	{
		Finish(false);
	}
}

void UCKSamplePartySessionsDriver::PrepareJoin(UGameInstance& GameInstance, const FCKPartyGameInfo& Game, FCKPartyGamePrepareDone Done)
{
	const bool bKnown = LastResults.IsValidIndex(Game.DriverIndex) && LastResults[Game.DriverIndex].GetSessionId() == Game.SessionId;
	if (!bKnown)
	{
		Done.ExecuteIfBound(false, FCKPartyGameTicket());
		return;
	}
	// The session plugin joins and travels in one call, so the leader only keeps the result now and joins in Travel To Game.
	ChosenGame = LastResults[Game.DriverIndex];
	FCKPartyGameTicket Where;
	Where.SessionId = Game.SessionId;
	Where.Attributes = Game.Attributes;
	FString ConnectString;
	const IOnlineSessionPtr Sessions = Online::GetSessionInterface(GameInstance.GetWorld());
	if (Sessions.IsValid() && Sessions->GetResolvedConnectString(ChosenGame.GetNative(), NAME_GamePort, ConnectString)
		&& CKParty::Protocol::IsUsableConnectString(ConnectString))
	{
		Where.ConnectString = ConnectString;
	}
	Done.ExecuteIfBound(true, Where);
}

void UCKSamplePartySessionsDriver::TravelToGame(UGameInstance& GameInstance, FCKPartyGameDone Done)
{
	if (Operation != EOperation::None)
	{
		Finish(false);
	}
	PendingDone = MoveTemp(Done);
	UCKSessionsSubsystem* Sessions = ChosenGame.IsValid() ? Begin(GameInstance, EOperation::Travel) : nullptr;
	if (Sessions == nullptr)
	{
		Finish(false);
		return;
	}
	Sessions->OnJoinComplete.AddDynamic(this, &UCKSamplePartySessionsDriver::HandleJoinComplete);
	if (!Sessions->JoinSession(ChosenGame) && Operation == EOperation::Travel)
	{
		Finish(false);
	}
}

void UCKSamplePartySessionsDriver::FollowLeader(UGameInstance& GameInstance, const FCKPartyGameTicket& Ticket, const float TimeoutSeconds, FCKPartyGameDone Done)
{
	if (Operation != EOperation::None)
	{
		Finish(false);
	}
	PendingDone = MoveTemp(Done);
	UCKSessionsSubsystem* Sessions = Begin(GameInstance, EOperation::Follow);
	if (Sessions == nullptr)
	{
		Finish(false);
		return;
	}
	FollowTicket = Ticket;
	FollowStep = EFollowStep::Waiting;
	FollowRounds = 0;
	NextRoundSeconds = 0.0;
	FollowDeadlineSeconds = FPlatformTime::Seconds() + FMath::Max(1.0f, TimeoutSeconds);
	Sessions->OnFindComplete.AddDynamic(this, &UCKSamplePartySessionsDriver::HandleFindComplete);
	Sessions->OnJoinComplete.AddDynamic(this, &UCKSamplePartySessionsDriver::HandleJoinComplete);
}

void UCKSamplePartySessionsDriver::LeaveGame(UGameInstance& GameInstance, const FString& MapPackageName, FCKPartyGameDone Done)
{
	using namespace CKSample::PartySessionsBridge;

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

// ---------- events of the session plugin ----------

void UCKSamplePartySessionsDriver::HandleHostComplete(const bool bSuccess, const ECKSessionsFailure Failure)
{
	if (Operation != EOperation::Host)
	{
		return;
	}
	if (!bSuccess)
	{
		UE_LOG(LogCKSamplePartySessions, Warning, TEXT("CoopKit Sessions could not host the party's game: %s."), *UEnum::GetValueAsString(Failure));
	}
	Finish(bSuccess);
}

void UCKSamplePartySessionsDriver::HandleFindComplete(const bool bSuccess, const TArray<FCKSessionsSearchResult>& Results)
{
	if (Operation == EOperation::Find)
	{
		LastResults = Results;
		TArray<FCKPartyGameInfo> Found;
		for (int32 Index = 0; bSuccess && Index < Results.Num(); ++Index)
		{
			const FCKSessionsSearchResult& Result = Results[Index];
			if (!UCKSessionsLibrary::IsValidResult(Result))
			{
				continue;
			}
			int32 Players = 0;
			int32 MaxPlayers = 0;
			UCKSessionsLibrary::GetPlayerCounts(Result, Players, MaxPlayers);
			FCKPartyGameInfo Info;
			Info.SessionId = Result.GetSessionId();
			Info.OwnerName = UCKSessionsLibrary::GetHostName(Result);
			Info.MaxPlayers = MaxPlayers;
			Info.OpenSlots = FMath::Max(0, MaxPlayers - Players);
			Info.PingMs = UCKSessionsLibrary::GetPingMs(Result);
			Info.Attributes = CKParty::Protocol::ToAttributeArray(CKSessions::Online::ReadAttributes(Result.GetNative().Session.SessionSettings));
			Info.DriverIndex = Index;
			Found.Add(MoveTemp(Info));
		}
		Unbind();
		FTSTicker::GetCoreTicker().RemoveTicker(TickerHandle);
		TickerHandle.Reset();
		Operation = EOperation::None;
		FCKPartyGameSearchDone Done = MoveTemp(PendingSearch);
		PendingSearch.Unbind();
		Done.ExecuteIfBound(bSuccess, Found);
		return;
	}
	if (Operation == EOperation::Follow && FollowStep == EFollowStep::Searching)
	{
		if (bSuccess)
		{
			FollowWithResults(Results);
		}
		else
		{
			RetryFollowLater();
		}
	}
}

void UCKSamplePartySessionsDriver::HandleJoinComplete(const bool bSuccess, const ECKSessionsFailure Failure)
{
	if (Operation == EOperation::Travel)
	{
		if (!bSuccess)
		{
			UE_LOG(LogCKSamplePartySessions, Warning, TEXT("CoopKit Sessions could not join the game the party's leader chose: %s."), *UEnum::GetValueAsString(Failure));
			Finish(false);
		}
		// Connected: the tick waits for the local player's player state.
		return;
	}
	if (Operation == EOperation::Follow && FollowStep == EFollowStep::Joining && !bSuccess)
	{
		UE_LOG(LogCKSamplePartySessions, Log, TEXT("CoopKit Sessions could not join the leader's game (%s); looking again."), *UEnum::GetValueAsString(Failure));
		RetryFollowLater();
	}
}

// ---------- following the leader ----------

void UCKSamplePartySessionsDriver::StartFollowRound()
{
	UCKSessionsSubsystem* Sessions = BoundSessions.Get();
	if (Sessions == nullptr)
	{
		Finish(false);
		return;
	}
	++FollowRounds;
	// Leader Hosts: the game session advertises the party's id. Leader Joins: the attributes the leader's game was found by.
	TMap<FName, FString> Filter;
	if (FollowTicket.Route == ECKPartyGameRoute::LeaderHosts)
	{
		Filter.Add(CKParty::Protocol::GameAttribute, FollowTicket.PartyId);
	}
	else
	{
		for (const FCKPartyAttribute& Attribute : FollowTicket.Attributes)
		{
			Filter.Add(Attribute.Key, Attribute.Value);
		}
	}
	FollowStep = EFollowStep::Searching;
	if (!Sessions->FindSessionsWithAttributes(nullptr, Filter) && FollowStep == EFollowStep::Searching)
	{
		RetryFollowLater();
	}
}

void UCKSamplePartySessionsDriver::FollowWithResults(const TArray<FCKSessionsSearchResult>& Results)
{
	UCKSessionsSubsystem* Sessions = BoundSessions.Get();
	TArray<FCKPartyGameInfo> Games;
	for (int32 Index = 0; Index < Results.Num(); ++Index)
	{
		int32 Players = 0;
		int32 MaxPlayers = 0;
		UCKSessionsLibrary::GetPlayerCounts(Results[Index], Players, MaxPlayers);
		FCKPartyGameInfo Info;
		Info.SessionId = Results[Index].GetSessionId();
		Info.MaxPlayers = MaxPlayers;
		Info.OpenSlots = FMath::Max(0, MaxPlayers - Players);
		Info.Attributes = CKParty::Protocol::ToAttributeArray(CKSessions::Online::ReadAttributes(Results[Index].GetNative().Session.SessionSettings));
		Info.DriverIndex = Index;
		Games.Add(MoveTemp(Info));
	}
	const int32 Pick = CKParty::Protocol::PickGameForTicket(Games, FollowTicket);
	if (Pick != INDEX_NONE && Sessions != nullptr)
	{
		UE_LOG(LogCKSamplePartySessions, Log, TEXT("%s: joining the leader's game session %s."), *GetNameSafe(ActiveInstance.Get()), *Games[Pick].SessionId);
		FollowStep = EFollowStep::Joining;
		if (!Sessions->JoinSession(Results[Games[Pick].DriverIndex]) && FollowStep == EFollowStep::Joining)
		{
			RetryFollowLater();
		}
		return;
	}
	// A joined game can also be reached by the address the leader travelled to (the ticket), after two searches.
	if (FollowTicket.Route == ECKPartyGameRoute::LeaderJoins && FollowRounds >= 2 && CKParty::Protocol::IsUsableConnectString(FollowTicket.ConnectString))
	{
		TravelToAddress(FollowTicket.ConnectString);
		return;
	}
	if (FollowTicket.Route == ECKPartyGameRoute::LeaderHosts)
	{
		StartRawSearch();
		return;
	}
	RetryFollowLater();
}

void UCKSamplePartySessionsDriver::StartRawSearch()
{
	using namespace CKSample::PartySessionsBridge;

	UGameInstance* GameInstance = ActiveInstance.Get();
	const UWorld* World = GameInstance != nullptr ? GameInstance->GetWorld() : nullptr;
	const IOnlineSubsystem* Online = World != nullptr ? Online::GetSubsystem(World) : nullptr;
	const IOnlineSessionPtr Sessions = Online != nullptr ? Online->GetSessionInterface() : nullptr;
	if (!Sessions.IsValid())
	{
		RetryFollowLater();
		return;
	}
	RawSearch = MakeShared<FOnlineSessionSearch>();
	RawSearch->bIsLanQuery = Online->GetSubsystemName() == NullSubsystemName;
	RawSearch->MaxSearchResults = 50;
	RawFindHandle = Sessions->AddOnFindSessionsCompleteDelegate_Handle(FOnFindSessionsCompleteDelegate::CreateUObject(this, &UCKSamplePartySessionsDriver::HandleRawFindComplete));
	FollowStep = EFollowStep::RawSearching;
	if (!Sessions->FindSessions(LocalUserNum, RawSearch.ToSharedRef()) && FollowStep == EFollowStep::RawSearching)
	{
		Sessions->ClearOnFindSessionsCompleteDelegate_Handle(RawFindHandle);
		RawFindHandle.Reset();
		RetryFollowLater();
	}
}

void UCKSamplePartySessionsDriver::HandleRawFindComplete(const bool bSuccess)
{
	if (FollowStep != EFollowStep::RawSearching || !RawSearch.IsValid() || RawSearch->SearchState == EOnlineAsyncTaskState::InProgress)
	{
		return;
	}
	const UGameInstance* GameInstance = ActiveInstance.Get();
	const UWorld* World = GameInstance != nullptr ? GameInstance->GetWorld() : nullptr;
	if (const IOnlineSessionPtr Sessions = World != nullptr ? Online::GetSessionInterface(World) : nullptr)
	{
		Sessions->ClearOnFindSessionsCompleteDelegate_Handle(RawFindHandle);
	}
	RawFindHandle.Reset();

	// The leader's party session names the address of the game it hosts (the party writes it there for exactly this case).
	FString Address;
	for (int32 Index = 0; bSuccess && Address.IsEmpty() && Index < RawSearch->SearchResults.Num(); ++Index)
	{
		const FOnlineSessionSearchResult& Result = RawSearch->SearchResults[Index];
		if (Result.IsValid())
		{
			Address = CKParty::Protocol::ReadHostedGameAddress(CKSessions::Online::ReadAttributes(Result.Session.SessionSettings), FollowTicket.PartyId);
		}
	}
	RawSearch.Reset();
	if (Address.IsEmpty())
	{
		RetryFollowLater();
		return;
	}
	UE_LOG(LogCKSamplePartySessions, Log, TEXT("%s: the leader's party session names its game at %s."), *GetNameSafe(ActiveInstance.Get()), *Address);
	TravelToAddress(Address);
}

void UCKSamplePartySessionsDriver::TravelToAddress(const FString& Address)
{
	const UGameInstance* GameInstance = ActiveInstance.Get();
	APlayerController* Controller = GameInstance != nullptr ? GameInstance->GetFirstLocalPlayerController(GameInstance->GetWorld()) : nullptr;
	if (Controller == nullptr)
	{
		RetryFollowLater();
		return;
	}
	FollowStep = EFollowStep::RawTravelling;
	RawTravelStartedSeconds = FPlatformTime::Seconds();
	Controller->ClientTravel(Address, TRAVEL_Absolute);
}

void UCKSamplePartySessionsDriver::RetryFollowLater()
{
	FollowStep = EFollowStep::Waiting;
	NextRoundSeconds = FPlatformTime::Seconds() + CKSample::PartySessionsBridge::FollowRetrySeconds;
}

// ---------- ticking ----------

bool UCKSamplePartySessionsDriver::Tick(const float DeltaSeconds)
{
	using namespace CKSample::PartySessionsBridge;

	if (Operation == EOperation::None)
	{
		TickerHandle.Reset();
		return false;
	}
	const double Now = FPlatformTime::Seconds();
	const double Elapsed = Now - StartedSeconds;
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
	case EOperation::Travel:
		if (IsInTheGame())
		{
			Finish(true);
			return false;
		}
		break;
	case EOperation::Follow:
		if (IsInTheGame())
		{
			Finish(true);
			return false;
		}
		if (Now > FollowDeadlineSeconds)
		{
			UE_LOG(LogCKSamplePartySessions, Warning, TEXT("%s: the leader's game was not found in time (%d rounds)."), *GetNameSafe(GameInstance), FollowRounds);
			Finish(false);
			return false;
		}
		if (FollowStep == EFollowStep::Waiting && Now >= NextRoundSeconds)
		{
			StartFollowRound();
		}
		else if (FollowStep == EFollowStep::RawTravelling && Now - RawTravelStartedSeconds > RawTravelSeconds)
		{
			RetryFollowLater();
		}
		return Operation == EOperation::Follow;
	case EOperation::Leave:
		if (Sessions->GetState() == ECKSessionsState::Idle && IsSettledStandalone() && (World != StartWorld.Get() || Elapsed >= LeaveSettleSeconds))
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
		UE_LOG(LogCKSamplePartySessions, Warning, TEXT("A session operation (%d) did not end in %.0f s; CoopKit Sessions is %s."),
			static_cast<int32>(Operation), OperationTimeoutSeconds, *UEnum::GetValueAsString(Sessions->GetState()));
		Finish(false);
		return false;
	}
	return true;
}

bool UCKSamplePartySessionsDriver::IsInTheGame() const
{
	const UGameInstance* GameInstance = ActiveInstance.Get();
	const UWorld* World = GameInstance != nullptr ? GameInstance->GetWorld() : nullptr;
	const APlayerController* Controller = World != nullptr ? World->GetFirstPlayerController() : nullptr;
	return World != nullptr && World != StartWorld.Get() && World->GetNetMode() == NM_Client && Controller != nullptr && Controller->PlayerState != nullptr;
}

bool UCKSamplePartySessionsDriver::IsSettledStandalone() const
{
	const UGameInstance* GameInstance = ActiveInstance.Get();
	const UWorld* World = GameInstance != nullptr ? GameInstance->GetWorld() : nullptr;
	const FWorldContext* Context = World != nullptr && GEngine != nullptr ? GEngine->GetWorldContextFromWorld(World) : nullptr;
	return Context != nullptr && World->GetNetMode() == NM_Standalone && World->HasBegunPlay() && Context->TravelURL.IsEmpty() && Context->PendingNetGame == nullptr;
}

void UCKSamplePartySessionsDriver::Finish(const bool bSuccess)
{
	Unbind();
	FTSTicker::GetCoreTicker().RemoveTicker(TickerHandle);
	TickerHandle.Reset();
	Operation = EOperation::None;
	FollowStep = EFollowStep::Waiting;
	// At most one callback is set: a search answers with its list, everything else with success.
	FCKPartyGameDone Done = MoveTemp(PendingDone);
	PendingDone.Unbind();
	FCKPartyGameSearchDone Search = MoveTemp(PendingSearch);
	PendingSearch.Unbind();
	Done.ExecuteIfBound(bSuccess);
	Search.ExecuteIfBound(bSuccess, TArray<FCKPartyGameInfo>());
}

void UCKSamplePartySessionsDriver::Unbind()
{
	if (UCKSessionsSubsystem* Sessions = BoundSessions.Get())
	{
		Sessions->OnHostComplete.RemoveAll(this);
		Sessions->OnFindComplete.RemoveAll(this);
		Sessions->OnJoinComplete.RemoveAll(this);
	}
	BoundSessions.Reset();
	if (RawFindHandle.IsValid())
	{
		const UGameInstance* GameInstance = ActiveInstance.Get();
		const UWorld* World = GameInstance != nullptr ? GameInstance->GetWorld() : nullptr;
		if (const IOnlineSessionPtr Sessions = World != nullptr ? Online::GetSessionInterface(World) : nullptr)
		{
			Sessions->ClearOnFindSessionsCompleteDelegate_Handle(RawFindHandle);
		}
		RawFindHandle.Reset();
	}
	RawSearch.Reset();
}

// ---------- the subsystem ----------

void UCKSamplePartySessionsBridge::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	Party = Collection.InitializeDependency<UCKPartySubsystem>();
	// The driver talks to the session subsystem: it must exist before the first party game.
	Collection.InitializeDependency<UCKSessionsSubsystem>();
	if (Party != nullptr && bUseSessionsDriver)
	{
		Driver = NewObject<UCKSamplePartySessionsDriver>(this);
		Party->SetGameDriver(Driver);
	}
}

void UCKSamplePartySessionsBridge::Deinitialize()
{
	if (Party != nullptr && Driver != nullptr)
	{
		// Back to the profile's driver before the object goes away.
		Party->SetGameDriver(nullptr);
	}
	Driver = nullptr;
	Super::Deinitialize();
}

bool UCKSamplePartySessionsBridge::PlayWithParty()
{
	if (Party == nullptr || !Party->IsLeader())
	{
		return false;
	}
	FCKPartyGameHostParams Params;
	Params.MapPackageName = HostMap.IsNull() ? FString() : HostMap.GetLongPackageName();
	Params.MaxPlayers = FMath::Max(GameMaxPlayers, Party->GetRoster().Members.Num());
	UE_LOG(LogCKSamplePartySessions, Log, TEXT("%s: the party leader takes %d members into %s."), *GetNameSafe(GetGameInstance()),
		Party->GetRoster().Members.Num(), Params.MapPackageName.IsEmpty() ? TEXT("the current map") : *Params.MapPackageName);
	return Party->HostGame(Params);
}
