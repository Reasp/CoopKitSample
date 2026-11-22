// Copyright 2026 Ivan Iaitckikh. All Rights Reserved.

// Host migration in the sample project through the bridges CKSampleHostMigrationBridge, CKSampleHostMigrationSaveBridge and
// CKSampleHostMigrationSessionsBridge (module CoopKitSampleBridges, TASK-107, decision-93): CoopKit Host Migration keeps the
// group alive; the snapshot of the world is a CoopKit Save capture and the group's session is a CoopKit Sessions session.
//
// Three standalone PIE instances start on the sample's main menu (L_MainMenu, with its screens), 150 ms lag and 2% loss on
// every connection. Instance 0 runs ck.sample.MigrationHost: it hosts the group on the match map L_ProvingGround; instances 1
// and 2 run ck.sample.MigrationJoin: they find the group's session through CoopKit Sessions and join it. The host moves every
// pawn, sends a snapshot, moves every pawn again (the migration must lose that) and goes away:
// - Graceful: it leaves the group (ck.sample.MigrationLeave); the clients learn it from the closed connection. Then the old
//   host rejoins (ck.sample.MigrationRejoin) and stands where the snapshot put it.
// - Crash: it goes silent (ck.sample.MigrationCrash); the clients learn it only from the missing pulse (timeout 3 s).
// - TwoHosts: it leaves, and with Fallback Seconds Per Rank 0 both survivors raise the group at once. The one of rank 1
//   watches for a better host with CoopKit Sessions' background search (TASK-124), finds rank 0 and hands itself over.
// Either way one of the others hosts the same map from the snapshot (CoopKit Save applies the bytes) and the other joins it
// through CoopKit Sessions; every player stands where the snapshot put them. The summary lines "HM ..." are the measured
// downtime. The test links neither plugin nor the bridges: it starts the flow with the console and reads the state of the
// plugins through reflection.
//
// Buttons (TASK-125): the Graceful run without the console. Instance 0 presses "Host a migration game" on the migration panel
// of the main menu, instances 1 and 2 press "Find a migration game", the host presses "Leave the game", the old host presses
// "Rejoin the game I left". The panels' state texts are read on every instance: host, successor, new host, joined the new host.
// The panel sits in the top right corner and must not overlap the party panel in the top left corner.
//
// PluginState (TASK-109): Graceful, with the plugins' state in the snapshot (CKSampleMigrationStateBridge, Inventory's own
// SaveGame items). Before the snapshot the host gives instance 1's player a timed Gameplay Effect with two stacks and 60% of
// its health, puts it down (CoopKit Revive, bleed-out stretched to 600 s for the run), gives instance 2's player three potions
// (CoopKit Inventory) and spawns a CoopKit AI demo enemy that is angry with instance 2's player and has half its health.
// L_ProvingGround has no enemies of its own: the test spawns one during play, the case of a game that spawns its enemies
// (L_AIArena's enemies are placed, L_Complete's bots down and revive players by themselves). On the new host the effect has
// the same stacks, level and the time it had left (not a fresh 300 s), the health is the snapshot's, the player lies with the
// bleed-out time it had left, the potions are in the bag, and the one enemy is there again, with its health, aiming at the
// same player.

#include "AbilitySystemComponent.h"
#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetTree.h"
#include "CKSampleLobbyTestHelpers.h"
#include "CKSampleMigrationTestEffect.h"
#include "CKSampleNetTestSession.h"
#include "CKSampleTestHelpers.h"
#include "CKSampleTestsCompat.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Controller.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerInput.h"
#include "GameFramework/PlayerState.h"
#include "HAL/PlatformTime.h"
#include "InputCoreTypes.h"
#include "Misc/AutomationTest.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "UObject/UnrealType.h"
#include "UObject/UObjectIterator.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace CKSample::Tests::HostMigrationTest
{
	namespace
	{
		const TCHAR* const MenuMap = TEXT("/Game/CoopKitSample/Maps/L_MainMenu");
		const TCHAR* const ProfilePath = TEXT("/Game/CoopKitSample/Data/DA_CKSampleHostMigrationProfile.DA_CKSampleHostMigrationProfile");
		const TCHAR* const MigrationClassPath = TEXT("/Script/CoopKitHostMigration.CKHostMigrationSubsystem");
		const TCHAR* const SessionsClassPath = TEXT("/Script/CoopKitSessions.CKSessionsSubsystem");
		const TCHAR* const SaveClassPath = TEXT("/Script/CoopKitSave.CKSaveSubsystem");
		const TCHAR* const BridgeClassPath = TEXT("/Script/CoopKitSampleBridges.CKSampleHostMigrationBridge");
		const TCHAR* const MigrationPanelClassPath = TEXT("/Script/CoopKitSampleBridges.CKSampleMigrationPanelWidget");
		const TCHAR* const PartyPanelClassPath = TEXT("/Script/CoopKitSampleBridges.CKSamplePartyPanelWidget");
		const TCHAR* const EnemyClassPath = TEXT("/CoopKitAI/Blueprints/BP_CKAIDemoEnemy.BP_CKAIDemoEnemy_C");
		const TCHAR* const PotionPath = TEXT("/CoopKitInventory/Data/DA_InventoryDemoPotion.DA_InventoryDemoPotion");
		const TCHAR* const CombatAttributeSetPath = TEXT("/Script/CoopKitCombat.CKCombatAttributeSet");

		/** PluginState: the downed player's bleed-out for the run (the plugin's 30 s could run out during the migration). */
		constexpr float StretchedBleedOutSeconds = 600.0f;

		/** PluginState: how long the player lies before the snapshot, so a bleed-out timer that starts over shows. */
		constexpr double LyingBeforeSnapshotSeconds = 6.0;

		/** PluginState: the enemy stands this far (+Y, on the floor) from its target, beyond the proximity threat radius (800). */
		constexpr double EnemyDistance = 1300.0;

		constexpr float EnemyThreat = 1000.0f;

		constexpr int32 Instances = 3;

		/** How far each pawn walks away from where it stands, in centimeters along +Y (see CKSampleSaveTest). */
		constexpr double FirstMove = 600.0;
		constexpr double LateMove = 450.0;

		/** A character may settle a little on the floor: positions are compared on the ground plane within this distance. */
		constexpr double Tolerance = 30.0;

		enum class EScenario : uint8
		{
			Graceful,
			Crash,
			/** Both survivors host the group at once; the conflict watch (a background search on a host) settles it. */
			TwoHosts,
			/** Graceful, started and driven by the migration panel's buttons instead of the console (TASK-125). */
			Buttons,
			/** Graceful, with the plugins' state (GAS, Inventory, Revive, AI) set before the snapshot and checked after (TASK-109). */
			PluginState
		};

		struct FMigrationRun
		{
			EScenario Scenario = EScenario::Graceful;
			TArray<FString> PlayerKeys;
			/** Pawn location by player key: what the snapshot holds. */
			TMap<FString, FVector> Snapshot;
			TMap<FString, FVector> Late;
			int32 SnapshotId = 0;
			int32 HostInstance = 0;
			TArray<int32> Survivors;
			double HostGoneAt = 0.0;
			int32 SettleTicks = 0;
			/** TwoHosts: both survivors were hosts of the group at the same time. */
			bool bSawTwoHosts = false;
			/** Buttons: "<instance>/<button>" of the buttons already pressed. */
			TSet<FString> Pressed;

			// PluginState: who lies and who is the enemy's target, by player key, and what the snapshot holds.
			FString DownedKey;
			FString TargetKey;
			TWeakObjectPtr<APawn> Enemy;
			double DownAt = 0.0;
			bool bThreatAdded = false;
			bool bEnemyHealthSet = false;
			float SnapshotHealth = -1.0f;
			float SnapshotEnemyHealth = -1.0f;
			float SnapshotEffectLeft = -1.0f;
			float SnapshotEffectLevel = -1.0f;
			int32 SnapshotStacks = 0;
			float SnapshotBleedOut = -1.0f;
			int64 SnapshotDownCount = 0;
			int64 SnapshotPotions = 0;
			bool bCheckedPlayers = false;
			/** Float properties of assets changed for the run, put back at the end. */
			TArray<TPair<TWeakObjectPtr<UObject>, TMap<FName, float>>> Restores;
			/** Console commands that put back what the run changed (log verbosity). */
			TArray<FString> ConsoleOnRestore;
		};

		UGameInstance* GameInstanceOf(const FNetTestSession& Session, const int32 Instance)
		{
			return Instance == 0 ? Session.GetHostGameInstance() : Session.GetClientGameInstance(Instance - 1);
		}

		UWorld* WorldOf(const FNetTestSession& Session, const int32 Instance)
		{
			const UGameInstance* GameInstance = GameInstanceOf(Session, Instance);
			return GameInstance != nullptr ? GameInstance->GetWorld() : nullptr;
		}

		UObject* SubsystemOf(const FNetTestSession& Session, const int32 Instance, const TCHAR* ClassPath)
		{
			UGameInstance* GameInstance = GameInstanceOf(Session, Instance);
			UClass* Class = FindObject<UClass>(nullptr, ClassPath);
			return GameInstance != nullptr && Class != nullptr && Class->IsChildOf(UGameInstanceSubsystem::StaticClass())
				? GameInstance->GetSubsystemBase(TSubclassOf<UGameInstanceSubsystem>(Class)) : nullptr;
		}

		// ---------- reflection: the test does not link to the plugins ----------

		/** Calls a function: Fill sets its inputs in the parameter block, Read gets the block afterwards (with the return value). */
		bool CallWithInputs(UObject* Object, const TCHAR* FunctionName, const TFunctionRef<void(UFunction&, uint8*)> Fill,
			const TFunctionRef<void(UFunction&, uint8*)> Read)
		{
			UFunction* Function = Object != nullptr ? Object->FindFunction(FunctionName) : nullptr;
			if (Function == nullptr)
			{
				return false;
			}
			uint8* Parameters = static_cast<uint8*>(FMemory_Alloca(Function->ParmsSize));
			FMemory::Memzero(Parameters, Function->ParmsSize);
			Function->InitializeStruct(Parameters);
			Fill(*Function, Parameters);
			Object->ProcessEvent(Function, Parameters);
			Read(*Function, Parameters);
			Function->DestroyStruct(Parameters);
			return true;
		}

		/** Calls a function that takes no input and hands its parameter block (with the return value) to Read. */
		bool CallAndRead(UObject* Object, const TCHAR* FunctionName, const TFunctionRef<void(UFunction&, uint8*)> Read)
		{
			return CallWithInputs(Object, FunctionName, [](UFunction&, uint8*) {}, Read);
		}

		void SetObjectInput(UFunction& Function, uint8* Parameters, const TCHAR* Name, UObject* Value)
		{
			if (const FObjectPropertyBase* Property = CastField<FObjectPropertyBase>(Function.FindPropertyByName(Name)))
			{
				Property->SetObjectPropertyValue_InContainer(Parameters, Value);
			}
		}

		void SetNumberInput(UFunction& Function, uint8* Parameters, const TCHAR* Name, const double Value)
		{
			const FProperty* Property = Function.FindPropertyByName(Name);
			if (const FFloatProperty* Float = CastField<FFloatProperty>(Property))
			{
				Float->SetPropertyValue_InContainer(Parameters, static_cast<float>(Value));
			}
			else if (const FNumericProperty* Number = CastField<FNumericProperty>(Property))
			{
				Number->SetIntPropertyValue(Number->ContainerPtrToValuePtr<void>(Parameters), static_cast<int64>(Value));
			}
		}

		/** The float return value of a function that takes no input; -1 when it returns no float. */
		float CallFloat(UObject* Object, const TCHAR* FunctionName)
		{
			float Result = -1.0f;
			CallAndRead(Object, FunctionName, [&Result](UFunction& Function, uint8* Parameters)
			{
				if (const FFloatProperty* Return = CastField<FFloatProperty>(Function.GetReturnProperty()))
				{
					Result = Return->GetPropertyValue_InContainer(Parameters);
				}
			});
			return Result;
		}

		bool CallVoid(UObject* Object, const TCHAR* FunctionName)
		{
			return CallAndRead(Object, FunctionName, [](UFunction&, uint8*) {});
		}

		/** The integer, boolean or enum value of a property as a number. */
		int64 ReadNumber(const FProperty* Property, const void* Value)
		{
			if (const FBoolProperty* Bool = CastField<FBoolProperty>(Property))
			{
				return Bool->GetPropertyValue(Value) ? 1 : 0;
			}
			if (const FEnumProperty* Enum = CastField<FEnumProperty>(Property))
			{
				return Enum->GetUnderlyingProperty()->GetSignedIntPropertyValue(Value);
			}
			if (const FNumericProperty* Number = CastField<FNumericProperty>(Property))
			{
				return Number->GetSignedIntPropertyValue(Value);
			}
			return INDEX_NONE;
		}

		/** The name of the enum value of a property (an enum class or a byte with an enum); empty for other properties. */
		FString ReadEnumName(const FProperty* Property, const void* Value)
		{
			const UEnum* Enum = nullptr;
			if (const FEnumProperty* EnumProperty = CastField<FEnumProperty>(Property))
			{
				Enum = EnumProperty->GetEnum();
			}
			else if (const FByteProperty* Byte = CastField<FByteProperty>(Property))
			{
				Enum = Byte->Enum;
			}
			return Enum != nullptr ? Enum->GetNameStringByValue(ReadNumber(Property, Value)) : FString();
		}

		int64 CallNumber(UObject* Object, const TCHAR* FunctionName, const int64 Default = INDEX_NONE)
		{
			int64 Result = Default;
			CallAndRead(Object, FunctionName, [&Result](UFunction& Function, uint8* Parameters)
			{
				if (const FProperty* Return = Function.GetReturnProperty())
				{
					Result = ReadNumber(Return, Return->ContainerPtrToValuePtr<void>(Parameters));
				}
			});
			return Result;
		}

		FString CallEnumName(UObject* Object, const TCHAR* FunctionName)
		{
			FString Result;
			CallAndRead(Object, FunctionName, [&Result](UFunction& Function, uint8* Parameters)
			{
				if (const FProperty* Return = Function.GetReturnProperty())
				{
					Result = ReadEnumName(Return, Return->ContainerPtrToValuePtr<void>(Parameters));
				}
			});
			return Result;
		}

		FString CallString(UObject* Object, const TCHAR* FunctionName)
		{
			FString Result;
			CallAndRead(Object, FunctionName, [&Result](UFunction& Function, uint8* Parameters)
			{
				if (const FStrProperty* Return = CastField<FStrProperty>(Function.GetReturnProperty()))
				{
					Result = Return->GetPropertyValue_InContainer(Parameters);
				}
			});
			return Result;
		}

		UObject* CallObject(UObject* Object, const TCHAR* FunctionName)
		{
			UObject* Result = nullptr;
			CallAndRead(Object, FunctionName, [&Result](UFunction& Function, uint8* Parameters)
			{
				if (const FObjectPropertyBase* Return = CastField<FObjectPropertyBase>(Function.GetReturnProperty()))
				{
					Result = Return->GetObjectPropertyValue_InContainer(Parameters);
				}
			});
			return Result;
		}

		/** A number field of the struct a function returns, e.g. SnapshotId of Get Last Snapshot. */
		int64 CallStructNumber(UObject* Object, const TCHAR* FunctionName, const TCHAR* FieldName)
		{
			int64 Result = INDEX_NONE;
			CallAndRead(Object, FunctionName, [&Result, FieldName](UFunction& Function, uint8* Parameters)
			{
				const FStructProperty* Return = CastField<FStructProperty>(Function.GetReturnProperty());
				const FProperty* Field = Return != nullptr ? Return->Struct->FindPropertyByName(FieldName) : nullptr;
				if (Field != nullptr)
				{
					Result = ReadNumber(Field, Field->ContainerPtrToValuePtr<void>(Return->ContainerPtrToValuePtr<void>(Parameters)));
				}
			});
			return Result;
		}

		/** A float field of a struct value; -1 when the struct has no such field. */
		float ReadFloatField(const UStruct& Struct, const void* Value, const TCHAR* FieldName)
		{
			const FFloatProperty* Property = CastField<FFloatProperty>(Struct.FindPropertyByName(FieldName));
			return Property != nullptr ? Property->GetPropertyValue_InContainer(Value) : -1.0f;
		}

		/** The latest migration report of the subsystem: whether there is one, how it ended and why the host was lost. */
		struct FReportInfo
		{
			bool bHasReport = false;
			FString Outcome;
			FString Reason;
			int64 Rank = INDEX_NONE;
			float StandaloneMs = -1.0f;
			float HostedOrFoundMs = -1.0f;
			float ResumedMs = -1.0f;
		};

		FReportInfo CallReport(UObject* Migration)
		{
			FReportInfo Info;
			CallAndRead(Migration, TEXT("GetLastReport"), [&Info](UFunction& Function, uint8* Parameters)
			{
				if (const FBoolProperty* Has = FindFProperty<FBoolProperty>(&Function, TEXT("bHasReport")))
				{
					Info.bHasReport = Has->GetPropertyValue_InContainer(Parameters);
				}
				const FStructProperty* Return = CastField<FStructProperty>(Function.GetReturnProperty());
				if (Return == nullptr)
				{
					return;
				}
				const void* Report = Return->ContainerPtrToValuePtr<void>(Parameters);
				const UStruct& Struct = *Return->Struct;
				if (const FProperty* Outcome = Struct.FindPropertyByName(TEXT("Outcome")))
				{
					Info.Outcome = ReadEnumName(Outcome, Outcome->ContainerPtrToValuePtr<void>(Report));
				}
				if (const FProperty* Reason = Struct.FindPropertyByName(TEXT("Reason")))
				{
					Info.Reason = ReadEnumName(Reason, Reason->ContainerPtrToValuePtr<void>(Report));
				}
				if (const FProperty* Rank = Struct.FindPropertyByName(TEXT("Rank")))
				{
					Info.Rank = ReadNumber(Rank, Rank->ContainerPtrToValuePtr<void>(Report));
				}
				Info.StandaloneMs = ReadFloatField(Struct, Report, TEXT("StandaloneMs"));
				Info.HostedOrFoundMs = ReadFloatField(Struct, Report, TEXT("HostedOrFoundMs"));
				Info.ResumedMs = ReadFloatField(Struct, Report, TEXT("ResumedMs"));
			});
			return Info;
		}

		// ---------- the group ----------

		FString StateOf(const FNetTestSession& Session, const int32 Instance)
		{
			return CallEnumName(SubsystemOf(Session, Instance, MigrationClassPath), TEXT("GetState"));
		}

		FString GroupOf(const FNetTestSession& Session, const int32 Instance)
		{
			return CallString(SubsystemOf(Session, Instance, MigrationClassPath), TEXT("GetGroupId"));
		}

		FString PlayerKeyOf(const FNetTestSession& Session, const int32 Instance)
		{
			return CallString(SubsystemOf(Session, Instance, MigrationClassPath), TEXT("GetLocalPlayerKey"));
		}

		int32 SnapshotIdOf(const FNetTestSession& Session, const int32 Instance)
		{
			return static_cast<int32>(CallStructNumber(SubsystemOf(Session, Instance, MigrationClassPath), TEXT("GetLastSnapshot"), TEXT("SnapshotId")));
		}

		int32 CountPlayers(const UWorld* World)
		{
			return World != nullptr && World->GetGameState() != nullptr ? World->GetGameState()->PlayerArray.Num() : -1;
		}

		FString KeyOf(const APlayerState& Player)
		{
			return Player.GetUniqueId().IsValid() ? Player.GetUniqueId().ToString() : FString();
		}

		/** Pawn location of every player that has a pawn, by player key. */
		TMap<FString, FVector> PawnsOf(const UWorld* World)
		{
			TMap<FString, FVector> Pawns;
			const AGameStateBase* GameState = World != nullptr ? World->GetGameState() : nullptr;
			if (GameState != nullptr)
			{
				for (const APlayerState* Player : GameState->PlayerArray)
				{
					if (Player != nullptr && Player->GetPawn() != nullptr && !KeyOf(*Player).IsEmpty())
					{
						Pawns.Add(KeyOf(*Player), Player->GetPawn()->GetActorLocation());
					}
				}
			}
			return Pawns;
		}

		/** Every key's pawn stands (on the ground plane) where Expected says. */
		bool PawnsAt(const UWorld* World, const TMap<FString, FVector>& Expected, const TArray<FString>& Keys)
		{
			const TMap<FString, FVector> Actual = PawnsOf(World);
			for (const FString& Key : Keys)
			{
				const FVector* Wanted = Expected.Find(Key);
				const FVector* Location = Actual.Find(Key);
				if (Wanted == nullptr || Location == nullptr || FVector::Dist2D(*Location, *Wanted) > Tolerance)
				{
					return false;
				}
			}
			return !Keys.IsEmpty();
		}

		/** Server: moves each pawn by Offset; a remote owner also gets ClientSetLocation (its own copy does not take replicated movement). */
		void MovePawns(const UWorld* World, const FVector& Offset)
		{
			for (const APlayerState* Player : World->GetGameState()->PlayerArray)
			{
				APawn* Pawn = Player != nullptr ? Player->GetPawn() : nullptr;
				if (Pawn == nullptr)
				{
					continue;
				}
				const FVector Target = Pawn->GetActorLocation() + Offset;
				Pawn->SetActorLocation(Target, false, nullptr, ETeleportType::TeleportPhysics);
				AController* Controller = Pawn->GetController();
				if (Controller != nullptr && !Controller->IsLocalController())
				{
					Controller->ClientSetLocation(Target, Pawn->GetActorRotation());
				}
			}
		}

		/** The pawn of the instance's own player on its own machine stands near the location. */
		bool OwnPawnAt(const UWorld* World, const FVector& Location)
		{
			const APlayerController* Controller = World != nullptr ? World->GetFirstPlayerController() : nullptr;
			return Controller != nullptr && Controller->GetPawn() != nullptr && FVector::Dist2D(Controller->GetPawn()->GetActorLocation(), Location) <= 2.0 * Tolerance;
		}

		void Exec(const FNetTestSession& Session, const int32 Instance, const TCHAR* Command)
		{
			if (UWorld* World = WorldOf(Session, Instance))
			{
				GEngine->Exec(World, Command);
			}
		}

		// ---------- the migration panel (Buttons) ----------

		/** The widget of the class (by path) that the instance's first local player has in its viewport; null when there is none. */
		UUserWidget* FindScreen(const UWorld* World, const TCHAR* ClassPath)
		{
			const UClass* ScreenClass = FindObject<UClass>(nullptr, ClassPath);
			const APlayerController* Controller = World != nullptr ? World->GetFirstPlayerController() : nullptr;
			if (ScreenClass == nullptr || Controller == nullptr)
			{
				return nullptr;
			}
			for (TObjectIterator<UUserWidget> It; It; ++It)
			{
				if (It->IsA(ScreenClass) && It->GetOwningPlayer() == Controller && It->IsInViewport())
				{
					return *It;
				}
			}
			return nullptr;
		}

		UWidget* FindChild(const UUserWidget* Screen, const TCHAR* Name)
		{
			return Screen != nullptr && Screen->WidgetTree != nullptr ? Screen->WidgetTree->FindWidget(FName(Name)) : nullptr;
		}

		/** A button of the migration panel that is shown and enabled: what a mouse can press. */
		UButton* FindPressable(const FNetTestSession& Session, const int32 Instance, const TCHAR* Name)
		{
			UButton* Button = Cast<UButton>(FindChild(FindScreen(WorldOf(Session, Instance), MigrationPanelClassPath), Name));
			return Button != nullptr && Button->IsVisible() && Button->GetIsEnabled() ? Button : nullptr;
		}

		/** Presses the button once (On Clicked, what a mouse click does); true once pressed, also on later calls. */
		bool PressOnce(const FNetTestSession& Session, FMigrationRun& Run, const int32 Instance, const TCHAR* Name)
		{
			const FString Key = FString::Printf(TEXT("%d/%s"), Instance, Name);
			if (Run.Pressed.Contains(Key))
			{
				return true;
			}
			if (UButton* Button = FindPressable(Session, Instance, Name))
			{
				Button->OnClicked.Broadcast();
				Run.Pressed.Add(Key);
				return true;
			}
			return false;
		}

		bool IsShown(const FNetTestSession& Session, const int32 Instance, const TCHAR* Name)
		{
			const UWidget* Widget = FindChild(FindScreen(WorldOf(Session, Instance), MigrationPanelClassPath), Name);
			return Widget != nullptr && Widget->IsVisible();
		}

		/** The text of the panel's state line on the instance. */
		FString PanelText(const FNetTestSession& Session, const int32 Instance)
		{
			const UTextBlock* Text = Cast<UTextBlock>(FindChild(FindScreen(WorldOf(Session, Instance), MigrationPanelClassPath), TEXT("MigrationStateText")));
			return Text != nullptr ? Text->GetText().ToString() : FString();
		}

		/** Screen rectangle of the panel's frame (the "Panel" border) of a screen; false while it is not laid out. */
		bool FrameRect(const UUserWidget* Screen, FBox2D& OutRect)
		{
			const UWidget* Frame = FindChild(Screen, TEXT("Panel"));
			if (Frame == nullptr || !Frame->IsVisible())
			{
				return false;
			}
			const FGeometry Geometry = Frame->GetCachedGeometry();
			const FVector2D Size = Geometry.GetAbsoluteSize();
			if (Size.X <= 0.0 || Size.Y <= 0.0)
			{
				return false;
			}
			OutRect = FBox2D(Geometry.GetAbsolutePosition(), Geometry.GetAbsolutePosition() + Size);
			return true;
		}

		/** The only instance among the given ones whose migration state is Host; INDEX_NONE when there is none or more than one. */
		int32 FindNewHost(const FNetTestSession& Session, const TArray<int32>& Candidates)
		{
			int32 Found = INDEX_NONE;
			for (const int32 Instance : Candidates)
			{
				if (StateOf(Session, Instance) == TEXT("Host"))
				{
					if (Found != INDEX_NONE)
					{
						return INDEX_NONE;
					}
					Found = Instance;
				}
			}
			return Found;
		}

		void AddReportLines(FAutomationTestBase& Test, const FNetTestSession& Session, const TCHAR* Mode, const TArray<int32>& Instance)
		{
			for (const int32 Index : Instance)
			{
				const FReportInfo Report = CallReport(SubsystemOf(Session, Index, MigrationClassPath));
				Test.AddInfo(FString::Printf(TEXT("HM %s instance %d rank %lld: %s after %s; standalone %.0f ms, %s %.0f ms, back in the game %.0f ms"),
					Mode, Index, Report.Rank, *Report.Outcome, *Report.Reason, Report.StandaloneMs,
					Report.Outcome == TEXT("NewHost") ? TEXT("hosted") : TEXT("found"), Report.HostedOrFoundMs, Report.ResumedMs));
			}
		}

		/** Sets a float property of the profile asset in memory (reflection: the property is private); returns the old value. */
		float SetProfileFloat(UObject& Profile, const TCHAR* Name, const float Value)
		{
			FFloatProperty* Property = FindFProperty<FFloatProperty>(Profile.GetClass(), Name);
			if (Property == nullptr)
			{
				return Value;
			}
			const float Old = Property->GetPropertyValue_InContainer(&Profile);
			Property->SetPropertyValue_InContainer(&Profile, Value);
			return Old;
		}

		/** Latent command: puts the profile's changed values back, also after a failure. */
		class FRestoreProfileCommand final : public IAutomationLatentCommand
		{
		public:
			FRestoreProfileCommand(UObject* InProfile, const TMap<FName, float>& InValues)
				: Profile(InProfile)
				, Values(InValues)
			{
			}

			virtual bool Update() override
			{
				if (UObject* Object = Profile.Get())
				{
					for (const TPair<FName, float>& Value : Values)
					{
						SetProfileFloat(*Object, *Value.Key.ToString(), Value.Value);
					}
				}
				return true;
			}

		private:
			TWeakObjectPtr<UObject> Profile;
			TMap<FName, float> Values;
		};

		// ---------- PluginState: the plugins' state on one machine ----------

		APlayerState* PlayerOf(const UWorld* World, const FString& Key)
		{
			const AGameStateBase* GameState = World != nullptr ? World->GetGameState() : nullptr;
			if (GameState != nullptr && !Key.IsEmpty())
			{
				for (APlayerState* Player : GameState->PlayerArray)
				{
					if (Player != nullptr && KeyOf(*Player) == Key)
					{
						return Player;
					}
				}
			}
			return nullptr;
		}

		APawn* PawnOf(const UWorld* World, const FString& Key)
		{
			const APlayerState* Player = PlayerOf(World, Key);
			return Player != nullptr ? Player->GetPawn() : nullptr;
		}

		UAbilitySystemComponent* AbilitiesOf(const AActor* Actor)
		{
			return Actor != nullptr ? Actor->FindComponentByClass<UAbilitySystemComponent>() : nullptr;
		}

		/** An attribute of CoopKit Combat's attribute set, by property name. */
		FGameplayAttribute CombatAttribute(const TCHAR* Name)
		{
			const UClass* SetClass = FindObject<UClass>(nullptr, CombatAttributeSetPath);
			FProperty* Property = SetClass != nullptr ? FindFProperty<FProperty>(SetClass, Name) : nullptr;
			return Property != nullptr ? FGameplayAttribute(Property) : FGameplayAttribute();
		}

		/** The base health of the actor's own Ability System Component; -1 while it has no combat attributes. */
		float HealthOf(const AActor* Actor)
		{
			const UAbilitySystemComponent* Abilities = AbilitiesOf(Actor);
			const FGameplayAttribute Health = CombatAttribute(TEXT("Health"));
			return Abilities != nullptr && Health.IsValid() && Abilities->HasAttributeSetForAttribute(Health) ? Abilities->GetNumericAttributeBase(Health) : -1.0f;
		}

		/** Sets the base health of the actor's own Ability System Component to a share of its maximum; the value set, or -1. */
		float SetHealthShare(const AActor* Actor, const float Share)
		{
			UAbilitySystemComponent* Abilities = AbilitiesOf(Actor);
			const FGameplayAttribute Health = CombatAttribute(TEXT("Health"));
			const FGameplayAttribute MaxHealth = CombatAttribute(TEXT("MaxHealth"));
			if (Abilities == nullptr || !Health.IsValid() || !MaxHealth.IsValid() || !Abilities->HasAttributeSetForAttribute(Health))
			{
				return -1.0f;
			}
			const float Value = FMath::RoundToFloat(Abilities->GetNumericAttribute(MaxHealth) * Share);
			Abilities->SetNumericAttributeBase(Health, Value);
			return Value;
		}

		const FActiveGameplayEffect* FindTestEffect(const UAbilitySystemComponent* Abilities)
		{
			if (Abilities != nullptr)
			{
				for (const FActiveGameplayEffect& Active : &Abilities->GetActiveGameplayEffects())
				{
					if (Active.Spec.Def != nullptr && Active.Spec.Def->GetClass() == UCKSampleMigrationTestEffect::StaticClass())
					{
						return &Active;
					}
				}
			}
			return nullptr;
		}

		TArray<APawn*> EnemiesOf(UWorld* World)
		{
			TArray<APawn*> Enemies;
			const UClass* EnemyClass = FindObject<UClass>(nullptr, EnemyClassPath);
			if (World != nullptr && EnemyClass != nullptr)
			{
				for (TActorIterator<APawn> It(World); It; ++It)
				{
					if (IsValid(*It) && !It->IsActorBeingDestroyed() && It->IsA(EnemyClass))
					{
						Enemies.Add(*It);
					}
				}
			}
			return Enemies;
		}

		/** True while the brain (StateTree, Behavior Tree) of the pawn's controller runs; read by reflection (AIModule is not linked). */
		bool IsBrainRunning(const APawn* Pawn)
		{
			const AController* Controller = Pawn != nullptr ? Pawn->GetController() : nullptr;
			const UClass* BrainClass = FindObject<UClass>(nullptr, TEXT("/Script/AIModule.BrainComponent"));
			if (Controller == nullptr || BrainClass == nullptr)
			{
				return false;
			}
			for (UActorComponent* Component : Controller->GetComponents())
			{
				if (Component != nullptr && Component->IsA(BrainClass))
				{
					return CallNumber(Component, TEXT("IsRunning")) == 1;
				}
			}
			return false;
		}

		UObject* ReadObjectProperty(const UObject* Object, const TCHAR* Name)
		{
			const FObjectPropertyBase* Property = Object != nullptr ? FindFProperty<FObjectPropertyBase>(Object->GetClass(), Name) : nullptr;
			return Property != nullptr ? Property->GetObjectPropertyValue_InContainer(Object) : nullptr;
		}

		int64 PotionsOf(const AActor* Player)
		{
			int64 Count = INDEX_NONE;
			UObject* Potion = LoadObject<UObject>(nullptr, PotionPath);
			CallWithInputs(CKSample::Tests::FindComponentNamed(Player, TEXT("CKInventoryComponent")), TEXT("GetItemCount"),
				[Potion](UFunction& Function, uint8* Parameters) { SetObjectInput(Function, Parameters, TEXT("Item"), Potion); },
				[&Count](UFunction& Function, uint8* Parameters)
				{
					if (const FProperty* Return = Function.GetReturnProperty())
					{
						Count = ReadNumber(Return, Return->ContainerPtrToValuePtr<void>(Parameters));
					}
				});
			return Count;
		}

		/** Latent command: puts back the asset values the run changed, also after a failure. */
		class FRestoreRunCommand final : public IAutomationLatentCommand
		{
		public:
			explicit FRestoreRunCommand(const TSharedRef<FMigrationRun>& InRun)
				: Run(InRun)
			{
			}

			virtual bool Update() override
			{
				for (const TPair<TWeakObjectPtr<UObject>, TMap<FName, float>>& Restore : Run->Restores)
				{
					FRestoreProfileCommand(Restore.Key.Get(), Restore.Value).Update();
				}
				Run->Restores.Reset();
				for (const FString& Command : Run->ConsoleOnRestore)
				{
					GEngine->Exec(nullptr, *Command);
				}
				Run->ConsoleOnRestore.Reset();
				return true;
			}

		private:
			TSharedRef<FMigrationRun> Run;
		};

		/** Host, PluginState: the plugins' state the snapshot must carry, set on the players and a new enemy. */
		void SetUpPluginState(FAutomationTestBase& Test, const FNetTestSession& Session, FMigrationRun& Run)
		{
			UWorld* Host = WorldOf(Session, 0);
			Run.DownedKey = Run.PlayerKeys[1];
			Run.TargetKey = Run.PlayerKeys[2];
			APlayerState* Downed = PlayerOf(Host, Run.DownedKey);
			APlayerState* Target = PlayerOf(Host, Run.TargetKey);
			if (!Test.TestTrue(TEXT("Both players of the plugin state have a pawn on the host"), Downed != nullptr && Downed->GetPawn() != nullptr && Target != nullptr && Target->GetPawn() != nullptr))
			{
				return;
			}

			// GAS: 60% health and a timed effect with two stacks at level 3 on the player state's ASC (the Inventory bridge puts it there).
			UAbilitySystemComponent* Abilities = AbilitiesOf(Downed);
			Test.TestTrue(TEXT("The player's health is set (CoopKit Combat's attributes on the player state's ASC)"), SetHealthShare(Downed, 0.6f) > 0.0f);
			if (Abilities != nullptr)
			{
				for (int32 Stack = 0; Stack < 2; ++Stack)
				{
					Abilities->ApplyGameplayEffectToSelf(GetDefault<UCKSampleMigrationTestEffect>(), 3.0f, Abilities->MakeEffectContext());
				}
			}

			// Revive: the player goes down; its bleed-out lasts the whole run.
			UActorComponent* Body = CKSample::Tests::FindComponentNamed(Downed->GetPawn(), TEXT("CKReviveBodyComponent"));
			UObject* BodyData = ReadObjectProperty(Body, TEXT("BodyData"));
			if (BodyData == nullptr)
			{
				// No body data of its own: the body uses Project Settings > CoopKit Revive > Default Body Data.
				const UClass* SettingsClass = FindObject<UClass>(nullptr, TEXT("/Script/CoopKitRevive.CKReviveSettings"));
				const FSoftObjectProperty* Default = SettingsClass != nullptr ? FindFProperty<FSoftObjectProperty>(SettingsClass, TEXT("DefaultBodyData")) : nullptr;
				BodyData = Default != nullptr ? Default->GetPropertyValue_InContainer(SettingsClass->GetDefaultObject()).LoadSynchronous() : nullptr;
			}
			if (BodyData == nullptr)
			{
				// No default body data either: the plugin uses the class defaults of the body data.
				UClass* DataClass = FindObject<UClass>(nullptr, TEXT("/Script/CoopKitRevive.CKReviveBodyData"));
				BodyData = DataClass != nullptr ? DataClass->GetDefaultObject() : nullptr;
			}
			if (Test.TestNotNull(TEXT("The downed player's Revive body has body data"), BodyData))
			{
				TMap<FName, float> Old;
				Old.Add(TEXT("BleedOutSeconds"), SetProfileFloat(*BodyData, TEXT("BleedOutSeconds"), StretchedBleedOutSeconds));
				Run.Restores.Emplace(BodyData, Old);
			}
			CallWithInputs(Body, TEXT("Down"), [](UFunction& Function, uint8* Parameters) { SetObjectInput(Function, Parameters, TEXT("DamageInstigator"), nullptr); },
				[](UFunction&, uint8*) {});
			Run.DownAt = FPlatformTime::Seconds();

			// Inventory: three potions in the target player's bag.
			UObject* Potion = LoadObject<UObject>(nullptr, PotionPath);
			Test.TestNotNull(TEXT("The demo potion exists"), Potion);
			CallWithInputs(CKSample::Tests::FindComponentNamed(Target, TEXT("CKInventoryComponent")), TEXT("AddItem"),
				[Potion](UFunction& Function, uint8* Parameters)
				{
					SetObjectInput(Function, Parameters, TEXT("Item"), Potion);
					SetNumberInput(Function, Parameters, TEXT("Count"), 3.0);
				},
				[](UFunction&, uint8*) {});

			// AI: an enemy spawned during play, on the floor away from the players.
			UClass* EnemyClass = LoadClass<APawn>(nullptr, EnemyClassPath);
			if (Test.TestNotNull(TEXT("The CoopKit AI demo enemy class exists"), EnemyClass))
			{
				FActorSpawnParameters Params;
				Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
				const FVector Location = Target->GetPawn()->GetActorLocation() + FVector(0.0, EnemyDistance, 0.0);
				Run.Enemy = Host->SpawnActor<APawn>(EnemyClass, Location, FRotator(0.0, -90.0, 0.0), Params);
			}
		}

		/** Host, PluginState: true once the enemy aims at the target with half its health and the player lay long enough. */
		bool IsPluginStateReady(const FNetTestSession& Session, FMigrationRun& Run)
		{
			const UWorld* Host = WorldOf(Session, 0);
			APawn* Enemy = Run.Enemy.Get();
			APawn* TargetPawn = PawnOf(Host, Run.TargetKey);
			UActorComponent* Threat = CKSample::Tests::FindComponentNamed(Enemy, TEXT("CKAIThreatComponent"));
			if (Threat == nullptr || TargetPawn == nullptr)
			{
				return false;
			}
			if (!Run.bThreatAdded)
			{
				Run.bThreatAdded = CallWithInputs(Threat, TEXT("AddThreat"),
					[TargetPawn](UFunction& Function, uint8* Parameters)
					{
						SetObjectInput(Function, Parameters, TEXT("Target"), TargetPawn);
						SetNumberInput(Function, Parameters, TEXT("Amount"), EnemyThreat);
					},
					[](UFunction&, uint8*) {});
			}
			// The enemy's ASC comes with its combat component, which refills the attributes once it is ready.
			if (!Run.bEnemyHealthSet && CallNumber(CKSample::Tests::FindComponentNamed(Enemy, TEXT("CKCombatComponent")), TEXT("IsReady")) == 1)
			{
				Run.bEnemyHealthSet = SetHealthShare(Enemy, 0.5f) > 0.0f;
			}
			const APawn* DownedPawn = PawnOf(Host, Run.DownedKey);
			const bool bDowned = CallNumber(CKSample::Tests::FindComponentNamed(DownedPawn, TEXT("CKReviveBodyComponent")), TEXT("IsDowned")) == 1;
			return Run.bEnemyHealthSet && bDowned && CallObject(Threat, TEXT("GetCurrentTarget")) == TargetPawn
				&& FPlatformTime::Seconds() - Run.DownAt >= LyingBeforeSnapshotSeconds;
		}

		/** Host, PluginState, in the frame of the snapshot: what the new host must show. */
		void RecordPluginState(FAutomationTestBase& Test, const FNetTestSession& Session, FMigrationRun& Run)
		{
			UWorld* Host = WorldOf(Session, 0);
			const APlayerState* Downed = PlayerOf(Host, Run.DownedKey);
			Run.SnapshotHealth = HealthOf(Downed);
			if (const FActiveGameplayEffect* Effect = FindTestEffect(AbilitiesOf(Downed)))
			{
				Run.SnapshotEffectLeft = Effect->GetTimeRemaining(static_cast<float>(Host->GetTimeSeconds()));
				Run.SnapshotEffectLevel = Effect->Spec.GetLevel();
				Run.SnapshotStacks = Effect->Spec.GetStackCount();
			}
			UActorComponent* Body = CKSample::Tests::FindComponentNamed(Downed != nullptr ? Downed->GetPawn() : nullptr, TEXT("CKReviveBodyComponent"));
			Run.SnapshotBleedOut = CallFloat(Body, TEXT("GetBleedOutSecondsLeft"));
			Run.SnapshotDownCount = CallNumber(Body, TEXT("GetDownCount"));
			Run.SnapshotPotions = PotionsOf(PlayerOf(Host, Run.TargetKey));
			Run.SnapshotEnemyHealth = HealthOf(Run.Enemy.Get());
			Test.AddInfo(FString::Printf(TEXT("HM plugin state in the snapshot: health %.0f, effect %d stacks level %.0f %.1f s left, lying %.1f s left (down %lld), %lld potions, enemy health %.0f"),
				Run.SnapshotHealth, Run.SnapshotStacks, Run.SnapshotEffectLevel, Run.SnapshotEffectLeft, Run.SnapshotBleedOut, Run.SnapshotDownCount, Run.SnapshotPotions, Run.SnapshotEnemyHealth));
			Test.TestEqual(TEXT("The snapshot's effect has two stacks"), Run.SnapshotStacks, 2);
			Test.TestTrue(TEXT("The snapshot's effect has run for a while (not its full 300 s)"), Run.SnapshotEffectLeft > 0.0f && Run.SnapshotEffectLeft < UCKSampleMigrationTestEffect::DurationSeconds - 1.0f);
			Test.TestTrue(TEXT("The player has lain a while before the snapshot"), Run.SnapshotBleedOut > 0.0f && Run.SnapshotBleedOut < StretchedBleedOutSeconds - LyingBeforeSnapshotSeconds + 1.0f);
			Test.TestTrue(TEXT("The target player has the three potions"), Run.SnapshotPotions >= 3);
			Test.TestTrue(TEXT("The enemy has half its health"), Run.SnapshotEnemyHealth > 0.0f);
		}

		/** New host, PluginState: true once everything the snapshot held is back; the one-time checks run on the first sight. */
		bool IsPluginStateBack(FAutomationTestBase& Test, const FNetTestSession& Session, FMigrationRun& Run)
		{
			UWorld* Server = WorldOf(Session, Run.HostInstance);
			const APlayerState* Downed = PlayerOf(Server, Run.DownedKey);
			const APlayerState* Target = PlayerOf(Server, Run.TargetKey);
			if (Downed == nullptr || Target == nullptr || Downed->GetPawn() == nullptr || Target->GetPawn() == nullptr)
			{
				return false;
			}
			UActorComponent* Body = CKSample::Tests::FindComponentNamed(Downed->GetPawn(), TEXT("CKReviveBodyComponent"));
			const FActiveGameplayEffect* Effect = FindTestEffect(AbilitiesOf(Downed));
			if (CallNumber(Body, TEXT("IsDowned")) != 1 || Effect == nullptr)
			{
				return false;
			}
			if (!Run.bCheckedPlayers)
			{
				// At first sight: a timer that started over would be ahead of the snapshot's time left for a few seconds more.
				Run.bCheckedPlayers = true;
				const float BleedOut = CallFloat(Body, TEXT("GetBleedOutSecondsLeft"));
				const float EffectLeft = Effect->GetTimeRemaining(static_cast<float>(Server->GetTimeSeconds()));
				Test.AddInfo(FString::Printf(TEXT("HM plugin state on the new host: health %.0f, effect %d stacks level %.0f, %.1f s of %.1f s left, lying %.1f s left (down %lld), %lld potions"),
					HealthOf(Downed), Effect->Spec.GetStackCount(), Effect->Spec.GetLevel(), EffectLeft, Effect->GetDuration(), BleedOut,
					CallNumber(Body, TEXT("GetDownCount")), PotionsOf(Target)));
				Test.TestEqual(TEXT("GAS: the effect has its stacks back"), Effect->Spec.GetStackCount(), Run.SnapshotStacks);
				Test.TestEqual(TEXT("GAS: the effect has its level back"), Effect->Spec.GetLevel(), Run.SnapshotEffectLevel);
				Test.TestTrue(TEXT("GAS: the effect lasts the time it had left, not a fresh 300 s"), Effect->GetDuration() <= Run.SnapshotEffectLeft + 0.5f && EffectLeft > 0.0f);
				Test.TestEqual(TEXT("GAS: the player's health is the snapshot's"), HealthOf(Downed), Run.SnapshotHealth, 0.01f);
				Test.TestEqual(TEXT("Revive: the down count is the snapshot's"), CallNumber(Body, TEXT("GetDownCount")), Run.SnapshotDownCount);
				Test.TestTrue(TEXT("Revive: the bleed-out goes on from the time it had left"), BleedOut > 0.0f && BleedOut <= Run.SnapshotBleedOut + 0.5f);
				Test.TestEqual(TEXT("Inventory: the potions are in the bag"), PotionsOf(Target), Run.SnapshotPotions);
			}

			// The enemy: spawned again by CoopKit Save, once, with its health, and angry with the same player once that player is here.
			const TArray<APawn*> Enemies = EnemiesOf(Server);
			if (Enemies.Num() != 1)
			{
				return false;
			}
			UActorComponent* Threat = CKSample::Tests::FindComponentNamed(Enemies[0], TEXT("CKAIThreatComponent"));
			// The enemy spawned during play runs its StateTree by itself once it is possessed (TASK-143).
			return CallObject(Threat, TEXT("GetCurrentTarget")) == Target->GetPawn() && FMath::IsNearlyEqual(HealthOf(Enemies[0]), Run.SnapshotEnemyHealth, 0.01f)
				&& IsBrainRunning(Enemies[0]);
		}

		bool RunScenario(FAutomationTestBase& Test, const EScenario Scenario)
		{
			FNetSessionSettings Settings;
			Settings.Host = ENetSessionHost::ListenServer;
			Settings.ClientConnect = ENetClientConnect::Manual;
			Settings.RemoteClients = Instances - 1;
			const TSharedRef<FNetTestSession> Session = MakeShared<FNetTestSession>(Settings);
			const TSharedRef<FMigrationRun> RunRef = MakeShared<FMigrationRun>();
			FMigrationRun* Run = &RunRef.Get();
			Run->Scenario = Scenario;
			FAutomationTestBase* TestPtr = &Test;
			const TCHAR* Mode = Scenario == EScenario::Graceful ? TEXT("graceful") : Scenario == EScenario::Crash ? TEXT("crash")
				: Scenario == EScenario::Buttons ? TEXT("buttons") : Scenario == EScenario::PluginState ? TEXT("plugin state") : TEXT("two hosts");
			const bool bPluginState = Scenario == EScenario::PluginState;
			const bool bButtons = Scenario == EScenario::Buttons;

			// Only the test's own snapshots: a periodic one could fall between the move that must be kept and the one that must be lost.
			UObject* Profile = LoadObject<UObject>(nullptr, ProfilePath);
			if (!Test.TestNotNull(TEXT("The sample's host migration profile exists (create_sample_hostmigration_content.py)"), Profile))
			{
				return false;
			}
			TMap<FName, float> OldValues;
			OldValues.Add(TEXT("SnapshotIntervalSeconds"), SetProfileFloat(*Profile, TEXT("SnapshotIntervalSeconds"), 600.0f));
			if (Scenario == EScenario::TwoHosts)
			{
				// No waiting by rank: both survivors raise the group. The watch lasts a few LAN searches (5 s each on Null).
				OldValues.Add(TEXT("FallbackSecondsPerRank"), SetProfileFloat(*Profile, TEXT("FallbackSecondsPerRank"), 0.0f));
				OldValues.Add(TEXT("ConflictWatchSeconds"), SetProfileFloat(*Profile, TEXT("ConflictWatchSeconds"), 60.0f));
			}

			TArray<FNetStep> Steps;
			Steps.Add({ TEXT("Every instance is on the main menu and has the migration, session and save subsystems and the bridge"), nullptr,
				[TestPtr, Session, bButtons]()
				{
					if (bButtons)
					{
						// The migration panel and the party panel are both up, with their buttons, before anything is pressed.
						for (int32 Instance = 0; Instance < Instances; ++Instance)
						{
							if (!FindPressable(*Session, Instance, TEXT("MigrationHostButton")) || !FindPressable(*Session, Instance, TEXT("MigrationFindButton"))
								|| FindScreen(WorldOf(*Session, Instance), PartyPanelClassPath) == nullptr)
							{
								return false;
							}
						}
					}
					for (int32 Instance = 0; Instance < Instances; ++Instance)
					{
						TestPtr->TestEqual(*FString::Printf(TEXT("Instance %d is on the main menu"), Instance), GetMapName(WorldOf(*Session, Instance)), FString(MenuMap));
						TestPtr->TestNotNull(*FString::Printf(TEXT("Instance %d has the host migration subsystem"), Instance), SubsystemOf(*Session, Instance, MigrationClassPath));
						TestPtr->TestNotNull(*FString::Printf(TEXT("Instance %d has the sessions subsystem"), Instance), SubsystemOf(*Session, Instance, SessionsClassPath));
						TestPtr->TestNotNull(*FString::Printf(TEXT("Instance %d has the save subsystem"), Instance), SubsystemOf(*Session, Instance, SaveClassPath));
						TestPtr->TestNotNull(*FString::Printf(TEXT("Instance %d has the bridge"), Instance), SubsystemOf(*Session, Instance, BridgeClassPath));
						TestPtr->TestEqual(*FString::Printf(TEXT("Instance %d: a lost host goes to CoopKit Sessions' own handling until a group exists"), Instance),
							CallNumber(SubsystemOf(*Session, Instance, SessionsClassPath), TEXT("IsHostLossHandledExternally")), static_cast<int64>(0));
						if (bButtons)
						{
							TestPtr->TestFalse(*FString::Printf(TEXT("Instance %d: no Leave or Rejoin button while in no group"), Instance),
								IsShown(*Session, Instance, TEXT("MigrationLeaveButton")) || IsShown(*Session, Instance, TEXT("MigrationRejoinButton")));
							TestPtr->TestTrue(*FString::Printf(TEXT("Instance %d: the panel invites to host or find a game"), Instance),
								PanelText(*Session, Instance).Contains(TEXT("Host a game")));
							FBox2D Migration;
							FBox2D Party;
							if (FrameRect(FindScreen(WorldOf(*Session, Instance), MigrationPanelClassPath), Migration)
								&& FrameRect(FindScreen(WorldOf(*Session, Instance), PartyPanelClassPath), Party))
							{
								TestPtr->TestFalse(*FString::Printf(TEXT("Instance %d: the migration panel does not overlap the party panel"), Instance), Migration.Intersect(Party));
								TestPtr->AddInfo(FString::Printf(TEXT("Instance %d panels: migration %s, party %s"), Instance, *Migration.ToString(), *Party.ToString()));
							}
							else
							{
								TestPtr->AddInfo(FString::Printf(TEXT("Instance %d: the panels are not laid out (no Slate geometry in this run), overlap not checked"), Instance));
							}
						}
					}
					return true;
				},
				5 });

			Steps.Add({ bButtons ? TEXT("Instance 0 presses Host a migration game on the panel: it hosts the group on the match map") : TEXT("Instance 0 hosts the group on the match map (ck.sample.MigrationHost)"),
				[Session, bButtons]()
				{
					if (!bButtons)
					{
						Exec(*Session, 0, TEXT("ck.sample.MigrationHost"));
					}
				},
				[Session, Run, bButtons]()
				{
					if (bButtons && !PressOnce(*Session, *Run, 0, TEXT("MigrationHostButton")))
					{
						return false;
					}
					const UWorld* World = WorldOf(*Session, 0);
					return StateOf(*Session, 0) == TEXT("Host") && World != nullptr && World->GetNetMode() == NM_ListenServer && GetMapName(World) == MatchMap;
				},
				900 });

			Steps.Add({ bButtons ? TEXT("Instances 1 and 2 press Find a migration game on the panel: they find the group through CoopKit Sessions and join it") : TEXT("Instances 1 and 2 find the group through CoopKit Sessions and join it (ck.sample.MigrationJoin)"),
				[Session, bButtons]()
				{
					if (!bButtons)
					{
						Exec(*Session, 1, TEXT("ck.sample.MigrationJoin"));
						Exec(*Session, 2, TEXT("ck.sample.MigrationJoin"));
					}
				},
				[TestPtr, Run, Session, bButtons]()
				{
					if (bButtons && (!PressOnce(*Session, *Run, 1, TEXT("MigrationFindButton")) || !PressOnce(*Session, *Run, 2, TEXT("MigrationFindButton"))))
					{
						return false;
					}
					const FString Group = GroupOf(*Session, 0);
					for (int32 Instance = 1; Instance < Instances; ++Instance)
					{
						if (StateOf(*Session, Instance) != TEXT("Client") || GroupOf(*Session, Instance) != Group)
						{
							return false;
						}
					}
					const UWorld* Server = WorldOf(*Session, 0);
					if (CountPlayers(Server) != Instances || PawnsOf(Server).Num() != Instances)
					{
						return false;
					}
					for (int32 Instance = 1; Instance < Instances; ++Instance)
					{
						if (CountPlayers(WorldOf(*Session, Instance)) != Instances || PawnsOf(WorldOf(*Session, Instance)).Num() != Instances)
						{
							return false;
						}
					}
					Run->PlayerKeys.Reset();
					for (int32 Instance = 0; Instance < Instances; ++Instance)
					{
						Run->PlayerKeys.Add(PlayerKeyOf(*Session, Instance));
					}
					TestPtr->TestFalse(TEXT("Every player has a key"), Run->PlayerKeys.Contains(FString()));
					return true;
				},
				1500 });

			Steps.Add({ TEXT("The joins' own snapshots are out before the test moves anyone"), nullptr,
				[Run, Session]()
				{
					// A snapshot follows every join by a second; give the last one time to reach the clients.
					return ++Run->SettleTicks > 120 && SnapshotIdOf(*Session, 1) > 0 && SnapshotIdOf(*Session, 2) > 0;
				},
				300 });

			if (bButtons)
			{
				Steps.Add({ TEXT("The panels tell every player its state: the host, the successors in order, and a way to leave"), nullptr,
					[Session]()
					{
						if (!PanelText(*Session, 0).Contains(TEXT("You are the host")))
						{
							return false;
						}
						for (int32 Instance = 1; Instance < Instances; ++Instance)
						{
							if (!PanelText(*Session, Instance).Contains(TEXT("Successor")))
							{
								return false;
							}
						}
						for (int32 Instance = 0; Instance < Instances; ++Instance)
						{
							if (!IsShown(*Session, Instance, TEXT("MigrationLeaveButton")) || IsShown(*Session, Instance, TEXT("MigrationHostButton")))
							{
								return false;
							}
						}
						return true;
					},
					300 });
			}

			Steps.Add({ TEXT("The host moves every pawn: the snapshot will hold these places"),
				[Session]() { MovePawns(WorldOf(*Session, 0), FVector(0.0, FirstMove, 0.0)); },
				[Run, Session]()
				{
					Run->Snapshot = PawnsOf(WorldOf(*Session, 0));
					for (int32 Instance = 0; Instance < Instances; ++Instance)
					{
						if (!PawnsAt(WorldOf(*Session, Instance), Run->Snapshot, Run->PlayerKeys))
						{
							return false;
						}
					}
					return Run->Snapshot.Num() == Instances;
				},
				150 });

			if (bPluginState)
			{
				Steps.Add({ TEXT("Plugin state on the host: instance 1's player has a timed GAS effect with two stacks and 60% health and lies (Revive), instance 2's player has three potions (Inventory), a spawned AI enemy with half its health aims at instance 2's player"),
					[TestPtr, Run, Session]() { SetUpPluginState(*TestPtr, *Session, *Run); },
					[Run, Session]() { return IsPluginStateReady(*Session, *Run); },
					900 });
			}

			Steps.Add({ TEXT("Snapshot: CoopKit Save captures the world on the host, both clients verify its length and hash (ck.sample.MigrationSnapshot)"),
				[TestPtr, Run, Session, bPluginState]()
				{
					Exec(*Session, 0, TEXT("ck.sample.MigrationSnapshot"));
					Run->SnapshotId = SnapshotIdOf(*Session, 0);
					if (bPluginState)
					{
						RecordPluginState(*TestPtr, *Session, *Run);
					}
				},
				[TestPtr, Run, Session]()
				{
					if (Run->SnapshotId <= 0 || SnapshotIdOf(*Session, 1) < Run->SnapshotId || SnapshotIdOf(*Session, 2) < Run->SnapshotId)
					{
						return false;
					}
					const int64 Bytes = CallStructNumber(SubsystemOf(*Session, 1, MigrationClassPath), TEXT("GetLastSnapshot"), TEXT("Bytes"));
					TestPtr->AddInfo(FString::Printf(TEXT("HM snapshot %d: %lld bytes on a client"), Run->SnapshotId, Bytes));
					TestPtr->TestTrue(TEXT("The snapshot is a CoopKit Save capture, not an empty one"), Bytes > 100);
					return true;
				},
				300 });

			Steps.Add({ TEXT("Progress after the snapshot: the host moves every pawn again (the migration must lose it)"),
				[Session]() { MovePawns(WorldOf(*Session, 0), FVector(0.0, LateMove, 0.0)); },
				[Run, Session]()
				{
					Run->Late = PawnsOf(WorldOf(*Session, 0));
					for (int32 Instance = 0; Instance < Instances; ++Instance)
					{
						if (!PawnsAt(WorldOf(*Session, Instance), Run->Late, Run->PlayerKeys))
						{
							return false;
						}
					}
					return true;
				},
				150 });

			if (bButtons)
			{
				// The match shows no cursor: the player presses the middle mouse button (the panel's hint says so) and then clicks Leave.
				Steps.Add({ TEXT("The match shows no cursor; the host presses the middle mouse button: the cursor and the hint appear, the camera stops"),
					[Session]()
					{
						if (APlayerController* Controller = WorldOf(*Session, 0)->GetFirstPlayerController())
						{
							CKSample::Compat::PressKey(*Controller, EKeys::MiddleMouseButton);
						}
					},
					[TestPtr, Session]()
					{
						const APlayerController* Controller = WorldOf(*Session, 0)->GetFirstPlayerController();
						const UWidget* Hint = FindChild(FindScreen(WorldOf(*Session, 0), MigrationPanelClassPath), TEXT("MigrationCursorHint"));
						const UTextBlock* HintText = Cast<UTextBlock>(Hint);
						if (Controller == nullptr || !Controller->bShowMouseCursor || HintText == nullptr || !HintText->GetText().ToString().Contains(TEXT("hide the cursor")))
						{
							return false;
						}
						TestPtr->TestTrue(TEXT("The camera ignores the look input while the cursor is shown"), Controller->IsLookInputIgnored());
						TestPtr->TestTrue(TEXT("The hint is shown on the match map"), Hint->IsVisible());
						return true;
					},
					60 });
			}

			Steps.Add({ Scenario == EScenario::Crash ? TEXT("The host crashes: nothing leaves it any more (ck.sample.MigrationCrash)")
					: bButtons ? TEXT("The host presses Leave the game on the panel") : TEXT("The host leaves the group (ck.sample.MigrationLeave)"),
				[Run, Session, Scenario, bButtons]()
				{
					Run->Survivors = { 1, 2 };
					if (!bButtons)
					{
						Exec(*Session, 0, Scenario == EScenario::Crash ? TEXT("ck.sample.MigrationCrash") : TEXT("ck.sample.MigrationLeave"));
					}
					Run->HostGoneAt = FPlatformTime::Seconds();
				},
				[Run, Session, bButtons]()
				{
					// The click only counts while the match shows the cursor (the middle mouse button step above).
					const APlayerController* Controller = bButtons ? WorldOf(*Session, 0)->GetFirstPlayerController() : nullptr;
					return !bButtons || (Controller != nullptr && Controller->bShowMouseCursor && PressOnce(*Session, *Run, 0, TEXT("MigrationLeaveButton")));
				},
				bButtons ? 60 : 2 });

			if (Scenario == EScenario::TwoHosts)
			{
				Steps.Add({ TEXT("Both survivors raise the group at once: two hosts of the group (Fallback Seconds Per Rank 0)"), nullptr,
					[Run, Session]()
					{
						Run->bSawTwoHosts = StateOf(*Session, 1) == TEXT("Host") && StateOf(*Session, 2) == TEXT("Host");
						return Run->bSawTwoHosts;
					},
					1800 });
			}

			Steps.Add({ TEXT("Migration: one survivor hosts the snapshot's map again, the other joins it through CoopKit Sessions"), nullptr,
				[Run, Session]()
				{
					const int32 NewHost = FindNewHost(*Session, Run->Survivors);
					if (NewHost == INDEX_NONE)
					{
						return false;
					}
					const UWorld* Server = WorldOf(*Session, NewHost);
					if (Server == nullptr || Server->GetNetMode() != NM_ListenServer || GetMapName(Server) != MatchMap || CountPlayers(Server) != Run->Survivors.Num())
					{
						return false;
					}
					for (const int32 Instance : Run->Survivors)
					{
						const FReportInfo Report = CallReport(SubsystemOf(*Session, Instance, MigrationClassPath));
						if (!Report.bHasReport || (Instance != NewHost && (StateOf(*Session, Instance) != TEXT("Client") || GroupOf(*Session, Instance) != GroupOf(*Session, NewHost))))
						{
							return false;
						}
					}
					Run->HostInstance = NewHost;
					return true;
				},
				Scenario == EScenario::TwoHosts ? 3600 : 1800 });

			Steps.Add({ TEXT("The new host has the snapshot's world: every player stands where the snapshot put them, not where the host walked afterwards"), nullptr,
				[TestPtr, Run, Session, Mode]()
				{
					TArray<FString> Keys;
					for (const int32 Instance : Run->Survivors)
					{
						Keys.Add(PlayerKeyOf(*Session, Instance));
					}
					if (!PawnsAt(WorldOf(*Session, Run->HostInstance), Run->Snapshot, Keys))
					{
						return false;
					}
					for (const int32 Instance : Run->Survivors)
					{
						const FVector* Saved = Run->Snapshot.Find(PlayerKeyOf(*Session, Instance));
						if (Saved == nullptr || !OwnPawnAt(WorldOf(*Session, Instance), *Saved))
						{
							return false;
						}
						const FVector* Lost = Run->Late.Find(PlayerKeyOf(*Session, Instance));
						TestPtr->TestTrue(*FString::Printf(TEXT("Instance %d: the late move is lost"), Instance), Lost != nullptr && FVector::Dist2D(*Lost, *Saved) > 2.0 * Tolerance);
					}
					AddReportLines(*TestPtr, *Session, Mode, Run->Survivors);
					return true;
				},
				450 });

			if (bPluginState)
			{
				Steps.Add({ TEXT("The new host has the plugins' state of the snapshot: the effect with its stacks and time left, the health, the lying player with its bleed-out left, the potions, the one enemy with its health aiming at the same player"), nullptr,
					[TestPtr, Run, Session]() { return IsPluginStateBack(*TestPtr, *Session, *Run); },
					900 });
			}

			if (bButtons)
			{
				Steps.Add({ TEXT("The panels tell what happened: the new host and the player who joined the new host"), nullptr,
					[Run, Session]()
					{
						for (const int32 Instance : Run->Survivors)
						{
							const FString Text = PanelText(*Session, Instance);
							if (!Text.Contains(Instance == Run->HostInstance ? TEXT("You are the new host (migration 1)") : TEXT("Joined the new host (migration 1)")))
							{
								return false;
							}
						}
						return true;
					},
					300 });
			}

			Steps.Add({ TEXT("CoopKit Save and CoopKit Sessions did the work: the new host holds the loaded save, the session is a CoopKit Sessions session"), nullptr,
				[TestPtr, Run, Session, Scenario]()
				{
					const int32 NewHost = Run->HostInstance;
					TestPtr->TestNotNull(TEXT("The new host's CoopKit Save has a session save (the snapshot was applied by Load From Bytes)"), CallObject(SubsystemOf(*Session, NewHost, SaveClassPath), TEXT("GetSessionSave")));
					TestPtr->TestEqual(TEXT("The new host's CoopKit Sessions hosts"), CallNumber(SubsystemOf(*Session, NewHost, SessionsClassPath), TEXT("IsHost")), static_cast<int64>(1));
					TestPtr->TestEqual(TEXT("The new host's session is up"), CallEnumName(SubsystemOf(*Session, NewHost, SessionsClassPath), TEXT("GetState")), FString(TEXT("InSession")));
					TestPtr->TestEqual(TEXT("The group's epoch is 1"), CallNumber(SubsystemOf(*Session, NewHost, MigrationClassPath), TEXT("GetEpoch")), static_cast<int64>(1));
					for (const int32 Instance : Run->Survivors)
					{
						TestPtr->TestEqual(*FString::Printf(TEXT("Instance %d is in a CoopKit Sessions session"), Instance),
							CallEnumName(SubsystemOf(*Session, Instance, SessionsClassPath), TEXT("GetState")), FString(TEXT("InSession")));
						TestPtr->TestEqual(*FString::Printf(TEXT("Instance %d: CoopKit Sessions hands a lost host to the migration while in the group"), Instance),
							CallNumber(SubsystemOf(*Session, Instance, SessionsClassPath), TEXT("IsHostLossHandledExternally")), static_cast<int64>(1));
						const FReportInfo Report = CallReport(SubsystemOf(*Session, Instance, MigrationClassPath));
						TestPtr->TestEqual(*FString::Printf(TEXT("Instance %d ended the migration as expected"), Instance), Report.Outcome, FString(Instance == NewHost ? TEXT("NewHost") : TEXT("Joined")));
						if (Scenario == EScenario::Crash)
						{
							TestPtr->TestEqual(*FString::Printf(TEXT("Instance %d lost the host by the pulse"), Instance), Report.Reason, FString(TEXT("PulseTimeout")));
						}
						if (Scenario == EScenario::TwoHosts)
						{
							// Rank 1 raised the group out of turn and gave way once its background search found rank 0.
							TestPtr->TestEqual(*FString::Printf(TEXT("Instance %d: the host that stays is rank 0, the one that gave way rank 1"), Instance),
								Report.Rank, static_cast<int64>(Instance == NewHost ? 0 : 1));
						}
					}
					return true;
				},
				5 });

			if (Scenario == EScenario::Graceful || bButtons)
			{
				Steps.Add({ bButtons ? TEXT("The old host presses Rejoin the game I left on the panel: it joins through CoopKit Sessions and stands where the snapshot put it")
						: TEXT("The old host rejoins the group through CoopKit Sessions and stands where the snapshot put it (ck.sample.MigrationRejoin)"),
					[Session, bButtons]()
					{
						if (!bButtons)
						{
							Exec(*Session, 0, TEXT("ck.sample.MigrationRejoin"));
						}
					},
					[Run, Session, bButtons]()
					{
						if (bButtons && !PressOnce(*Session, *Run, 0, TEXT("MigrationRejoinButton")))
						{
							return false;
						}
						if (StateOf(*Session, 0) != TEXT("Client") || GroupOf(*Session, 0) != GroupOf(*Session, Run->HostInstance))
						{
							return false;
						}
						const FString OldKey = Run->PlayerKeys[0];
						const FVector* Saved = Run->Snapshot.Find(OldKey);
						return Saved != nullptr && CountPlayers(WorldOf(*Session, Run->HostInstance)) == Instances
							&& PawnsAt(WorldOf(*Session, Run->HostInstance), Run->Snapshot, { OldKey }) && OwnPawnAt(WorldOf(*Session, 0), *Saved);
					},
					1800 });
			}

			Steps.Add({ TEXT("Summary"), nullptr,
				[TestPtr, Run, Mode]()
				{
					TestPtr->AddInfo(FString::Printf(TEXT("HM %s: the host went away %.1f s before the end of the run"), Mode, FPlatformTime::Seconds() - Run->HostGoneAt));
					return true;
				},
				2 });

			// The lost host shows up once per client as a network failure (an engine error): FailureReceived when the host
			// left, ConnectionTimeout when a client drops the connection itself after the missing pulse.
			Test.AddExpectedErrorPlain(TEXT("UEngine::BroadcastNetworkFailure: FailureType = "), EAutomationExpectedErrorFlags::Contains, 0);

			if (!Session->Start(Test, MenuMap))
			{
				FRestoreProfileCommand(Profile, OldValues).Update();
				FRestoreRunCommand(RunRef).Update();
				return false;
			}
			Test.AddInfo(FString::Printf(TEXT("Host migration %s: %s -> %s, CoopKit Save snapshot, CoopKit Sessions session"), Mode, *Session->Describe(), MatchMap));
			// The run lives as long as the steps that point to it.
			Steps.Add({ TEXT("Release"), [RunRef]() {}, []() { return true; }, 2 });
			Test.AddCommand(new FNetScenarioCommand(Test, Session, MoveTemp(Steps)));
			Test.AddCommand(new FRestoreProfileCommand(Profile, OldValues));
			Test.AddCommand(new FRestoreRunCommand(RunRef));
			return true;
		}
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCKSampleHostMigrationGracefulTest, "CoopKit.Sample.HostMigration.Graceful",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCKSampleHostMigrationGracefulTest::RunTest(const FString& Parameters)
{
	return CKSample::Tests::HostMigrationTest::RunScenario(*this, CKSample::Tests::HostMigrationTest::EScenario::Graceful);
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCKSampleHostMigrationCrashTest, "CoopKit.Sample.HostMigration.Crash",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCKSampleHostMigrationCrashTest::RunTest(const FString& Parameters)
{
	return CKSample::Tests::HostMigrationTest::RunScenario(*this, CKSample::Tests::HostMigrationTest::EScenario::Crash);
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCKSampleHostMigrationTwoHostsTest, "CoopKit.Sample.HostMigration.TwoHosts",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCKSampleHostMigrationTwoHostsTest::RunTest(const FString& Parameters)
{
	return CKSample::Tests::HostMigrationTest::RunScenario(*this, CKSample::Tests::HostMigrationTest::EScenario::TwoHosts);
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCKSampleHostMigrationButtonsTest, "CoopKit.Sample.HostMigration.Buttons",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCKSampleHostMigrationButtonsTest::RunTest(const FString& Parameters)
{
	return CKSample::Tests::HostMigrationTest::RunScenario(*this, CKSample::Tests::HostMigrationTest::EScenario::Buttons);
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCKSampleHostMigrationPluginStateTest, "CoopKit.Sample.HostMigration.PluginState",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCKSampleHostMigrationPluginStateTest::RunTest(const FString& Parameters)
{
	return CKSample::Tests::HostMigrationTest::RunScenario(*this, CKSample::Tests::HostMigrationTest::EScenario::PluginState);
}

#endif // WITH_DEV_AUTOMATION_TESTS
