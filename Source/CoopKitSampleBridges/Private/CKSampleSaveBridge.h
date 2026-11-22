// Copyright 2026 Ivan Iaitckikh. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Engine/TimerHandle.h"
#include "Subsystems/WorldSubsystem.h"
#include "UObject/SoftObjectPtr.h"
#include "CKSampleSaveBridge.generated.h"

class APlayerController;
class UInputAction;
class UInputMappingContext;
struct FInputActionValue;

/**
 * Bridge: sample project <-> CoopKit Save.
 *
 * CoopKit Save needs no component on the sample's classes: it saves the game state, every player state and every
 * player's pawn (with its position) by itself. This world subsystem only gives the host the quick save keys: on the maps
 * listed in SaveMaps, the local player controller of the listen host (or of a standalone game) gets
 * UCKSampleSaveInputComponent. Clients get no keys: in a co-op game the host saves.
 *
 * Configured in Config/DefaultGame.ini, section [/Script/CoopKitSampleBridges.CKSampleSaveBridge].
 */
UCLASS(Config = Game)
class UCKSampleSaveBridge final : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Deinitialize() override;

protected:
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;

private:
	/** True when the world's map is one of SaveMaps. */
	bool IsSaveMap(const UWorld& World) const;

	/** Adds the input component to the host's local player controller once it is ready for input. */
	void AddInputComponents();

	/** Maps with the quick save keys. The lobby is not listed: nothing there is worth saving. */
	UPROPERTY(Config)
	TArray<FSoftObjectPath> SaveMaps;

	/** Slot of the quick save and quick load. */
	UPROPERTY(Config)
	FString SlotName = TEXT("Quick");

	/** Mapping context with the save keys, added on top of the sample's default context. */
	UPROPERTY(Config)
	TSoftObjectPtr<UInputMappingContext> InputMappingContext;

	/** Priority of InputMappingContext; higher than the default context so that its keys win on conflicts. */
	UPROPERTY(Config)
	int32 InputMappingPriority = 1;

	/** Boolean action: the host saves to SlotName. */
	UPROPERTY(Config)
	TSoftObjectPtr<UInputAction> SaveAction;

	/** Boolean action: the host loads SlotName. */
	UPROPERTY(Config)
	TSoftObjectPtr<UInputAction> LoadAction;

	/** Looks for the host's player controller: in PIE it may get its input component after begin play. */
	FTimerHandle InputTimer;
};

/** Quick save keys of the host in the sample project, on the host's local player controller. Calls UCKSaveSubsystem. */
UCLASS(ClassGroup = "CoopKit Sample", meta = (BlueprintSpawnableComponent))
class UCKSampleSaveInputComponent final : public UActorComponent
{
	GENERATED_BODY()

public:
	UCKSampleSaveInputComponent();

	/** Called by the bridge before the component is registered. */
	void Configure(const FString& InSlotName, UInputMappingContext* InMappingContext, int32 InMappingPriority, UInputAction* InSaveAction, UInputAction* InLoadAction);

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	APlayerController* GetPlayerController() const;

	void HandleSave(const FInputActionValue& Value);
	void HandleLoad(const FInputActionValue& Value);

	FString SlotName;

	UPROPERTY()
	TObjectPtr<UInputMappingContext> MappingContext;

	UPROPERTY()
	TObjectPtr<UInputAction> SaveAction;

	UPROPERTY()
	TObjectPtr<UInputAction> LoadAction;

	int32 MappingPriority = 1;

	/** Binding handles, removed on end play so that a component removed at runtime leaves no dangling bindings. */
	TArray<uint32> BindingHandles;
};
