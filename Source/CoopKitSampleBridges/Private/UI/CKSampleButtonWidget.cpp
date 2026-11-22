// Copyright 2026 Ivan Iaitckikh. All Rights Reserved.

#include "UI/CKSampleButtonWidget.h"
#include "Blueprint/WidgetTree.h"
#include "CommonTextBlock.h"
#include "Components/Border.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(CKSampleButtonWidget)

bool UCKSampleButtonWidget::Initialize()
{
	// UCommonButtonBase wraps the tree that exists when it initializes into its internal button and binds the click to
	// it; with no tree, the button would have nothing to click. So the label is built before the parent initializes.
	if (WidgetTree == nullptr && !HasAnyFlags(RF_ClassDefaultObject))
	{
		WidgetTree = NewObject<UWidgetTree>(this, TEXT("WidgetTree"), RF_Transient);
		UBorder* LabelFrame = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("LabelFrame"));
		LabelFrame->SetBrushColor(FLinearColor::Transparent);
		LabelFrame->SetPadding(FMargin(16.0f, 6.0f));
		Label = WidgetTree->ConstructWidget<UCommonTextBlock>(UCommonTextBlock::StaticClass(), TEXT("Label"));
		Label->SetColorAndOpacity(FSlateColor(FLinearColor::Black));
		LabelFrame->SetContent(Label);
		WidgetTree->RootWidget = LabelFrame;
	}
	return Super::Initialize();
}

void UCKSampleButtonWidget::SetLabel(const FText& InText)
{
	if (Label != nullptr)
	{
		Label->SetText(InText);
	}
}

FText UCKSampleButtonWidget::GetLabel() const
{
	return Label != nullptr ? Label->GetText() : FText::GetEmpty();
}
