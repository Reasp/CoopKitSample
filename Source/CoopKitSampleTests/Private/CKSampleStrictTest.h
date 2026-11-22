// Copyright 2026 Ivan Iaitckikh. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"

/**
 * Automation test base that fails on any log warning raised while the test runs.
 * Used where "loads without errors or warnings" is the acceptance criterion, independent of project-wide settings.
 */
class FCKSampleStrictTestBase : public FAutomationTestBase
{
public:
	FCKSampleStrictTestBase(const FString& InName, const bool bInComplexTask)
		: FAutomationTestBase(InName, bInComplexTask)
	{
	}

	virtual bool SuppressLogWarnings() override
	{
		return false;
	}

	virtual bool SuppressLogErrors() override
	{
		return false;
	}

	virtual bool ElevateLogWarningsToErrors() override
	{
		return true;
	}
};
