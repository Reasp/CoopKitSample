// Copyright 2026 Ivan Iaitckikh. All Rights Reserved.

#include "UI/CKSampleUmgMainMenuWidget.h"
#include "Blueprint/WidgetTree.h"
#include "CKSessionsLibrary.h"
#include "CKSessionsSubsystem.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "CoopPlugins.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Misc/CommandLine.h"
#include "TimerManager.h"
#include "UI/CKSampleSessionsUiShared.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(CKSampleUmgMainMenuWidget)

// The texts are the CommonUI menu's: one namespace and the same keys, so a translation covers both sets of screens.
#define LOCTEXT_NAMESPACE "CKSampleSessionsUi"

void UCKSampleUmgMainMenuWidget::SetHostMap(const TSoftObjectPtr<UWorld>& InHostMap)
{
	HostMap = InHostMap;
}

void UCKSampleUmgMainMenuWidget::ApplyInputMode(APlayerController& Controller)
{
	// The menu takes every input; the mouse stays free for the buttons.
	FInputModeUIOnly InputMode;
	InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	if (UWidget* Focus = GetFocusWidget())
	{
		InputMode.SetWidgetToFocus(Focus->TakeWidget());
	}
	Controller.SetInputMode(InputMode);
}

UWidget* UCKSampleUmgMainMenuWidget::GetFocusWidget()
{
	return HostButton;
}

void UCKSampleUmgMainMenuWidget::BuildDefaultTree()
{
	UVerticalBox* Column = BuildFrame(TEXT("TitleText"), LOCTEXT("MenuTitle", "CoopKit Sample"));
	HostButton = AddButton(Column, TEXT("HostButton"), LOCTEXT("Host", "Host"));
	FindButton = AddButton(Column, TEXT("FindButton"), LOCTEXT("Find", "Find sessions"));
	StatusText = AddText(Column, TEXT("StatusText"), FText::GetEmpty());
	SessionList = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("SessionList"));
	Column->AddChild(SessionList);
}

void UCKSampleUmgMainMenuWidget::NativeConstruct()
{
	RefreshIntervalSeconds = CKSample::SessionsUi::MenuRefreshSeconds;
	if (HostButton != nullptr)
	{
		HostButton->OnClicked.RemoveAll(this);
		HostButton->OnClicked.AddDynamic(this, &UCKSampleUmgMainMenuWidget::HandleHostClicked);
	}
	if (FindButton != nullptr)
	{
		FindButton->OnClicked.RemoveAll(this);
		FindButton->OnClicked.AddDynamic(this, &UCKSampleUmgMainMenuWidget::HandleFindClicked);
	}
	if (UCKSessionsSubsystem* Sessions = GetSessions())
	{
		Sessions->OnHostComplete.AddUniqueDynamic(this, &UCKSampleUmgMainMenuWidget::HandleHostComplete);
		Sessions->OnFindComplete.AddUniqueDynamic(this, &UCKSampleUmgMainMenuWidget::HandleFindComplete);
		Sessions->OnJoinComplete.AddUniqueDynamic(this, &UCKSampleUmgMainMenuWidget::HandleJoinComplete);

		// Back in the menu after the session was lost for good (or after leaving it): say why.
		const ECKSessionsFailure LastFailure = Sessions->GetLastFailure();
		SetStatus(LastFailure == ECKSessionsFailure::None
			? LOCTEXT("Welcome", "Host a game, or find one on the local network.")
			: FText::Format(LOCTEXT("SessionEnded", "The last session ended: {0}."), CKSample::SessionsUi::DescribeFailure(LastFailure)));
	}
	else
	{
		SetStatus(LOCTEXT("NoSessions", "CoopKit Sessions is not available in this game."));
	}
	Super::NativeConstruct();

	if (UWorld* World = GetWorld(); World != nullptr && !CKSample::SessionsUi::AutoStartDone())
	{
		World->GetTimerManager().SetTimerForNextTick(FTimerDelegate::CreateUObject(this, &UCKSampleUmgMainMenuWidget::RunAutoStart));
	}
}

void UCKSampleUmgMainMenuWidget::RunAutoStart()
{
	if (CKSample::SessionsUi::AutoStartDone())
	{
		return;
	}
	const bool bAutoHost = FParse::Param(FCommandLine::Get(), TEXT("CKSampleAutoHost"));
	const bool bAutoJoin = !bAutoHost && FParse::Param(FCommandLine::Get(), TEXT("CKSampleAutoJoin"));
	if (!bAutoHost && !bAutoJoin)
	{
		return;
	}
	CKSample::SessionsUi::AutoStartDone() = true;
	if (bAutoHost)
	{
		UE_LOG(LogCKSample, Display, TEXT("CKSample auto start: hosting a session (-CKSampleAutoHost)."));
		HandleHostClicked();
		return;
	}
	AutoJoinAttempts = 1;
	UE_LOG(LogCKSample, Display, TEXT("CKSample auto start: searching for a session to join (-CKSampleAutoJoin), attempt 1 of %d."),
		CKSample::SessionsUi::AutoJoinMaxAttempts);
	HandleFindClicked();
}

void UCKSampleUmgMainMenuWidget::RetryAutoJoin()
{
	if (AutoJoinAttempts >= CKSample::SessionsUi::AutoJoinMaxAttempts)
	{
		UE_LOG(LogCKSample, Warning, TEXT("CKSample auto start: no session found after %d searches."), AutoJoinAttempts);
		AutoJoinAttempts = 0;
		return;
	}
	++AutoJoinAttempts;
	UE_LOG(LogCKSample, Display, TEXT("CKSample auto start: searching again, attempt %d of %d."), AutoJoinAttempts,
		CKSample::SessionsUi::AutoJoinMaxAttempts);
	HandleFindClicked();
}

void UCKSampleUmgMainMenuWidget::NativeDestruct()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(AutoJoinTimer);
	}
	if (UCKSessionsSubsystem* Sessions = GetSessions())
	{
		Sessions->OnHostComplete.RemoveAll(this);
		Sessions->OnFindComplete.RemoveAll(this);
		Sessions->OnJoinComplete.RemoveAll(this);
	}
	Super::NativeDestruct();
}

void UCKSampleUmgMainMenuWidget::Refresh()
{
	// Requests start only while idle (see Host Session, Find Sessions, Join Session).
	const UCKSessionsSubsystem* Sessions = GetSessions();
	const bool bIdle = Sessions != nullptr && Sessions->GetState() == ECKSessionsState::Idle;
	if (HostButton != nullptr)
	{
		HostButton->SetIsEnabled(bIdle);
	}
	if (FindButton != nullptr)
	{
		FindButton->SetIsEnabled(bIdle);
	}
	if (SessionList != nullptr)
	{
		for (UWidget* Row : SessionList->GetAllChildren())
		{
			Row->SetIsEnabled(bIdle);
		}
	}
}

UCKSessionsSubsystem* UCKSampleUmgMainMenuWidget::GetSessions() const
{
	// The Blueprint node "Get CKSessionsSubsystem": the game instance subsystem of this player's game.
	const UGameInstance* GameInstance = GetGameInstance();
	return GameInstance != nullptr ? GameInstance->GetSubsystem<UCKSessionsSubsystem>() : nullptr;
}

void UCKSampleUmgMainMenuWidget::SetStatus(const FText& InText)
{
	if (StatusText != nullptr)
	{
		StatusText->SetText(InText);
	}
}

void UCKSampleUmgMainMenuWidget::HandleHostClicked()
{
	UCKSessionsSubsystem* Sessions = GetSessions();
	if (Sessions == nullptr)
	{
		return;
	}
	SetStatus(LOCTEXT("Hosting", "Hosting a session..."));
	// Null profile: the project's default profile. A refused request still fires On Host Complete with the reason.
	Sessions->HostSession(nullptr, HostMap);
	Refresh();
}

void UCKSampleUmgMainMenuWidget::HandleFindClicked()
{
	UCKSessionsSubsystem* Sessions = GetSessions();
	if (Sessions == nullptr)
	{
		return;
	}
	ShowResults({});
	SetStatus(LOCTEXT("Searching", "Searching for sessions..."));
	Sessions->FindSessions(nullptr);
	Refresh();
}

void UCKSampleUmgMainMenuWidget::HandleHostComplete(const bool bSuccess, const ECKSessionsFailure Failure)
{
	// On success the game is already on the lobby map and this menu is gone; only a failure reaches the menu.
	if (!bSuccess)
	{
		SetStatus(FText::Format(LOCTEXT("HostFailed", "Could not host: {0}."), CKSample::SessionsUi::DescribeFailure(Failure)));
	}
	Refresh();
}

void UCKSampleUmgMainMenuWidget::HandleFindComplete(const bool bSuccess, const TArray<FCKSessionsSearchResult>& Results)
{
	if (!bSuccess)
	{
		const UCKSessionsSubsystem* Sessions = GetSessions();
		SetStatus(FText::Format(LOCTEXT("FindFailed", "Search failed: {0}."),
			CKSample::SessionsUi::DescribeFailure(Sessions != nullptr ? Sessions->GetLastFailure() : ECKSessionsFailure::SearchFailed)));
		ShowResults({});
	}
	else
	{
		SetStatus(Results.IsEmpty()
			? LOCTEXT("NothingFound", "No sessions found. Host one, or search again.")
			: FText::Format(LOCTEXT("Found", "{0} {0}|plural(one=session,other=sessions) found. Click one to join."), Results.Num()));
		ShowResults(Results);
	}
	Refresh();

	if (AutoJoinAttempts > 0)
	{
		if (bSuccess && !Results.IsEmpty())
		{
			UE_LOG(LogCKSample, Display, TEXT("CKSample auto start: joining %s."), *UCKSessionsLibrary::GetHostName(Results[0]));
			AutoJoinAttempts = 0;
			JoinResult(0);
		}
		else if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().SetTimer(AutoJoinTimer, FTimerDelegate::CreateUObject(this, &UCKSampleUmgMainMenuWidget::RetryAutoJoin),
				CKSample::SessionsUi::AutoJoinRetrySeconds, false);
		}
	}
}

void UCKSampleUmgMainMenuWidget::HandleJoinComplete(const bool bSuccess, const ECKSessionsFailure Failure)
{
	if (!bSuccess)
	{
		SetStatus(FText::Format(LOCTEXT("JoinFailed", "Could not join: {0}."), CKSample::SessionsUi::DescribeFailure(Failure)));
	}
	Refresh();
}

void UCKSampleUmgMainMenuWidget::ShowResults(const TArray<FCKSessionsSearchResult>& Results)
{
	if (SessionList == nullptr)
	{
		return;
	}
	SessionList->ClearChildren();
	for (int32 Index = 0; Index < Results.Num(); ++Index)
	{
		const FCKSessionsSearchResult& Result = Results[Index];
		int32 Players = 0;
		int32 MaxPlayers = 0;
		UCKSessionsLibrary::GetPlayerCounts(Result, Players, MaxPlayers);
		const FText Label = FText::Format(LOCTEXT("SessionRow", "{0}   {1}/{2} players   {3} ms{4}"),
			FText::FromString(UCKSessionsLibrary::GetHostName(Result)), Players, MaxPlayers, UCKSessionsLibrary::GetPingMs(Result),
			UCKSessionsLibrary::IsMatchInProgress(Result) ? LOCTEXT("InMatch", "   in match") : FText::GetEmpty());

		// No fixed name: rows of an earlier search may still exist in the widget tree. The list's order is the search order.
		UCKSampleUmgRowButton* Row = WidgetTree->ConstructWidget<UCKSampleUmgRowButton>(UCKSampleUmgRowButton::StaticClass());
		UTextBlock* RowText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
		RowText->SetText(Label);
		RowText->SetColorAndOpacity(FSlateColor(FLinearColor::Black));
		Row->SetContent(RowText);
		Row->Setup(Index, FCKSampleUmgRowClicked::CreateUObject(this, &UCKSampleUmgMainMenuWidget::JoinResult));
		SessionList->AddChild(Row);
	}
}

void UCKSampleUmgMainMenuWidget::JoinResult(const int32 ResultIndex)
{
	UCKSessionsSubsystem* Sessions = GetSessions();
	if (Sessions == nullptr || !Sessions->GetSearchResults().IsValidIndex(ResultIndex))
	{
		return;
	}
	const FCKSessionsSearchResult& Result = Sessions->GetSearchResults()[ResultIndex];
	SetStatus(FText::Format(LOCTEXT("Joining", "Joining {0}..."), FText::FromString(UCKSessionsLibrary::GetHostName(Result))));
	Sessions->JoinSession(Result);
	Refresh();
}

#undef LOCTEXT_NAMESPACE
