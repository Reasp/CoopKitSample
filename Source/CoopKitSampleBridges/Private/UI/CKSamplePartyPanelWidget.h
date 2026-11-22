// Copyright 2026 Ivan Iaitckikh. All Rights Reserved.

#pragma once

#include "Blueprint/UserWidget.h"
#include "Components/Button.h"
#include "CoreMinimal.h"
#include "Engine/TimerHandle.h"
#include "CKSamplePartyPanelWidget.generated.h"

class UCKPartyMenuWidget;
class UTextBlock;

/** Names of the party panel's widgets: tests and Widget Blueprint children find them by these names. */
namespace CKSample::PartyPanel::WidgetNames
{
	/** The plugin's ready-made party menu (CoopKit Party UCKPartyMenuWidget); its own buttons keep the plugin's names. */
	inline constexpr const TCHAR* PartyMenu = TEXT("PartyMenu");
	inline constexpr const TCHAR* PlayButton = TEXT("PlayButton");
	inline constexpr const TCHAR* FindMatchButton = TEXT("FindMatchButton");
	inline constexpr const TCHAR* StatusText = TEXT("PanelStatusText");
}

/**
 * The party panel of the sample's menu screens (TASK-118, decision-94): the plugin's ready-made party menu (Create party, Find
 * parties, Join, members, Ready, Leave; decision-107) with two buttons of the sample under it.
 * - Play (the party leader): takes the whole party into the sample's lobby (CKSamplePartySessionsBridge).
 * - Find match (the party leader, or a player with no party): queues for a match, with a group ticket for the party
 *   (CKSamplePartyMatchmakingBridge).
 * The panel only calls the two bridges; the party and its menu are the plugin's. It sits in a corner of the screen and takes
 * clicks on itself only; the CommonUI and the plain UMG main menus show under it. A member of a party sees what its leader does.
 */
UCLASS()
class UCKSamplePartyPanelWidget final : public UUserWidget
{
	GENERATED_BODY()

public:
	virtual bool Initialize() override;

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

private:
	void BuildTree();
	UButton* AddButton(class UPanelWidget* Parent, FName Name, const FText& Label);
	void Refresh();

	UFUNCTION()
	void HandlePlayClicked();

	UFUNCTION()
	void HandleFindMatchClicked();

	UPROPERTY(Transient)
	TObjectPtr<UCKPartyMenuWidget> Menu;

	UPROPERTY(Transient)
	TObjectPtr<UButton> PlayButton;

	UPROPERTY(Transient)
	TObjectPtr<UButton> FindMatchButton;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> StatusText;

	FTimerHandle RefreshTimer;
};
