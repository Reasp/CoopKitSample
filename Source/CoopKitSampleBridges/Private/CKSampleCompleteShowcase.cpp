// Copyright 2026 Ivan Iaitckikh. All Rights Reserved.

#include "CKSampleCompleteShowcase.h"
#include "CKCombatComponent.h"
#include "CKInventoryComponent.h"
#include "CKInventoryGameplayTags.h"
#include "CKInventoryItemData.h"
#include "CKInventoryPickup.h"
#include "CKPingControllerComponent.h"
#include "CKReviveBodyComponent.h"
#include "CKSaveSubsystem.h"
#include "Components/TextRenderComponent.h"
#include "CoopPlugins.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "Net/UnrealNetwork.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(CKSampleCompleteShowcase)

namespace CKSampleCompleteShowcase
{
	/** Steps of one loop; the captions are shown in the same order. */
	constexpr int32 LoopSteps = 12;
	constexpr int32 AdaIndex = 0;
	constexpr int32 BoIndex = 1;

	/** Text of the caption lines, English and ASCII (product text). Index = step; an empty line keeps the previous one. */
	const TCHAR* const Captions[LoopSteps] = {
		TEXT("CoopKit: two players, one match"),
		TEXT("CoopKit Ping + AI: Ada pings, the enemies turn on her"),
		TEXT("CoopKit Inventory: Ada picks up the sword"),
		TEXT("CoopKit Inventory: Ada equips the sword"),
		TEXT("CoopKit Combat: three-hit combo, judged by the server"),
		TEXT(""),
		TEXT(""),
		TEXT("CoopKit Revive: Bo goes down, not dead"),
		TEXT("CoopKit Revive: Ada revives Bo"),
		TEXT("CoopKit Revive: Bo is back on his feet"),
		TEXT("CoopKit Save: the host saves the world and both players"),
		TEXT(""),
	};
}

ACKSampleCompleteShowcase::ACKSampleCompleteShowcase()
{
	bReplicates = true;
	StepSeconds = 3.0f;
	StepCount = CKSampleCompleteShowcase::LoopSteps;
	FirstStepDelaySeconds = 2.0f;

	CaptionText = CreateDefaultSubobject<UTextRenderComponent>(TEXT("Caption"));
	CaptionText->SetupAttachment(RootComponent);
	// The front of the text looks along +X; players stand at -Y and look along +Y.
	CaptionText->SetRelativeRotation(FRotator(0.0, -90.0, 0.0));
	CaptionText->SetHorizontalAlignment(EHTA_Center);
	CaptionText->SetVerticalAlignment(EVRTA_TextCenter);
	CaptionText->SetWorldSize(34.0f);
	CaptionText->SetTextRenderColor(FColor::White);
	CaptionText->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	CaptionText->SetCastShadow(false);
}

void ACKSampleCompleteShowcase::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ACKSampleCompleteShowcase, Caption);
}

void ACKSampleCompleteShowcase::OnRep_Caption()
{
	CaptionText->SetText(FText::FromString(Caption));
}

void ACKSampleCompleteShowcase::SetCaption(const FString& Text)
{
	if (Text.IsEmpty() || Text == Caption)
	{
		return;
	}
	Caption = Text;
	OnRep_Caption();
}

void ACKSampleCompleteShowcase::RunStep_Implementation(const int32 Step)
{
	using namespace CKSampleCompleteShowcase;

	SetCaption(Captions[FMath::Clamp(Step, 0, LoopSteps - 1)]);
	switch (Step)
	{
	case 0:
		StartLoop();
		break;
	case 1:
		PingEnemy();
		break;
	case 2:
		TakeSword();
		break;
	case 3:
		EquipSword();
		break;
	case 4:
	case 5:
		Strike(AdaIndex);
		break;
	case 6:
		// One fighter per step: the ability system allows two gameplay cue multicasts per net update.
		Strike(BoIndex);
		break;
	case 7:
		DownBo();
		break;
	case 8:
		ReviveBo();
		break;
	case 10:
		SaveGame();
		break;
	default:
		break;
	}
}

void ACKSampleCompleteShowcase::EnsureBots()
{
	using namespace CKSampleCompleteShowcase;

	const FRotator North(0.0, 90.0, 0.0);
	if (GetBots().Num() < 1)
	{
		CreateBot(TEXT("Ada"), AdaLocation, North);
	}
	if (GetBots().Num() < 2)
	{
		CreateBot(TEXT("Bo"), BoLocation, North);
	}
}

APawn* ACKSampleCompleteShowcase::BotPawn(const int32 Index) const
{
	return GetBots().IsValidIndex(Index) && IsValid(GetBots()[Index]) ? GetBots()[Index]->GetPawn() : nullptr;
}

AActor* ACKSampleCompleteShowcase::NearestEnemy(const FVector& From) const
{
	AActor* Best = nullptr;
	double BestDistance = TNumericLimits<double>::Max();
	for (AActor* Enemy : Enemies)
	{
		if (IsValid(Enemy))
		{
			const double Distance = FVector::DistSquared(Enemy->GetActorLocation(), From);
			if (Distance < BestDistance)
			{
				BestDistance = Distance;
				Best = Enemy;
			}
		}
	}
	return Best;
}

void ACKSampleCompleteShowcase::StartLoop()
{
	using namespace CKSampleCompleteShowcase;

	EnsureBots();
	const FRotator North(0.0, 90.0, 0.0);
	// A body the enemies downed (or the scenario did) stands up for the next round.
	for (int32 Index = 0; Index < 2; ++Index)
	{
		if (UCKReviveBodyComponent* Body = UCKReviveBodyComponent::FindBodyComponent(BotPawn(Index)))
		{
			if (Body->IsDowned())
			{
				Body->ReviveNow(nullptr);
			}
		}
	}
	if (GetBots().IsValidIndex(AdaIndex))
	{
		PlaceBot(GetBots()[AdaIndex], AdaLocation, North);
	}
	if (GetBots().IsValidIndex(BoIndex))
	{
		PlaceBot(GetBots()[BoIndex], BoLocation, North);
	}
}

void ACKSampleCompleteShowcase::PingEnemy()
{
	using namespace CKSampleCompleteShowcase;

	APlayerController* Ada = GetBots().IsValidIndex(AdaIndex) ? GetBots()[AdaIndex].Get() : nullptr;
	AActor* Enemy = Ada != nullptr && Ada->GetPawn() != nullptr ? NearestEnemy(Ada->GetPawn()->GetActorLocation()) : nullptr;
	UCKPingControllerComponent* Pings = UCKPingControllerComponent::FindControllerComponent(Ada);
	if (Enemy == nullptr || Pings == nullptr)
	{
		return;
	}
	AimBot(Ada, Enemy->GetActorLocation());
	Pings->RequestPing(Enemy->GetActorLocation(), Enemy, nullptr);
}

void ACKSampleCompleteShowcase::TakeSword()
{
	using namespace CKSampleCompleteShowcase;

	APlayerController* Ada = GetBots().IsValidIndex(AdaIndex) ? GetBots()[AdaIndex].Get() : nullptr;
	UCKInventoryComponent* Inventory = Ada != nullptr ? UCKInventoryComponent::FindInventoryComponent(Ada->PlayerState) : nullptr;
	if (Inventory == nullptr)
	{
		return;
	}
	for (ACKInventoryPickup* Pickup : Pickups)
	{
		if (IsValid(Pickup))
		{
			Inventory->RequestPickUp(Pickup);
		}
	}
}

void ACKSampleCompleteShowcase::EquipSword()
{
	using namespace CKSampleCompleteShowcase;

	APlayerController* Ada = GetBots().IsValidIndex(AdaIndex) ? GetBots()[AdaIndex].Get() : nullptr;
	UCKInventoryComponent* Inventory = Ada != nullptr ? UCKInventoryComponent::FindInventoryComponent(Ada->PlayerState) : nullptr;
	if (Inventory == nullptr)
	{
		return;
	}
	const FGameplayTag WeaponSlot = CKInventory::Tags::Slot_Weapon;
	FCKInventoryEntry Equipped;
	if (Inventory->FindEquippedEntry(WeaponSlot, Equipped))
	{
		return;
	}
	const FCKInventoryEntry* Weapon = Inventory->GetEntries().FindByPredicate([&WeaponSlot](const FCKInventoryEntry& Entry)
	{
		return !Entry.IsEquipped() && Entry.Item != nullptr && WeaponSlot.MatchesTag(Entry.Item->GetEquipSlot());
	});
	if (Weapon != nullptr)
	{
		Inventory->RequestEquip(Weapon->EntryId, WeaponSlot);
	}
}

void ACKSampleCompleteShowcase::Strike(const int32 BotIndex)
{
	APlayerController* Bot = GetBots().IsValidIndex(BotIndex) ? GetBots()[BotIndex].Get() : nullptr;
	APawn* Pawn = Bot != nullptr ? Bot->GetPawn() : nullptr;
	UCKCombatComponent* Combat = UCKCombatComponent::FindCombatComponent(Pawn);
	if (Combat == nullptr)
	{
		return;
	}
	if (const AActor* Enemy = NearestEnemy(Pawn->GetActorLocation()))
	{
		AimBot(Bot, Enemy->GetActorLocation());
	}
	Combat->Attack();
}

void ACKSampleCompleteShowcase::DownBo()
{
	using namespace CKSampleCompleteShowcase;

	if (UCKReviveBodyComponent* Body = UCKReviveBodyComponent::FindBodyComponent(BotPawn(BoIndex)))
	{
		if (!Body->IsDowned())
		{
			Body->Down(NearestEnemy(BotPawn(BoIndex)->GetActorLocation()));
		}
	}
}

void ACKSampleCompleteShowcase::ReviveBo()
{
	using namespace CKSampleCompleteShowcase;

	APawn* AdaPawn = BotPawn(AdaIndex);
	APawn* BoPawn = BotPawn(BoIndex);
	if (AdaPawn == nullptr || BoPawn == nullptr)
	{
		return;
	}
	if (UCKReviveBodyComponent* Body = UCKReviveBodyComponent::FindBodyComponent(BoPawn))
	{
		AimBot(GetBots()[AdaIndex], BoPawn->GetActorLocation());
		Body->BeginRevive(AdaPawn);
	}
}

void ACKSampleCompleteShowcase::SaveGame()
{
	if (UCKSaveSubsystem* Saves = UCKSaveSubsystem::GetSaveSubsystem(this))
	{
		Saves->SaveToSlot(SaveSlotName, TEXT("Complete showcase"));
	}
}
