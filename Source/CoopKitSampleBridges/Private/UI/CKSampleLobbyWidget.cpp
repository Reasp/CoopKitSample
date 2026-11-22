// Copyright 2026 Ivan Iaitckikh. All Rights Reserved.

#include "UI/CKSampleLobbyWidget.h"
#include "Blueprint/WidgetTree.h"
#include "CKSessionsPlayerComponent.h"
#include "CKSessionsSubsystem.h"
#include "CommonTextBlock.h"
#include "Components/VerticalBox.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "Input/UIActionBindingHandle.h"
#include "UI/CKSampleButtonWidget.h"
#include "UI/CKSampleSessionsUiShared.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(CKSampleLobbyWidget)

#define LOCTEXT_NAMESPACE "CKSampleSessionsUi"

TOptional<FUIInputConfig> UCKSampleLobbyWidget::GetDesiredInputConfig() const
{
	// The lobby is a room: the player may walk around while the mouse stays free for the buttons.
	return FUIInputConfig(ECommonInputMode::All, EMouseCaptureMode::NoCapture);
}

void UCKSampleLobbyWidget::BuildDefaultTree()
{
	UVerticalBox* Column = BuildFrame(TEXT("TitleText"), LOCTEXT("LobbyTitle", "Lobby"));
	PhaseText = AddText(Column, TEXT("PhaseText"), FText::GetEmpty());
	PlayerList = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("PlayerList"));
	Column->AddChild(PlayerList);
	ReadyButton = AddButton(Column, TEXT("ReadyButton"), LOCTEXT("Ready", "Ready"));
	StartButton = AddButton(Column, TEXT("StartButton"), LOCTEXT("Start", "Start the match"));
	LeaveButton = AddButton(Column, TEXT("LeaveButton"), LOCTEXT("Leave", "Leave"));
	StatusText = AddText(Column, TEXT("StatusText"), FText::GetEmpty());
}

void UCKSampleLobbyWidget::NativeConstruct()
{
	RefreshIntervalSeconds = CKSample::SessionsUi::LobbyRefreshSeconds;
	if (ReadyButton != nullptr)
	{
		ReadyButton->OnClicked().RemoveAll(this);
		ReadyButton->OnClicked().AddUObject(this, &UCKSampleLobbyWidget::HandleReadyClicked);
	}
	if (StartButton != nullptr)
	{
		StartButton->OnClicked().RemoveAll(this);
		StartButton->OnClicked().AddUObject(this, &UCKSampleLobbyWidget::HandleStartClicked);
		// Only the host of a listen server starts early; a dedicated server has no player to press it.
		StartButton->SetVisibility(IsListenHost() ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}
	if (LeaveButton != nullptr)
	{
		LeaveButton->OnClicked().RemoveAll(this);
		LeaveButton->OnClicked().AddUObject(this, &UCKSampleLobbyWidget::HandleLeaveClicked);
	}
	Super::NativeConstruct();
}

void UCKSampleLobbyWidget::NativeDestruct()
{
	if (UCKSessionsLobbyComponent* Lobby = UCKSessionsLobbyComponent::FindLobbyComponent(this))
	{
		Lobby->OnPhaseChanged.RemoveAll(this);
	}
	const UWorld* World = GetWorld();
	const AGameStateBase* GameState = World != nullptr ? World->GetGameState() : nullptr;
	if (GameState != nullptr)
	{
		for (const APlayerState* PlayerState : GameState->PlayerArray)
		{
			if (UCKSessionsPlayerComponent* Player = UCKSessionsPlayerComponent::FindPlayerComponent(PlayerState))
			{
				Player->OnReadyChanged.RemoveAll(this);
			}
		}
	}
	Super::NativeDestruct();
}

UWidget* UCKSampleLobbyWidget::NativeGetDesiredFocusTarget() const
{
	return ReadyButton;
}

void UCKSampleLobbyWidget::Refresh()
{
	UCKSessionsLobbyComponent* Lobby = UCKSessionsLobbyComponent::FindLobbyComponent(this);
	if (Lobby != nullptr)
	{
		Lobby->OnPhaseChanged.AddUniqueDynamic(this, &UCKSampleLobbyWidget::HandlePhaseChanged);
		if (Lobby->GetPhase() == ECKSessionsLobbyPhase::Traveling || Lobby->GetPhase() == ECKSessionsLobbyPhase::InMatch)
		{
			// The lobby is over. Removing the screen deactivates it, which gives the game its input back.
			RemoveFromParent();
			return;
		}
	}

	RefreshPhase(Lobby);
	RefreshPlayers();

	const UCKSessionsPlayerComponent* LocalPlayer = GetLocalPlayerComponent();
	const bool bLobbyOpen = CKSample::SessionsUi::IsLobbyOpen(Lobby);
	if (ReadyButton != nullptr)
	{
		// The lobby gives each player state its component on the server; on a client it arrives a moment later.
		ReadyButton->SetIsEnabled(bLobbyOpen && LocalPlayer != nullptr);
		ReadyButton->SetLabel(LocalPlayer != nullptr && LocalPlayer->IsReady() ? LOCTEXT("NotReady", "Not ready") : LOCTEXT("Ready", "Ready"));
	}
	if (StartButton != nullptr)
	{
		StartButton->SetIsEnabled(bLobbyOpen);
	}
}

void UCKSampleLobbyWidget::RefreshPhase(const UCKSessionsLobbyComponent* Lobby)
{
	if (PhaseText == nullptr)
	{
		return;
	}
	if (Lobby == nullptr)
	{
		PhaseText->SetText(LOCTEXT("Connecting", "Connecting to the lobby..."));
		return;
	}
	switch (Lobby->GetPhase())
	{
	case ECKSessionsLobbyPhase::Countdown:
		PhaseText->SetText(FText::Format(LOCTEXT("Countdown", "Match starts in {0}"), FMath::CeilToInt(Lobby->GetCountdownSecondsLeft())));
		break;
	case ECKSessionsLobbyPhase::Traveling:
	case ECKSessionsLobbyPhase::InMatch:
		PhaseText->SetText(LOCTEXT("Traveling", "Starting the match..."));
		break;
	default:
		PhaseText->SetText(FText::Format(LOCTEXT("Waiting", "Waiting for players: {0} of {1} ready"),
			Lobby->GetReadyPlayerCount(), Lobby->GetPlayerCount()));
		break;
	}
}

void UCKSampleLobbyWidget::RefreshPlayers()
{
	const UWorld* World = GetWorld();
	const AGameStateBase* GameState = World != nullptr ? World->GetGameState() : nullptr;
	if (PlayerList == nullptr || GameState == nullptr)
	{
		return;
	}

	const APlayerController* Owner = GetOwningPlayer();
	const APlayerState* OwnState = Owner != nullptr ? Owner->PlayerState.Get() : nullptr;
	TArray<FText> Lines;
	for (const APlayerState* PlayerState : GameState->PlayerArray)
	{
		if (PlayerState == nullptr || PlayerState->IsOnlyASpectator() || PlayerState->IsInactive())
		{
			continue;
		}
		UCKSessionsPlayerComponent* Player = UCKSessionsPlayerComponent::FindPlayerComponent(PlayerState);
		if (Player != nullptr)
		{
			Player->OnReadyChanged.AddUniqueDynamic(this, &UCKSampleLobbyWidget::HandleReadyChanged);
		}
		Lines.Add(FText::Format(LOCTEXT("PlayerLine", "{0}{1} - {2}"), FText::FromString(PlayerState->GetPlayerName()),
			PlayerState == OwnState ? LOCTEXT("You", " (you)") : FText::GetEmpty(),
			Player != nullptr && Player->IsReady() ? LOCTEXT("PlayerReady", "Ready") : LOCTEXT("PlayerNotReady", "Not ready")));
	}

	// Lines have no fixed names: the list's order is the game state's order.
	while (PlayerList->GetChildrenCount() > Lines.Num())
	{
		PlayerList->RemoveChildAt(PlayerList->GetChildrenCount() - 1);
	}
	while (PlayerList->GetChildrenCount() < Lines.Num())
	{
		PlayerList->AddChild(WidgetTree->ConstructWidget<UCommonTextBlock>(UCommonTextBlock::StaticClass()));
	}
	for (int32 Index = 0; Index < Lines.Num(); ++Index)
	{
		if (UCommonTextBlock* Line = Cast<UCommonTextBlock>(PlayerList->GetChildAt(Index)))
		{
			Line->SetText(Lines[Index]);
		}
	}
}

UCKSessionsSubsystem* UCKSampleLobbyWidget::GetSessions() const
{
	const UGameInstance* GameInstance = GetGameInstance();
	return GameInstance != nullptr ? GameInstance->GetSubsystem<UCKSessionsSubsystem>() : nullptr;
}

UCKSessionsPlayerComponent* UCKSampleLobbyWidget::GetLocalPlayerComponent() const
{
	const APlayerController* Owner = GetOwningPlayer();
	return Owner != nullptr ? UCKSessionsPlayerComponent::FindPlayerComponent(Owner->PlayerState) : nullptr;
}

bool UCKSampleLobbyWidget::IsListenHost() const
{
	const UWorld* World = GetWorld();
	return World != nullptr && World->GetNetMode() == NM_ListenServer;
}

void UCKSampleLobbyWidget::SetStatus(const FText& InText)
{
	if (StatusText != nullptr)
	{
		StatusText->SetText(InText);
	}
}

void UCKSampleLobbyWidget::HandleReadyClicked()
{
	if (UCKSessionsPlayerComponent* LocalPlayer = GetLocalPlayerComponent())
	{
		// On a client the flag goes to the server and comes back through replication (On Ready Changed).
		LocalPlayer->SetReady(!LocalPlayer->IsReady());
		SetStatus(FText::GetEmpty());
	}
	Refresh();
}

void UCKSampleLobbyWidget::HandleStartClicked()
{
	UCKSessionsLobbyComponent* Lobby = UCKSessionsLobbyComponent::FindLobbyComponent(this);
	if (Lobby == nullptr || !Lobby->StartMatchNow())
	{
		SetStatus(LOCTEXT("StartRefused", "The lobby rules do not allow starting yet."));
	}
}

void UCKSampleLobbyWidget::HandleLeaveClicked()
{
	if (UCKSessionsSubsystem* Sessions = GetSessions())
	{
		SetStatus(LOCTEXT("Leaving", "Leaving the session..."));
		// Destroys the session and travels to the menu map (Project Settings > CoopKit Sessions > Menu Map).
		Sessions->LeaveSession();
	}
}

void UCKSampleLobbyWidget::HandlePhaseChanged(const ECKSessionsLobbyPhase Phase)
{
	Refresh();
}

void UCKSampleLobbyWidget::HandleReadyChanged(UCKSessionsPlayerComponent* Component, const bool bIsReady)
{
	Refresh();
}

#undef LOCTEXT_NAMESPACE
