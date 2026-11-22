// Copyright 2026 Ivan Iaitckikh. All Rights Reserved.

#pragma once

#include "CKPartyGameDriver.h"
#include "CKSessionsTypes.h"
#include "Containers/Ticker.h"
#include "CoreMinimal.h"
#include "Interfaces/OnlineSessionInterface.h"
#include "OnlineSessionSettings.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "UObject/Object.h"
#include "UObject/SoftObjectPtr.h"
#include "UObject/WeakObjectPtrTemplates.h"
#include "CKSamplePartySessionsBridge.generated.h"

class UCKPartySubsystem;
class UCKSessionsSubsystem;
class UGameInstance;
class UWorld;

/**
 * Bridge: CoopKit Party -> CoopKit Sessions (TASK-118, decision-94, decision-105).
 *
 * The party's game driver on CoopKit Sessions. The party decides who goes where and when (the leader, the places, the
 * ticket every member gets); this driver only turns its operations into the session plugin's calls:
 * - Host Game: Host Session With Attributes on the game's map, advertising CKPARTY_GAME = the party's id (the sample's
 *   lobby; the session plugin gives the session its own profile's slots and map options, not the ones of the request).
 * - Find Games: Find Sessions With Attributes, the results described for the party.
 * - Prepare Join and Travel To Game: the leader keeps the chosen result, tells the members its session id and address, then
 *   Join Session.
 * - Follow Leader: Find Sessions With Attributes for CKPARTY_GAME = the party's id (or the ticket's session), then Join
 *   Session, again every second until the party's follow timeout. A Null LAN host answers a search with only one of its
 *   sessions before UE 5.8, and that is the party session, which the session plugin's search filters out; so when the
 *   search finds nothing, a plain Online Subsystem search reads the game's address from the leader's party session
 *   (CKPARTY_GAMEADDR) and the player travels there.
 * - Leave Game: Leave Session To Map, then wait until the new standalone map runs.
 *
 * One operation at a time, as the party subsystem starts them.
 */
UCLASS()
class UCKSamplePartySessionsDriver final : public UObject, public ICKPartyGameDriver
{
	GENERATED_BODY()

public:
	virtual void BeginDestroy() override;

	virtual void HostGame(UGameInstance& GameInstance, const FCKPartyGameHostParams& Params, FCKPartyGameDone Done) override;
	virtual void FindGames(UGameInstance& GameInstance, const FCKPartyGameSearchParams& Params, FCKPartyGameSearchDone Done) override;
	virtual void PrepareJoin(UGameInstance& GameInstance, const FCKPartyGameInfo& Game, FCKPartyGamePrepareDone Done) override;
	virtual void TravelToGame(UGameInstance& GameInstance, FCKPartyGameDone Done) override;
	virtual void FollowLeader(UGameInstance& GameInstance, const FCKPartyGameTicket& Ticket, float TimeoutSeconds, FCKPartyGameDone Done) override;
	virtual void LeaveGame(UGameInstance& GameInstance, const FString& MapPackageName, FCKPartyGameDone Done) override;

	/** Seconds a host, search, join or leave may take before it counts as failed (a follow has the party's own timeout). */
	static constexpr double OperationTimeoutSeconds = 60.0;

private:
	enum class EOperation : uint8
	{
		None,
		Host,
		Find,
		/** Leader: joining the game it chose; the session plugin joins and travels. */
		Travel,
		Follow,
		Leave
	};

	/** What a member does while it follows its leader. */
	enum class EFollowStep : uint8
	{
		/** Pause between two rounds. */
		Waiting,
		/** Find Sessions With Attributes runs. */
		Searching,
		/** Join Session runs; then the player waits for its player state on the host. */
		Joining,
		/** A plain Online Subsystem search reads the game's address from the leader's party session. */
		RawSearching,
		/** The member travels to the address itself. */
		RawTravelling
	};

	UFUNCTION()
	void HandleHostComplete(bool bSuccess, ECKSessionsFailure Failure);

	UFUNCTION()
	void HandleFindComplete(bool bSuccess, const TArray<FCKSessionsSearchResult>& Results);

	UFUNCTION()
	void HandleJoinComplete(bool bSuccess, ECKSessionsFailure Failure);

	void HandleRawFindComplete(bool bSuccess);

	/** Starts an operation; null (the caller finishes with failure) when the game instance has no session subsystem. */
	UCKSessionsSubsystem* Begin(UGameInstance& GameInstance, EOperation NewOperation);
	bool Tick(float DeltaSeconds);
	void Finish(bool bSuccess);
	void Unbind();

	void StartFollowRound();
	void StartRawSearch();
	void RetryFollowLater();
	void FollowWithResults(const TArray<FCKSessionsSearchResult>& Results);
	void TravelToAddress(const FString& Address);

	/** A client world other than the one the operation started in, with the local player's player state replicated. */
	bool IsInTheGame() const;

	/** The world of the game instance is a standalone map that finished loading and is not travelling. */
	bool IsSettledStandalone() const;

	EOperation Operation = EOperation::None;
	EFollowStep FollowStep = EFollowStep::Waiting;
	TWeakObjectPtr<UGameInstance> ActiveInstance;
	TWeakObjectPtr<UWorld> StartWorld;
	TWeakObjectPtr<UCKSessionsSubsystem> BoundSessions;
	FCKPartyGameDone PendingDone;
	FCKPartyGameSearchDone PendingSearch;
	double StartedSeconds = 0.0;
	FTSTicker::FDelegateHandle TickerHandle;

	/** Results of the latest Find Games, in the order DriverIndex refers to; the leader's chosen game while it travels. */
	TArray<FCKSessionsSearchResult> LastResults;
	FCKSessionsSearchResult ChosenGame;

	FCKPartyGameTicket FollowTicket;
	double FollowDeadlineSeconds = 0.0;
	double NextRoundSeconds = 0.0;
	double RawTravelStartedSeconds = 0.0;
	int32 FollowRounds = 0;
	TSharedPtr<FOnlineSessionSearch> RawSearch;
	FDelegateHandle RawFindHandle;
};

/**
 * Bridge: CoopKit Party -> CoopKit Sessions, the subsystem side (TASK-118).
 *
 * Game instance subsystem. With bUseSessionsDriver on (the sample's Config/DefaultGame.ini) it gives the party subsystem the
 * driver above (Set Game Driver), so the party goes into the sample's CoopKit Sessions lobby behind its leader and comes back
 * after the match. With the setting off the party runs on its own online driver. Play With Party is what the party panel's
 * Play button calls: the leader hosts the lobby map for the whole party (Host Game of the party subsystem, which checks the
 * places and sends every member its ticket first). Console, for developers: ck.sample.PartyPlay.
 *
 * Configured in Config/DefaultGame.ini, section [/Script/CoopKitSampleBridges.CKSamplePartySessionsBridge].
 * Test: CoopKit.Sample.Party.Sessions.
 */
UCLASS(Config = Game)
class UCKSamplePartySessionsBridge final : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	/** Leader: takes the whole party into the game on the bridge's HostMap. False when refused at once (see the party's Last Game Failure). */
	bool PlayWithParty();

	/** The sample's CoopKit Sessions driver is the party's game driver. */
	bool IsDriverInstalled() const
	{
		return bUseSessionsDriver && Driver != nullptr;
	}

private:
	/** The party's game driver is the sample's CoopKit Sessions driver; off: the plugin's own online driver. */
	UPROPERTY(Config)
	bool bUseSessionsDriver = true;

	/** Map the party's game opens as a listen server (the sample's lobby). */
	UPROPERTY(Config)
	TSoftObjectPtr<UWorld> HostMap;

	/** Places the party's game asks for when the party is smaller; the session plugin takes the Max Players of its own profile. */
	UPROPERTY(Config)
	int32 GameMaxPlayers = 4;

	UPROPERTY(Transient)
	TObjectPtr<UCKPartySubsystem> Party;

	UPROPERTY(Transient)
	TObjectPtr<UObject> Driver;
};
