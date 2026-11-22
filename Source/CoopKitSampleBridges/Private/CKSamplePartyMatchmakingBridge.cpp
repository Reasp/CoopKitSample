// Copyright 2026 Ivan Iaitckikh. All Rights Reserved.

#include "CKSamplePartyMatchmakingBridge.h"
#include "CKMatchmakingProvider.h"
#include "CKMatchmakingQueueData.h"
#include "CKMatchmakingSubsystem.h"
#include "CKPartySubsystem.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"

DEFINE_LOG_CATEGORY_STATIC(LogCKSamplePartyMatchmaking, Log, All);

namespace CKSample::PartyMatchmakingBridge
{
	/** Party attributes that carry the leader's match. The match id is written last and read first. */
	const FName MatchIdAttribute(TEXT("CKSAMPLE_MM_ID"));
	const FName HostAttribute(TEXT("CKSAMPLE_MM_HOST"));
	const FName PlayerCountAttribute(TEXT("CKSAMPLE_MM_N"));

	/**
	 * Member-call providers: the leader's request for member ids (party attribute "<nonce>|"), a member's answer (member attribute
	 * "<nonce>|<group member id>"), and the leader's group ticket id (party attribute "<nonce>|<ticket id>").
	 */
	const FName MemberIdRequestAttribute(TEXT("CKSAMPLE_MM_REQ"));
	const FName MemberIdAttribute(TEXT("CKSAMPLE_MM_MID"));
	const FName TicketAttribute(TEXT("CKSAMPLE_MM_TICKET"));

	/** Splits "<nonce>|<value>"; false when there is no separator. */
	bool SplitNonce(const FString& Text, FString& OutNonce, FString& OutValue)
	{
		return Text.Split(TEXT("|"), &OutNonce, &OutValue);
	}

	/** The attribute of the player with this index: "<player id>|<ticket id>" (a value is kept to 256 characters). */
	FName MakePlayerAttribute(const int32 Index)
	{
		return FName(*FString::Printf(TEXT("CKSAMPLE_MM_P%d"), Index));
	}

	void FindMatchCommand(const TArray<FString>& Args, UWorld* World)
	{
		const UGameInstance* GameInstance = World != nullptr ? World->GetGameInstance() : nullptr;
		UCKSamplePartyMatchmakingBridge* Bridge = GameInstance != nullptr ? GameInstance->GetSubsystem<UCKSamplePartyMatchmakingBridge>() : nullptr;
		if (Bridge == nullptr)
		{
			UE_LOG(LogCKSamplePartyMatchmaking, Warning, TEXT("ck.sample.PartyFindMatch: run it in a game (PIE or standalone)."));
			return;
		}
		Bridge->FindMatch();
	}

	FAutoConsoleCommandWithWorldAndArgs PartyFindMatchCommand(TEXT("ck.sample.PartyFindMatch"),
		TEXT("Starts matchmaking in the sample's queue: the party leader sends a group ticket for the whole party, a player with no party a single ticket."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&FindMatchCommand));
}

void UCKSamplePartyMatchmakingBridge::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	Matchmaking = Collection.InitializeDependency<UCKMatchmakingSubsystem>();
	Party = Collection.InitializeDependency<UCKPartySubsystem>();
	if (!IsEnabled())
	{
		return;
	}
	Matchmaking->OnMatchFound.AddDynamic(this, &UCKSamplePartyMatchmakingBridge::HandleMatchFound);
	Matchmaking->OnTicketSubmitted.AddDynamic(this, &UCKSamplePartyMatchmakingBridge::HandleTicketSubmitted);
	Matchmaking->OnMatchmakingFailed.AddDynamic(this, &UCKSamplePartyMatchmakingBridge::HandleMatchmakingFailed);
	Matchmaking->OnGroupMemberIdResolved.AddDynamic(this, &UCKSamplePartyMatchmakingBridge::HandleGroupMemberId);
	Party->OnRosterChanged.AddDynamic(this, &UCKSamplePartyMatchmakingBridge::HandleRosterChanged);
	Party->OnMemberAttributeChanged.AddDynamic(this, &UCKSamplePartyMatchmakingBridge::HandleMemberAttributeChanged);
}

void UCKSamplePartyMatchmakingBridge::Deinitialize()
{
	FTSTicker::GetCoreTicker().RemoveTicker(MemberIdTimeoutHandle);
	if (Matchmaking != nullptr)
	{
		Matchmaking->OnMatchFound.RemoveAll(this);
		Matchmaking->OnTicketSubmitted.RemoveAll(this);
		Matchmaking->OnMatchmakingFailed.RemoveAll(this);
		Matchmaking->OnGroupMemberIdResolved.RemoveAll(this);
	}
	if (Party != nullptr)
	{
		Party->OnRosterChanged.RemoveAll(this);
		Party->OnMemberAttributeChanged.RemoveAll(this);
	}
	Super::Deinitialize();
}

bool UCKSamplePartyMatchmakingBridge::IsEnabled() const
{
	return bUseMatchmaking && Matchmaking != nullptr && Party != nullptr;
}

bool UCKSamplePartyMatchmakingBridge::CanFindMatch() const
{
	if (!IsEnabled() || Matchmaking->GetState() != ECKMatchmakingState::Idle)
	{
		return false;
	}
	// A member does not queue: its leader does, and the match comes to it through the party.
	return !Party->IsInParty() || (Party->IsLeader() && !Party->IsPassive());
}

bool UCKSamplePartyMatchmakingBridge::IsBusy() const
{
	return IsEnabled() && (Matchmaking->GetState() != ECKMatchmakingState::Idle || !MemberCallNonce.IsEmpty());
}

bool UCKSamplePartyMatchmakingBridge::FindMatch()
{
	const UCKMatchmakingQueueData* QueueData = Queue.LoadSynchronous();
	if (!CanFindMatch() || QueueData == nullptr || !MemberCallNonce.IsEmpty())
	{
		return false;
	}
	if (!Party->IsInParty())
	{
		return Matchmaking->StartMatchmaking(QueueData, TMap<FName, FString>());
	}

	FString MemberTicketReason;
	if (QueueData->GetProvider() != nullptr && QueueData->GetProvider()->SupportsGroupMemberTickets(MemberTicketReason))
	{
		// A provider whose members join the leader's ticket (Azure PlayFab, Nakama) needs the members' ids before the leader queues and
		// the leader's ticket id after: they travel as member and party attributes (TASK-132, TASK-128, TASK-127, the plugin's README,
		// Groups on PlayFab and Groups on Nakama).
		return BeginMemberCallSearch(QueueData);
	}

	ClearPublishedMatch();
	TArray<FString> MemberIds;
	for (const FCKPartyMember& Member : Party->GetRoster().Members)
	{
		if (Member.PlayerKey != Party->GetLocalPlayerKey())
		{
			MemberIds.Add(Member.PlayerKey);
		}
	}
	bGroupSearch = true;
	UE_LOG(LogCKSamplePartyMatchmaking, Log, TEXT("%s: the party leader queues for %d members."), *GetNameSafe(GetGameInstance()), MemberIds.Num());
	if (!Matchmaking->StartMatchmakingForGroup(QueueData, TMap<FName, FString>(), MemberIds))
	{
		bGroupSearch = false;
		return false;
	}
	return true;
}

void UCKSamplePartyMatchmakingBridge::HandleMatchFound(const FCKMatchResult& Match)
{
	if (!MemberCallNonce.IsEmpty() && Party->IsLeader())
	{
		// A member-call search: every machine got its match from its own ticket, nothing is published.
		EndMemberCallSearch();
		return;
	}
	// A match the leader's group ticket found; the members' own On Match Found (they joined it as group members) do not publish.
	if (bGroupSearch && Party->IsLeader())
	{
		bGroupSearch = false;
		PublishMatch(Match);
	}
}

void UCKSamplePartyMatchmakingBridge::PublishMatch(const FCKMatchResult& Match)
{
	using namespace CKSample::PartyMatchmakingBridge;

	PublishedPlayers = Match.Players.Num();
	for (int32 Index = 0; Index < Match.Players.Num(); ++Index)
	{
		Party->SetPartyAttribute(MakePlayerAttribute(Index), FString::Printf(TEXT("%s|%s"), *Match.Players[Index].PlayerId, *Match.Players[Index].TicketId));
	}
	Party->SetPartyAttribute(PlayerCountAttribute, FString::FromInt(Match.Players.Num()));
	Party->SetPartyAttribute(HostAttribute, Match.HostPlayerId);
	Party->SetPartyAttribute(MatchIdAttribute, Match.MatchId);
	UE_LOG(LogCKSamplePartyMatchmaking, Log, TEXT("%s: match %s with %d players published to the party."), *GetNameSafe(GetGameInstance()), *Match.MatchId, Match.Players.Num());
}

void UCKSamplePartyMatchmakingBridge::ClearPublishedMatch()
{
	using namespace CKSample::PartyMatchmakingBridge;

	if (!Party->IsLeader() || Party->IsPassive())
	{
		return;
	}
	bool bFound = false;
	Party->GetPartyAttribute(MatchIdAttribute, bFound);
	if (!bFound)
	{
		return;
	}
	Party->SetPartyAttribute(MatchIdAttribute, FString());
	Party->SetPartyAttribute(HostAttribute, FString());
	Party->SetPartyAttribute(PlayerCountAttribute, FString());
	for (int32 Index = 0; Index < PublishedPlayers; ++Index)
	{
		Party->SetPartyAttribute(MakePlayerAttribute(Index), FString());
	}
	PublishedPlayers = 0;
}

bool UCKSamplePartyMatchmakingBridge::ReadMatch(const FCKPartyRoster& Roster, FCKMatchResult& OutMatch) const
{
	using namespace CKSample::PartyMatchmakingBridge;

	const TFunction<bool(FName, FString&)> Find = [&Roster](const FName Key, FString& OutValue)
	{
		const FCKPartyAttribute* Attribute = Roster.Attributes.FindByPredicate([Key](const FCKPartyAttribute& Candidate) { return Candidate.Key == Key; });
		OutValue = Attribute != nullptr ? Attribute->Value : FString();
		return Attribute != nullptr;
	};

	FString MatchId;
	FString Host;
	FString Count;
	if (!Find(MatchIdAttribute, MatchId) || !Find(HostAttribute, Host) || !Find(PlayerCountAttribute, Count) || !Count.IsNumeric())
	{
		return false;
	}
	const UCKMatchmakingQueueData* QueueData = Queue.LoadSynchronous();
	OutMatch = FCKMatchResult();
	OutMatch.MatchId = MatchId;
	OutMatch.HostPlayerId = Host;
	OutMatch.QueueName = QueueData != nullptr ? QueueData->GetQueueName() : FName();
	for (int32 Index = 0; Index < FCString::Atoi(*Count); ++Index)
	{
		FString Value;
		FString PlayerId;
		FString TicketId;
		if (!Find(MakePlayerAttribute(Index), Value) || !Value.Split(TEXT("|"), &PlayerId, &TicketId))
		{
			return false;
		}
		FCKMatchPlayer& Player = OutMatch.Players.AddDefaulted_GetRef();
		Player.PlayerId = PlayerId;
		Player.TicketId = TicketId;
	}
	return OutMatch.IsValid();
}

bool UCKSamplePartyMatchmakingBridge::FindPartyAttribute(const FCKPartyRoster& Roster, const FName Key, FString& OutValue)
{
	const FCKPartyAttribute* Attribute = Roster.Attributes.FindByPredicate([Key](const FCKPartyAttribute& Candidate) { return Candidate.Key == Key; });
	OutValue = Attribute != nullptr ? Attribute->Value : FString();
	return Attribute != nullptr;
}

bool UCKSamplePartyMatchmakingBridge::BeginMemberCallSearch(const UCKMatchmakingQueueData* QueueData)
{
	using namespace CKSample::PartyMatchmakingBridge;

	ClearPublishedMatch();
	const FString Nonce = FString::FromInt(NextNonce++);
	// The old search's ticket id goes first, so that no member joins it again under the new request.
	Party->SetPartyAttribute(TicketAttribute, FString());
	if (!Party->SetPartyAttribute(MemberIdRequestAttribute, Nonce + TEXT("|")))
	{
		UE_LOG(LogCKSamplePartyMatchmaking, Warning, TEXT("%s: the request for the members' group member ids could not be published to the party."), *GetNameSafe(GetGameInstance()));
		return false;
	}
	MemberCallNonce = Nonce;
	MemberCallQueue = QueueData;
	bMemberCallTicketSent = false;
	FTSTicker::GetCoreTicker().RemoveTicker(MemberIdTimeoutHandle);
	MemberIdTimeoutHandle = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateUObject(this, &UCKSamplePartyMatchmakingBridge::HandleMemberIdTimeout), FMath::Max(1.0f, MemberIdTimeoutSeconds));
	UE_LOG(LogCKSamplePartyMatchmaking, Log, TEXT("%s: the party leader asks the members for their group member ids (search %s)."), *GetNameSafe(GetGameInstance()), *Nonce);
	return true;
}

void UCKSamplePartyMatchmakingBridge::TryStartMemberCallTicket()
{
	using namespace CKSample::PartyMatchmakingBridge;

	const UCKMatchmakingQueueData* QueueData = MemberCallQueue.Get();
	if (MemberCallNonce.IsEmpty() || bMemberCallTicketSent || !Party->IsLeader())
	{
		return;
	}
	if (QueueData == nullptr)
	{
		EndMemberCallSearch();
		return;
	}
	TArray<FString> MemberIds;
	for (const FCKPartyMember& Member : Party->GetRoster().Members)
	{
		if (Member.PlayerKey == Party->GetLocalPlayerKey())
		{
			continue;
		}
		bool bFound = false;
		FString Nonce;
		FString MemberId;
		const FString Answer = Party->GetMemberAttribute(Member.PlayerKey, MemberIdAttribute, bFound);
		if (!bFound || !SplitNonce(Answer, Nonce, MemberId) || Nonce != MemberCallNonce || MemberId.IsEmpty())
		{
			return;
		}
		MemberIds.Add(MemberId);
	}
	bMemberCallTicketSent = true;
	FTSTicker::GetCoreTicker().RemoveTicker(MemberIdTimeoutHandle);
	MemberIdTimeoutHandle.Reset();
	UE_LOG(LogCKSamplePartyMatchmaking, Log, TEXT("%s: the party leader queues for %d members with the ids they gave."), *GetNameSafe(GetGameInstance()), MemberIds.Num());
	if (!Matchmaking->StartMatchmakingForGroup(QueueData, TMap<FName, FString>(), MemberIds))
	{
		UE_LOG(LogCKSamplePartyMatchmaking, Warning, TEXT("%s: the group ticket was refused: %s."), *GetNameSafe(GetGameInstance()), *UEnum::GetValueAsString(Matchmaking->GetLastFailure()));
		EndMemberCallSearch();
	}
}

void UCKSamplePartyMatchmakingBridge::EndMemberCallSearch()
{
	using namespace CKSample::PartyMatchmakingBridge;

	FTSTicker::GetCoreTicker().RemoveTicker(MemberIdTimeoutHandle);
	MemberIdTimeoutHandle.Reset();
	const bool bWasSearching = !MemberCallNonce.IsEmpty();
	MemberCallNonce.Reset();
	MemberCallQueue.Reset();
	bMemberCallTicketSent = false;
	if (bWasSearching && Party->IsLeader() && !Party->IsPassive())
	{
		Party->SetPartyAttribute(MemberIdRequestAttribute, FString());
		Party->SetPartyAttribute(TicketAttribute, FString());
	}
}

bool UCKSamplePartyMatchmakingBridge::HandleMemberIdTimeout(float DeltaTime)
{
	MemberIdTimeoutHandle.Reset();
	if (!MemberCallNonce.IsEmpty() && !bMemberCallTicketSent)
	{
		UE_LOG(LogCKSamplePartyMatchmaking, Warning, TEXT("%s: a member did not give its group member id in %.0f s: the party does not queue."), *GetNameSafe(GetGameInstance()), MemberIdTimeoutSeconds);
		EndMemberCallSearch();
	}
	return false;
}

void UCKSamplePartyMatchmakingBridge::HandleTicketSubmitted(const FString& TicketId)
{
	using namespace CKSample::PartyMatchmakingBridge;

	// The leader's group ticket is out: the members can join it.
	if (!MemberCallNonce.IsEmpty() && bMemberCallTicketSent && Party->IsLeader() && !TicketId.IsEmpty())
	{
		Party->SetPartyAttribute(TicketAttribute, MemberCallNonce + TEXT("|") + TicketId);
		UE_LOG(LogCKSamplePartyMatchmaking, Log, TEXT("%s: ticket %s published to the party."), *GetNameSafe(GetGameInstance()), *TicketId);
	}
}

void UCKSamplePartyMatchmakingBridge::HandleMatchmakingFailed(const ECKMatchmakingFailure Failure, const FString& Message)
{
	// The leader's group search ended without a match (cancelled, timed out): the members stop waiting for its ticket.
	if (!MemberCallNonce.IsEmpty() && bMemberCallTicketSent && Party->IsLeader())
	{
		EndMemberCallSearch();
	}
}

void UCKSamplePartyMatchmakingBridge::HandleGroupMemberId(const bool bSuccess, const FString& MemberId, const FString& Message)
{
	using namespace CKSample::PartyMatchmakingBridge;

	// Member: the provider told the id the leader asked for; it goes to the leader as the member's own attribute.
	if (AnsweredNonce.IsEmpty() || !Party->IsInParty() || Party->IsLeader())
	{
		return;
	}
	if (!bSuccess || MemberId.IsEmpty())
	{
		UE_LOG(LogCKSamplePartyMatchmaking, Warning, TEXT("%s: the group member id could not be resolved: %s."), *GetNameSafe(GetGameInstance()), *Message);
		return;
	}
	Party->SetMemberAttribute(MemberIdAttribute, AnsweredNonce + TEXT("|") + MemberId);
}

void UCKSamplePartyMatchmakingBridge::HandleMemberAttributeChanged(const FString& PlayerKey, const FName Key, const FString& Value)
{
	if (Key == CKSample::PartyMatchmakingBridge::MemberIdAttribute)
	{
		TryStartMemberCallTicket();
	}
}

void UCKSamplePartyMatchmakingBridge::HandleMemberCallRoster(const FCKPartyRoster& Roster)
{
	using namespace CKSample::PartyMatchmakingBridge;

	FString Request;
	FString Nonce;
	FString Unused;
	if (!FindPartyAttribute(Roster, MemberIdRequestAttribute, Request) || !SplitNonce(Request, Nonce, Unused) || Nonce.IsEmpty())
	{
		// The leader took the request away: the search is over, the next one starts clean.
		if (!AnsweredNonce.IsEmpty())
		{
			AnsweredNonce.Reset();
			JoinedTicketNonce.Reset();
			Party->SetMemberAttribute(MemberIdAttribute, FString());
		}
		return;
	}
	const UCKMatchmakingQueueData* QueueData = Queue.LoadSynchronous();
	if (QueueData == nullptr)
	{
		return;
	}
	if (Nonce != AnsweredNonce)
	{
		if (Matchmaking->GetState() != ECKMatchmakingState::Idle)
		{
			return;
		}
		AnsweredNonce = Nonce;
		JoinedTicketNonce.Reset();
		UE_LOG(LogCKSamplePartyMatchmaking, Log, TEXT("%s: the party leader asks for the group member id (search %s)."), *GetNameSafe(GetGameInstance()), *Nonce);
		if (!Matchmaking->RequestGroupMemberId(QueueData))
		{
			UE_LOG(LogCKSamplePartyMatchmaking, Warning, TEXT("%s: the group member id could not be requested: %s."), *GetNameSafe(GetGameInstance()),
				*UEnum::GetValueAsString(Matchmaking->GetLastFailure()));
			AnsweredNonce.Reset();
		}
		return;
	}

	FString Ticket;
	FString TicketNonce;
	FString TicketId;
	if (JoinedTicketNonce == Nonce || !FindPartyAttribute(Roster, TicketAttribute, Ticket) || !SplitNonce(Ticket, TicketNonce, TicketId) || TicketNonce != Nonce || TicketId.IsEmpty())
	{
		return;
	}
	if (Matchmaking->GetState() != ECKMatchmakingState::Idle)
	{
		return;
	}
	JoinedTicketNonce = Nonce;
	UE_LOG(LogCKSamplePartyMatchmaking, Log, TEXT("%s: joining the party leader's ticket %s."), *GetNameSafe(GetGameInstance()), *TicketId);
	if (!Matchmaking->JoinGroupTicket(QueueData, TMap<FName, FString>(), TicketId))
	{
		UE_LOG(LogCKSamplePartyMatchmaking, Warning, TEXT("%s: the leader's ticket %s could not be joined: %s."), *GetNameSafe(GetGameInstance()), *TicketId,
			*UEnum::GetValueAsString(Matchmaking->GetLastFailure()));
	}
}

void UCKSamplePartyMatchmakingBridge::HandleRosterChanged(const FCKPartyRoster& Roster)
{
	if (!Party->IsInParty())
	{
		return;
	}
	if (Party->IsLeader())
	{
		TryStartMemberCallTicket();
		return;
	}
	HandleMemberCallRoster(Roster);
	FCKMatchResult Match;
	if (!ReadMatch(Roster, Match))
	{
		// The leader took the match away (a new search, or the match is over): the next one is joined.
		bool bFound = false;
		Party->GetPartyAttribute(CKSample::PartyMatchmakingBridge::MatchIdAttribute, bFound);
		if (!bFound)
		{
			JoinedMatchId.Reset();
		}
		return;
	}
	if (Match.MatchId == JoinedMatchId)
	{
		return;
	}
	const UCKMatchmakingQueueData* QueueData = Queue.LoadSynchronous();
	if (QueueData == nullptr || Matchmaking->GetState() != ECKMatchmakingState::Idle)
	{
		return;
	}
	JoinedMatchId = Match.MatchId;
	UE_LOG(LogCKSamplePartyMatchmaking, Log, TEXT("%s: joining match %s found by the party leader."), *GetNameSafe(GetGameInstance()), *Match.MatchId);
	if (!Matchmaking->JoinMatchAsGroupMember(QueueData, Match))
	{
		UE_LOG(LogCKSamplePartyMatchmaking, Warning, TEXT("%s: the leader's match %s could not be joined: %s."), *GetNameSafe(GetGameInstance()), *Match.MatchId,
			*UEnum::GetValueAsString(Matchmaking->GetLastFailure()));
	}
}
