// Copyright 2026 Ivan Iaitckikh. All Rights Reserved.

#pragma once

#include "CKDraftTypes.h"
#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "UI/CKSampleUmgScreenWidget.h"
#include "CKSampleUmgDraftWidget.generated.h"

class UCKDraftComponent;
class UCKDraftPlayerComponent;
class UTextBlock;
class UUniformGridPanel;
class UVerticalBox;

/**
 * Draft screen of the sample project on plain UMG (TASK-52), on CoopKit Draft's Blueprint API only:
 * - the mode, the turn (pick, ban or swap window, whose turn) and its time left, then the player's reserve time
 *   (CKDraft Component: Get Current Turn, Get Turn Seconds Left, Get Reserve Seconds Left, Is Using Reserve);
 * - the heroes of the pool as buttons: a click picks or bans in the player's turn (CKDraft Player Component: Request Pick,
 *   Request Ban); heroes the rules refuse are greyed out with a short reason (Preview Action);
 * - the bans and one line per player: hero, hidden pick ("locked in"), hover and reserve; in the swap window a Swap button
 *   for each teammate (Request Swap) and Accept / Decline for an offer to the player (Answer Swap);
 * - in a team ban vote (TASK-55) the votes of the player's own team (the state's Ban Votes, On Ban Vote Changed): a hero
 *   click is the player's vote and may change it until the turn ends; the other team's votes never reach this machine;
 * - the last refusal of the server with its reason (On Action Rejected, Get Reject Reason Text) and the last event.
 * The screen refreshes on the draft's events (phase, turn, pick, ban, hover, swap offer, swap) and on a short timer for
 * the clocks. It shows only what the server sent this player: in a blind pick an opponent's hero appears at the reveal.
 * The sample's draft bridge shows it while a draft runs on a draft lobby map (or anywhere with ck.sample.DraftScreen 1).
 */
UCLASS()
class UCKSampleUmgDraftWidget : public UCKSampleUmgScreenWidget
{
	GENERATED_BODY()

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void BuildDefaultTree() override;
	virtual void Refresh() override;
	virtual void ApplyInputMode(APlayerController& Controller) override;

	/** Mode name and draft phase. */
	UPROPERTY(BlueprintReadOnly, Category = "CoopKit Sample|Draft", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> ModeText;

	/** "Turn 2 of 5: Pick - your turn" or who the turn waits for. */
	UPROPERTY(BlueprintReadOnly, Category = "CoopKit Sample|Draft", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> TurnText;

	/** Time left in the turn, or the reserve the player is spending. */
	UPROPERTY(BlueprintReadOnly, Category = "CoopKit Sample|Draft", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> TimerText;

	/** One button per hero of the pool. */
	UPROPERTY(BlueprintReadOnly, Category = "CoopKit Sample|Draft", meta = (BindWidgetOptional))
	TObjectPtr<UUniformGridPanel> HeroGrid;

	UPROPERTY(BlueprintReadOnly, Category = "CoopKit Sample|Draft", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> BansText;

	/** In a team ban vote: the votes of the player's team, the only votes this machine receives. */
	UPROPERTY(BlueprintReadOnly, Category = "CoopKit Sample|Draft", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> VotesText;

	/** One line per participant, with a Swap button for teammates in the swap window. */
	UPROPERTY(BlueprintReadOnly, Category = "CoopKit Sample|Draft", meta = (BindWidgetOptional))
	TObjectPtr<UVerticalBox> PlayerList;

	/** An offer to this player, or the state of the player's own offer. */
	UPROPERTY(BlueprintReadOnly, Category = "CoopKit Sample|Draft", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> SwapText;

	UPROPERTY(BlueprintReadOnly, Category = "CoopKit Sample|Draft", meta = (BindWidgetOptional))
	TObjectPtr<UButton> AcceptSwapButton;

	UPROPERTY(BlueprintReadOnly, Category = "CoopKit Sample|Draft", meta = (BindWidgetOptional))
	TObjectPtr<UButton> DeclineSwapButton;

	/** The server's last refusal with its reason. */
	UPROPERTY(BlueprintReadOnly, Category = "CoopKit Sample|Draft", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> RejectText;

	/** The last draft event this machine saw. */
	UPROPERTY(BlueprintReadOnly, Category = "CoopKit Sample|Draft", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> EventText;

private:
	UCKDraftComponent* GetDraft() const;
	UCKDraftPlayerComponent* GetPlayer() const;
	APlayerState* GetLocalPlayerState() const;
	void BindEvents();
	void RebuildLists(const FCKDraftState& State);
	void RefreshHeroes(const UCKDraftComponent& Draft, const FCKDraftState& State);
	void RefreshPlayers(const UCKDraftComponent& Draft, const FCKDraftState& State);
	void RefreshSwap(const FCKDraftState& State);
	void RefreshVotes(const UCKDraftComponent& Draft, const FCKDraftState& State);
	FString DescribeParticipant(const UCKDraftComponent& Draft, const FCKDraftState& State, int32 Index) const;
	FString GetPlayerName(const FCKDraftState& State, int32 Index) const;
	void SetEvent(const FString& Line);

	void HandleHeroClicked(int32 HeroIndex);
	void HandleSwapClicked(int32 Participant);

	UFUNCTION()
	void HandleAcceptClicked();

	UFUNCTION()
	void HandleDeclineClicked();

	UFUNCTION()
	void HandlePhaseChanged(ECKDraftPhase Phase);

	UFUNCTION()
	void HandleTurnChanged(int32 TurnIndex);

	UFUNCTION()
	void HandleHeroPicked(int32 Participant, APlayerState* Player, FName Hero);

	UFUNCTION()
	void HandleHeroBanned(int32 Participant, APlayerState* Player, FName Hero);

	UFUNCTION()
	void HandleHeroHovered(int32 Participant, APlayerState* Player, FName Hero);

	UFUNCTION()
	void HandleBanVoteChanged(int32 Participant, APlayerState* Player, FName Hero);

	UFUNCTION()
	void HandleSwapOffered(int32 FromParticipant, int32 ToParticipant);

	UFUNCTION()
	void HandleHeroesSwapped(int32 FromParticipant, int32 ToParticipant);

	UFUNCTION()
	void HandleActionRejected(ECKDraftAction Action, FName Hero, FGameplayTag Reason);

	TWeakObjectPtr<UCKDraftComponent> BoundDraft;
	TWeakObjectPtr<UCKDraftPlayerComponent> BoundPlayer;

	/** Hero ids in the order of the grid's buttons, and the draft the grid and the player rows were built for. */
	TArray<FName> GridHeroes;
	int32 BuiltDraftId = INDEX_NONE;
	int32 BuiltParticipants = INDEX_NONE;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UButton>> HeroButtons;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UTextBlock>> PlayerTexts;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UButton>> SwapButtons;

	/** Participant whose offer Accept and Decline answer; INDEX_NONE without an offer. */
	int32 OfferFrom = INDEX_NONE;
};
