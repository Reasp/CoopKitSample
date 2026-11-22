// Copyright 2026 Ivan Iaitckikh. All Rights Reserved.

#include "UI/CKSampleUmgDraftWidget.h"
#include "Blueprint/WidgetTree.h"
#include "CKDraftComponent.h"
#include "CKDraftHeroPoolData.h"
#include "CKDraftLibrary.h"
#include "CKDraftModeData.h"
#include "CKDraftPlayerComponent.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/TextBlock.h"
#include "Components/UniformGridPanel.h"
#include "Components/UniformGridSlot.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(CKSampleUmgDraftWidget)

#define LOCTEXT_NAMESPACE "CKSampleDraftUi"

namespace CKSample::DraftUi
{
	namespace
	{
		/** The clocks (turn time, reserve) change every frame: a few refreshes per second keep them readable. */
		constexpr float DraftRefreshSeconds = 0.2f;

		/** Heroes per row of the grid. */
		constexpr int32 HeroColumns = 5;

		const FLinearColor RejectColor(1.0f, 0.45f, 0.35f, 1.0f);
		const FLinearColor HintColor(0.75f, 0.85f, 1.0f, 1.0f);

		/** Short reason next to a hero the rules refuse; empty for reasons about the turn, not the hero. */
		FString GetHeroRefusal(const FGameplayTag& Reason)
		{
			static const TMap<FName, FString> Words = { { TEXT("CK.Draft.Reject.Banned"), TEXT("banned") }, { TEXT("CK.Draft.Reject.Taken"), TEXT("taken") },
				{ TEXT("CK.Draft.Reject.NotInPool"), TEXT("not offered") }, { TEXT("CK.Draft.Reject.RoleLimit"), TEXT("role full") },
				{ TEXT("CK.Draft.Reject.Rule"), TEXT("not allowed") } };
			const FString* Word = Reason.IsValid() ? Words.Find(Reason.GetTagName()) : nullptr;
			return Word != nullptr ? *Word : FString();
		}

		FString GetActionWord(const ECKDraftAction Action)
		{
			switch (Action)
			{
			case ECKDraftAction::Ban:
				return TEXT("ban");
			case ECKDraftAction::Swap:
				return TEXT("swap");
			default:
				return TEXT("pick");
			}
		}
	}
}

void UCKSampleUmgDraftWidget::ApplyInputMode(APlayerController& Controller)
{
	// The draft screen sits over the lobby: the mouse stays free for the hero buttons.
	FInputModeGameAndUI InputMode;
	InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	InputMode.SetHideCursorDuringCapture(false);
	Controller.SetInputMode(InputMode);
}

void UCKSampleUmgDraftWidget::BuildDefaultTree()
{
	UVerticalBox* Column = BuildFrame(TEXT("TitleText"), LOCTEXT("DraftTitle", "Hero Draft"));
	ModeText = AddText(Column, TEXT("ModeText"), FText::GetEmpty());
	TurnText = AddText(Column, TEXT("TurnText"), FText::GetEmpty());
	TimerText = AddText(Column, TEXT("TimerText"), FText::GetEmpty());
	HeroGrid = WidgetTree->ConstructWidget<UUniformGridPanel>(UUniformGridPanel::StaticClass(), TEXT("HeroGrid"));
	HeroGrid->SetSlotPadding(FMargin(4.0f));
	if (UVerticalBoxSlot* GridSlot = Column->AddChildToVerticalBox(HeroGrid))
	{
		GridSlot->SetPadding(FMargin(0.0f, 8.0f));
	}
	BansText = AddText(Column, TEXT("BansText"), FText::GetEmpty());
	VotesText = AddText(Column, TEXT("VotesText"), FText::GetEmpty());
	VotesText->SetColorAndOpacity(FSlateColor(CKSample::DraftUi::HintColor));
	PlayerList = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("PlayerList"));
	Column->AddChild(PlayerList);
	SwapText = AddText(Column, TEXT("SwapText"), FText::GetEmpty());
	SwapText->SetColorAndOpacity(FSlateColor(CKSample::DraftUi::HintColor));
	AcceptSwapButton = AddButton(Column, TEXT("AcceptSwapButton"), LOCTEXT("AcceptSwap", "Accept the swap"));
	DeclineSwapButton = AddButton(Column, TEXT("DeclineSwapButton"), LOCTEXT("DeclineSwap", "Decline the swap"));
	RejectText = AddText(Column, TEXT("RejectText"), FText::GetEmpty());
	RejectText->SetColorAndOpacity(FSlateColor(CKSample::DraftUi::RejectColor));
	EventText = AddText(Column, TEXT("EventText"), FText::GetEmpty());
}

void UCKSampleUmgDraftWidget::NativeConstruct()
{
	RefreshIntervalSeconds = CKSample::DraftUi::DraftRefreshSeconds;
	bRestoreGameInput = false;
	if (AcceptSwapButton != nullptr)
	{
		AcceptSwapButton->OnClicked.RemoveAll(this);
		AcceptSwapButton->OnClicked.AddDynamic(this, &UCKSampleUmgDraftWidget::HandleAcceptClicked);
	}
	if (DeclineSwapButton != nullptr)
	{
		DeclineSwapButton->OnClicked.RemoveAll(this);
		DeclineSwapButton->OnClicked.AddDynamic(this, &UCKSampleUmgDraftWidget::HandleDeclineClicked);
	}
	Super::NativeConstruct();
}

void UCKSampleUmgDraftWidget::NativeDestruct()
{
	if (UCKDraftComponent* Draft = BoundDraft.Get())
	{
		Draft->OnPhaseChanged.RemoveAll(this);
		Draft->OnTurnChanged.RemoveAll(this);
		Draft->OnHeroPicked.RemoveAll(this);
		Draft->OnHeroBanned.RemoveAll(this);
		Draft->OnHeroHovered.RemoveAll(this);
		Draft->OnBanVoteChanged.RemoveAll(this);
		Draft->OnSwapOffered.RemoveAll(this);
		Draft->OnHeroesSwapped.RemoveAll(this);
	}
	if (UCKDraftPlayerComponent* Player = BoundPlayer.Get())
	{
		Player->OnActionRejected.RemoveAll(this);
	}
	Super::NativeDestruct();
}

UCKDraftComponent* UCKSampleUmgDraftWidget::GetDraft() const
{
	return UCKDraftComponent::FindDraftComponent(this);
}

UCKDraftPlayerComponent* UCKSampleUmgDraftWidget::GetPlayer() const
{
	return UCKDraftPlayerComponent::FindPlayerComponent(GetOwningPlayer());
}

APlayerState* UCKSampleUmgDraftWidget::GetLocalPlayerState() const
{
	const APlayerController* Controller = GetOwningPlayer();
	return Controller != nullptr ? Controller->PlayerState.Get() : nullptr;
}

void UCKSampleUmgDraftWidget::BindEvents()
{
	// The components may reach a client after the screen: bind whenever they appear.
	UCKDraftComponent* Draft = GetDraft();
	if (Draft != nullptr && BoundDraft.Get() != Draft)
	{
		BoundDraft = Draft;
		Draft->OnPhaseChanged.AddUniqueDynamic(this, &UCKSampleUmgDraftWidget::HandlePhaseChanged);
		Draft->OnTurnChanged.AddUniqueDynamic(this, &UCKSampleUmgDraftWidget::HandleTurnChanged);
		Draft->OnHeroPicked.AddUniqueDynamic(this, &UCKSampleUmgDraftWidget::HandleHeroPicked);
		Draft->OnHeroBanned.AddUniqueDynamic(this, &UCKSampleUmgDraftWidget::HandleHeroBanned);
		Draft->OnHeroHovered.AddUniqueDynamic(this, &UCKSampleUmgDraftWidget::HandleHeroHovered);
		Draft->OnBanVoteChanged.AddUniqueDynamic(this, &UCKSampleUmgDraftWidget::HandleBanVoteChanged);
		Draft->OnSwapOffered.AddUniqueDynamic(this, &UCKSampleUmgDraftWidget::HandleSwapOffered);
		Draft->OnHeroesSwapped.AddUniqueDynamic(this, &UCKSampleUmgDraftWidget::HandleHeroesSwapped);
	}
	UCKDraftPlayerComponent* Player = GetPlayer();
	if (Player != nullptr && BoundPlayer.Get() != Player)
	{
		BoundPlayer = Player;
		Player->OnActionRejected.AddUniqueDynamic(this, &UCKSampleUmgDraftWidget::HandleActionRejected);
	}
}

void UCKSampleUmgDraftWidget::Refresh()
{
	BindEvents();
	const UCKDraftComponent* Draft = GetDraft();
	if (Draft == nullptr || ModeText == nullptr)
	{
		if (ModeText != nullptr)
		{
			ModeText->SetText(LOCTEXT("NoDraft", "Waiting for the draft..."));
		}
		return;
	}
	const FCKDraftState& State = Draft->GetState();
	if (State.DraftId != BuiltDraftId || State.Participants.Num() != BuiltParticipants)
	{
		RebuildLists(State);
	}

	const FText ModeName = State.Mode != nullptr && !State.Mode->GetDisplayName().IsEmptyOrWhitespace() ? State.Mode->GetDisplayName() : LOCTEXT("NoMode", "Draft");
	ModeText->SetText(FText::Format(LOCTEXT("ModeLine", "{0} - {1}"), ModeName, State.Phase == ECKDraftPhase::Drafting ? LOCTEXT("PhaseDrafting", "drafting")
		: State.Phase == ECKDraftPhase::Completed ? LOCTEXT("PhaseCompleted", "complete, the picks are final") : LOCTEXT("PhaseIdle", "not started")));

	APlayerState* Me = GetLocalPlayerState();
	FCKDraftTurn Turn;
	const bool bTurn = Draft->GetCurrentTurn(Turn);
	if (TurnText != nullptr)
	{
		FString Line;
		if (!bTurn)
		{
			Line = State.Phase == ECKDraftPhase::Completed ? TEXT("The draft is over.") : TEXT("No turn.");
		}
		else if (Turn.Action == ECKDraftAction::Swap)
		{
			Line = FString::Printf(TEXT("Turn %d of %d: swap window - trade heroes with a teammate"), State.TurnIndex + 1, State.Turns.Num());
		}
		else
		{
			TArray<FString> Waiting;
			for (const int32 Actor : Turn.Actors)
			{
				if (!State.ActedThisTurn.Contains(Actor))
				{
					Waiting.Add(GetPlayerName(State, Actor));
				}
			}
			const FString Whose = Draft->IsPlayerToAct(Me) ? FString(TEXT("your turn")) : FString::Printf(TEXT("waiting for %s"), *FString::Join(Waiting, TEXT(", ")));
			const TCHAR* Blind = Turn.Visibility == ECKDraftVisibility::Team ? TEXT(" (blind: hidden from the other team until the turn ends)")
				: Turn.Visibility == ECKDraftVisibility::Self ? TEXT(" (blind: hidden from everyone until the turn ends)") : TEXT("");
			if (Turn.bBanVote)
			{
				const int32 MyIndex = Draft->GetParticipantIndex(Me);
				const bool bVoter = Turn.Actors.Contains(MyIndex);
				Line = FString::Printf(TEXT("Turn %d of %d: BAN VOTE - %s (most votes wins, only your team sees the votes)"), State.TurnIndex + 1, State.Turns.Num(),
					bVoter ? (State.ActedThisTurn.Contains(MyIndex) ? TEXT("you voted, click another hero to change your vote") : TEXT("your team votes, click a hero"))
						: *FString::Printf(TEXT("the other team votes, waiting for %s"), *FString::Join(Waiting, TEXT(", "))));
			}
			else
			{
				Line = FString::Printf(TEXT("Turn %d of %d: %s - %s%s"), State.TurnIndex + 1, State.Turns.Num(), *CKSample::DraftUi::GetActionWord(Turn.Action).ToUpper(),
					*Whose, Blind);
			}
		}
		TurnText->SetText(FText::FromString(Line));
	}
	if (TimerText != nullptr)
	{
		FString Line;
		const float Reserve = Draft->GetReserveSecondsLeft(Me);
		if (bTurn && Draft->IsUsingReserve(Me))
		{
			Line = FString::Printf(TEXT("Turn time is up - on your reserve: %.1f s"), Reserve);
		}
		else if (bTurn)
		{
			const float Left = Draft->GetTurnSecondsLeft();
			Line = Left > 0.0f ? FString::Printf(TEXT("%.0f s left"), FMath::CeilToFloat(Left)) : FString(TEXT("No time limit"));
			if (Reserve > 0.0f)
			{
				Line += FString::Printf(TEXT("  |  your reserve: %.0f s"), Reserve);
			}
		}
		TimerText->SetText(FText::FromString(Line));
	}

	RefreshHeroes(*Draft, State);
	if (BansText != nullptr)
	{
		TArray<FString> Bans;
		for (const FCKDraftBan& Ban : State.Bans)
		{
			FString Entry = Ban.Hero.IsNone() ? FString(TEXT("(hidden)")) : UCKDraftLibrary::GetHeroDisplayName(State, Ban.Hero).ToString();
			if (Ban.Participant == INDEX_NONE && Ban.Team != INDEX_NONE)
			{
				Entry += FString::Printf(TEXT(" (team %d vote)"), Ban.Team + 1);
			}
			Bans.Add(Entry);
		}
		BansText->SetText(FText::FromString(FString::Printf(TEXT("Bans: %s"), Bans.IsEmpty() ? TEXT("none") : *FString::Join(Bans, TEXT(", ")))));
	}
	RefreshVotes(*Draft, State);
	RefreshPlayers(*Draft, State);
	RefreshSwap(State);
}

void UCKSampleUmgDraftWidget::RebuildLists(const FCKDraftState& State)
{
	if (BuiltDraftId != State.DraftId)
	{
		// A new draft starts with a clean screen: the last refusal and event belong to the previous one.
		if (RejectText != nullptr)
		{
			RejectText->SetText(FText::GetEmpty());
		}
		if (EventText != nullptr)
		{
			EventText->SetText(FText::GetEmpty());
		}
	}
	BuiltDraftId = State.DraftId;
	BuiltParticipants = State.Participants.Num();
	GridHeroes = State.HeroPool != nullptr ? State.HeroPool->GetHeroIds() : State.Pool;
	HeroButtons.Reset();
	PlayerTexts.Reset();
	SwapButtons.Reset();
	if (HeroGrid != nullptr)
	{
		HeroGrid->ClearChildren();
		for (int32 Index = 0; Index < GridHeroes.Num(); ++Index)
		{
			UCKSampleUmgRowButton* Button = WidgetTree->ConstructWidget<UCKSampleUmgRowButton>(UCKSampleUmgRowButton::StaticClass(), *FString::Printf(TEXT("Hero_%d"), Index));
			UTextBlock* Label = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), *FString::Printf(TEXT("Hero_%d_Label"), Index));
			Label->SetColorAndOpacity(FSlateColor(FLinearColor::Black));
			Button->SetContent(Label);
			Button->Setup(Index, FCKSampleUmgRowClicked::CreateUObject(this, &UCKSampleUmgDraftWidget::HandleHeroClicked));
			HeroGrid->AddChildToUniformGrid(Button, Index / CKSample::DraftUi::HeroColumns, Index % CKSample::DraftUi::HeroColumns);
			HeroButtons.Add(Button);
		}
	}
	if (PlayerList != nullptr)
	{
		PlayerList->ClearChildren();
		for (int32 Index = 0; Index < State.Participants.Num(); ++Index)
		{
			UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), *FString::Printf(TEXT("Player_%d"), Index));
			UTextBlock* Text = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), *FString::Printf(TEXT("Player_%d_Text"), Index));
			if (UHorizontalBoxSlot* TextSlot = Row->AddChildToHorizontalBox(Text))
			{
				TextSlot->SetVerticalAlignment(VAlign_Center);
				TextSlot->SetPadding(FMargin(0.0f, 2.0f, 12.0f, 2.0f));
			}
			UCKSampleUmgRowButton* Swap = WidgetTree->ConstructWidget<UCKSampleUmgRowButton>(UCKSampleUmgRowButton::StaticClass(), *FString::Printf(TEXT("Swap_%d"), Index));
			UTextBlock* SwapLabel = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), *FString::Printf(TEXT("Swap_%d_Label"), Index));
			SwapLabel->SetText(LOCTEXT("SwapButton", "Swap"));
			SwapLabel->SetColorAndOpacity(FSlateColor(FLinearColor::Black));
			Swap->SetContent(SwapLabel);
			Swap->Setup(Index, FCKSampleUmgRowClicked::CreateUObject(this, &UCKSampleUmgDraftWidget::HandleSwapClicked));
			Row->AddChildToHorizontalBox(Swap);
			PlayerList->AddChildToVerticalBox(Row);
			PlayerTexts.Add(Text);
			SwapButtons.Add(Swap);
		}
	}
}

void UCKSampleUmgDraftWidget::RefreshHeroes(const UCKDraftComponent& Draft, const FCKDraftState& State)
{
	APlayerState* Me = GetLocalPlayerState();
	const int32 MyIndex = Draft.GetParticipantIndex(Me);
	FCKDraftTurn Turn;
	const bool bTurn = Draft.GetCurrentTurn(Turn);
	const ECKDraftAction Action = bTurn && Turn.Action == ECKDraftAction::Ban ? ECKDraftAction::Ban : ECKDraftAction::Pick;
	// In a team vote a member who voted may still change the vote: Preview Action says whether the server allows it.
	const bool bMyTurn = bTurn && Turn.Action != ECKDraftAction::Swap && (Draft.IsPlayerToAct(Me) || (Turn.bBanVote && Turn.Actors.Contains(MyIndex)));
	const FName Hovered = State.Participants.IsValidIndex(MyIndex) ? State.Participants[MyIndex].HoveredHero : NAME_None;
	for (int32 Index = 0; Index < HeroButtons.Num() && Index < GridHeroes.Num(); ++Index)
	{
		const FName Hero = GridHeroes[Index];
		const FGameplayTag Reason = Draft.PreviewAction(Me, Action, Hero);
		const FString Refusal = CKSample::DraftUi::GetHeroRefusal(Reason);
		FString Label = UCKDraftLibrary::GetHeroDisplayName(State, Hero).ToString();
		if (Hero == Hovered)
		{
			Label = TEXT("> ") + Label;
		}
		const int32 Votes = State.BanVotes.FilterByPredicate([Hero](const FCKDraftBanVote& Vote) { return Vote.Hero == Hero; }).Num();
		if (Votes > 0)
		{
			Label += FString::Printf(TEXT(" - %d %s"), Votes, Votes == 1 ? TEXT("vote") : TEXT("votes"));
		}
		if (!Refusal.IsEmpty())
		{
			Label += TEXT(" - ") + Refusal;
		}
		UButton* Button = HeroButtons[Index];
		Button->SetIsEnabled(bMyTurn && !Reason.IsValid());
		if (UTextBlock* LabelText = Cast<UTextBlock>(Button->GetContent()))
		{
			LabelText->SetText(FText::FromString(Label));
		}
	}
}

FString UCKSampleUmgDraftWidget::GetPlayerName(const FCKDraftState& State, const int32 Index) const
{
	const APlayerState* Player = State.Participants.IsValidIndex(Index) ? State.Participants[Index].Player.Get() : nullptr;
	if (Player == nullptr)
	{
		return TEXT("(left)");
	}
	return Player == GetLocalPlayerState() ? FString::Printf(TEXT("%s (you)"), *Player->GetPlayerName()) : Player->GetPlayerName();
}

FString UCKSampleUmgDraftWidget::DescribeParticipant(const UCKDraftComponent& Draft, const FCKDraftState& State, const int32 Index) const
{
	const FCKDraftParticipant& Participant = State.Participants[Index];
	FString Hero;
	if (!Participant.Hero.IsNone())
	{
		Hero = UCKDraftLibrary::GetHeroDisplayName(State, Participant.Hero).ToString();
		if (Participant.PickVisibility == ECKDraftVisibility::Team)
		{
			Hero += TEXT(" (hidden from the other team)");
		}
		else if (Participant.PickVisibility == ECKDraftVisibility::Self)
		{
			Hero += TEXT(" (hidden)");
		}
	}
	else if (Participant.PickVisibility != ECKDraftVisibility::Public)
	{
		Hero = TEXT("locked in (hidden until the turn ends)");
	}
	else if (!Participant.HoveredHero.IsNone())
	{
		Hero = FString::Printf(TEXT("thinking of %s"), *UCKDraftLibrary::GetHeroDisplayName(State, Participant.HoveredHero).ToString());
	}
	else
	{
		Hero = TEXT("no hero yet");
	}
	FString Line = FString::Printf(TEXT("%s - team %d: %s"), *GetPlayerName(State, Index), Participant.Team + 1, *Hero);
	const bool bReserveMode = State.Turns.ContainsByPredicate([](const FCKDraftTurn& Turn) { return Turn.bUsesReserve; });
	if (bReserveMode && Participant.Player != nullptr)
	{
		Line += FString::Printf(TEXT(", reserve %.0f s"), FMath::CeilToFloat(Draft.GetReserveSecondsLeft(Participant.Player)));
	}
	return Line;
}

void UCKSampleUmgDraftWidget::RefreshPlayers(const UCKDraftComponent& Draft, const FCKDraftState& State)
{
	APlayerState* Me = GetLocalPlayerState();
	for (int32 Index = 0; Index < PlayerTexts.Num() && Index < State.Participants.Num(); ++Index)
	{
		PlayerTexts[Index]->SetText(FText::FromString(DescribeParticipant(Draft, State, Index)));
		// The Swap button only where the server would accept the offer now (a teammate with a hero, in the window).
		const bool bCanSwap = Draft.PreviewSwap(Me, Index) == FGameplayTag();
		SwapButtons[Index]->SetVisibility(bCanSwap ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}
}

void UCKSampleUmgDraftWidget::RefreshSwap(const FCKDraftState& State)
{
	const UCKDraftComponent* Draft = GetDraft();
	const int32 MyIndex = Draft != nullptr ? Draft->GetParticipantIndex(GetLocalPlayerState()) : INDEX_NONE;
	OfferFrom = INDEX_NONE;
	int32 OfferTo = INDEX_NONE;
	for (const FCKDraftSwapOffer& Offer : State.SwapOffers)
	{
		if (Offer.To == MyIndex)
		{
			OfferFrom = Offer.From;
		}
		if (Offer.From == MyIndex)
		{
			OfferTo = Offer.To;
		}
	}
	FString Line;
	if (OfferFrom != INDEX_NONE && State.Participants.IsValidIndex(MyIndex))
	{
		Line = FString::Printf(TEXT("%s offers you a swap: their %s for your %s"), *GetPlayerName(State, OfferFrom),
			*UCKDraftLibrary::GetHeroDisplayName(State, State.Participants[OfferFrom].Hero).ToString(),
			*UCKDraftLibrary::GetHeroDisplayName(State, State.Participants[MyIndex].Hero).ToString());
	}
	else if (OfferTo != INDEX_NONE)
	{
		Line = FString::Printf(TEXT("Swap offer sent to %s, waiting for the answer"), *GetPlayerName(State, OfferTo));
	}
	else if (State.Turns.IsValidIndex(State.TurnIndex) && State.Turns[State.TurnIndex].Action == ECKDraftAction::Swap)
	{
		Line = TEXT("Swap window: press Swap next to a teammate to offer a trade");
	}
	if (SwapText != nullptr)
	{
		SwapText->SetText(FText::FromString(Line));
	}
	const ESlateVisibility Answer = OfferFrom != INDEX_NONE ? ESlateVisibility::Visible : ESlateVisibility::Collapsed;
	if (AcceptSwapButton != nullptr)
	{
		AcceptSwapButton->SetVisibility(Answer);
	}
	if (DeclineSwapButton != nullptr)
	{
		DeclineSwapButton->SetVisibility(Answer);
	}
}

void UCKSampleUmgDraftWidget::RefreshVotes(const UCKDraftComponent& Draft, const FCKDraftState& State)
{
	if (VotesText == nullptr)
	{
		return;
	}
	FCKDraftTurn Turn;
	const int32 MyIndex = Draft.GetParticipantIndex(GetLocalPlayerState());
	if (!Draft.GetCurrentTurn(Turn) || !Turn.bBanVote || !State.Participants.IsValidIndex(MyIndex))
	{
		VotesText->SetText(FText::GetEmpty());
		VotesText->SetVisibility(ESlateVisibility::Collapsed);
		return;
	}
	// The state holds only this player's team's votes: the server never sends the other team's.
	const int32 MyTeam = State.Participants[MyIndex].Team;
	TArray<FString> Lines;
	for (const int32 Actor : Turn.Actors)
	{
		if (!State.Participants.IsValidIndex(Actor) || State.Participants[Actor].Team != MyTeam)
		{
			continue;
		}
		const FCKDraftBanVote* Vote = State.BanVotes.FindByPredicate([Actor](const FCKDraftBanVote& Entry) { return Entry.Participant == Actor; });
		Lines.Add(FString::Printf(TEXT("%s: %s"), *GetPlayerName(State, Actor),
			Vote != nullptr ? *UCKDraftLibrary::GetHeroDisplayName(State, Vote->Hero).ToString() : TEXT("not voted yet")));
	}
	const FString Body = Lines.IsEmpty() ? FString(TEXT("your team does not vote in this turn")) : FString::Join(Lines, TEXT(", "));
	VotesText->SetText(FText::FromString(FString::Printf(TEXT("Your team's ban votes: %s"), *Body)));
	VotesText->SetVisibility(ESlateVisibility::Visible);
}

void UCKSampleUmgDraftWidget::SetEvent(const FString& Line)
{
	if (EventText != nullptr)
	{
		EventText->SetText(FText::FromString(Line));
	}
	Refresh();
}

void UCKSampleUmgDraftWidget::HandleHeroClicked(const int32 HeroIndex)
{
	const UCKDraftComponent* Draft = GetDraft();
	UCKDraftPlayerComponent* Player = GetPlayer();
	FCKDraftTurn Turn;
	if (Draft == nullptr || Player == nullptr || !GridHeroes.IsValidIndex(HeroIndex) || !Draft->GetCurrentTurn(Turn))
	{
		return;
	}
	if (Turn.Action == ECKDraftAction::Ban)
	{
		Player->RequestBan(GridHeroes[HeroIndex]);
	}
	else if (Turn.Action == ECKDraftAction::Pick)
	{
		Player->RequestPick(GridHeroes[HeroIndex]);
	}
}

void UCKSampleUmgDraftWidget::HandleSwapClicked(const int32 Participant)
{
	if (UCKDraftPlayerComponent* Player = GetPlayer())
	{
		Player->RequestSwap(Participant);
	}
}

void UCKSampleUmgDraftWidget::HandleAcceptClicked()
{
	if (UCKDraftPlayerComponent* Player = GetPlayer(); Player != nullptr && OfferFrom != INDEX_NONE)
	{
		Player->AnswerSwap(OfferFrom, true);
	}
}

void UCKSampleUmgDraftWidget::HandleDeclineClicked()
{
	if (UCKDraftPlayerComponent* Player = GetPlayer(); Player != nullptr && OfferFrom != INDEX_NONE)
	{
		Player->AnswerSwap(OfferFrom, false);
	}
}

void UCKSampleUmgDraftWidget::HandlePhaseChanged(const ECKDraftPhase Phase)
{
	SetEvent(Phase == ECKDraftPhase::Completed ? FString(TEXT("The draft is complete.")) : Phase == ECKDraftPhase::Drafting ? FString(TEXT("The draft started.")) : FString());
}

void UCKSampleUmgDraftWidget::HandleTurnChanged(const int32 TurnIndex)
{
	Refresh();
}

void UCKSampleUmgDraftWidget::HandleHeroPicked(const int32 Participant, APlayerState* Player, const FName Hero)
{
	const UCKDraftComponent* Draft = GetDraft();
	if (Draft != nullptr)
	{
		SetEvent(FString::Printf(TEXT("%s has %s"), *GetPlayerName(Draft->GetState(), Participant), *UCKDraftLibrary::GetHeroDisplayName(Draft->GetState(), Hero).ToString()));
	}
}

void UCKSampleUmgDraftWidget::HandleHeroBanned(const int32 Participant, APlayerState* Player, const FName Hero)
{
	const UCKDraftComponent* Draft = GetDraft();
	if (Draft == nullptr)
	{
		return;
	}
	const FCKDraftState& State = Draft->GetState();
	const FCKDraftBan* Ban = State.Bans.FindByPredicate([Hero](const FCKDraftBan& Entry) { return Entry.Hero == Hero; });
	const FString Who = Participant == INDEX_NONE && Ban != nullptr && Ban->Team != INDEX_NONE ? FString::Printf(TEXT("Team %d's vote"), Ban->Team + 1)
		: GetPlayerName(State, Participant);
	SetEvent(FString::Printf(TEXT("%s banned %s"), *Who, *UCKDraftLibrary::GetHeroDisplayName(State, Hero).ToString()));
}

void UCKSampleUmgDraftWidget::HandleBanVoteChanged(const int32 Participant, APlayerState* Player, const FName Hero)
{
	const UCKDraftComponent* Draft = GetDraft();
	if (Draft != nullptr)
	{
		SetEvent(FString::Printf(TEXT("%s votes to ban %s"), *GetPlayerName(Draft->GetState(), Participant), *UCKDraftLibrary::GetHeroDisplayName(Draft->GetState(), Hero).ToString()));
	}
}

void UCKSampleUmgDraftWidget::HandleHeroHovered(const int32 Participant, APlayerState* Player, const FName Hero)
{
	Refresh();
}

void UCKSampleUmgDraftWidget::HandleSwapOffered(const int32 FromParticipant, const int32 ToParticipant)
{
	const UCKDraftComponent* Draft = GetDraft();
	if (Draft != nullptr)
	{
		SetEvent(FString::Printf(TEXT("%s offers %s a swap"), *GetPlayerName(Draft->GetState(), FromParticipant), *GetPlayerName(Draft->GetState(), ToParticipant)));
	}
}

void UCKSampleUmgDraftWidget::HandleHeroesSwapped(const int32 FromParticipant, const int32 ToParticipant)
{
	const UCKDraftComponent* Draft = GetDraft();
	if (Draft != nullptr)
	{
		SetEvent(FString::Printf(TEXT("%s and %s swapped heroes"), *GetPlayerName(Draft->GetState(), FromParticipant), *GetPlayerName(Draft->GetState(), ToParticipant)));
	}
}

void UCKSampleUmgDraftWidget::HandleActionRejected(const ECKDraftAction Action, const FName Hero, const FGameplayTag Reason)
{
	const UCKDraftComponent* Draft = GetDraft();
	const FString HeroPart = Hero.IsNone() || Draft == nullptr ? FString() : FString::Printf(TEXT(" of %s"), *UCKDraftLibrary::GetHeroDisplayName(Draft->GetState(), Hero).ToString());
	if (RejectText != nullptr)
	{
		RejectText->SetText(FText::FromString(FString::Printf(TEXT("Server refused your %s%s: %s"), *CKSample::DraftUi::GetActionWord(Action), *HeroPart,
			*UCKDraftLibrary::GetRejectReasonText(Reason).ToString())));
	}
	Refresh();
}

#undef LOCTEXT_NAMESPACE
