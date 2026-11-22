// Copyright 2026 Ivan Iaitckikh. All Rights Reserved.

#pragma once

#include "CKMatchmakingTypes.h"
#include "CKPartyTypes.h"
#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Containers/Ticker.h"
#include "UObject/SoftObjectPtr.h"
#include "CKSamplePartyMatchmakingBridge.generated.h"

class UCKMatchmakingQueueData;
class UCKMatchmakingSubsystem;
class UCKPartySubsystem;

/**
 * Bridge: CoopKit Party <-> CoopKit Matchmaking (TASK-118, decision-94, decision-101).
 *
 * The party queues together. Its leader sends one group ticket for the whole party (Start Matchmaking For Group with the
 * party members' keys, which are also their matchmaking ids), and the match the leader gets goes to the members as party
 * attributes (Set Party Attribute): the match id, the host, and one attribute per player with its ticket. Every member reads
 * the attributes from its roster and calls Join Match As Group Member with the match rebuilt from them. From there the match
 * is joined like any other match of the queue (see Matchmaking -> Sessions, CKSampleMatchmakingSessionsBridge). A player with
 * no party sends a single ticket. Neither plugin knows the other.
 *
 * The party's own beacon closes when the leader's map changes, so the leader publishes the match as soon as it has it; a
 * leader that hosts the match opens the map only after its session exists, which is later than the beacon message.
 *
 * A provider whose members join the leader's ticket with their own call (Azure PlayFab, Nakama: SupportsGroupMemberTickets)
 * needs more than the match to travel (TASK-132, decision-116). The leader asks the members for their group member ids with
 * the party attribute CKSAMPLE_MM_REQ; each member asks its provider (Request Group Member Id) and answers with its own member
 * attribute CKSAMPLE_MM_MID (TASK-133, decision-115); when every member has answered, the leader sends the group ticket with
 * their ids and publishes the ticket id as CKSAMPLE_MM_TICKET; each member calls Join Group Ticket with it. Every machine then
 * gets the match from its own ticket, so no match is published to the party. A request, its answers and its ticket id carry
 * one nonce ("<nonce>|<value>"), so that an old answer is never taken for a new search.
 *
 * With bUseMatchmaking off the bridge does nothing and the party panel shows no Find match button. Console, for developers:
 * ck.sample.PartyFindMatch. Configured in Config/DefaultGame.ini, section
 * [/Script/CoopKitSampleBridges.CKSamplePartyMatchmakingBridge]. Test: CoopKit.Sample.Party.Matchmaking.
 */
UCLASS(Config = Game)
class UCKSamplePartyMatchmakingBridge final : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	/** The bridge is on (bUseMatchmaking) and found both plugins. */
	bool IsEnabled() const;

	/** True when Find Match would start: matchmaking is idle and the player is not a member of somebody else's party. */
	bool CanFindMatch() const;

	/** A ticket waits for a match, or a match is being joined. */
	bool IsBusy() const;

	/**
	 * Starts matchmaking in the bridge's queue: the party leader with a group ticket, a player with no party with a single
	 * ticket. False when refused (a member does not queue: its leader does; or matchmaking is busy).
	 */
	bool FindMatch();

private:
	UFUNCTION()
	void HandleMatchFound(const FCKMatchResult& Match);

	UFUNCTION()
	void HandleTicketSubmitted(const FString& TicketId);

	UFUNCTION()
	void HandleMatchmakingFailed(ECKMatchmakingFailure Failure, const FString& Message);

	UFUNCTION()
	void HandleGroupMemberId(bool bSuccess, const FString& MemberId, const FString& Message);

	UFUNCTION()
	void HandleMemberAttributeChanged(const FString& PlayerKey, FName Key, const FString& Value);

	UFUNCTION()
	void HandleRosterChanged(const FCKPartyRoster& Roster);

	/** Leader: puts the match into the party's attributes, the match id last so a member sees all of it at once. */
	void PublishMatch(const FCKMatchResult& Match);

	/** Member: rebuilds the match from the roster's attributes; false when the leader has not published a whole one. */
	bool ReadMatch(const FCKPartyRoster& Roster, FCKMatchResult& OutMatch) const;

	/** Leader: removes the attributes of the last match. */
	void ClearPublishedMatch();

	/** Leader: asks the members for their group member ids (member-call providers). False when the request could not be published. */
	bool BeginMemberCallSearch(const UCKMatchmakingQueueData* QueueData);

	/** Leader: sends the group ticket once every member in the roster has answered the request. */
	void TryStartMemberCallTicket();

	/** Leader: the member-call search is over (ticket matched, failed or never started): removes its attributes and timer. */
	void EndMemberCallSearch();

	/** Member: answers a new request of the leader, joins the leader's ticket once it is published, forgets both when they go away. */
	void HandleMemberCallRoster(const FCKPartyRoster& Roster);

	/** The id the leader asks for expires: the leader gives up when a member does not answer in time. */
	bool HandleMemberIdTimeout(float DeltaTime);

	/** Reads a party attribute of the roster; false when it is not set. */
	static bool FindPartyAttribute(const FCKPartyRoster& Roster, FName Key, FString& OutValue);

	/** The leader's group ticket for this bridge's search is out and its match is not published yet. */
	bool bGroupSearch = false;

	/** Leader: nonce of the member-call search that waits for the members' ids; empty when none does. */
	FString MemberCallNonce;

	/** Leader: queue of that search. */
	TWeakObjectPtr<const UCKMatchmakingQueueData> MemberCallQueue;

	/** Leader: the group ticket of that search is out. */
	bool bMemberCallTicketSent = false;

	/** Leader: the next search's nonce. */
	int32 NextNonce = 1;

	/** Member: nonce of the request it answered last (its id is being resolved, or was published). */
	FString AnsweredNonce;

	/** Member: nonce of the leader's ticket it has already joined. */
	FString JoinedTicketNonce;

	/** Leader: the timer that ends a search whose members do not answer. */
	FTSTicker::FDelegateHandle MemberIdTimeoutHandle;

	/** Match id of the leader's match the local player has already joined (member side). */
	FString JoinedMatchId;

	/** Number of player attributes the leader published last, to clear them. */
	int32 PublishedPlayers = 0;

	/** Queue the party searches in (the sample's Local queue: 3 players, Join Mode Manual). */
	UPROPERTY(Config)
	TSoftObjectPtr<UCKMatchmakingQueueData> Queue;

	/** The party queues together: off, the bridge does nothing. */
	UPROPERTY(Config)
	bool bUseMatchmaking = true;

	/** Seconds the leader waits for every member's group member id (member-call providers) before it gives up. */
	UPROPERTY(Config)
	float MemberIdTimeoutSeconds = 30.0f;

	UPROPERTY(Transient)
	TObjectPtr<UCKMatchmakingSubsystem> Matchmaking;

	UPROPERTY(Transient)
	TObjectPtr<UCKPartySubsystem> Party;
};
