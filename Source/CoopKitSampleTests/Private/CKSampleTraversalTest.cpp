// Copyright 2026 Ivan Iaitckikh. All Rights Reserved.

// CoopKit Traversal in the sample project through the bridge CKSampleTraversalBridge (module CoopKitSampleBridges): on
// L_ProvingGround every player gets the sample's character with the plugin's movement component, Revive's downed and dead
// tags stop the moves, and Jump at a crate mantles it: on the listen server host, replayed by the remote client. The test
// does not link against the plugins or the bridges: it finds their classes by name and calls their Blueprint functions
// through reflection.

#include "CKSampleNetTestSession.h"
#include "CKSampleTestHelpers.h"
#include "Components/CapsuleComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameplayTagContainer.h"
#include "Misc/AutomationTest.h"
#include "Subsystems/WorldSubsystem.h"
#include "UObject/UnrealType.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace CKSample::Tests
{
	namespace TraversalTest
	{
		const TCHAR* const Map = TEXT("/Game/CoopKitSample/Maps/L_ProvingGround");
		const TCHAR* const BridgePath = TEXT("/Script/CoopKitSampleBridges.CKSampleTraversalBridge");
		const TCHAR* const BlockPath = TEXT("/Script/CoopKitTraversal.CKTraversalCourseBlock");

		/** The test's own floor and crate stand this high over the host, away from the map's geometry. */
		constexpr double TestHeight = 4000.0;

		/** A 100 cm crate, deep: a low mantle. */
		constexpr double CrateHeight = 100.0;

		/** The movement component of a character as a plain object: its Blueprint functions are called by name. */
		UObject* TraversalMovement(const APawn* Pawn)
		{
			UObject* Movement = Pawn != nullptr ? Pawn->GetMovementComponent() : nullptr;
			return Movement != nullptr && Movement->GetClass()->GetName() == TEXT("CKTraversalMovementComponent") ? Movement : nullptr;
		}

		/** Name of the move a traversal movement component is making (None when it makes none). */
		FString TraversalKind(UObject* Movement)
		{
			UFunction* Function = Movement != nullptr ? Movement->FindFunction(TEXT("GetTraversalKind")) : nullptr;
			const FEnumProperty* Return = Function != nullptr ? CastField<FEnumProperty>(Function->GetReturnProperty()) : nullptr;
			if (Return == nullptr)
			{
				return FString();
			}
			TArray<uint8> Parameters;
			Parameters.SetNumZeroed(Function->ParmsSize);
			Movement->ProcessEvent(Function, Parameters.GetData());
			const int64 Value = Return->GetUnderlyingProperty()->GetSignedIntPropertyValue(Return->ContainerPtrToValuePtr<void>(Parameters.GetData()));
			return Return->GetEnum()->GetNameStringByValue(Value);
		}

		bool BridgeIsActive(UWorld* World)
		{
			UClass* BridgeClass = FindObject<UClass>(nullptr, BridgePath);
			UWorldSubsystem* Bridge = World != nullptr && BridgeClass != nullptr ? World->GetSubsystemBase(BridgeClass) : nullptr;
			UFunction* Function = Bridge != nullptr ? Bridge->FindFunction(TEXT("IsTraversalActive")) : nullptr;
			if (Function == nullptr)
			{
				return false;
			}
			TArray<uint8> Parameters;
			Parameters.SetNumZeroed(Function->ParmsSize);
			Bridge->ProcessEvent(Function, Parameters.GetData());
			const FBoolProperty* Return = CastField<FBoolProperty>(Function->GetReturnProperty());
			return Return != nullptr && Return->GetPropertyValue_InContainer(Parameters.GetData());
		}

		/** Spawns a box of the plugin's course with a size, before its Begin Play applies it. */
		AActor* SpawnBlock(UWorld& World, const FVector& Center, const FVector& Size)
		{
			UClass* BlockClass = FindObject<UClass>(nullptr, BlockPath);
			if (BlockClass == nullptr)
			{
				return nullptr;
			}
			AActor* Block = World.SpawnActorDeferred<AActor>(BlockClass, FTransform(Center), nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
			if (Block == nullptr)
			{
				return nullptr;
			}
			if (const FStructProperty* SizeProperty = FindFProperty<FStructProperty>(BlockClass, TEXT("Size")))
			{
				*SizeProperty->ContainerPtrToValuePtr<FVector>(Block) = Size;
			}
			Block->FinishSpawning(FTransform(Center));
			return Block;
		}

		/** The world's own local player's character: the host's on the server, the client's own on a client. */
		ACharacter* LocalCharacter(UWorld* World)
		{
			const APlayerController* Controller = World != nullptr ? World->GetFirstPlayerController() : nullptr;
			return Controller != nullptr ? Cast<ACharacter>(Controller->GetPawn()) : nullptr;
		}

		/** What the jump scenario saw. */
		struct FJumpState
		{
			int32 SettleTicks = 0;
			bool bJumped = false;
			FString ServerKind;
			bool bClientReplayed = false;
		};
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCKSampleTraversalListenTest, "CoopKit.Sample.Traversal.ListenServer",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCKSampleTraversalListenTest::RunTest(const FString& Parameters)
{
	using namespace CKSample::Tests;
	using namespace CKSample::Tests::TraversalTest;

	FNetSessionSettings Settings;
	Settings.Host = ENetSessionHost::ListenServer;
	Settings.RemoteClients = 1;
	const TSharedRef<FNetTestSession> Session = MakeShared<FNetTestSession>(Settings);
	const TSharedRef<FJumpState> State = MakeShared<FJumpState>();
	FAutomationTestBase* TestPtr = this;

	TArray<FNetStep> Steps;
	Steps.Add({ TEXT("Every player has the sample character with CoopKit Traversal's movement; the client sees the host's"),
		nullptr,
		[Session]()
		{
			UWorld* Server = Session->GetServerWorld();
			const TArray<UWorld*> Clients = Session->GetClientWorlds();
			if (Clients.IsEmpty() || !BridgeIsActive(Server))
			{
				return false;
			}
			int32 Players = 0;
			for (FConstPlayerControllerIterator It = Server->GetPlayerControllerIterator(); It; ++It)
			{
				const APawn* Pawn = It->Get() != nullptr ? It->Get()->GetPawn() : nullptr;
				if (Pawn == nullptr || Pawn->GetClass()->GetName() != TEXT("CKSampleTraversalCharacter") || TraversalMovement(Pawn) == nullptr)
				{
					return false;
				}
				++Players;
			}
			return Players == 2 && TraversalMovement(LocalCharacter(Clients[0])) != nullptr;
		}, 300 });

	Steps.Add({ TEXT("CoopKit Revive's downed and dead states stop the moves (tags on the movement component)"),
		nullptr,
		[Session, TestPtr]()
		{
			UObject* Movement = TraversalMovement(LocalCharacter(Session->GetServerWorld()));
			const FStructProperty* Property = Movement != nullptr ? FindFProperty<FStructProperty>(Movement->GetClass(), TEXT("BlockedTags")) : nullptr;
			if (Property == nullptr)
			{
				return false;
			}
			const FGameplayTagContainer& Blocked = *Property->ContainerPtrToValuePtr<FGameplayTagContainer>(Movement);
			TestPtr->TestTrue(TEXT("CK.Revive.State.Downed stops the moves"), Blocked.HasTagExact(FGameplayTag::RequestGameplayTag(TEXT("CK.Revive.State.Downed"), false)));
			TestPtr->TestTrue(TEXT("CK.Revive.State.Dead stops the moves"), Blocked.HasTagExact(FGameplayTag::RequestGameplayTag(TEXT("CK.Revive.State.Dead"), false)));
			return true;
		}, 30 });

	Steps.Add({ TEXT("The host stands before a 100 cm crate and jumps: the jump is a mantle, the remote client replays it"),
		[Session]()
		{
			UWorld* World = Session->GetServerWorld();
			ACharacter* Host = LocalCharacter(World);
			if (World == nullptr || Host == nullptr)
			{
				return;
			}
			const float Radius = Host->GetCapsuleComponent()->GetScaledCapsuleRadius();
			const float HalfHeight = Host->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
			const FVector Floor = Host->GetActorLocation() + FVector(0.0, 0.0, TestHeight);
			SpawnBlock(*World, Floor - FVector(0.0, 0.0, 50.0), FVector(1200.0, 1200.0, 100.0));
			SpawnBlock(*World, Floor + FVector(Radius + 30.0 + 100.0, 0.0, CrateHeight * 0.5), FVector(200.0, 300.0, CrateHeight));
			Host->TeleportTo(Floor + FVector(0.0, 0.0, HalfHeight + 5.0), FRotator::ZeroRotator);
			if (AController* Controller = Host->GetController())
			{
				Controller->SetControlRotation(FRotator::ZeroRotator);
			}
		},
		[Session, State]()
		{
			ACharacter* Host = LocalCharacter(Session->GetServerWorld());
			UObject* Movement = TraversalMovement(Host);
			if (Movement == nullptr)
			{
				return false;
			}
			// Jump once standing on the test's floor: the sample's jump key does the same.
			if (!State->bJumped)
			{
				State->SettleTicks = Host->GetCharacterMovement()->IsMovingOnGround() ? State->SettleTicks + 1 : 0;
				if (State->SettleTicks >= 10)
				{
					Host->Jump();
					State->bJumped = true;
				}
				return false;
			}
			const FString Kind = TraversalKind(Movement);
			if (Kind != TEXT("None") && !Kind.IsEmpty())
			{
				State->ServerKind = Kind;
			}
			const TArray<UWorld*> Clients = Session->GetClientWorlds();
			if (!Clients.IsEmpty())
			{
				for (TActorIterator<ACharacter> It(Clients[0]); It; ++It)
				{
					const FString Seen = It->GetLocalRole() == ROLE_SimulatedProxy ? TraversalKind(TraversalMovement(*It)) : FString();
					State->bClientReplayed |= Seen == TEXT("MantleLow");
				}
			}
			return State->ServerKind == TEXT("MantleLow") && State->bClientReplayed;
		}, 240 });

	if (!Session->Start(*this, Map))
	{
		return false;
	}
	AddInfo(FString::Printf(TEXT("Scenario: %s"), *Session->Describe()));
	AddCommand(new FNetScenarioCommand(*this, Session, MoveTemp(Steps)));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
