// Copyright 2026 Ivan Iaitckikh. All Rights Reserved.

#pragma once

#include "CKSessionsTypes.h"
#include "CoreMinimal.h"
#include "Engine/TimerHandle.h"
#include "UI/CKSampleUmgScreenWidget.h"
#include "CKSampleUmgMainMenuWidget.generated.h"

class UCKSessionsSubsystem;
class UTextBlock;
class UVerticalBox;
class UWorld;

/**
 * Main menu of the sample project on plain UMG: the same screen, logic and widget names as the CommonUI menu
 * (UCKSampleMainMenuWidget), on CoopKit Sessions' Blueprint API only (TASK-31, decision-47):
 * - Host: Host Session with the default profile on HostMap (the sample's lobby); the game reopens it as a listen server.
 * - Find: Find Sessions, then one button per joinable session (host, players, ping, "in match"); a click joins it.
 * - A status line shows what the subsystem does and why a request failed.
 * The command line's auto start (-CKSampleAutoHost, -CKSampleAutoJoin) works as on the CommonUI menu.
 */
UCLASS()
class UCKSampleUmgMainMenuWidget : public UCKSampleUmgScreenWidget
{
	GENERATED_BODY()

public:
	/** Map that Host opens as a listen server. Set by the sessions bridge from its configuration. */
	void SetHostMap(const TSoftObjectPtr<UWorld>& InHostMap);

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void BuildDefaultTree() override;
	virtual void Refresh() override;
	virtual void ApplyInputMode(APlayerController& Controller) override;
	virtual UWidget* GetFocusWidget() override;

	UPROPERTY(BlueprintReadOnly, Category = "CoopKit Sample|Sessions", meta = (BindWidgetOptional))
	TObjectPtr<UButton> HostButton;

	UPROPERTY(BlueprintReadOnly, Category = "CoopKit Sample|Sessions", meta = (BindWidgetOptional))
	TObjectPtr<UButton> FindButton;

	/** What the menu is doing, or why the last request failed. */
	UPROPERTY(BlueprintReadOnly, Category = "CoopKit Sample|Sessions", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> StatusText;

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
