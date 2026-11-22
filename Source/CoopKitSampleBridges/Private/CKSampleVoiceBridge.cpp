// Copyright 2026 Ivan Iaitckikh. All Rights Reserved.

#include "CKSampleVoiceBridge.h"
#include "CKInventoryComponent.h"
#include "CKInventoryItemData.h"
#include "CKSamplePlayerState.h"
#include "CoopPlugins.h"
#include "CKVoiceGameplayTags.h"
#include "CKVoiceProfileData.h"
#include "CKVoiceSubsystem.h"
#include "Engine/World.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "Misc/PackageName.h"

namespace
{
	FGameplayTagQuery HasTagNamed(const TCHAR* TagName)
	{
		const FGameplayTag Tag = FGameplayTag::RequestGameplayTag(FName(TagName), false);
		return Tag.IsValid() ? FGameplayTagQuery::MakeQuery_MatchAnyTags(FGameplayTagContainer(Tag)) : FGameplayTagQuery();
	}
}

void UCKSampleVoiceRadioTags::GetOwnedGameplayTags(FGameplayTagContainer& TagContainer) const
{
	const UCKInventoryComponent* Inventory = UCKInventoryComponent::FindInventoryComponent(GetOwner());
	const UCKSampleVoiceBridge* Bridge = GetWorld() != nullptr ? GetWorld()->GetSubsystem<UCKSampleVoiceBridge>() : nullptr;
	const UCKInventoryItemData* Radio = Bridge != nullptr ? Bridge->GetRadioItem() : nullptr;
	if (Inventory != nullptr && Radio != nullptr && Inventory->GetItemCount(Radio) > 0)
	{
		TagContainer.AddTag(CKVoice::Tags::Item_Radio);
	}
}

bool UCKSampleVoiceBridge::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

FCKVoiceProfile UCKSampleVoiceBridge::MakeSampleProfile()
{
	FCKVoiceProfile Profile = UCKVoiceProfileData::MakePreset(ECKVoicePreset::CoopHorror);
	const FGameplayTagQuery Dead = HasTagNamed(TEXT("CK.Revive.State.Dead"));
	const FGameplayTagQuery Downed = HasTagNamed(TEXT("CK.Revive.State.Downed"));
	if (Dead.IsEmpty() || Downed.IsEmpty() || Profile.Rules.Num() < 3)
	{
		return Profile;
	}
	// The preset's rules, in the same order, on CoopKit Revive's tags instead of CoopKit Voice's own state tags.
	Profile.Rules[0].SpeakerQuery = Dead;
	Profile.Rules[0].ListenerQuery = Dead;
	Profile.Rules[1].SpeakerQuery = Dead;
	Profile.Rules[2].SpeakerQuery = Downed;
	for (FCKVoiceChannel& Channel : Profile.Channels)
	{
		if (Channel.RequiredTag == CKVoice::Tags::State_Dead)
		{
			Channel.RequiredTag = FGameplayTag::RequestGameplayTag(FName(TEXT("CK.Revive.State.Dead")), false);
		}
	}
	return Profile;
}

const UCKInventoryItemData* UCKSampleVoiceBridge::GetRadioItem() const
{
	return RadioItem.LoadSynchronous();
}

void UCKSampleVoiceBridge::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	if (InWorld.GetNetMode() == NM_Client)
	{
		return;
	}
	const FString MapName = InWorld.GetOutermost()->GetName();
	const bool bListed = VoiceMaps.ContainsByPredicate([&MapName](const FString& Map) { return MapName.EndsWith(FPackageName::GetShortName(Map)); });
	if (bListed)
	{
		Activate();
	}
}

void UCKSampleVoiceBridge::Activate()
{
	UWorld* World = GetWorld();
	UCKVoiceSubsystem* Voice = UCKVoiceSubsystem::GetVoiceSubsystem(World);
	if (bActive || World == nullptr || World->GetNetMode() == NM_Client || Voice == nullptr)
	{
		return;
	}
	bActive = true;
	const FCKVoiceProfile Profile = MakeSampleProfile();
	Voice->SetActiveProfile(Profile);
	UE_LOG(LogCKSample, Log, TEXT("CoopKit Voice bridge: the sample's voice profile is on in %s (%d rules, %d channels; Revive tags %s)."),
		*World->GetName(), Profile.Rules.Num(), Profile.Channels.Num(),
		FGameplayTag::RequestGameplayTag(FName(TEXT("CK.Revive.State.Dead")), false).IsValid() ? TEXT("found") : TEXT("missing: the preset's own tags stay"));
	for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
	{
		AddRadioLink(It->Get());
	}
	PostLoginHandle = FGameModeEvents::GameModePostLoginEvent.AddUObject(this, &UCKSampleVoiceBridge::HandlePostLogin);
}

void UCKSampleVoiceBridge::Deinitialize()
{
	FGameModeEvents::GameModePostLoginEvent.Remove(PostLoginHandle);
	Super::Deinitialize();
}

void UCKSampleVoiceBridge::HandlePostLogin(AGameModeBase* GameMode, APlayerController* NewPlayer)
{
	if (GameMode != nullptr && GameMode->GetWorld() == GetWorld())
	{
		AddRadioLink(NewPlayer);
	}
}

void UCKSampleVoiceBridge::AddRadioLink(APlayerController* Controller) const
{
	ACKSamplePlayerState* PlayerState = Controller != nullptr ? Controller->GetPlayerState<ACKSamplePlayerState>() : nullptr;
	if (PlayerState != nullptr && PlayerState->FindComponentByClass<UCKSampleVoiceRadioTags>() == nullptr)
	{
		UCKSampleVoiceRadioTags* Link = NewObject<UCKSampleVoiceRadioTags>(PlayerState, TEXT("CKSampleVoiceRadioTags"));
		Link->RegisterComponent();
		UE_LOG(LogCKSample, Log, TEXT("CoopKit Voice bridge: %s carries the radio link (inventory item %s)."), *PlayerState->GetPlayerName(),
			RadioItem.IsNull() ? TEXT("not set") : *RadioItem.ToString());
	}
}
