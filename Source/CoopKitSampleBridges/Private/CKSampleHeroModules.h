// Copyright 2026 Ivan Iaitckikh. All Rights Reserved.

#pragma once

#include "CKHeroModule.h"
#include "CoreMinimal.h"
#include "CKSampleHeroModules.generated.h"

class UCKCombatProfileData;
class UCKInventoryItemData;
class UCKReviveBodyData;

/**
 * Hero module (bridge CoopKit Heroes -> CoopKit Combat): the hero fights with its own combat profile (health, stamina,
 * armor, combo). The server sets the profile on the pawn's combat component and refills the attributes; when the
 * player stops playing the hero, the pawn goes back to the project's default profile.
 */
UCLASS(meta = (DisplayName = "Combat Profile (CoopKit Combat)"))
class UCKSampleHeroCombatModule final : public UCKHeroModule
{
	GENERATED_BODY()

public:
	virtual void ApplyToPawn_Implementation(APawn* Pawn, APlayerState* Player) const override;
	virtual void RemoveFromPawn_Implementation(APawn* Pawn, APlayerState* Player) const override;
	virtual FText GetSummary_Implementation() const override;
	virtual void CollectErrors_Implementation(TArray<FText>& OutErrors) const override;

	const UCKCombatProfileData* GetProfile() const
	{
		return Profile;
	}

private:
	/** Combat profile of the hero. Required. */
	UPROPERTY(EditAnywhere, Category = "Combat", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UCKCombatProfileData> Profile;
};

/** One item the hero starts with. BlueprintType: the sample's content script fills it from Python. */
USTRUCT(BlueprintType)
struct FCKSampleHeroStartingItem
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Inventory")
	TObjectPtr<UCKInventoryItemData> Item;

	/** The player gets this many when they have fewer. */
	UPROPERTY(EditAnywhere, Category = "Inventory", meta = (ClampMin = "1"))
	int32 Count = 1;
};

/**
 * Hero module (bridge CoopKit Heroes -> CoopKit Inventory): the hero's starting items. Every time the hero is applied to
 * a pawn (spawn, respawn, switching to the hero), the server tops the player's inventory up to the counts; items the
 * player already has count, so a respawn does not duplicate them. Items stay with the player when the hero changes.
 */
UCLASS(meta = (DisplayName = "Starting Items (CoopKit Inventory)"))
class UCKSampleHeroInventoryModule final : public UCKHeroModule
{
	GENERATED_BODY()

public:
	virtual void ApplyToPawn_Implementation(APawn* Pawn, APlayerState* Player) const override;
	virtual FText GetSummary_Implementation() const override;
	virtual void CollectErrors_Implementation(TArray<FText>& OutErrors) const override;

	const TArray<FCKSampleHeroStartingItem>& GetStartingItems() const
	{
		return StartingItems;
	}

private:
	UPROPERTY(EditAnywhere, Category = "Inventory", meta = (AllowPrivateAccess = "true"))
	TArray<FCKSampleHeroStartingItem> StartingItems;
};

/**
 * Hero module (bridge CoopKit Heroes -> CoopKit Revive): how the hero goes down and gets up (bleed-out time, downs
 * before death, revive time). The server gives the pawn's Revive body the hero's body data; when CoopKit Revive has not
 * given the pawn its body yet, the module adds it with this data (the match component keeps an existing body).
 */
UCLASS(meta = (DisplayName = "Revive Body (CoopKit Revive)"))
class UCKSampleHeroReviveModule final : public UCKHeroModule
{
	GENERATED_BODY()

public:
	virtual void ApplyToPawn_Implementation(APawn* Pawn, APlayerState* Player) const override;
	virtual void RemoveFromPawn_Implementation(APawn* Pawn, APlayerState* Player) const override;
	virtual FText GetSummary_Implementation() const override;
	virtual void CollectErrors_Implementation(TArray<FText>& OutErrors) const override;

	const UCKReviveBodyData* GetBodyData() const
	{
		return BodyData;
	}

private:
	/** Body data of the hero. Required. */
	UPROPERTY(EditAnywhere, Category = "Revive", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UCKReviveBodyData> BodyData;
};
