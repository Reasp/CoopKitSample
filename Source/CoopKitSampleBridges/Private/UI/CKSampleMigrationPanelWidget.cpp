// Copyright 2026 Ivan Iaitckikh. All Rights Reserved.

#include "UI/CKSampleMigrationPanelWidget.h"
#include "Blueprint/WidgetTree.h"
#include "CKHostMigrationSubsystem.h"
#include "CKSampleHostMigrationBridge.h"
#include "CKSessionsSubsystem.h"
#include "Components/Border.h"
#include "Components/ButtonSlot.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/InputComponent.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "InputCoreTypes.h"
#include "Styling/CoreStyle.h"
#include "TimerManager.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(CKSampleMigrationPanelWidget)

#define LOCTEXT_NAMESPACE "CKSampleMigrationPanel"

namespace CKSample::MigrationPanel
{
	namespace
	{
		const FLinearColor PanelColor(0.02f, 0.03f, 0.05f, 0.85f);
		const FLinearColor TitleColor(0.55f, 0.9f, 0.55f);
		const FLinearColor StateColor(1.0f, 0.85f, 0.4f);

		/** Width of the panel in slate units. */
		constexpr float PanelWidth = 360.0f;
		constexpr float RowSpacing = 3.0f;
		constexpr float RefreshIntervalSeconds = 0.2f;

		void Show(UWidget* Widget, const bool bShow)
		{
			if (Widget != nullptr)
			{
				Widget->SetVisibility(bShow ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
			}
		}
	}
}

bool UCKSampleMigrationPanelWidget::Initialize()
{
	const bool bSuperInitialized = Super::Initialize();
	if (WidgetTree != nullptr && WidgetTree->RootWidget == nullptr)
	{
		BuildTree();
	}
	// The panel takes clicks; the rest of the screen stays with the screen under it.
	SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	return bSuperInitialized;
}

void UCKSampleMigrationPanelWidget::SetMenuMode(const bool bInMenu)
{
	bMenuMode = bInMenu;
	Refresh();
}

void UCKSampleMigrationPanelWidget::BuildTree()
{
	using namespace CKSample::MigrationPanel;

	UBorder* Root = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("Root"));
	Root->SetBrushColor(FLinearColor::Transparent);
	Root->SetPadding(FMargin(24.0f));
	Root->SetHorizontalAlignment(HAlign_Right);
	Root->SetVerticalAlignment(VAlign_Top);
	Root->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	WidgetTree->RootWidget = Root;

	USizeBox* Size = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("PanelSize"));
	Size->SetWidthOverride(PanelWidth);
	Root->SetContent(Size);

	UBorder* Panel = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("Panel"));
	Panel->SetBrushColor(PanelColor);
	Panel->SetPadding(FMargin(16.0f, 12.0f));
	Size->SetContent(Panel);
	Frame = Panel;

	UVerticalBox* Column = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("Column"));
	Panel->SetContent(Column);

	UTextBlock* Title = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("TitleText"));
	Title->SetText(LOCTEXT("Title", "Host migration"));
	Title->SetFont(FCoreStyle::GetDefaultFontStyle("Bold", 24));
	Title->SetColorAndOpacity(FSlateColor(TitleColor));
	Column->AddChild(Title);

	StateText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), WidgetNames::StateText);
	StateText->SetFont(FCoreStyle::GetDefaultFontStyle("Regular", 16));
	StateText->SetColorAndOpacity(FSlateColor(StateColor));
	StateText->SetAutoWrapText(true);
	if (UVerticalBoxSlot* TextSlot = Cast<UVerticalBoxSlot>(Column->AddChild(StateText)))
	{
		TextSlot->SetPadding(FMargin(0.0f, 6.0f, 0.0f, 6.0f));
	}

	CursorHint = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), WidgetNames::CursorHint);
	CursorHint->SetFont(FCoreStyle::GetDefaultFontStyle("Regular", 14));
	CursorHint->SetColorAndOpacity(FSlateColor(FLinearColor(0.75f, 0.75f, 0.75f)));
	CursorHint->SetAutoWrapText(true);
	if (UVerticalBoxSlot* HintSlot = Cast<UVerticalBoxSlot>(Column->AddChild(CursorHint)))
	{
		HintSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 6.0f));
	}

	HostButton = AddButton(Column, WidgetNames::HostButton, LOCTEXT("Host", "Host a migration game"));
	HostButton->OnClicked.AddUniqueDynamic(this, &UCKSampleMigrationPanelWidget::HandleHostClicked);
	FindButton = AddButton(Column, WidgetNames::FindButton, LOCTEXT("Find", "Find a migration game"));
	FindButton->OnClicked.AddUniqueDynamic(this, &UCKSampleMigrationPanelWidget::HandleFindClicked);
	RejoinButton = AddButton(Column, WidgetNames::RejoinButton, LOCTEXT("Rejoin", "Rejoin the game I left"));
	RejoinButton->OnClicked.AddUniqueDynamic(this, &UCKSampleMigrationPanelWidget::HandleRejoinClicked);
	LeaveButton = AddButton(Column, WidgetNames::LeaveButton, LOCTEXT("Leave", "Leave the game"));
	LeaveButton->OnClicked.AddUniqueDynamic(this, &UCKSampleMigrationPanelWidget::HandleLeaveClicked);
}

UButton* UCKSampleMigrationPanelWidget::AddButton(UPanelWidget* Parent, const FName Name, const FText& Label)
{
	UButton* Button = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), Name);
	UTextBlock* LabelText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), *(Name.ToString() + TEXT("_Label")));
	LabelText->SetText(Label);
	LabelText->SetFont(FCoreStyle::GetDefaultFontStyle("Regular", 18));
	LabelText->SetColorAndOpacity(FSlateColor(FLinearColor::Black));
	LabelText->SetJustification(ETextJustify::Center);
	if (UButtonSlot* LabelSlot = Cast<UButtonSlot>(Button->SetContent(LabelText)))
	{
		LabelSlot->SetHorizontalAlignment(HAlign_Fill);
	}
	if (UVerticalBoxSlot* ButtonSlot = Cast<UVerticalBoxSlot>(Parent->AddChild(Button)))
	{
		ButtonSlot->SetPadding(FMargin(0.0f, CKSample::MigrationPanel::RowSpacing));
		ButtonSlot->SetHorizontalAlignment(HAlign_Fill);
	}
	return Button;
}

void UCKSampleMigrationPanelWidget::NativeConstruct()
{
	Super::NativeConstruct();
	// A match shows no cursor: the middle mouse button is the mouse's own way to get one (TASK-135). It does not consume the key.
	if (APlayerController* Controller = GetOwningPlayer(); Controller != nullptr && Controller->IsLocalController() && CursorInput == nullptr)
	{
		CursorInput = NewObject<UInputComponent>(Controller, NAME_None, RF_Transient);
		CursorInput->bBlockInput = false;
		CursorInput->BindKey(EKeys::MiddleMouseButton, IE_Pressed, this, &UCKSampleMigrationPanelWidget::HandleCursorKey).bConsumeInput = false;
		Controller->PushInputComponent(CursorInput);
	}
	Refresh();
	if (UWorld* World = GetWorld())
	{
		// A world timer, not NativeTick: a widget ticks only while Slate paints it.
		World->GetTimerManager().SetTimer(RefreshTimer, FTimerDelegate::CreateUObject(this, &UCKSampleMigrationPanelWidget::Refresh),
			CKSample::MigrationPanel::RefreshIntervalSeconds, true);
	}
}

void UCKSampleMigrationPanelWidget::NativeDestruct()
{
	SetMatchCursor(false);
	if (CursorInput != nullptr)
	{
		if (APlayerController* Controller = GetOwningPlayer())
		{
			Controller->PopInputComponent(CursorInput);
		}
		CursorInput = nullptr;
	}
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(RefreshTimer);
	}
	Super::NativeDestruct();
}

bool UCKSampleMigrationPanelWidget::IsInGroup() const
{
	const UGameInstance* GameInstance = GetGameInstance();
	const UCKHostMigrationSubsystem* Migration = GameInstance != nullptr ? GameInstance->GetSubsystem<UCKHostMigrationSubsystem>() : nullptr;
	const ECKHostMigrationState State = Migration != nullptr ? Migration->GetState() : ECKHostMigrationState::Idle;
	return State == ECKHostMigrationState::Host || State == ECKHostMigrationState::Client;
}

void UCKSampleMigrationPanelWidget::HandleCursorKey()
{
	if (!bMenuMode && IsInGroup())
	{
		SetMatchCursor(!bMatchCursor);
		Refresh();
	}
}

void UCKSampleMigrationPanelWidget::SetMatchCursor(const bool bShow)
{
	APlayerController* Controller = GetOwningPlayer();
	if (Controller == nullptr || bShow == bMatchCursor)
	{
		return;
	}
	bMatchCursor = bShow;
	if (bShow)
	{
		// The mouse stays free over the game: the panel takes clicks, the camera waits.
		FInputModeGameAndUI InputMode;
		InputMode.SetHideCursorDuringCapture(false);
		InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
		Controller->SetInputMode(InputMode);
		Controller->SetShowMouseCursor(true);
		Controller->SetIgnoreLookInput(true);
	}
	else
	{
		Controller->SetIgnoreLookInput(false);
		Controller->SetShowMouseCursor(false);
		Controller->SetInputMode(FInputModeGameOnly());
	}
}

FText UCKSampleMigrationPanelWidget::DescribeState() const
{
	const UGameInstance* GameInstance = GetGameInstance();
	const UCKHostMigrationSubsystem* Migration = GameInstance != nullptr ? GameInstance->GetSubsystem<UCKHostMigrationSubsystem>() : nullptr;
	const UCKSampleHostMigrationBridge* Bridge = GameInstance != nullptr ? GameInstance->GetSubsystem<UCKSampleHostMigrationBridge>() : nullptr;
	if (Migration == nullptr)
	{
		return FText::GetEmpty();
	}
	const int32 Epoch = Migration->GetEpoch();
	switch (Migration->GetState())
	{
	case ECKHostMigrationState::Hosting:
		return LOCTEXT("Hosting", "Starting the migration game...");
	case ECKHostMigrationState::Joining:
		return LOCTEXT("Joining", "Joining the migration game...");
	case ECKHostMigrationState::Host:
		return Epoch == 0
			? LOCTEXT("HostFirst", "You are the host. If you leave, the game moves to the next player.")
			: FText::Format(LOCTEXT("HostNew", "You are the new host (migration {0}). The game goes on from the last snapshot."), Epoch);
	case ECKHostMigrationState::Client:
		{
			const int32 Rank = Migration->GetSuccessorRank();
			const FText Joined = Epoch == 0 ? FText::GetEmpty() : FText::Format(LOCTEXT("JoinedNew", "Joined the new host (migration {0}). "), Epoch);
			if (Rank == INDEX_NONE)
			{
				return FText::Format(LOCTEXT("ClientNoRank", "{0}You are a player. Waiting for the host's first snapshot."), Joined);
			}
			return Rank == 0
				? FText::Format(LOCTEXT("ClientNext", "{0}Successor 1: you host the game next if the host leaves."), Joined)
				: FText::Format(LOCTEXT("ClientRanked", "{0}Successor {1}: you host the game if the successors before you cannot."), Joined, Rank + 1);
		}
	case ECKHostMigrationState::Migrating:
		return LOCTEXT("Migrating", "The host is gone. Migrating the game...");
	case ECKHostMigrationState::Rejoining:
		return LOCTEXT("Rejoining", "Looking for the game's new host...");
	case ECKHostMigrationState::Leaving:
		return LOCTEXT("Leaving", "Leaving the migration game...");
	default:
		break;
	}
	if (Bridge != nullptr && Bridge->IsSearching())
	{
		return LOCTEXT("Searching", "Looking for a migration game...");
	}
	if (Bridge != nullptr && Bridge->HasSearchTimedOut())
	{
		return LOCTEXT("NothingFound", "No migration game found. Host one, or search again.");
	}
	return Migration->GetGroupId().IsEmpty()
		? LOCTEXT("Welcome", "Host a game that survives its host leaving, or find one.")
		: LOCTEXT("Left", "You are out of the migration game. Rejoin it, or host or find another.");
}

void UCKSampleMigrationPanelWidget::Refresh()
{
	using namespace CKSample::MigrationPanel;

	const UGameInstance* GameInstance = GetGameInstance();
	const UCKHostMigrationSubsystem* Migration = GameInstance != nullptr ? GameInstance->GetSubsystem<UCKHostMigrationSubsystem>() : nullptr;
	const UCKSampleHostMigrationBridge* Bridge = GameInstance != nullptr ? GameInstance->GetSubsystem<UCKSampleHostMigrationBridge>() : nullptr;
	const UCKSessionsSubsystem* Sessions = GameInstance != nullptr ? GameInstance->GetSubsystem<UCKSessionsSubsystem>() : nullptr;
	if (Migration == nullptr || Bridge == nullptr)
	{
		Show(Frame, false);
		return;
	}

	const ECKHostMigrationState State = Migration->GetState();
	const bool bIdle = State == ECKHostMigrationState::Idle;
	const bool bPlaying = State == ECKHostMigrationState::Host || State == ECKHostMigrationState::Client;
	// A request starts only while the migration and CoopKit Sessions are both idle and no search of this panel is running.
	const bool bCanStart = bIdle && !Bridge->IsSearching() && Sessions != nullptr && Sessions->GetState() == ECKSessionsState::Idle;

	Show(Frame, bMenuMode || !bIdle);
	Show(HostButton, bMenuMode && bIdle);
	Show(FindButton, bMenuMode && bIdle);
	Show(RejoinButton, bMenuMode && bIdle && !Migration->GetGroupId().IsEmpty());
	Show(LeaveButton, bPlaying);
	// A match that took the group away from the player gives the mouse back to the game.
	if (bMatchCursor && (bMenuMode || !bPlaying))
	{
		SetMatchCursor(false);
	}
	Show(CursorHint, !bMenuMode && bPlaying);
	if (CursorHint != nullptr)
	{
		CursorHint->SetText(bMatchCursor ? LOCTEXT("HintHide", "Middle mouse button: hide the cursor and steer the camera.")
			: LOCTEXT("HintShow", "Middle mouse button: show the cursor to click here."));
	}
	for (UButton* Button : { HostButton.Get(), FindButton.Get(), RejoinButton.Get() })
	{
		if (Button != nullptr)
		{
			Button->SetIsEnabled(bCanStart);
		}
	}
	if (StateText != nullptr)
	{
		StateText->SetText(DescribeState());
	}
}

void UCKSampleMigrationPanelWidget::HandleHostClicked()
{
	const UGameInstance* GameInstance = GetGameInstance();
	if (UCKSampleHostMigrationBridge* Bridge = GameInstance != nullptr ? GameInstance->GetSubsystem<UCKSampleHostMigrationBridge>() : nullptr)
	{
		Bridge->HostGroup(FString());
	}
	Refresh();
}

void UCKSampleMigrationPanelWidget::HandleFindClicked()
{
	const UGameInstance* GameInstance = GetGameInstance();
	if (UCKSampleHostMigrationBridge* Bridge = GameInstance != nullptr ? GameInstance->GetSubsystem<UCKSampleHostMigrationBridge>() : nullptr)
	{
		Bridge->JoinGroup();
	}
	Refresh();
}

void UCKSampleMigrationPanelWidget::HandleRejoinClicked()
{
	const UGameInstance* GameInstance = GetGameInstance();
	if (UCKHostMigrationSubsystem* Migration = GameInstance != nullptr ? GameInstance->GetSubsystem<UCKHostMigrationSubsystem>() : nullptr)
	{
		Migration->RejoinGroup();
	}
	Refresh();
}

void UCKSampleMigrationPanelWidget::HandleLeaveClicked()
{
	SetMatchCursor(false);
	const UGameInstance* GameInstance = GetGameInstance();
	if (UCKHostMigrationSubsystem* Migration = GameInstance != nullptr ? GameInstance->GetSubsystem<UCKHostMigrationSubsystem>() : nullptr)
	{
		Migration->LeaveSession();
	}
	Refresh();
}

#undef LOCTEXT_NAMESPACE
