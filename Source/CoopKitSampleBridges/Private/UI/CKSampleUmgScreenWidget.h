// Copyright 2026 Ivan Iaitckikh. All Rights Reserved.

#pragma once

#include "Blueprint/UserWidget.h"
#include "Components/Button.h"
#include "CoreMinimal.h"
#include "Engine/TimerHandle.h"
#include "CKSampleUmgScreenWidget.generated.h"

class UPanelWidget;
class UTextBlock;
class UVerticalBox;

/**
 * Base of the plain UMG copies of the sample's session screens (main menu, lobby; TASK-31, decision-47). The same
 * screens exist on CommonUI (UCKSampleScreenWidget); these use only the widgets of the UMG module (User Widget, Button,
 * Text Block, Vertical Box), for a project without CommonUI. The default widget tree is built in C++ when the screen's
 * Widget Blueprint has no tree of its own, with the same widget names as the CommonUI screens.
 *
 * Without the CommonUI action router the screen sets the player controller's input mode itself when it is shown and gives
 * the game its input back (game only, no mouse cursor) when it goes away.
 */
UCLASS(Abstract)
class UCKSampleUmgScreenWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	virtual bool Initialize() override;

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	/** Builds the screen's default tree; called once, only when the widget has no tree of its own. */
	virtual void BuildDefaultTree()
	{
	}

	/** Reads the screen's state; called on construct and then every RefreshIntervalSeconds while the screen is shown. */
	virtual void Refresh()
	{
	}

	/** Sets the input mode of the owning player controller while the screen is shown. */
	virtual void ApplyInputMode(APlayerController& Controller)
	{
	}

	/** The widget that gets keyboard and gamepad focus when the screen opens; null for none. */
	virtual UWidget* GetFocusWidget()
	{
		return nullptr;
	}

	/** Seconds between two Refresh calls; 0 turns the periodic refresh off. */
	float RefreshIntervalSeconds = 0.0f;

	/**
	 * True: the screen gives the game its input back (game only, no cursor) when it goes away. False for a screen shown
	 * over another screen (the draft over the lobby), which keeps the input mode of the screen below.
	 */
	bool bRestoreGameInput = true;

	/** Default tree: a full-screen dark frame with a centered column under a title. Returns the column. */
	UVerticalBox* BuildFrame(FName TitleName, const FText& Title);

	/** Default tree: a text line added to Parent. */
	UTextBlock* AddText(UPanelWidget* Parent, FName Name, const FText& Text);

	/** Default tree: a button with a text label added to Parent. */
	UButton* AddButton(UPanelWidget* Parent, FName Name, const FText& Label);

	/** Sets the label of a button made by AddButton (or of any button whose first child is a text block). */
	static void SetButtonLabel(const UButton* Button, const FText& Label);

private:
	FTimerHandle RefreshTimer;
};

DECLARE_DELEGATE_OneParam(FCKSampleUmgRowClicked, int32 /*RowIndex*/);

/**
 * One row of a list of choices (the found sessions): a button that tells its screen which row was clicked. A dynamic
 * On Clicked delegate carries no sender, so each row is its own class with its own index.
 */
UCLASS()
class UCKSampleUmgRowButton final : public UButton
{
	GENERATED_BODY()

public:
	/** Binds the row to its index and to the owner's handler; call once after the row was constructed. */
	void Setup(int32 InRowIndex, const FCKSampleUmgRowClicked& InOnRowClicked);

private:
	UFUNCTION()
	void HandleClicked();

	int32 RowIndex = INDEX_NONE;
	FCKSampleUmgRowClicked OnRowClicked;
};
