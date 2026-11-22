// Copyright 2026 Ivan Iaitckikh. All Rights Reserved.

// CoopKit Heroes in the sample project through the bridges of TASK-58.3 (module CoopKitSampleBridges): on L_ProvingGround
// every player state gets the hero component with DA_CKSampleHeroRoster; the sample's heroes carry hero modules that set
// the CoopKit Combat profile, top up CoopKit Inventory and give the CoopKit Revive body its data. The draft pool of the
// sample's draft lobby is built from the same roster. The tests do not link against the plugins or the bridges: they pick
// heroes with the plugin's console command ck.heroes.Select, find components by class name and read state through
// reflection, like the other sample tests.

#include "AssetRegistry/AssetData.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "CKSampleNetTestSession.h"
#include "CKSampleTestHelpers.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "Materials/MaterialInterface.h"
#include "Misc/AutomationTest.h"
#include "Misc/DataValidation.h"
#include "UObject/UnrealType.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace CKSample::Tests
{
	namespace HeroesTest
	{
		const TCHAR* const Map = TEXT("/Game/CoopKitSample/Maps/L_ProvingGround");
		const TCHAR* const RosterPath = TEXT("/Game/CoopKitSample/Data/DA_CKSampleHeroRoster.DA_CKSampleHeroRoster");
		const TCHAR* const PoolPath = TEXT("/Game/CoopKitSample/Data/DA_CKSampleHeroDraftPool.DA_CKSampleHeroDraftPool");
		const TCHAR* const DraftModePath = TEXT("/Game/CoopKitSample/Data/DA_CKSampleDraftMode.DA_CKSampleDraftMode");
		const TCHAR* const HeroClassPath = TEXT("/Script/CoopKitHeroes.CKHeroData");
		const TCHAR* const PotionPath = TEXT("/CoopKitInventory/Data/DA_InventoryDemoPotion.DA_InventoryDemoPotion");
		const TCHAR* const GuardianCombatPath = TEXT("/Game/CoopKitSample/Heroes/Guardian/DA_HeroCombat_Guardian.DA_HeroCombat_Guardian");
		const TCHAR* const GuardianBodyPath = TEXT("/Game/CoopKitSample/Heroes/Guardian/DA_HeroBody_Guardian.DA_HeroBody_Guardian");
		const TCHAR* const SorcererCombatPath = TEXT("/Game/CoopKitSample/Heroes/Sorcerer/DA_HeroCombat_Sorcerer.DA_HeroCombat_Sorcerer");
		const TCHAR* const HealerBodyPath = TEXT("/Game/CoopKitSample/Heroes/Healer/DA_HeroBody_Healer.DA_HeroBody_Healer");
		const TCHAR* const GoldPath = TEXT("/CoopKitHeroes/Materials/MI_CKHeroesSkin_Gold.MI_CKHeroesSkin_Gold");

		/** Health of the Guardian's combat profile (create_sample_heroes_content.py). */
		constexpr float GuardianMaxHealth = 150.0f;

		/** Potions of the Healer's inventory module; the sample's inventory rules give every player 2. */
		constexpr int32 HealerPotions = 4;

		/** A console request goes client -> server -> every machine: two lag legs plus a replication update. */
		constexpr int32 RequestTicks = 120;

		/** Frames the test waits before it checks that a refused request changed nothing (about two seconds). */
		constexpr int32 RefusalTicks = 60;

		APlayerState* FindHeroPlayer(const UWorld* World, const int32 PlayerId)
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

		/** Object property of an object read through reflection; null when the object or the property is missing. */
		UObject* ReadObject(const UObject* Object, const TCHAR* PropertyName)
		{
			const FObjectPropertyBase* Property = Object != nullptr ? FindFProperty<FObjectPropertyBase>(Object->GetClass(), PropertyName) : nullptr;
			return Property != nullptr ? Property->GetObjectPropertyValue_InContainer(Object) : nullptr;
		}

		FName ReadName(const UObject* Object, const TCHAR* PropertyName)
		{
			const FNameProperty* Property = Object != nullptr ? FindFProperty<FNameProperty>(Object->GetClass(), PropertyName) : nullptr;
			return Property != nullptr ? Property->GetPropertyValue_InContainer(Object) : NAME_None;
		}

		/** Id of the hero the player plays (hero component -> Selection.Hero -> HeroId); None without a hero. */
		FName GetPlayedHero(const APlayerState* Player)
		{
			const UActorComponent* Heroes = FindComponentNamed(Player, TEXT("CKHeroComponent"));
			const FStructProperty* SelectionProperty = Heroes != nullptr ? FindFProperty<FStructProperty>(Heroes->GetClass(), TEXT("Selection")) : nullptr;
			const FObjectPropertyBase* HeroProperty = SelectionProperty != nullptr ? FindFProperty<FObjectPropertyBase>(SelectionProperty->Struct, TEXT("Hero")) : nullptr;
			const UObject* Hero = HeroProperty != nullptr ? HeroProperty->GetObjectPropertyValue_InContainer(SelectionProperty->ContainerPtrToValuePtr<void>(Heroes)) : nullptr;
			return ReadName(Hero, TEXT("HeroId"));
		}

		/** Path of the combat profile of the player's pawn (CKCombatComponent.Profile). */
		FString GetCombatProfile(const APlayerState* Player)
		{
			return GetPathNameSafe(ReadObject(FindComponentNamed(Player != nullptr ? Player->GetPawn() : nullptr, TEXT("CKCombatComponent")), TEXT("Profile")));
		}

		/** Path of the Revive body data of the player's pawn (CKReviveBodyComponent.BodyData). */
		FString GetBodyData(const APlayerState* Player)
		{
			return GetPathNameSafe(ReadObject(FindComponentNamed(Player != nullptr ? Player->GetPawn() : nullptr, TEXT("CKReviveBodyComponent")), TEXT("BodyData")));
		}

		/** Maximum health of the player's pawn: the Blueprint function GetMaxHealth of its combat component. */
		float GetMaxHealth(const APlayerState* Player)
		{
			UActorComponent* Combat = FindComponentNamed(Player != nullptr ? Player->GetPawn() : nullptr, TEXT("CKCombatComponent"));
			UFunction* Function = Combat != nullptr ? Combat->FindFunction(TEXT("GetMaxHealth")) : nullptr;
			const FFloatProperty* Result = Function != nullptr ? CastField<FFloatProperty>(Function->GetReturnProperty()) : nullptr;
			if (Result == nullptr)
			{
				return -1.0f;
			}
			uint8* Parameters = static_cast<uint8*>(FMemory_Alloca(Function->ParmsSize));
			FMemory::Memzero(Parameters, Function->ParmsSize);
			Combat->ProcessEvent(Function, Parameters);
			return Result->GetPropertyValue_InContainer(Parameters);
		}

		/** Units of the item in the player's inventory, read through reflection: Items.Entries[].Item/Count. */
		int32 CountHeroItems(const APlayerState* Player, const TCHAR* ItemPath)
		{
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

		/** Material of the first slot of the player's pawn body on this machine. */
		FString GetBodyMaterial(const APlayerState* Player)
		{
			const ACharacter* Character = Player != nullptr ? Cast<ACharacter>(Player->GetPawn()) : nullptr;
			return Character != nullptr && Character->GetMesh() != nullptr ? GetPathNameSafe(Character->GetMesh()->GetMaterial(0)) : FString();
		}

		/** Hero ids of an array property of objects (the roster's Heroes) in order. */
		TArray<FName> ReadRosterIds(const UObject* Roster)
		{
			TArray<FName> Ids;
			const FArrayProperty* Property = Roster != nullptr ? FindFProperty<FArrayProperty>(Roster->GetClass(), TEXT("Heroes")) : nullptr;
			const FObjectPropertyBase* Inner = Property != nullptr ? CastField<FObjectPropertyBase>(Property->Inner) : nullptr;
			if (Inner != nullptr)
			{
				FScriptArrayHelper Heroes(Property, Property->ContainerPtrToValuePtr<void>(Roster));
				for (int32 Index = 0; Index < Heroes.Num(); ++Index)
				{
					Ids.Add(ReadName(Inner->GetObjectPropertyValue(Heroes.GetRawPtr(Index)), TEXT("HeroId")));
				}
			}
			return Ids;
		}

		/** Ids of the draft pool's heroes (Heroes[].Id) in order. */
		TArray<FName> ReadPoolIds(const UObject* Pool)
		{
			TArray<FName> Ids;
			const FArrayProperty* Property = Pool != nullptr ? FindFProperty<FArrayProperty>(Pool->GetClass(), TEXT("Heroes")) : nullptr;
			const FStructProperty* Inner = Property != nullptr ? CastField<FStructProperty>(Property->Inner) : nullptr;
			const FNameProperty* IdProperty = Inner != nullptr ? FindFProperty<FNameProperty>(Inner->Struct, TEXT("Id")) : nullptr;
			if (IdProperty != nullptr)
			{
				FScriptArrayHelper Heroes(Property, Property->ContainerPtrToValuePtr<void>(Pool));
				for (int32 Index = 0; Index < Heroes.Num(); ++Index)
				{
					Ids.Add(IdProperty->GetPropertyValue_InContainer(Heroes.GetRawPtr(Index)));
				}
			}
			return Ids;
		}

		FString JoinIds(const TArray<FName>& Ids)
		{
			return FString::JoinBy(Ids, TEXT(" "), [](const FName Id) { return Id.ToString(); });
		}

		struct FScenario
		{
			int32 HostId = INDEX_NONE;
			int32 ClientId = INDEX_NONE;
			FString RulesBody;

			/** Frames the refusal step has waited. */
			int32 WaitedTicks = 0;
		};
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCKSampleHeroesDataTest, "CoopKit.Sample.Heroes.Data",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCKSampleHeroesDataTest::RunTest(const FString& Parameters)
{
	using namespace CKSample::Tests::HeroesTest;

	const UObject* Roster = LoadObject<UObject>(nullptr, RosterPath);
	const UObject* Pool = LoadObject<UObject>(nullptr, PoolPath);
	const UObject* DraftMode = LoadObject<UObject>(nullptr, DraftModePath);
	if (!TestNotNull(TEXT("The sample's hero roster exists"), Roster) || !TestNotNull(TEXT("The sample's draft pool exists"), Pool)
		|| !TestNotNull(TEXT("The sample's draft mode exists"), DraftMode))
	{
		return false;
	}

	const TArray<FName> RosterIds = ReadRosterIds(Roster);
	const TArray<FName> PoolIds = ReadPoolIds(Pool);
	TestEqual(TEXT("The roster has the four sample heroes"), JoinIds(RosterIds), FString(TEXT("Guardian Hunter Sorcerer Healer")));
	TestEqual(TEXT("The draft pool has the roster's heroes in roster order (FillDraftPoolFromRoster)"), JoinIds(PoolIds), JoinIds(RosterIds));
	TestEqual(TEXT("The draft mode of the draft lobby picks from this pool"), GetPathNameSafe(ReadObject(DraftMode, TEXT("HeroPool"))), FString(PoolPath));
	TestNull(TEXT("The roster has no default hero: a player who does not pick plays the sample character"), ReadObject(Roster, TEXT("DefaultHero")));

	FDataValidationContext RosterContext;
	TestTrue(TEXT("The roster passes its data validation"), Roster->IsDataValid(RosterContext) != EDataValidationResult::Invalid);

	// Every hero asset of the project, with the plugin's demo heroes: the CoopKit hub reports two heroes with one id as an error.
	TArray<FAssetData> Heroes;
	IAssetRegistry& Registry = IAssetRegistry::GetChecked();
	Registry.GetAssetsByClass(FTopLevelAssetPath(HeroClassPath), Heroes);
	TMap<FName, FString> Owners;
	for (const FAssetData& Hero : Heroes)
	{
		FString IdText;
		const FName Id = Hero.GetTagValue(TEXT("HeroId"), IdText) ? FName(*IdText) : NAME_None;
		if (Id.IsNone())
		{
			continue;
		}
		if (const FString* Owner = Owners.Find(Id))
		{
			AddError(FString::Printf(TEXT("Hero id %s belongs to %s and %s."), *Id.ToString(), **Owner, *Hero.AssetName.ToString()));
		}
		Owners.Add(Id, Hero.AssetName.ToString());
		if (Hero.PackagePath.ToString().StartsWith(TEXT("/Game/CoopKitSample/Heroes")))
		{
			const UObject* Asset = Hero.GetAsset();
			FDataValidationContext Context;
			TestTrue(FString::Printf(TEXT("%s passes its data validation"), *Hero.AssetName.ToString()),
				Asset != nullptr && Asset->IsDataValid(Context) != EDataValidationResult::Invalid);
		}
	}
	TestTrue(TEXT("The registry knows the sample's heroes and the plugin's demo heroes"), Owners.Num() >= 8);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCKSampleHeroesListenTest, "CoopKit.Sample.Heroes.ListenServer",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCKSampleHeroesListenTest::RunTest(const FString& Parameters)
{
	using namespace CKSample::Tests;
	using namespace CKSample::Tests::HeroesTest;

	FNetSessionSettings Settings;
	Settings.Host = ENetSessionHost::ListenServer;
	Settings.RemoteClients = 1;
	const TSharedRef<FNetTestSession> Session = MakeShared<FNetTestSession>(Settings);
	const TSharedRef<FScenario> Scenario = MakeShared<FScenario>();
	FAutomationTestBase* TestPtr = this;

	TArray<FNetStep> Steps;
	Steps.Add({ TEXT("Both player states have the hero component (bridge CKSampleHeroesBridge) and no hero yet, on host and client"), nullptr,
		[Session, Scenario]()
		{
			UWorld* Server = Session->GetServerWorld();
			const TArray<UWorld*> Clients = Session->GetClientWorlds();
			if (Clients.IsEmpty() || Server->GetFirstPlayerController() == nullptr || Clients[0]->GetFirstPlayerController() == nullptr
				|| Clients[0]->GetFirstPlayerController()->PlayerState == nullptr)
			{
				return false;
			}
			Scenario->HostId = Server->GetFirstPlayerController()->PlayerState->GetPlayerId();
			Scenario->ClientId = Clients[0]->GetFirstPlayerController()->PlayerState->GetPlayerId();
			for (const UWorld* World : { Server, Clients[0] })
			{
				for (const int32 PlayerId : { Scenario->HostId, Scenario->ClientId })
				{
					const APlayerState* Player = FindHeroPlayer(World, PlayerId);
					if (Player == nullptr || Player->GetPawn() == nullptr || !HasComponentNamed(Player, TEXT("CKHeroComponent")) || !GetPlayedHero(Player).IsNone())
					{
						return false;
					}
				}
			}
			// The pawns' Revive bodies start with the body data of the match rules.
			Scenario->RulesBody = GetBodyData(FindHeroPlayer(Server, Scenario->ClientId));
			return !Scenario->RulesBody.IsEmpty();
		}, 150 });

	Steps.Add({ TEXT("ck.heroes.Select Guardian on the host: the combat profile, maximum health and Revive body of the Guardian"),
		[Session]() { GEngine->Exec(Session->GetServerWorld(), TEXT("ck.heroes.Select Guardian")); },
		[Session, Scenario]()
		{
			const APlayerState* Host = FindHeroPlayer(Session->GetServerWorld(), Scenario->HostId);
			return GetPlayedHero(Host) == TEXT("Guardian") && GetCombatProfile(Host) == GuardianCombatPath
				&& FMath::IsNearlyEqual(GetMaxHealth(Host), GuardianMaxHealth) && GetBodyData(Host) == GuardianBodyPath
				&& GetPlayedHero(FindHeroPlayer(Session->GetClientWorlds()[0], Scenario->HostId)) == TEXT("Guardian");
		}, RequestTicks });

	Steps.Add({ TEXT("ck.heroes.Select Healer on the client: Healer's potions and Revive body on the server, its gold color on the client"),
		[Session]() { GEngine->Exec(Session->GetClientWorlds()[0], TEXT("ck.heroes.Select Healer")); },
		[Session, Scenario]()
		{
			const APlayerState* OnServer = FindHeroPlayer(Session->GetServerWorld(), Scenario->ClientId);
			const APlayerState* OnClient = FindHeroPlayer(Session->GetClientWorlds()[0], Scenario->ClientId);
			return GetPlayedHero(OnServer) == TEXT("Healer") && CountHeroItems(OnServer, PotionPath) == HealerPotions
				&& GetBodyData(OnServer) == HealerBodyPath && GetPlayedHero(OnClient) == TEXT("Healer")
				&& CountHeroItems(OnClient, PotionPath) == HealerPotions && GetBodyMaterial(OnClient) == GoldPath;
		}, RequestTicks });

	Steps.Add({ TEXT("ck.heroes.Select Guardian on the client: refused, the Guardian is taken (one per match); the client stays the Healer"),
		[Session]() { GEngine->Exec(Session->GetClientWorlds()[0], TEXT("ck.heroes.Select Guardian")); },
		[TestPtr, Session, Scenario]()
		{
			// The refusal changes nothing: wait out a round trip, then check.
			if (++Scenario->WaitedTicks < RefusalTicks)
			{
				return false;
			}
			TestPtr->TestEqual(TEXT("The client still plays the Healer"), GetPlayedHero(FindHeroPlayer(Session->GetServerWorld(), Scenario->ClientId)), FName(TEXT("Healer")));
			TestPtr->TestEqual(TEXT("The host still plays the Guardian"), GetPlayedHero(FindHeroPlayer(Session->GetServerWorld(), Scenario->HostId)), FName(TEXT("Guardian")));
			return true;
		}, RequestTicks });

	Steps.Add({ TEXT("ck.heroes.Select Sorcerer on the client: Sorcerer's profile, Revive body back to the rules', potions kept"),
		[Session]() { GEngine->Exec(Session->GetClientWorlds()[0], TEXT("ck.heroes.Select Sorcerer")); },
		[Session, Scenario]()
		{
			const APlayerState* OnServer = FindHeroPlayer(Session->GetServerWorld(), Scenario->ClientId);
			return GetPlayedHero(OnServer) == TEXT("Sorcerer") && GetCombatProfile(OnServer) == SorcererCombatPath
				&& GetBodyData(OnServer) == Scenario->RulesBody && CountHeroItems(OnServer, PotionPath) == HealerPotions;
		}, RequestTicks });

	if (!Session->Start(*this, Map))
	{
		return false;
	}
	AddInfo(FString::Printf(TEXT("Scenario: %s"), *Session->Describe()));
	AddCommand(new FNetScenarioCommand(*this, Session, MoveTemp(Steps)));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
