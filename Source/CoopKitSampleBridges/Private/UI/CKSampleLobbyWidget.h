// Copyright 2026 Ivan Iaitckikh. All Rights Reserved.

#pragma once

#include "CKSessionsLobbyComponent.h"
#include "CoreMinimal.h"
#include "UI/CKSampleScreenWidget.h"
#include "CKSampleLobbyWidget.generated.h"

class UCKSampleButtonWidget;
class UCKSessionsPlayerComponent;
class UCKSessionsSubsystem;
class UCommonTextBlock;
class UVerticalBox;

/**
 * Lobby screen of the sample project, on CoopKit Sessions' Blueprint API only:
 * - one line per player of the game state with the player's ready flag (CKSessions Player Component: Is Ready);
 * - Ready toggles the local player's flag (Set Ready); it waits until the lobby gave the player its component;
 * - the lobby phase and the countdown (CKSessions Lobby Component: Get Phase, Get Countdown Seconds Left);
 * - Start, for the listen server's host only (Start Match Now: allowed when the rules are met or let the host start early);
 * - Leave: Leave Session, back to the menu map.
 * The screen refreshes on the components' events (On Phase Changed, On Ready Changed) and on a short timer, because a
 * client learns about a new player from replication, with no event. It goes away when the lobby travels to the match.
 */
UCLASS()
class UCKSampleLobbyWidget : public UCKSampleScreenWidget
{
	GENERATED_BODY()

public:
	virtual TOptional<FUIInputConfig> GetDesiredInputConfig() const override;

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual UWidget* NativeGetDesiredFocusTarget() const override;
	virtual void BuildDefaultTree() override;
	virtual void Refresh() override;

	/** Lobby phase, or the countdown. */
	UPROPERTY(BlueprintReadOnly, Category = "CoopKit Sample|Sessions", meta = (BindWidgetOptional))
	TObjectPtr<UCommonTextBlock> PhaseText;

	/** One text line per player, in the game state's order: "<name> - Ready" or "<name> - Not ready". */
	UPROPERTY(BlueprintReadOnly, Category = "CoopKit Sample|Sessions", meta = (BindWidgetOptional))
	TObjectPtr<UVerticalBox> PlayerList;

	UPROPERTY(BlueprintReadOnly, Category = "CoopKit Sample|Sessions", meta = (BindWidgetOptional))
	TObjectPtr<UCKSampleButtonWidget> ReadyButton;

	UPROPERTY(BlueprintReadOnly, Category = "CoopKit Sample|Sessions", meta = (BindWidgetOptional))
	TObjectPtr<UCKSampleButtonWidget> StartButton;

	UPROPERTY(BlueprintReadOnly, Category = "CoopKit Sample|Sessions", meta = (BindWidgetOptional))
	TObjectPtr<UCKSampleButtonWidget> LeaveButton;

	/** Answer to the last button, e.g. why Start was refused. */
	UPROPERTY(BlueprintReadOnly, Category = "CoopKit Sample|Sessions", meta = (BindWidgetOptional))
	TObjectPtr<UCommonTextBlock> StatusText;

private:
	UCKSessionsSubsystem* GetSessions() const;
	UCKSessionsPlayerComponent* GetLocalPlayerComponent() const;
	bool IsListenHost() const;
	void RefreshPlayers();
	void RefreshPhase(const UCKSessionsLobbyComponent* Lobby);
	void SetStatus(const FText& InText);

	void HandleReadyClicked();
	void HandleStartClicked();
	void HandleLeaveClicked();

	UFUNCTION()
	void HandlePhaseChanged(ECKSessionsLobbyPhase Phase);

	UFUNCTION()
	void HandleReadyChanged(UCKSessionsPlayerComponent* Component, bool bIsReady);
};
