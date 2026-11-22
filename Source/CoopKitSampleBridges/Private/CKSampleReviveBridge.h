// Copyright 2026 Ivan Iaitckikh. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Engine/TimerHandle.h"
#include "Subsystems/WorldSubsystem.h"
#include "UObject/SoftObjectPtr.h"
#include "CKSampleReviveBridge.generated.h"

class AGameStateBase;
class APlayerController;
class UInputAction;
class UInputMappingContext;
struct FInputActionValue;

/**
 * Bridge: sample project <-> CoopKit Revive.
 *
 * The sample framework (module CoopPlugins) knows nothing about CoopKit Revive. This world subsystem connects them:
 * - server: adds a CoopKit Revive match component to the game state on the maps listed in ReviveMaps. The match
 *   component then adds the player, controller and body components itself (Revive Rules > Add Components);
 * - every machine: adds UCKSampleReviveInputComponent to each local player controller, which maps the revive keys.
 *
 * Configured in Config/DefaultGame.ini, section [/Script/CoopKitSampleBridges.CKSampleReviveBridge]. The sample has
 * no health: down a player with the console command ck.revive.Down <player index> on the server.
 */
UCLASS(Config = Game)
class UCKSampleReviveBridge final : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Deinitialize() override;

protected:
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;

private:
	/** True when the world's map is one of ReviveMaps. */
	bool IsReviveMap(const UWorld& World) const;

	/** Server: adds the match component unless the game state already has one (from a Blueprint, for example). */
	void AddMatchComponent(AGameStateBase& GameState) const;

	/** Adds the input component to local player controllers that are ready for input and do not have it yet. */
	void AddInputComponents();

	/** Maps that use CoopKit Revive. Other maps (the lobby) stay untouched. */
	UPROPERTY(Config)
	TArray<FSoftObjectPath> ReviveMaps;

	/** Mapping context with the revive keys, added on top of the sample's default context. */
	UPROPERTY(Config)
	TSoftObjectPtr<UInputMappingContext> InputMappingContext;

	/** Priority of InputMappingContext; higher than the default context so that its keys win on conflicts. */
	UPROPERTY(Config)
	int32 InputMappingPriority = 1;

	/** Boolean action held to revive the nearest downed teammate. */
	UPROPERTY(Config)
	TSoftObjectPtr<UInputAction> ReviveAction;

	/** Boolean action: a downed player gives up and dies at once. */
	UPROPERTY(Config)
	TSoftObjectPtr<UInputAction> GiveUpAction;

	/** Boolean action: a spectating player watches the next living teammate. */
	UPROPERTY(Config)
	TSoftObjectPtr<UInputAction> SpectateNextAction;

	/** Boolean action: a spectating player watches the previous living teammate. */
	UPROPERTY(Config)
	TSoftObjectPtr<UInputAction> SpectatePreviousAction;

	/** Every machine: lays downed characters on the floor and stands the revived ones up (visual only). */
	void UpdateBodyPoses();

	/** Looks for new local player controllers: on a client they arrive by replication some time after begin play. */
	FTimerHandle InputTimer;

	/** Reads the replicated downed state of every character and poses its body. */
	FTimerHandle PoseTimer;
};

/**
 * Local input of CoopKit Revive in the sample project, on a local player controller.
 * Binds the bridge's input actions on the controller's Enhanced Input component (bindings survive pawn changes, so
 * spectating without a pawn still works) and calls the CoopKit Revive controller component, which relays to the server.
 */
UCLASS(ClassGroup = "CoopKit Sample", meta = (BlueprintSpawnableComponent))
class UCKSampleReviveInputComponent final : public UActorComponent
{
	GENERATED_BODY()

public:
	UCKSampleReviveInputComponent();

	/** Called by the bridge before the component is registered. */
	void Configure(UInputMappingContext* InMappingContext, int32 InMappingPriority, UInputAction* InReviveAction, UInputAction* InGiveUpAction,
		UInputAction* InSpectateNextAction, UInputAction* InSpectatePreviousAction);

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	APlayerController* GetPlayerController() const;

	void HandleReviveStarted(const FInputActionValue& Value);
	void HandleReviveStopped(const FInputActionValue& Value);
	void HandleGiveUp(const FInputActionValue& Value);
	void HandleSpectateNext(const FInputActionValue& Value);
	void HandleSpectatePrevious(const FInputActionValue& Value);

	UPROPERTY()
	TObjectPtr<UInputMappingContext> MappingContext;

	UPROPERTY()
	TObjectPtr<UInputAction> ReviveAction;

	UPROPERTY()
	TObjectPtr<UInputAction> GiveUpAction;

	UPROPERTY()
	TObjectPtr<UInputAction> SpectateNextAction;

	UPROPERTY()
	TObjectPtr<UInputAction> SpectatePreviousAction;

	int32 MappingPriority = 1;

	/** Binding handles, removed on end play so that a component removed at runtime leaves no dangling bindings. */
	TArray<uint32> BindingHandles;
};
