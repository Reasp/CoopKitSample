// Copyright 2026 Ivan Iaitckikh. All Rights Reserved.

#include "UI/CKSamplePartyPanelWidget.h"
#include "Blueprint/WidgetTree.h"
#include "CKPartyMenuWidget.h"
#include "CKPartySubsystem.h"
#include "CKSamplePartyMatchmakingBridge.h"
#include "CKSamplePartySessionsBridge.h"
#include "Components/Border.h"
#include "Components/ButtonSlot.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Styling/CoreStyle.h"
#include "TimerManager.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(CKSamplePartyPanelWidget)

#define LOCTEXT_NAMESPACE "CKSamplePartyPanel"

namespace CKSample::PartyPanel
{
	namespace
	{
		const FLinearColor PanelColor(0.02f, 0.03f, 0.05f, 0.85f);
		const FLinearColor TitleColor(0.45f, 0.75f, 1.0f);
		const FLinearColor StatusColor(1.0f, 0.85f, 0.4f);

		/** Width of the panel in slate units: room for the party's rows while the middle of the window stays free. */
		constexpr float PanelWidth = 420.0f;
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

bool UCKSamplePartyPanelWidget::Initialize()
{
	const bool bSuperInitialized = Super::Initialize();
	if (WidgetTree != nullptr && WidgetTree->RootWidget == nullptr)
	{
		BuildTree();
	}
	// The panel takes clicks; the rest of the screen stays with the menu under it.
	SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	return bSuperInitialized;
}

void UCKSamplePartyPanelWidget::BuildTree()
{
	using namespace CKSample::PartyPanel;

	UBorder* Root = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("Root"));
	Root->SetBrushColor(FLinearColor::Transparent);
	Root->SetPadding(FMargin(24.0f));
	Root->SetHorizontalAlignment(HAlign_Left);
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

	UVerticalBox* Column = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("Column"));
	Panel->SetContent(Column);

	UTextBlock* Title = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("TitleText"));
	Title->SetText(LOCTEXT("Title", "Party"));
	Title->SetFont(FCoreStyle::GetDefaultFontStyle("Bold", 24));
	Title->SetColorAndOpacity(FSlateColor(TitleColor));
	Column->AddChild(Title);

	// The plugin's ready-made menu: what a game puts on its own menu screen.
	Menu = WidgetTree->ConstructWidget<UCKPartyMenuWidget>(UCKPartyMenuWidget::StaticClass(), WidgetNames::PartyMenu);
	Menu->SetShowPlayerNumbers(true);
	if (UVerticalBoxSlot* MenuSlot = Cast<UVerticalBoxSlot>(Column->AddChild(Menu)))
	{
		MenuSlot->SetPadding(FMargin(0.0f, 6.0f, 0.0f, 6.0f));
		MenuSlot->SetHorizontalAlignment(HAlign_Fill);
	}

	PlayButton = AddButton(Column, WidgetNames::PlayButton, LOCTEXT("Play", "Play with the party"));
	PlayButton->OnClicked.AddUniqueDynamic(this, &UCKSamplePartyPanelWidget::HandlePlayClicked);
	FindMatchButton = AddButton(Column, WidgetNames::FindMatchButton, LOCTEXT("FindMatch", "Find match"));
	FindMatchButton->OnClicked.AddUniqueDynamic(this, &UCKSamplePartyPanelWidget::HandleFindMatchClicked);

	StatusText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), WidgetNames::StatusText);
	StatusText->SetFont(FCoreStyle::GetDefaultFontStyle("Regular", 16));
	StatusText->SetColorAndOpacity(FSlateColor(StatusColor));
	StatusText->SetAutoWrapText(true);
	Column->AddChild(StatusText);
}

UButton* UCKSamplePartyPanelWidget::AddButton(UPanelWidget* Parent, const FName Name, const FText& Label)
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
		ButtonSlot->SetPadding(FMargin(0.0f, CKSample::PartyPanel::RowSpacing));
		ButtonSlot->SetHorizontalAlignment(HAlign_Fill);
	}
	return Button;
}

void UCKSamplePartyPanelWidget::NativeConstruct()
{
	Super::NativeConstruct();
	Refresh();
	if (UWorld* World = GetWorld())
	{
		// A world timer, not NativeTick: a widget ticks only while Slate paints it.
		World->GetTimerManager().SetTimer(RefreshTimer, FTimerDelegate::CreateUObject(this, &UCKSamplePartyPanelWidget::Refresh),
			CKSample::PartyPanel::RefreshIntervalSeconds, true);
	}
}

void UCKSamplePartyPanelWidget::NativeDestruct()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(RefreshTimer);
	}
	Super::NativeDestruct();
}

void UCKSamplePartyPanelWidget::Refresh()
{
	using namespace CKSample::PartyPanel;

	const UGameInstance* GameInstance = GetGameInstance();
	const UCKPartySubsystem* Party = GameInstance != nullptr ? GameInstance->GetSubsystem<UCKPartySubsystem>() : nullptr;
	const UCKSamplePartyMatchmakingBridge* Matchmaking = GameInstance != nullptr ? GameInstance->GetSubsystem<UCKSamplePartyMatchmakingBridge>() : nullptr;
	const bool bInParty = Party != nullptr && Party->IsInParty();
	const bool bLeader = Party != nullptr && Party->IsLeader();
	const bool bIdleGame = Party != nullptr && Party->GetGamePhase() == ECKPartyGamePhase::None && !Party->IsPassive();
	const bool bMatchmaking = Matchmaking != nullptr && Matchmaking->IsEnabled();

	Show(PlayButton, bInParty);
	if (PlayButton != nullptr)
	{
		PlayButton->SetIsEnabled(bLeader && bIdleGame);
	}
	Show(FindMatchButton, bMatchmaking);
	if (FindMatchButton != nullptr)
	{
		FindMatchButton->SetIsEnabled(bMatchmaking && Matchmaking->CanFindMatch());
	}

	FText Status;
	if (Party != nullptr && Party->GetGamePhase() != ECKPartyGamePhase::None)
	{
		Status = LOCTEXT("GoingIn", "The party is going into the game...");
	}
	else if (bMatchmaking && Matchmaking->IsBusy())
	{
		Status = LOCTEXT("Searching", "Looking for a match...");
	}
	else if (Party != nullptr && Party->GetLastGameFailure() != ECKPartyGameFailure::None)
	{
		Status = StaticEnum<ECKPartyGameFailure>()->GetDisplayNameTextByValue(static_cast<int64>(Party->GetLastGameFailure()));
	}
	else if (bInParty && !bLeader)
	{
		Status = LOCTEXT("MemberWaits", "Your leader takes the party into the game.");
	}
	if (StatusText != nullptr)
	{
		StatusText->SetText(Status);
		StatusText->SetVisibility(Status.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
	}
}

void UCKSamplePartyPanelWidget::HandlePlayClicked()
{
	const UGameInstance* GameInstance = GetGameInstance();
	if (UCKSamplePartySessionsBridge* Bridge = GameInstance != nullptr ? GameInstance->GetSubsystem<UCKSamplePartySessionsBridge>() : nullptr)
	{
		Bridge->PlayWithParty();
	}
	Refresh();
}

void UCKSamplePartyPanelWidget::HandleFindMatchClicked()
{
	const UGameInstance* GameInstance = GetGameInstance();
	if (UCKSamplePartyMatchmakingBridge* Bridge = GameInstance != nullptr ? GameInstance->GetSubsystem<UCKSamplePartyMatchmakingBridge>() : nullptr)
	{
		Bridge->FindMatch();
	}
	Refresh();
}

#undef LOCTEXT_NAMESPACE
