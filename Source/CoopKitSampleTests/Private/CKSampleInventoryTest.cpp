// Copyright 2026 Ivan Iaitckikh. All Rights Reserved.

// CoopKit Inventory in the sample project through the bridge CKSampleInventoryBridge (module CoopKitSampleBridges): on
// L_ProvingGround every player state gets the plugin's inventory and an Ability System Component, every local player the
// inventory keys (F pick up, 1 equip weapon, T give, X drop). The test does not link against the plugin or the bridges: it
// finds components by class name, reads the inventory list and the pick-ups through reflection, reads the weapon slot as
// a Gameplay Tag the inventory publishes and presses the keys by injecting the bridge's Enhanced Input actions on the
// client, the way the player does. At the end the host saves and loads with CoopKit Save's keys (F5, F9): the inventory
// comes back with the player state, with no bridge between the two plugins.

#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "CKSampleNetTestSession.h"
#include "CKSampleTestHelpers.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "EnhancedInputSubsystems.h"
#include "GameFramework/Controller.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "GameplayTagAssetInterface.h"
#include "GameplayTagContainer.h"
#include "InputAction.h"
#include "InputActionValue.h"
#include "Engine/Engine.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/AutomationTest.h"
#include "UObject/UnrealType.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace CKSample::Tests
{
	namespace InventoryTest
	{
		const TCHAR* const Map = TEXT("/Game/CoopKitSample/Maps/L_ProvingGround");
		const TCHAR* const PickUpActionPath = TEXT("/Game/CoopKitSample/Input/IA_InventoryPickUp.IA_InventoryPickUp");
		const TCHAR* const EquipActionPath = TEXT("/Game/CoopKitSample/Input/IA_InventoryEquipWeapon.IA_InventoryEquipWeapon");
		const TCHAR* const GiveActionPath = TEXT("/Game/CoopKitSample/Input/IA_InventoryGive.IA_InventoryGive");
		const TCHAR* const DropActionPath = TEXT("/Game/CoopKitSample/Input/IA_InventoryDrop.IA_InventoryDrop");
		const TCHAR* const SwordPath = TEXT("/CoopKitInventory/Data/DA_InventoryDemoSword.DA_InventoryDemoSword");
		const TCHAR* const PotionPath = TEXT("/CoopKitInventory/Data/DA_InventoryDemoPotion.DA_InventoryDemoPotion");
		const TCHAR* const WeaponAbilityPath = TEXT("/CoopKitInventory/Blueprints/GA_InventoryDemoWeaponReady.GA_InventoryDemoWeaponReady_C");
		const TCHAR* const SaveActionPath = TEXT("/Game/CoopKitSample/Input/IA_SaveGame.IA_SaveGame");
		const TCHAR* const LoadActionPath = TEXT("/Game/CoopKitSample/Input/IA_LoadGame.IA_LoadGame");

		/** Files of the save bridge's slot Quick with CoopKit Save's default profile (prefix CoopKit_) and its slot list. */
		const TCHAR* const QuickSlotFile = TEXT("CoopKit_Quick");
		const TCHAR* const SlotListFile = TEXT("CoopKit_Index");

		/** Potions every player starts with (DA_InventoryDemoRules). */
		constexpr int32 StartingPotions = 2;

		/** A request goes client -> server -> every machine: two lag legs plus a replication update. */
		constexpr int32 RequestTicks = 90;

		UActorComponent* FindInventory(const APlayerState* PlayerState)
		{
			return FindComponentNamed(PlayerState, TEXT("CKInventoryComponent"));
		}

		/** Units of the item (asset path) in an inventory component, read through reflection: Items.Entries[].Item/Count. */
		int32 CountUnits(const UActorComponent* Inventory, const TCHAR* ItemPath)
		{
			const FStructProperty* ItemsProperty = Inventory != nullptr ? FindFProperty<FStructProperty>(Inventory->GetClass(), TEXT("Items")) : nullptr;
			const FArrayProperty* EntriesProperty = ItemsProperty != nullptr ? FindFProperty<FArrayProperty>(ItemsProperty->Struct, TEXT("Entries")) : nullptr;
			const FStructProperty* EntryProperty = EntriesProperty != nullptr ? CastField<FStructProperty>(EntriesProperty->Inner) : nullptr;
			const FObjectPropertyBase* ItemProperty = EntryProperty != nullptr ? FindFProperty<FObjectPropertyBase>(EntryProperty->Struct, TEXT("Item")) : nullptr;
			const FIntProperty* CountProperty = EntryProperty != nullptr ? FindFProperty<FIntProperty>(EntryProperty->Struct, TEXT("Count")) : nullptr;
			if (ItemProperty == nullptr || CountProperty == nullptr)
			{
				return INDEX_NONE;
			}
			const void* List = ItemsProperty->ContainerPtrToValuePtr<void>(Inventory);
			FScriptArrayHelper Entries(EntriesProperty, EntriesProperty->ContainerPtrToValuePtr<void>(List));
			int32 Units = 0;
			for (int32 Index = 0; Index < Entries.Num(); ++Index)
			{
				const UObject* Item = ItemProperty->GetObjectPropertyValue_InContainer(Entries.GetRawPtr(Index));
				Units += Item != nullptr && Item->GetPathName() == ItemPath ? CountProperty->GetPropertyValue_InContainer(Entries.GetRawPtr(Index)) : 0;
			}
			return Units;
		}

		int32 CountUnitsOf(const UWorld* World, const int32 PlayerId, const TCHAR* ItemPath)
		{
			const AGameStateBase* GameState = World != nullptr ? World->GetGameState() : nullptr;
			if (GameState != nullptr)
			{
				for (const APlayerState* Player : GameState->PlayerArray)
				{
					if (Player != nullptr && Player->GetPlayerId() == PlayerId)
					{
						return CountUnits(FindInventory(Player), ItemPath);
					}
				}
			}
			return INDEX_NONE;
		}

		/** Pick-ups of the item lying in the world (class CKInventoryPickup, Stack.Item read through reflection). */
		TArray<AActor*> FindPickups(UWorld* World, const TCHAR* ItemPath)
		{
			TArray<AActor*> Found;
			for (TActorIterator<AActor> It(World); It; ++It)
			{
				if (It->GetClass()->GetName() != TEXT("CKInventoryPickup") || !IsValid(*It))
				{
					continue;
				}
				const FStructProperty* StackProperty = FindFProperty<FStructProperty>(It->GetClass(), TEXT("Stack"));
				const FObjectPropertyBase* ItemProperty = StackProperty != nullptr ? FindFProperty<FObjectPropertyBase>(StackProperty->Struct, TEXT("Item")) : nullptr;
				const UObject* Item = ItemProperty != nullptr ? ItemProperty->GetObjectPropertyValue_InContainer(StackProperty->ContainerPtrToValuePtr<void>(*It)) : nullptr;
				if (Item != nullptr && Item->GetPathName() == ItemPath)
				{
					Found.Add(*It);
				}
			}
			return Found;
		}

		APlayerState* FindPlayer(const UWorld* World, const int32 PlayerId)
		{
			const AGameStateBase* GameState = World != nullptr ? World->GetGameState() : nullptr;
			if (GameState != nullptr)
			{
				for (APlayerState* Player : GameState->PlayerArray)
				{
					if (Player != nullptr && Player->GetPlayerId() == PlayerId)
					{
						return Player;
					}
				}
			}
			return nullptr;
		}

		/** True when the player's inventory publishes the tag (IGameplayTagAssetInterface of the component). */
		bool InventoryHasTag(const UWorld* World, const int32 PlayerId, const FGameplayTag& Tag)
		{
			const IGameplayTagAssetInterface* Tags = Cast<IGameplayTagAssetInterface>(FindInventory(FindPlayer(World, PlayerId)));
			return Tags != nullptr && Tags->HasMatchingGameplayTag(Tag);
		}

		/** Moves a pawn on the server and its owning client: replicated movement skips the owner (autonomous proxy). */
		void MoveInventoryPawn(APawn& Pawn, const FVector& Target)
		{
			Pawn.SetActorLocation(Target, false, nullptr, ETeleportType::TeleportPhysics);
			AController* Controller = Pawn.GetController();
			if (Controller != nullptr && !Controller->IsLocalController())
			{
				Controller->ClientSetLocation(Target, Pawn.GetActorRotation());
			}
		}

		void Press(UWorld* World, const UInputAction* Action)
		{
			const APlayerController* Controller = World != nullptr ? World->GetFirstPlayerController() : nullptr;
			if (UEnhancedInputLocalPlayerSubsystem* Input = Controller != nullptr ? ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(Controller->GetLocalPlayer()) : nullptr)
			{
				Input->InjectInputForAction(Action, FInputActionValue(true), {}, {});
			}
		}

		void DeleteQuickSave()
		{
			UGameplayStatics::DeleteGameInSlot(QuickSlotFile, 0);
			UGameplayStatics::DeleteGameInSlot(SlotListFile, 0);
		}

		/** Latent command: removes the slot files the test wrote, also after a failure. */
		class FDeleteQuickSaveCommand : public IAutomationLatentCommand
		{
		public:
			virtual bool Update() override
			{
				DeleteQuickSave();
				return true;
			}
		};

		/** Index of the player in the server's player array (the argument of the ck.inventory console commands). */
		int32 GetPlayerIndex(const UWorld* World, const int32 PlayerId)
		{
			const AGameStateBase* GameState = World != nullptr ? World->GetGameState() : nullptr;
			return GameState != nullptr ? GameState->PlayerArray.IndexOfByPredicate([PlayerId](const APlayerState* Player) { return Player != nullptr && Player->GetPlayerId() == PlayerId; }) : INDEX_NONE;
		}

		struct FScenario
		{
			int32 HostId = INDEX_NONE;
			int32 ClientId = INDEX_NONE;
			FVector ClientSpot = FVector::ZeroVector;
			FVector HostSpot = FVector::ZeroVector;
			int32 PotionPickupsBefore = 0;
		};
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCKSampleInventoryListenTest, "CoopKit.Sample.Inventory.ListenServer",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCKSampleInventoryListenTest::RunTest(const FString& Parameters)
{
	using namespace CKSample::Tests;
	using namespace CKSample::Tests::InventoryTest;

	FNetSessionSettings Settings;
	Settings.Host = ENetSessionHost::ListenServer;
	Settings.RemoteClients = 1;
	const TSharedRef<FNetTestSession> Session = MakeShared<FNetTestSession>(Settings);
	const TSharedRef<FScenario> Scenario = MakeShared<FScenario>();
	const TWeakObjectPtr<const UInputAction> PickUp = LoadObject<UInputAction>(nullptr, PickUpActionPath);
	const TWeakObjectPtr<const UInputAction> Equip = LoadObject<UInputAction>(nullptr, EquipActionPath);
	const TWeakObjectPtr<const UInputAction> Give = LoadObject<UInputAction>(nullptr, GiveActionPath);
	const TWeakObjectPtr<const UInputAction> Drop = LoadObject<UInputAction>(nullptr, DropActionPath);
	const TWeakObjectPtr<UClass> WeaponAbility = LoadClass<UObject>(nullptr, WeaponAbilityPath);
	const TWeakObjectPtr<const UInputAction> SaveAction = LoadObject<UInputAction>(nullptr, SaveActionPath);
	const TWeakObjectPtr<const UInputAction> LoadAction = LoadObject<UInputAction>(nullptr, LoadActionPath);
	const FGameplayTag WeaponSlot = FGameplayTag::RequestGameplayTag(TEXT("CK.Inventory.Slot.Weapon"), false);
	if (!TestTrue(TEXT("Input actions of the inventory bridge exist"), PickUp.IsValid() && Equip.IsValid() && Give.IsValid() && Drop.IsValid())
		|| !TestTrue(TEXT("The plugin's demo ability and weapon slot tag exist"), WeaponAbility.IsValid() && WeaponSlot.IsValid())
		|| !TestTrue(TEXT("Input actions of the save bridge exist"), SaveAction.IsValid() && LoadAction.IsValid()))
	{
		return false;
	}
	DeleteQuickSave();

	TArray<FNetStep> Steps;
	Steps.Add({ TEXT("Both player states have an inventory with the starting potions and an ASC; both local players have the inventory keys"), nullptr,
		[Session]()
		{
			UWorld* Server = Session->GetServerWorld();
			const TArray<UWorld*> Clients = Session->GetClientWorlds();
			if (Clients.IsEmpty() || !HasComponentNamed(Server->GetFirstPlayerController(), TEXT("CKSampleInventoryInputComponent"))
				|| !HasComponentNamed(Clients[0]->GetFirstPlayerController(), TEXT("CKSampleInventoryInputComponent")))
			{
				return false;
			}
			for (UWorld* World : { Server, Clients[0] })
			{
				const AGameStateBase* GameState = World->GetGameState();
				if (GameState == nullptr || GameState->PlayerArray.Num() != 2)
				{
					return false;
				}
				for (const APlayerState* Player : GameState->PlayerArray)
				{
					if (CountUnits(FindInventory(Player), PotionPath) != StartingPotions || Player->GetPawn() == nullptr
						|| (World == Server && UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Player) == nullptr))
					{
						return false;
					}
				}
			}
			return true;
		}, 150 });

	Steps.Add({ TEXT("The server moves the client next to the sword and the host next to the client"),
		[Session, Scenario]()
		{
			UWorld* Server = Session->GetServerWorld();
			const APlayerController* Host = Server->GetFirstPlayerController();
			Scenario->HostId = Host->PlayerState->GetPlayerId();
			Scenario->ClientId = Session->GetClientWorlds()[0]->GetFirstPlayerController()->PlayerState->GetPlayerId();
			const AActor* Sword = FindPickups(Server, SwordPath)[0];
			APawn* ClientPawn = FindPlayer(Server, Scenario->ClientId)->GetPawn();
			Scenario->ClientSpot = FVector(Sword->GetActorLocation().X, Sword->GetActorLocation().Y - 100.0, ClientPawn->GetActorLocation().Z);
			Scenario->HostSpot = Scenario->ClientSpot + FVector(0.0, -200.0, 0.0);
			MoveInventoryPawn(*ClientPawn, Scenario->ClientSpot);
			MoveInventoryPawn(*Host->GetPawn(), Scenario->HostSpot);
		},
		[Session, Scenario]()
		{
			UWorld* Client = Session->GetClientWorlds()[0];
			const APawn* ClientPawn = FindPlayer(Client, Scenario->ClientId)->GetPawn();
			const APawn* HostPawn = FindPlayer(Client, Scenario->HostId)->GetPawn();
			return FVector::Dist2D(ClientPawn->GetActorLocation(), Scenario->ClientSpot) < 30.0 && FVector::Dist2D(HostPawn->GetActorLocation(), Scenario->HostSpot) < 30.0;
		}, 150 });

	Steps.Add({ TEXT("F on the client: the sword moves into the client's inventory on host and client, the pick-up is gone"),
		[Session, PickUp]() { Press(Session->GetClientWorlds()[0], PickUp.Get()); },
		[Session, Scenario]()
		{
			UWorld* Client = Session->GetClientWorlds()[0];
			return CountUnitsOf(Session->GetServerWorld(), Scenario->ClientId, SwordPath) == 1 && CountUnitsOf(Client, Scenario->ClientId, SwordPath) == 1
				&& FindPickups(Session->GetServerWorld(), SwordPath).IsEmpty() && FindPickups(Client, SwordPath).IsEmpty();
		}, RequestTicks });

	Steps.Add({ TEXT("1 on the client: the sword is in the weapon slot on host and client, the server ASC has the demo ability"),
		[Session, Equip]() { Press(Session->GetClientWorlds()[0], Equip.Get()); },
		[Session, Scenario, WeaponSlot, WeaponAbility]()
		{
			UWorld* Server = Session->GetServerWorld();
			const UAbilitySystemComponent* Abilities = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(FindPlayer(Server, Scenario->ClientId));
			return InventoryHasTag(Server, Scenario->ClientId, WeaponSlot) && InventoryHasTag(Session->GetClientWorlds()[0], Scenario->ClientId, WeaponSlot)
				&& Abilities != nullptr && Abilities->FindAbilitySpecFromClass(WeaponAbility.Get()) != nullptr;
		}, RequestTicks });

	Steps.Add({ TEXT("T on the client: one potion goes to the host, on host and client"),
		[Session, Give]() { Press(Session->GetClientWorlds()[0], Give.Get()); },
		[Session, Scenario]()
		{
			UWorld* Client = Session->GetClientWorlds()[0];
			return CountUnitsOf(Client, Scenario->HostId, PotionPath) == StartingPotions + 1 && CountUnitsOf(Client, Scenario->ClientId, PotionPath) == StartingPotions - 1
				&& CountUnitsOf(Session->GetServerWorld(), Scenario->HostId, PotionPath) == StartingPotions + 1;
		}, RequestTicks });

	Steps.Add({ TEXT("X on the client: the last potion lies in front of the client, on host and client"),
		[Session, Scenario, Drop]()
		{
			Scenario->PotionPickupsBefore = FindPickups(Session->GetServerWorld(), PotionPath).Num();
			Press(Session->GetClientWorlds()[0], Drop.Get());
		},
		[Session, Scenario]()
		{
			UWorld* Client = Session->GetClientWorlds()[0];
			return CountUnitsOf(Client, Scenario->ClientId, PotionPath) == 0 && FindPickups(Client, PotionPath).Num() == Scenario->PotionPickupsBefore + 1
				&& FindPickups(Session->GetServerWorld(), PotionPath).Num() == Scenario->PotionPickupsBefore + 1;
		}, RequestTicks });

	// CoopKit Save keeps the inventory with the player state: the list is a SaveGame property, the component is named
	// CKInventory and exists before a save is applied. No bridge links the two plugins.
	Steps.Add({ TEXT("F5 on the host writes the quick save (CoopKit Save)"),
		[Session, SaveAction]() { Press(Session->GetServerWorld(), SaveAction.Get()); },
		[]() { return UGameplayStatics::DoesSaveGameExist(QuickSlotFile, 0); }, 300 });

	Steps.Add({ TEXT("The server empties the client's inventory (ck.inventory.Clear): the sword leaves the hands on host and client"),
		[Session, Scenario]()
		{
			UWorld* Server = Session->GetServerWorld();
			GEngine->Exec(Server, *FString::Printf(TEXT("ck.inventory.Clear %d"), GetPlayerIndex(Server, Scenario->ClientId)));
		},
		[Session, Scenario, WeaponSlot]()
		{
			return CountUnitsOf(Session->GetClientWorlds()[0], Scenario->ClientId, SwordPath) == 0 && !InventoryHasTag(Session->GetServerWorld(), Scenario->ClientId, WeaponSlot);
		}, 90 });

	Steps.Add({ TEXT("F9 on the host loads: the client's sword is back in the weapon slot on host and client, with the demo ability"),
		[Session, LoadAction]() { Press(Session->GetServerWorld(), LoadAction.Get()); },
		[Session, Scenario, WeaponSlot, WeaponAbility]()
		{
			UWorld* Server = Session->GetServerWorld();
			const UAbilitySystemComponent* Abilities = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(FindPlayer(Server, Scenario->ClientId));
			return CountUnitsOf(Session->GetClientWorlds()[0], Scenario->ClientId, SwordPath) == 1 && InventoryHasTag(Session->GetClientWorlds()[0], Scenario->ClientId, WeaponSlot)
				&& InventoryHasTag(Server, Scenario->ClientId, WeaponSlot) && Abilities != nullptr && Abilities->FindAbilitySpecFromClass(WeaponAbility.Get()) != nullptr;
		}, 300 });

	if (!Session->Start(*this, Map))
	{
		return false;
	}
	AddInfo(FString::Printf(TEXT("Scenario: %s"), *Session->Describe()));
	AddCommand(new FNetScenarioCommand(*this, Session, MoveTemp(Steps)));
	AddCommand(new FDeleteQuickSaveCommand());
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
