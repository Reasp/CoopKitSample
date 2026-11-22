// Copyright 2026 Ivan Iaitckikh. All Rights Reserved.

#pragma once

#include "CKSessionsLobbyComponent.h"
#include "CoreMinimal.h"
#include "UI/CKSampleUmgScreenWidget.h"
#include "CKSampleUmgLobbyWidget.generated.h"

class UCKSessionsPlayerComponent;
class UCKSessionsSubsystem;
class UTextBlock;
class UVerticalBox;

/**
 * Lobby screen of the sample project on plain UMG: the same screen, logic and widget names as the CommonUI lobby
 * (UCKSampleLobbyWidget), on CoopKit Sessions' Blueprint API only (TASK-31, decision-47):
 * - one line per player of the game state with the player's ready flag (CKSessions Player Component: Is Ready);
 * - Ready toggles the local player's flag (Set Ready); it waits until the lobby gave the player its component;
 * - the lobby phase and the countdown (CKSessions Lobby Component: Get Phase, Get Countdown Seconds Left);
 * - Start, for the listen server's host only (Start Match Now);
 * - Leave: Leave Session, back to the menu map.
 * The screen goes away when the lobby travels to the match.
 */
UCLASS()
class UCKSampleUmgLobbyWidget : public UCKSampleUmgScreenWidget
{
	GENERATED_BODY()

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void BuildDefaultTree() override;
	virtual void Refresh() override;
	virtual void ApplyInputMode(APlayerController& Controller) override;
	virtual UWidget* GetFocusWidget() override;

	/** Lobby phase, or the countdown. */
	UPROPERTY(BlueprintReadOnly, Category = "CoopKit Sample|Sessions", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> PhaseText;

	/** One text line per player, in the game state's order: "<name> - Ready" or "<name> - Not ready". */
	UPROPERTY(BlueprintReadOnly, Category = "CoopKit Sample|Sessions", meta = (BindWidgetOptional))
	TObjectPtr<UVerticalBox> PlayerList;

	UPROPERTY(BlueprintReadOnly, Category = "CoopKit Sample|Sessions", meta = (BindWidgetOptional))
	TObjectPtr<UButton> ReadyButton;

	UPROPERTY(BlueprintReadOnly, Category = "CoopKit Sample|Sessions", meta = (BindWidgetOptional))
	TObjectPtr<UButton> StartButton;

	UPROPERTY(BlueprintReadOnly, Category = "CoopKit Sample|Sessions", meta = (BindWidgetOptional))
	TObjectPtr<UButton> LeaveButton;

	/** Answer to the last button, e.g. why Start was refused. */
	UPROPERTY(BlueprintReadOnly, Category = "CoopKit Sample|Sessions", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> StatusText;

private:
	UCKSessionsSubsystem* GetSessions() const;
	UCKSessionsPlayerComponent* GetLocalPlayerComponent() const;
	bool IsListenHost() const;
	void RefreshPlayers();
	void RefreshPhase(const UCKSessionsLobbyComponent* Lobby);
	void SetStatus(const FText& InText);

	UFUNCTION()
	void HandleReadyClicked();

	UFUNCTION()
	void HandleStartClicked();

	UFUNCTION()
	void HandleLeaveClicked();

	UFUNCTION()
	void HandlePhaseChanged(ECKSessionsLobbyPhase Phase);

	UFUNCTION()
	void HandleReadyChanged(UCKSessionsPlayerComponent* Component, bool bIsReady);
};
