// Copyright 2026 Ivan Iaitckikh. All Rights Reserved.

#include "UI/CKSampleScreenWidget.h"
#include "Blueprint/WidgetTree.h"
#include "CommonTextBlock.h"
#include "Components/Border.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Input/CommonUIActionRouterBase.h"
#include "Input/UIActionBindingHandle.h"
#include "TimerManager.h"
#include "UI/CKSampleButtonWidget.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(CKSampleScreenWidget)

namespace CKSample::SessionsUi
{
	namespace
	{
		const FLinearColor FrameColor(0.02f, 0.02f, 0.03f, 0.85f);
		constexpr float ColumnSpacing = 8.0f;
	}
}

UCKSampleScreenWidget::UCKSampleScreenWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bAutoActivate = true;
}

bool UCKSampleScreenWidget::Initialize()
{
	const bool bSuperInitialized = Super::Initialize();
	if (WidgetTree != nullptr && WidgetTree->RootWidget == nullptr)
	{
		BuildDefaultTree();
	}
	return bSuperInitialized;
}

void UCKSampleScreenWidget::NativeConstruct()
{
	Super::NativeConstruct();
	Refresh();
	UWorld* World = GetWorld();
	if (World != nullptr && RefreshIntervalSeconds > 0.0f)
	{
		// A world timer, not NativeTick: a widget ticks only while Slate paints it, which a server without rendering never does.
		World->GetTimerManager().SetTimer(RefreshTimer, FTimerDelegate::CreateUObject(this, &UCKSampleScreenWidget::Refresh),
			RefreshIntervalSeconds, true);
	}
}

void UCKSampleScreenWidget::NativeDestruct()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(RefreshTimer);
	}
	Super::NativeDestruct();
}

void UCKSampleScreenWidget::NativeOnDeactivated()
{
	Super::NativeOnDeactivated();
	// The action router keeps the input config of the last deactivated widget; without this, the match would keep the
	// menu's input mode and mouse cursor. A screen deactivated while its world tears down (a client leaving the lobby)
	// has a player controller without player input: there is no input mode to restore, and from UE 5.8 the router
	// forwards the mode to Enhanced Input, which logs an error for a missing player input.
	const APlayerController* Controller = GetOwningPlayer();
	if (Controller == nullptr || Controller->PlayerInput == nullptr)
	{
		return;
	}
	if (UCommonUIActionRouterBase* Router = UCommonUIActionRouterBase::Get(*this))
	{
		Router->SetActiveUIInputConfig(FUIInputConfig(ECommonInputMode::Game, EMouseCaptureMode::CapturePermanently), this);
	}
}

UVerticalBox* UCKSampleScreenWidget::BuildFrame(const FName TitleName, const FText& Title)
{
	UBorder* Frame = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("Frame"));
	Frame->SetBrushColor(CKSample::SessionsUi::FrameColor);
	Frame->SetHorizontalAlignment(HAlign_Center);
	Frame->SetVerticalAlignment(VAlign_Center);
	WidgetTree->RootWidget = Frame;

	UVerticalBox* Column = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("Column"));
	Frame->SetContent(Column);
	AddText(Column, TitleName, Title);
	return Column;
}

UCommonTextBlock* UCKSampleScreenWidget::AddText(UPanelWidget* Parent, const FName Name, const FText& Text)
{
	UCommonTextBlock* TextBlock = WidgetTree->ConstructWidget<UCommonTextBlock>(UCommonTextBlock::StaticClass(), Name);
	TextBlock->SetText(Text);
	if (UVerticalBoxSlot* BoxSlot = Cast<UVerticalBoxSlot>(Parent->AddChild(TextBlock)))
	{
		BoxSlot->SetPadding(FMargin(0.0f, CKSample::SessionsUi::ColumnSpacing * 0.5f));
	}
	return TextBlock;
}

UCKSampleButtonWidget* UCKSampleScreenWidget::AddButton(UPanelWidget* Parent, const FName Name, const FText& Label)
{
	UCKSampleButtonWidget* Button = WidgetTree->ConstructWidget<UCKSampleButtonWidget>(UCKSampleButtonWidget::StaticClass(), Name);
	Button->SetLabel(Label);
	if (UVerticalBoxSlot* BoxSlot = Cast<UVerticalBoxSlot>(Parent->AddChild(Button)))
	{
		BoxSlot->SetPadding(FMargin(0.0f, CKSample::SessionsUi::ColumnSpacing * 0.5f));
		BoxSlot->SetHorizontalAlignment(HAlign_Fill);
	}
	return Button;
}
