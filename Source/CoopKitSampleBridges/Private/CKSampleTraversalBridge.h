// Copyright 2026 Ivan Iaitckikh. All Rights Reserved.

#pragma once

#include "CKSampleCharacter.h"
#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "UObject/SoftObjectPath.h"
#include "CKSampleTraversalBridge.generated.h"

class AGameModeBase;
class UCKTraversalMovementComponent;

/**
 * The sample's character with CoopKit Traversal's movement component: the same body, camera and input actions (taken
 * from the sample's character Blueprint), plus vaults, mantles and the two-player moves. Jump at an obstacle asks for a
 * move first and jumps only when there is none. CoopKit Revive's downed and dead states (CK.Revive.State.Downed and
 * CK.Revive.State.Dead, read as tags: neither plugin knows the other) stop the moves.
 */
UCLASS(NotBlueprintable)
class ACKSampleTraversalCharacter final : public ACKSampleCharacter
{
	GENERATED_BODY()

public:
	ACKSampleTraversalCharacter(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	virtual void PostInitializeComponents() override;
	virtual void Jump() override;

	/** The character's movement component (CoopKit Traversal's). */
	UCKTraversalMovementComponent* GetTraversalMovement() const;

private:
	/** Takes the editable defaults of the sample's character Blueprint (its input actions) the native class does not have. */
	void CopySampleDefaults();
};

/**
 * Bridge: sample project <-> CoopKit Traversal, with CoopKit Revive.
 *
 * On the maps listed in TraversalMaps the server's game mode gives players ACKSampleTraversalCharacter instead of the
 * sample's character: a character needs the plugin's movement component from its construction, so a component cannot be
 * added later. Jump at a crate vaults or mantles it; a friend's open offer (from the plugin's API) gives the two-player
 * moves. Revive -> Traversal: a downed or dead player makes no move.
 *
 * Configured in Config/DefaultGame.ini, section [/Script/CoopKitSampleBridges.CKSampleTraversalBridge]. The maps come
 * only from that section: without +TraversalMaps lines the bridge is off. The character default works without it.
 */
UCLASS(Config = Game)
class UCKSampleTraversalBridge final : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	UCKSampleTraversalBridge();

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	/** Server: true when this world's game mode gives players the traversal character. */
	UFUNCTION(BlueprintPure, Category = "CoopKitSample")
	bool IsTraversalActive() const
	{
		return bActive;
	}

	/** The sample character Blueprint whose editable defaults (input actions) the traversal character takes. */
	const FSoftClassPath& GetCharacterDefaults() const
	{
		return CharacterDefaults;
	}

protected:
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;

private:
	void HandleGameModeInitialized(AGameModeBase* GameMode);
	bool IsTraversalMap(const UWorld& World) const;

	/** Maps where players get the traversal character. */
	UPROPERTY(Config)
	TArray<FSoftObjectPath> TraversalMaps;

	/** The sample's character Blueprint: the traversal character takes its input actions. */
	UPROPERTY(Config)
	FSoftClassPath CharacterDefaults;

	FDelegateHandle InitializedHandle;
	bool bActive = false;
};
