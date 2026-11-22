// Copyright 2026 Ivan Iaitckikh. All Rights Reserved.

#pragma once

#include "Blueprint/UserWidget.h"
#include "Components/Button.h"
#include "CoreMinimal.h"
#include "Engine/TimerHandle.h"
#include "CKSampleMigrationPanelWidget.generated.h"

class UInputComponent;
class UTextBlock;

/** Names of the migration panel's widgets: tests and Widget Blueprint children find them by these names. */
namespace CKSample::MigrationPanel::WidgetNames
{
	inline constexpr const TCHAR* HostButton = TEXT("MigrationHostButton");
	inline constexpr const TCHAR* FindButton = TEXT("MigrationFindButton");
	inline constexpr const TCHAR* RejoinButton = TEXT("MigrationRejoinButton");
	inline constexpr const TCHAR* LeaveButton = TEXT("MigrationLeaveButton");

	/** The match's hint about the middle mouse button, which shows and hides the cursor. */
	inline constexpr const TCHAR* CursorHint = TEXT("MigrationCursorHint");

	/** What the migration is doing, in words a player understands. */
	inline constexpr const TCHAR* StateText = TEXT("MigrationStateText");
}

/**
 * The host migration panel of the sample (TASK-125, decision-111): a mouse-only way to start a migration game and a
 * readable state of the group. It talks to the bridge UCKSampleHostMigrationBridge and to CoopKit Host Migration's
 * Blueprint API, and sits in the top right corner of the screen: the party panel (UCKSamplePartyPanelWidget) has the
 * top left, the menu screens the middle.
 * - On a menu map and without a group: Host a migration game and Find a migration game (with a way to rejoin the game
 *   this player left). They are the menu's Host and Find through the migration subsystem: the group hosted here is the
 *   one that survives the loss of its host.
 * - In a group (a menu map or a match): the state text (host, successor rank, migrating, new host) and Leave game.
 * Nothing is shown on a match map while the player is in no group.
 * - In a match the game shows no mouse cursor, so the panel binds the middle mouse button (TASK-135): one press shows the
 *   cursor (the camera stops turning) so Leave game can be clicked, the next press, Leave or leaving the group hides it
 *   again. A hint line on the panel says so.
 */
UCLASS()
class UCKSampleMigrationPanelWidget final : public UUserWidget
{
	GENERATED_BODY()

public:
	virtual bool Initialize() override;

	/** True on a menu map: Host and Find are offered there. Set by the bridge (UCKSampleMigrationPanelBridge). */
	void SetMenuMode(bool bInMenu);

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

private:
	void BuildTree();
	UButton* AddButton(class UPanelWidget* Parent, FName Name, const FText& Label);
	void Refresh();
	FText DescribeState() const;

	UFUNCTION()
	void HandleHostClicked();

	UFUNCTION()
	void HandleFindClicked();

	UFUNCTION()
	void HandleRejoinClicked();

	UFUNCTION()
	void HandleLeaveClicked();

	/** True while the player is in a group (host or client): the only time a match shows the panel. */
	bool IsInGroup() const;

	/** The middle mouse button: shows or hides the cursor in a match. */
	void HandleCursorKey();

	/** Shows the cursor and stops the camera, or gives the game its mouse back; only the panel's own change is undone. */
	void SetMatchCursor(bool bShow);

	UPROPERTY(Transient)
	TObjectPtr<UWidget> Frame;

	UPROPERTY(Transient)
	TObjectPtr<UButton> HostButton;

	UPROPERTY(Transient)
	TObjectPtr<UButton> FindButton;

	UPROPERTY(Transient)
	TObjectPtr<UButton> RejoinButton;

	UPROPERTY(Transient)
	TObjectPtr<UButton> LeaveButton;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> StateText;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> CursorHint;

	/** The key binding of the middle mouse button on the owning player controller. */
	UPROPERTY(Transient)
	TObjectPtr<UInputComponent> CursorInput;

	bool bMenuMode = false;

	/** True while the panel itself shows the cursor and ignores the look input. */
	bool bMatchCursor = false;
	FTimerHandle RefreshTimer;
};
