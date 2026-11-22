// Copyright 2026 Ivan Iaitckikh. All Rights Reserved.

#pragma once

#include "CKVoiceTypes.h"
#include "Components/ActorComponent.h"
#include "CoreMinimal.h"
#include "GameplayTagAssetInterface.h"
#include "Subsystems/WorldSubsystem.h"
#include "UObject/SoftObjectPtr.h"
#include "CKSampleVoiceBridge.generated.h"

class AGameModeBase;
class APlayerController;
class UCKInventoryItemData;

/**
 * Bridge component: CoopKit Voice <-> CoopKit Inventory. On a sample player state it publishes CK.Voice.Item.Radio while
 * the player's CoopKit Inventory holds the bridge's radio item, so the voice profile's radio channel (which requires that
 * tag) works with an inventory item. Neither plugin knows the other: this component is the whole link.
 */
UCLASS(ClassGroup = (CoopKitSample))
class UCKSampleVoiceRadioTags final : public UActorComponent, public IGameplayTagAssetInterface
{
	GENERATED_BODY()

public:
	virtual void GetOwnedGameplayTags(FGameplayTagContainer& TagContainer) const override;
};

/**
 * Bridge: sample project <-> CoopKit Voice, with CoopKit Revive (by tags only) and CoopKit Inventory.
 *
 * On the maps listed in VoiceMaps (or after Activate) the server sets the sample's voice profile: the Coop Horror preset
 * whose rules read CoopKit Revive's own tags (CK.Revive.State.Downed whispers, CK.Revive.State.Dead is silent to the living
 * and hears the dead everywhere). CoopKit Revive publishes them on the pawn and CoopKit Voice reads them there: no code
 * links the two. Every player state gets UCKSampleVoiceRadioTags, so the radio item of the inventory is the voice's radio.
 * Teams: the sample's co-op players are one team (CoopKit Party is a lobby before the game and gives no team in it).
 *
 * Configured in Config/DefaultGame.ini, section [/Script/CoopKitSampleBridges.CKSampleVoiceBridge].
 */
UCLASS(Config = Game)
class UCKSampleVoiceBridge final : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	/** The sample's voice profile: Coop Horror with CoopKit Revive's tags in its rules. */
	static FCKVoiceProfile MakeSampleProfile();

	/** Server: switches the sample's voice on in this world (profile, radio link for every player). Tests call it by name. */
	UFUNCTION(BlueprintCallable, Category = "CoopKitSample")
	void Activate();

	bool IsActive() const
	{
		return bActive;
	}

	/** The inventory item that is the voice's radio (Radio Item in the config); null when none is set. */
	const UCKInventoryItemData* GetRadioItem() const;

	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Deinitialize() override;

protected:
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;

private:
	void AddRadioLink(APlayerController* Controller) const;
	void HandlePostLogin(AGameModeBase* GameMode, APlayerController* NewPlayer);

	/** Maps where the sample's voice is on when the match starts (package names). Empty: only after Activate. */
	UPROPERTY(Config)
	TArray<FString> VoiceMaps;

	/** CoopKit Inventory item that is the voice's radio. */
	UPROPERTY(Config)
	TSoftObjectPtr<UCKInventoryItemData> RadioItem;

	bool bActive = false;
	FDelegateHandle PostLoginHandle;
};
