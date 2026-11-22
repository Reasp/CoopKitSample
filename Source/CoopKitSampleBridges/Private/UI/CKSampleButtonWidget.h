// Copyright 2026 Ivan Iaitckikh. All Rights Reserved.

#pragma once

#include "CommonButtonBase.h"
#include "CoreMinimal.h"
#include "CKSampleButtonWidget.generated.h"

class UCommonTextBlock;

/**
 * Button of the sample's session screens: a CommonUI button with one text label. Clicks, focus and gamepad come from
 * UCommonButtonBase; the label (named "Label") is built in C++, so the button needs no Widget Blueprint.
 */
UCLASS()
class UCKSampleButtonWidget final : public UCommonButtonBase
{
	GENERATED_BODY()

public:
	virtual bool Initialize() override;

	/** Sets the label text. */
	void SetLabel(const FText& InText);

	FText GetLabel() const;

private:
	UPROPERTY()
	TObjectPtr<UCommonTextBlock> Label;
};
