// Copyright 2026 Ivan Iaitckikh. All Rights Reserved.

// CoopKit Craft in the sample project through the bridge CKSampleCraftBridge (module CoopKitSampleBridges): on
// L_ProvingGround the server spawns the sample workbench, every player state gets the inventory link to its CoopKit
// Inventory and every player controller the craft player component. The test does not link against the plugins or the
// bridges: it finds the components by class name and calls their Blueprint functions through reflection. The host
// queues "armor from potions" and brings one potion, CoopKit Save captures the world, the client brings the other
// potion; the load puts the job back as it was saved (the client's potion back in its inventory), the client brings it
// again, and the armor lands in the host's CoopKit Inventory.

#include "CKSampleNetTestSession.h"
#include "CKSampleTestHelpers.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "Misc/AutomationTest.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "UObject/UnrealType.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace CKSample::Tests
{
	namespace CraftTest
	{
		const TCHAR* const Map = TEXT("/Game/CoopKitSample/Maps/L_ProvingGround");
		const TCHAR* const RecipePath = TEXT("/Game/CoopKitSample/Craft/DA_CKSampleRecipeArmor.DA_CKSampleRecipeArmor");
		const TCHAR* const PotionPath = TEXT("/CoopKitInventory/Data/DA_InventoryDemoPotion.DA_InventoryDemoPotion");
		const TCHAR* const ArmorPath = TEXT("/CoopKitInventory/Data/DA_InventoryDemoArmor.DA_InventoryDemoArmor");
		const TCHAR* const SaveSubsystemPath = TEXT("/Script/CoopKitSave.CKSaveSubsystem");

		/** Potions every player starts with (DA_InventoryDemoRules). */
		constexpr int32 CraftStartingPotions = 2;

		/** A request goes client -> server -> every machine: two lag legs plus a replication update. */
		constexpr int32 CraftRequestTicks = 120;

		/** A Blueprint function call through reflection: parameters by name, then Call. */
		class FCraftCall
		{
		public:
			FCraftCall(UObject* InTarget, const TCHAR* FunctionName)
				: Target(InTarget)
			{
				Function = Target != nullptr ? Target->FindFunction(FunctionName) : nullptr;
				if (Function != nullptr)
				{
					Parameters.SetNumZeroed(Function->ParmsSize);
					Function->InitializeStruct(Parameters.GetData());
				}
			}

			~FCraftCall()
			{
				if (Function != nullptr)
				{
					Function->DestroyStruct(Parameters.GetData());
				}
			}

			bool IsValid() const
			{
				return Function != nullptr;
			}

			template <typename InValueType>
			FCraftCall& Set(const TCHAR* Name, const InValueType& Value)
			{
				if (const FProperty* Property = Function != nullptr ? FindFProperty<FProperty>(Function, Name) : nullptr)
				{
					*Property->ContainerPtrToValuePtr<InValueType>(Parameters.GetData()) = Value;
				}
				return *this;
			}

			template <typename InValueType>
			InValueType Get(const TCHAR* Name) const
			{
				const FProperty* Property = Function != nullptr ? FindFProperty<FProperty>(Function, Name) : nullptr;
				return Property != nullptr ? *Property->ContainerPtrToValuePtr<InValueType>(Parameters.GetData()) : InValueType();
			}

			bool Call()
			{
				if (Function == nullptr)
				{
					return false;
				}
				Target->ProcessEvent(Function, Parameters.GetData());
				return true;
			}

		private:
			UObject* Target = nullptr;
			UFunction* Function = nullptr;
			TArray<uint8> Parameters;
		};

		APlayerState* CraftFindPlayer(const UWorld* World, const int32 PlayerId)
		{
			const AGameStateBase* GameState = World != nullptr ? World->GetGameState() : nullptr;
			for (APlayerState* Candidate : GameState != nullptr ? GameState->PlayerArray : TArray<TObjectPtr<APlayerState>>())
			{
				if (Candidate != nullptr && Candidate->GetPlayerId() == PlayerId)
				{
					return Candidate;
				}
			}
			return nullptr;
		}

		/** Units of the item (asset path) in a player's CoopKit Inventory, read through reflection: Items.Entries[].Item/Count. */
		int32 CraftCountUnits(const UWorld* World, const int32 PlayerId, const TCHAR* ItemPath)
		{
			const UActorComponent* Inventory = FindComponentNamed(CraftFindPlayer(World, PlayerId), TEXT("CKInventoryComponent"));
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

		/** The station component of the sample workbench (class CKSampleCraftWorkbench); null before it exists or replicated. */
		UActorComponent* CraftFindStation(UWorld* World)
		{
			if (World == nullptr)
			{
				return nullptr;
			}
			for (TActorIterator<AActor> It(World); It; ++It)
			{
				if (IsValid(*It) && It->GetClass()->GetName() == TEXT("CKSampleCraftWorkbench"))
				{
					return FindComponentNamed(*It, TEXT("CKCraftStationComponent"));
				}
			}
			return nullptr;
		}

		/** Jobs in the station's queue (property Jobs); INDEX_NONE without a station. */
		int32 CraftJobCount(const UActorComponent* Station)
		{
			const FArrayProperty* JobsProperty = Station != nullptr ? FindFProperty<FArrayProperty>(Station->GetClass(), TEXT("Jobs")) : nullptr;
			if (JobsProperty == nullptr)
			{
				return INDEX_NONE;
			}
			FScriptArrayHelper Jobs(JobsProperty, JobsProperty->ContainerPtrToValuePtr<void>(Station));
			return Jobs.Num();
		}

		/** Id of the first job in the queue; 0 when the queue is empty. */
		int32 CraftFirstJobId(const UActorComponent* Station)
		{
			const FArrayProperty* JobsProperty = Station != nullptr ? FindFProperty<FArrayProperty>(Station->GetClass(), TEXT("Jobs")) : nullptr;
			const FStructProperty* JobProperty = JobsProperty != nullptr ? CastField<FStructProperty>(JobsProperty->Inner) : nullptr;
			const FIntProperty* IdProperty = JobProperty != nullptr ? FindFProperty<FIntProperty>(JobProperty->Struct, TEXT("JobId")) : nullptr;
			if (IdProperty == nullptr)
			{
				return 0;
			}
			FScriptArrayHelper Jobs(JobsProperty, JobsProperty->ContainerPtrToValuePtr<void>(Station));
			return Jobs.Num() > 0 ? IdProperty->GetPropertyValue_InContainer(Jobs.GetRawPtr(0)) : 0;
		}

		/** Get Missing Units of the station for input 0 of the job. */
		int32 CraftMissing(UActorComponent* Station, const int32 JobId)
		{
			FCraftCall Call(Station, TEXT("GetMissingUnits"));
			Call.Set<int32>(TEXT("JobId"), JobId).Set<int32>(TEXT("InputIndex"), 0);
			return Call.Call() ? Call.Get<int32>(TEXT("ReturnValue")) : INDEX_NONE;
		}

		/** Get Contributed Units of the station for the player. */
		int32 CraftContributed(UActorComponent* Station, const int32 JobId, APlayerState* Player)
		{
			FCraftCall Call(Station, TEXT("GetContributedUnits"));
			Call.Set<int32>(TEXT("JobId"), JobId).Set<APlayerState*>(TEXT("Player"), Player);
			return Call.Call() ? Call.Get<int32>(TEXT("ReturnValue")) : INDEX_NONE;
		}

		/** The craft player component of the world's first local player controller. */
		UActorComponent* CraftLocalHands(UWorld* World)
		{
			return World != nullptr ? FindComponentNamed(World->GetFirstPlayerController(), TEXT("CKCraftPlayerComponent")) : nullptr;
		}

		/** Request Contribute of the world's local player; false while its station or player component is missing. */
		bool CraftRequestContribute(UWorld* World, const int32 JobId, const int32 MaxUnits)
		{
			UActorComponent* Station = CraftFindStation(World);
			FCraftCall Call(CraftLocalHands(World), TEXT("RequestContribute"));
			Call.Set<UObject*>(TEXT("Station"), Station).Set<int32>(TEXT("JobId"), JobId).Set<int32>(TEXT("MaxUnits"), MaxUnits);
			return Station != nullptr && Call.Call();
		}

		UObject* CraftSaveSubsystem(const UWorld* World)
		{
			UClass* SubsystemClass = FindObject<UClass>(nullptr, SaveSubsystemPath);
			UGameInstance* GameInstance = World != nullptr ? World->GetGameInstance() : nullptr;
			return SubsystemClass != nullptr && GameInstance != nullptr ? GameInstance->GetSubsystemBase(SubsystemClass) : nullptr;
		}

		struct FCraftScenario
		{
			int32 HostId = INDEX_NONE;
			int32 ClientId = INDEX_NONE;
			int32 JobId = 0;
			int32 HostArmorBefore = 0;
			bool bContributedAgain = false;
			TArray<uint8> Saved;
		};
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCKSampleCraftListenTest, "CoopKit.Sample.Craft.ListenServer",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCKSampleCraftListenTest::RunTest(const FString& Parameters)
{
	using namespace CKSample::Tests;
	using namespace CKSample::Tests::CraftTest;

	FNetSessionSettings Settings;
	Settings.Host = ENetSessionHost::ListenServer;
	Settings.RemoteClients = 1;
	const TSharedRef<FNetTestSession> Session = MakeShared<FNetTestSession>(Settings);
	const TSharedRef<FCraftScenario> Scenario = MakeShared<FCraftScenario>();
	FAutomationTestBase* TestPtr = this;

	TArray<FNetStep> Steps;
	Steps.Add({ TEXT("The server spawned the sample workbench; every player state has the inventory link, the client has its craft player component"), nullptr,
		[Session, Scenario]()
		{
			const TArray<UWorld*> Clients = Session->GetClientWorlds();
			UWorld* Server = Session->GetServerWorld();
			const APlayerController* ClientController = Clients.IsEmpty() ? nullptr : Clients[0]->GetFirstPlayerController();
			const APlayerController* HostController = Server->GetFirstPlayerController();
			if (ClientController == nullptr || ClientController->PlayerState == nullptr || HostController == nullptr || HostController->PlayerState == nullptr)
			{
				return false;
			}
			Scenario->ClientId = ClientController->PlayerState->GetPlayerId();
			Scenario->HostId = HostController->PlayerState->GetPlayerId();
			for (const APlayerState* Player : Server->GetGameState()->PlayerArray)
			{
				if (!HasComponentNamed(Player, TEXT("CKSampleCraftInventory")))
				{
					return false;
				}
			}
			Scenario->HostArmorBefore = CraftCountUnits(Server, Scenario->HostId, ArmorPath);
			return CraftFindStation(Server) != nullptr && CraftFindStation(Clients[0]) != nullptr && CraftLocalHands(Clients[0]) != nullptr
				&& CraftCountUnits(Clients[0], Scenario->ClientId, PotionPath) == CraftStartingPotions;
		}, 300 });

	Steps.Add({ TEXT("The host queues armor from potions and brings one potion from its CoopKit Inventory; the client sees one potion missing"),
		[Session, Scenario, TestPtr]()
		{
			UWorld* Server = Session->GetServerWorld();
			UObject* Recipe = LoadObject<UObject>(nullptr, RecipePath);
			FCraftCall Queue(CraftLocalHands(Server), TEXT("RequestQueue"));
			Queue.Set<UObject*>(TEXT("Station"), CraftFindStation(Server)).Set<UObject*>(TEXT("Recipe"), Recipe);
			TestPtr->TestTrue(TEXT("Request Queue of the host's craft player component is called"), Recipe != nullptr && Queue.Call());
			Scenario->JobId = CraftFirstJobId(CraftFindStation(Server));
			TestPtr->TestTrue(TEXT("The job is in the queue"), Scenario->JobId > 0);
			TestPtr->TestTrue(TEXT("Request Contribute (one potion) is called"), CraftRequestContribute(Server, Scenario->JobId, 1));
		},
		[Session, Scenario]()
		{
			UWorld* Client = Session->GetClientWorlds()[0];
			return CraftMissing(CraftFindStation(Client), Scenario->JobId) == 1
				&& CraftCountUnits(Session->GetServerWorld(), Scenario->HostId, PotionPath) == CraftStartingPotions - 1;
		}, CraftRequestTicks });

	Steps.Add({ TEXT("CoopKit Save captures the world (the workbench's Prepare For Save runs); the client brings the other potion: the job is complete"),
		[Session, Scenario, TestPtr]()
		{
			FCraftCall Capture(CraftSaveSubsystem(Session->GetServerWorld()), TEXT("CaptureToBytes"));
			TestPtr->TestTrue(TEXT("Capture To Bytes of the save subsystem is called"), Capture.Call());
			Scenario->Saved = Capture.Get<TArray<uint8>>(TEXT("OutBytes"));
			TestPtr->TestTrue(TEXT("The capture holds data"), !Scenario->Saved.IsEmpty());
			TestPtr->TestTrue(TEXT("The client's Request Contribute is called"), CraftRequestContribute(Session->GetClientWorlds()[0], Scenario->JobId, 1));
		},
		[Session, Scenario]()
		{
			// The client's view settles too: the load step then proves that the load, not stale replication, changed it.
			UWorld* Client = Session->GetClientWorlds()[0];
			return CraftMissing(CraftFindStation(Session->GetServerWorld()), Scenario->JobId) == 0
				&& CraftCountUnits(Session->GetServerWorld(), Scenario->ClientId, PotionPath) == CraftStartingPotions - 1
				&& CraftCountUnits(Client, Scenario->ClientId, PotionPath) == CraftStartingPotions - 1 && CraftMissing(CraftFindStation(Client), Scenario->JobId) == 0;
		}, CraftRequestTicks });

	Steps.Add({ TEXT("CoopKit Save loads the capture: the workbench has the job as saved, one potion missing, the host's part kept; the client has both potions again"),
		[Session, Scenario, TestPtr]()
		{
			FCraftCall Load(CraftSaveSubsystem(Session->GetServerWorld()), TEXT("LoadFromBytes"));
			Load.Set<TArray<uint8>>(TEXT("Bytes"), Scenario->Saved);
			TestPtr->TestTrue(TEXT("Load From Bytes of the save subsystem is called"), Load.Call());
		},
		[Session, Scenario]()
		{
			UWorld* Server = Session->GetServerWorld();
			UWorld* Client = Session->GetClientWorlds()[0];
			UActorComponent* Station = CraftFindStation(Server);
			return CraftJobCount(Station) == 1 && CraftMissing(Station, Scenario->JobId) == 1
				&& CraftContributed(Station, Scenario->JobId, CraftFindPlayer(Server, Scenario->HostId)) == 1
				&& CraftContributed(Station, Scenario->JobId, CraftFindPlayer(Server, Scenario->ClientId)) == 0
				&& CraftCountUnits(Client, Scenario->ClientId, PotionPath) == CraftStartingPotions
				&& CraftMissing(CraftFindStation(Client), Scenario->JobId) == 1;
		}, CraftRequestTicks * 2 });

	Steps.Add({ TEXT("The client brings its potion again; after 2 s the armor lands in the CoopKit Inventory of the host, who queued the job"),
		nullptr,
		[Session, Scenario]()
		{
			if (!Scenario->bContributedAgain)
			{
				Scenario->bContributedAgain = CraftRequestContribute(Session->GetClientWorlds()[0], Scenario->JobId, 1);
				return false;
			}
			// A CoopKit Inventory replicates to its owner only: the host's armor is read on the server, the client's on the client.
			UWorld* Server = Session->GetServerWorld();
			return CraftJobCount(CraftFindStation(Server)) == 0 && CraftCountUnits(Server, Scenario->HostId, ArmorPath) == Scenario->HostArmorBefore + 1
				&& CraftCountUnits(Session->GetClientWorlds()[0], Scenario->ClientId, ArmorPath) == 0;
		}, CraftRequestTicks + 90 });

	if (!Session->Start(*this, Map))
	{
		return false;
	}
	AddInfo(FString::Printf(TEXT("Scenario: %s"), *Session->Describe()));
	AddCommand(new FNetScenarioCommand(*this, Session, MoveTemp(Steps)));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
