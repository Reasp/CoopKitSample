// Copyright 2026 Ivan Iaitckikh. All Rights Reserved.

// CoopKit Loot in the sample project through the bridge CKSampleLootInventoryBridge (module CoopKitSampleBridges): on
// L_ProvingGround every player state gets the loot receiver next to its CoopKit Inventory. The test does not link
// against the plugins or the bridges: it finds the loot subsystem and components by class name and calls their Blueprint
// functions through reflection. The server drops the sample chest (personal loot: two potions guaranteed); the client
// picks its own drop up through its loot component, and the potions land in the client's inventory.

#include "CKSampleNetTestSession.h"
#include "CKSampleTestHelpers.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "Misc/AutomationTest.h"
#include "Subsystems/WorldSubsystem.h"
#include "UObject/UnrealType.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace CKSample::Tests
{
	namespace LootTest
	{
		const TCHAR* const Map = TEXT("/Game/CoopKitSample/Maps/L_ProvingGround");
		const TCHAR* const ChestPath = TEXT("/Game/CoopKitSample/Loot/DA_CKSampleLootChest.DA_CKSampleLootChest");
		const TCHAR* const PotionPath = TEXT("/CoopKitInventory/Data/DA_InventoryDemoPotion.DA_InventoryDemoPotion");
		const TCHAR* const SubsystemClassPath = TEXT("/Script/CoopKitLoot.CKLootSubsystem");

		/** Potions every player starts with (DA_InventoryDemoRules) and the chest's guaranteed potions. */
		constexpr int32 LootStartingPotions = 2;
		constexpr int32 ChestPotions = 2;

		/** A request goes client -> server -> every machine: two lag legs plus a replication update. */
		constexpr int32 LootRequestTicks = 120;

		/** Units of the item (asset path) in a player's inventory, read through reflection: Items.Entries[].Item/Count. */
		int32 LootCountUnits(const UWorld* World, const int32 PlayerId, const TCHAR* ItemPath)
		{
			const AGameStateBase* GameState = World != nullptr ? World->GetGameState() : nullptr;
			const APlayerState* Player = nullptr;
			for (const APlayerState* Candidate : GameState != nullptr ? GameState->PlayerArray : TArray<TObjectPtr<APlayerState>>())
			{
				Player = Candidate != nullptr && Candidate->GetPlayerId() == PlayerId ? Candidate : Player;
			}
			const UActorComponent* Inventory = FindComponentNamed(Player, TEXT("CKInventoryComponent"));
			const FStructProperty* ItemsProperty = Inventory != nullptr ? FindFProperty<FStructProperty>(Inventory->GetClass(), TEXT("Items")) : nullptr;
			const FArrayProperty* EntriesProperty = ItemsProperty != nullptr ? FindFProperty<FArrayProperty>(ItemsProperty->Struct, TEXT("Entries")) : nullptr;
			const FStructProperty* EntryProperty = EntriesProperty != nullptr ? CastField<FStructProperty>(EntriesProperty->Inner) : nullptr;
			const FObjectPropertyBase* ItemProperty = EntryProperty != nullptr ? FindFProperty<FObjectPropertyBase>(EntryProperty->Struct, TEXT("Item")) : nullptr;
			const FIntProperty* CountProperty = EntryProperty != nullptr ? FindFProperty<FIntProperty>(EntryProperty->Struct, TEXT("Count")) : nullptr;
			if (ItemProperty == nullptr || CountProperty == nullptr)
			{
				return INDEX_NONE;
			}
			FScriptArrayHelper Entries(EntriesProperty, EntriesProperty->ContainerPtrToValuePtr<void>(ItemsProperty->ContainerPtrToValuePtr<void>(Inventory)));
			int32 Units = 0;
			for (int32 Index = 0; Index < Entries.Num(); ++Index)
			{
				const UObject* Item = ItemProperty->GetObjectPropertyValue_InContainer(Entries.GetRawPtr(Index));
				Units += Item != nullptr && Item->GetPathName() == ItemPath ? CountProperty->GetPropertyValue_InContainer(Entries.GetRawPtr(Index)) : 0;
			}
			return Units;
		}

		/** Loot drops (class CKLootDrop) this world has. */
		TArray<AActor*> LootFindDrops(UWorld* World)
		{
			TArray<AActor*> Found;
			for (TActorIterator<AActor> It(World); It; ++It)
			{
				if (IsValid(*It) && It->GetClass()->GetName() == TEXT("CKLootDrop"))
				{
					Found.Add(*It);
				}
			}
			return Found;
		}

		/** Calls a Blueprint function with one object parameter by name; false when the function is missing. */
		bool LootCallWithObject(UObject* Target, const TCHAR* FunctionName, const TCHAR* ParameterName, UObject* Value)
		{
			UFunction* Function = Target != nullptr ? Target->FindFunction(FunctionName) : nullptr;
			FObjectPropertyBase* Parameter = Function != nullptr ? FindFProperty<FObjectPropertyBase>(Function, ParameterName) : nullptr;
			if (Parameter == nullptr)
			{
				return false;
			}
			uint8* Parameters = static_cast<uint8*>(FMemory_Alloca(Function->ParmsSize));
			FMemory::Memzero(Parameters, Function->ParmsSize);
			Function->InitializeStruct(Parameters);
			Parameter->SetObjectPropertyValue_InContainer(Parameters, Value);
			Target->ProcessEvent(Function, Parameters);
			Function->DestroyStruct(Parameters);
			return true;
		}

		/** Server: Drop Loot of the loot subsystem with the sample chest at Location, through reflection. */
		bool LootDropChest(UWorld* World, const FVector& Location)
		{
			UClass* SubsystemClass = FindObject<UClass>(nullptr, SubsystemClassPath);
			UObject* Subsystem = SubsystemClass != nullptr ? World->GetSubsystemBase(SubsystemClass) : nullptr;
			UObject* Chest = LoadObject<UObject>(nullptr, ChestPath);
			UFunction* Function = Subsystem != nullptr ? Subsystem->FindFunction(TEXT("DropLoot")) : nullptr;
			FObjectPropertyBase* TableParameter = Function != nullptr ? FindFProperty<FObjectPropertyBase>(Function, TEXT("Table")) : nullptr;
			FStructProperty* LocationParameter = Function != nullptr ? FindFProperty<FStructProperty>(Function, TEXT("Location")) : nullptr;
			if (Chest == nullptr || TableParameter == nullptr || LocationParameter == nullptr)
			{
				return false;
			}
			uint8* Parameters = static_cast<uint8*>(FMemory_Alloca(Function->ParmsSize));
			FMemory::Memzero(Parameters, Function->ParmsSize);
			Function->InitializeStruct(Parameters);
			TableParameter->SetObjectPropertyValue_InContainer(Parameters, Chest);
			*LocationParameter->ContainerPtrToValuePtr<FVector>(Parameters) = Location;
			Subsystem->ProcessEvent(Function, Parameters);
			Function->DestroyStruct(Parameters);
			return true;
		}
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCKSampleLootListenTest, "CoopKit.Sample.Loot.ListenServer",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCKSampleLootListenTest::RunTest(const FString& Parameters)
{
	using namespace CKSample::Tests;
	using namespace CKSample::Tests::LootTest;

	FNetSessionSettings Settings;
	Settings.Host = ENetSessionHost::ListenServer;
	Settings.RemoteClients = 1;
	const TSharedRef<FNetTestSession> Session = MakeShared<FNetTestSession>(Settings);
	const TSharedRef<int32> ClientId = MakeShared<int32>(INDEX_NONE);
	FAutomationTestBase* TestPtr = this;

	TArray<FNetStep> Steps;
	Steps.Add({ TEXT("Every player state has the inventory and the loot receiver of the bridge; the client starts with its potions"), nullptr,
		[Session, ClientId]()
		{
			const TArray<UWorld*> Clients = Session->GetClientWorlds();
			const APlayerController* Controller = Clients.IsEmpty() ? nullptr : Clients[0]->GetFirstPlayerController();
			*ClientId = Controller != nullptr && Controller->PlayerState != nullptr ? Controller->PlayerState->GetPlayerId() : INDEX_NONE;
			const AGameStateBase* ServerState = Session->GetServerWorld()->GetGameState();
			if (*ClientId == INDEX_NONE || ServerState == nullptr || ServerState->PlayerArray.Num() != 2)
			{
				return false;
			}
			for (const APlayerState* Player : ServerState->PlayerArray)
			{
				if (!HasComponentNamed(Player, TEXT("CKSampleLootInventoryReceiver")) || !HasComponentNamed(Player, TEXT("CKInventoryComponent")))
				{
					return false;
				}
			}
			return LootCountUnits(Clients[0], *ClientId, PotionPath) == LootStartingPotions;
		}, 300 });

	Steps.Add({ TEXT("The server drops the sample chest (personal loot): the client gets only its own drops, among them two potions"),
		[Session, TestPtr]()
		{
			UWorld* Server = Session->GetServerWorld();
			const APawn* Host = Server->GetFirstPlayerController()->GetPawn();
			TestPtr->TestTrue(TEXT("Drop Loot of the loot subsystem is called with the sample chest"),
				LootDropChest(Server, Host != nullptr ? Host->GetActorLocation() : FVector::ZeroVector));
		},
		[Session]()
		{
			const TArray<UWorld*> Clients = Session->GetClientWorlds();
			if (Clients.IsEmpty())
			{
				return false;
			}
			// Two players each rolled the chest alone; the server has both sets, the client only its own.
			const int32 ServerDrops = LootFindDrops(Session->GetServerWorld()).Num();
			const int32 ClientDrops = LootFindDrops(Clients[0]).Num();
			return ServerDrops >= 4 && ClientDrops >= 2 && ClientDrops * 2 == ServerDrops;
		}, LootRequestTicks });

	Steps.Add({ TEXT("The client picks up each of its drops through its loot component: the potions land in its inventory"),
		[Session]()
		{
			UWorld* Client = Session->GetClientWorlds()[0];
			UActorComponent* LootPlayer = FindComponentNamed(Client->GetFirstPlayerController(), TEXT("CKLootPlayerComponent"));
			for (AActor* Drop : LootFindDrops(Client))
			{
				LootCallWithObject(LootPlayer, TEXT("PickUp"), TEXT("Drop"), Drop);
			}
		},
		[Session, ClientId]()
		{
			UWorld* Client = Session->GetClientWorlds()[0];
			return LootCountUnits(Client, *ClientId, PotionPath) == LootStartingPotions + ChestPotions
				&& LootCountUnits(Session->GetServerWorld(), *ClientId, PotionPath) == LootStartingPotions + ChestPotions;
		}, LootRequestTicks });

	if (!Session->Start(*this, Map))
	{
		return false;
	}
	AddInfo(FString::Printf(TEXT("Scenario: %s"), *Session->Describe()));
	AddCommand(new FNetScenarioCommand(*this, Session, MoveTemp(Steps)));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
