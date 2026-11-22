// Copyright 2026 Ivan Iaitckikh. All Rights Reserved.

#pragma once

#include "CommonActivatableWidget.h"
#include "CoreMinimal.h"
#include "Engine/TimerHandle.h"
#include "CKSampleScreenWidget.generated.h"

class UCKSampleButtonWidget;
class UCommonTextBlock;
class UPanelWidget;
class UVerticalBox;

/**
 * Base of the sample's session screens (main menu, lobby): a CommonUI activatable widget that activates itself when it
 * is added to the viewport. The CommonUI action router applies the screen's input config while it is active; when the
 * screen goes away, it gives the game its input back (the router keeps the last config otherwise). The default widget
 * tree is built in C++ when the screen's Widget Blueprint has no tree of its own; a Blueprint designer may draw its own
 * tree, keeping the names of the bound widgets.
 */
UCLASS(Abstract)
class UCKSampleScreenWidget : public UCommonActivatableWidget
{
	GENERATED_BODY()

public:
	UCKSampleScreenWidget(const FObjectInitializer& ObjectInitializer);

	virtual bool Initialize() override;

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeOnDeactivated() override;

	/** Builds the screen's default tree; called once, only when the widget has no tree of its own. */
	virtual void BuildDefaultTree()
	{
	}

	/** Reads the screen's state; called on construct and then every RefreshIntervalSeconds while the screen is shown. */
	virtual void Refresh()
	{
	}

	/** Seconds between two Refresh calls; 0 turns the periodic refresh off. */
	float RefreshIntervalSeconds = 0.0f;

	/** Default tree: a full-screen dark frame with a centered column under a title. Returns the column. */
	UVerticalBox* BuildFrame(FName TitleName, const FText& Title);

	/** Default tree: a text line added to Parent. */
	UCommonTextBlock* AddText(UPanelWidget* Parent, FName Name, const FText& Text);

	/** Default tree: a button added to Parent. */
	UCKSampleButtonWidget* AddButton(UPanelWidget* Parent, FName Name, const FText& Label);

private:
	FTimerHandle RefreshTimer;
};
