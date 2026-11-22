// Copyright 2026 Ivan Iaitckikh. All Rights Reserved.

#pragma once

#include "CKSessionsTypes.h"
#include "CoreMinimal.h"
#include "Engine/TimerHandle.h"
#include "UI/CKSampleScreenWidget.h"
#include "CKSampleMainMenuWidget.generated.h"

class UCKSampleButtonWidget;
class UCKSessionsSubsystem;
class UCommonTextBlock;
class UVerticalBox;
class UWorld;

/**
 * Main menu of the sample project, on CoopKit Sessions' Blueprint API only:
 * - Host: Host Session with the default profile on HostMap (the sample's lobby); the game reopens it as a listen server.
 * - Find: Find Sessions, then one button per joinable session (host, players, ping, "in match"); a click joins it.
 * - A status line shows what the subsystem does and why a request failed (the failure reason), and why the last session
 *   ended when the player comes back to the menu after losing it.
 * The Blueprint nodes Host Session, Find Sessions and Join Session wrap the same subsystem functions and events.
 *
 * Auto start (the packaged sample's two-window launcher and its smoke test, TASK-18.6): with -CKSampleAutoHost on the
 * command line the first menu of the process presses Host by itself; with -CKSampleAutoJoin it presses Find, joins the
 * first session found and searches again while none is found. Each happens once per process: a player who comes back to
 * the menu chooses by hand.
 */
UCLASS()
class UCKSampleMainMenuWidget : public UCKSampleScreenWidget
{
	GENERATED_BODY()

public:
	/** Map that Host opens as a listen server. Set by the sessions bridge from its configuration. */
	void SetHostMap(const TSoftObjectPtr<UWorld>& InHostMap);

	virtual TOptional<FUIInputConfig> GetDesiredInputConfig() const override;

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual UWidget* NativeGetDesiredFocusTarget() const override;
	virtual void BuildDefaultTree() override;
	virtual void Refresh() override;

	UPROPERTY(BlueprintReadOnly, Category = "CoopKit Sample|Sessions", meta = (BindWidgetOptional))
	TObjectPtr<UCKSampleButtonWidget> HostButton;

	UPROPERTY(BlueprintReadOnly, Category = "CoopKit Sample|Sessions", meta = (BindWidgetOptional))
	TObjectPtr<UCKSampleButtonWidget> FindButton;

	/** What the menu is doing, or why the last request failed. */
	UPROPERTY(BlueprintReadOnly, Category = "CoopKit Sample|Sessions", meta = (BindWidgetOptional))
	TObjectPtr<UCommonTextBlock> StatusText;

	/** One button per found session, in the order of the search results. */
	UPROPERTY(BlueprintReadOnly, Category = "CoopKit Sample|Sessions", meta = (BindWidgetOptional))
	TObjectPtr<UVerticalBox> SessionList;

private:
	UCKSessionsSubsystem* GetSessions() const;
	void SetStatus(const FText& InText);
	void ShowResults(const TArray<FCKSessionsSearchResult>& Results);
	void JoinResult(int32 ResultIndex);

	UFUNCTION()
	void HandleHostClicked();

	UFUNCTION()
	void HandleFindClicked();

	UFUNCTION()
	void HandleHostComplete(bool bSuccess, ECKSessionsFailure Failure);

	UFUNCTION()
	void HandleFindComplete(bool bSuccess, const TArray<FCKSessionsSearchResult>& Results);

	UFUNCTION()
	void HandleJoinComplete(bool bSuccess, ECKSessionsFailure Failure);

	/** Runs the command line's auto start, if any, on the tick after construction (the bridge sets HostMap after it). */
	void RunAutoStart();

	/** Searches again for the auto join, or gives up after the last attempt. */
	void RetryAutoJoin();

	TSoftObjectPtr<UWorld> HostMap;

	/** Search attempts made by the auto join; 0 when this menu does not auto join. */
	int32 AutoJoinAttempts = 0;

	FTimerHandle AutoJoinTimer;
};
