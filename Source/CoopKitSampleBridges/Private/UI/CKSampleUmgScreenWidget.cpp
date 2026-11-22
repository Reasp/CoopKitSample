// Copyright 2026 Ivan Iaitckikh. All Rights Reserved.

#include "UI/CKSampleUmgScreenWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "TimerManager.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(CKSampleUmgScreenWidget)

namespace CKSample::SessionsUmg
{
	namespace
	{
		const FLinearColor FrameColor(0.02f, 0.02f, 0.03f, 0.85f);
		constexpr float ColumnSpacing = 8.0f;
	}
}

bool UCKSampleUmgScreenWidget::Initialize()
{
	const bool bSuperInitialized = Super::Initialize();
	if (WidgetTree != nullptr && WidgetTree->RootWidget == nullptr)
	{
		BuildDefaultTree();
	}
	return bSuperInitialized;
}

void UCKSampleUmgScreenWidget::NativeConstruct()
{
	Super::NativeConstruct();

	APlayerController* Controller = GetOwningPlayer();
	if (Controller != nullptr && Controller->IsLocalController())
	{
		ApplyInputMode(*Controller);
		Controller->SetShowMouseCursor(true);
	}

	Refresh();
	UWorld* World = GetWorld();
	if (World != nullptr && RefreshIntervalSeconds > 0.0f)
	{
		// A world timer, not NativeTick: a widget ticks only while Slate paints it, which a server without rendering never does.
		World->GetTimerManager().SetTimer(RefreshTimer, FTimerDelegate::CreateUObject(this, &UCKSampleUmgScreenWidget::Refresh),
			RefreshIntervalSeconds, true);
	}
}

void UCKSampleUmgScreenWidget::NativeDestruct()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(RefreshTimer);
	}

	// Give the game its input back. A screen destroyed while its world tears down (a client leaving the lobby) has a
	// player controller without player input: there is no input mode to restore.
	APlayerController* Controller = GetOwningPlayer();
	if (bRestoreGameInput && Controller != nullptr && Controller->IsLocalController() && Controller->PlayerInput != nullptr)
	{
		Controller->SetInputMode(FInputModeGameOnly());
		Controller->SetShowMouseCursor(false);
	}
	Super::NativeDestruct();
}

UVerticalBox* UCKSampleUmgScreenWidget::BuildFrame(const FName TitleName, const FText& Title)
{
	UBorder* Frame = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("Frame"));
	Frame->SetBrushColor(CKSample::SessionsUmg::FrameColor);
	Frame->SetHorizontalAlignment(HAlign_Center);
	Frame->SetVerticalAlignment(VAlign_Center);
	WidgetTree->RootWidget = Frame;

	UVerticalBox* Column = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("Column"));
	Frame->SetContent(Column);
	AddText(Column, TitleName, Title);
	return Column;
}

UTextBlock* UCKSampleUmgScreenWidget::AddText(UPanelWidget* Parent, const FName Name, const FText& Text)
{
	UTextBlock* TextBlock = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), Name);
	TextBlock->SetText(Text);
	if (UVerticalBoxSlot* BoxSlot = Cast<UVerticalBoxSlot>(Parent->AddChild(TextBlock)))
	{
		BoxSlot->SetPadding(FMargin(0.0f, CKSample::SessionsUmg::ColumnSpacing * 0.5f));
	}
	return TextBlock;
}

UButton* UCKSampleUmgScreenWidget::AddButton(UPanelWidget* Parent, const FName Name, const FText& Label)
{
	UButton* Button = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), Name);
	UTextBlock* LabelText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), *(Name.ToString() + TEXT("_Label")));
	LabelText->SetText(Label);
	LabelText->SetColorAndOpacity(FSlateColor(FLinearColor::Black));
	Button->SetContent(LabelText);
	if (UVerticalBoxSlot* BoxSlot = Cast<UVerticalBoxSlot>(Parent->AddChild(Button)))
	{
		BoxSlot->SetPadding(FMargin(0.0f, CKSample::SessionsUmg::ColumnSpacing * 0.5f));
		BoxSlot->SetHorizontalAlignment(HAlign_Fill);
	}
	return Button;
}

void UCKSampleUmgScreenWidget::SetButtonLabel(const UButton* Button, const FText& Label)
{
	if (Button == nullptr)
	{
		return;
	}
	if (UTextBlock* LabelText = Cast<UTextBlock>(Button->GetContent()))
	{
		LabelText->SetText(Label);
	}
}

void UCKSampleUmgRowButton::Setup(const int32 InRowIndex, const FCKSampleUmgRowClicked& InOnRowClicked)
{
	RowIndex = InRowIndex;
	OnRowClicked = InOnRowClicked;
	OnClicked.AddDynamic(this, &UCKSampleUmgRowButton::HandleClicked);
}

void UCKSampleUmgRowButton::HandleClicked()
{
	OnRowClicked.ExecuteIfBound(RowIndex);
}
