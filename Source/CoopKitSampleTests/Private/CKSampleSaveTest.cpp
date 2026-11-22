// Copyright 2026 Ivan Iaitckikh. All Rights Reserved.

// CoopKit Save in the sample project through the bridge CKSampleSaveBridge (module CoopKitSampleBridges): on
// L_ProvingGround the listen host gets the quick save keys F5 and F9; clients get none. The plugin saves every
// player's pawn with its position by itself. The test does not link against the plugin or the bridges: it finds the
// bridge's component by class name, presses the keys by injecting the bridge's Enhanced Input actions on the host, the
// way the host player does, and checks the slot file through the engine's save game API.

#include "CKSampleNetTestSession.h"
#include "CKSampleTestHelpers.h"
#include "Components/ActorComponent.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "EnhancedInputSubsystems.h"
#include "GameFramework/Controller.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "InputAction.h"
#include "InputActionValue.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace CKSample::Tests
{
	namespace
	{
		const TCHAR* const SaveMap = TEXT("/Game/CoopKitSample/Maps/L_ProvingGround");
		const TCHAR* const SaveActionPath = TEXT("/Game/CoopKitSample/Input/IA_SaveGame.IA_SaveGame");
		const TCHAR* const LoadActionPath = TEXT("/Game/CoopKitSample/Input/IA_LoadGame.IA_LoadGame");

		/** Files of the bridge's slot Quick with the plugin's default profile (prefix CoopKit_) and its slot list. */
		const TCHAR* const QuickSlotFile = TEXT("CoopKit_Quick");
		const TCHAR* const SlotListFile = TEXT("CoopKit_Index");

		/**
		 * How far the host walks away after the save, in centimeters, along +Y. The player starts of L_ProvingGround stand
		 * in a row along X, 300 cm apart, and the engine picks them at random: a move along X could land on a spot another
		 * pawn still holds, and a teleport through APawn::TeleportTo (ClientSetLocation) would nudge the pawn aside (TASK-29).
		 */
		constexpr double MoveAway = 300.0;

		/** Moves a pawn on the server and its owning client: replicated movement skips the owner (autonomous proxy). */
		void MovePawn(APawn& Pawn, const FVector& Target)
		{
			Pawn.SetActorLocation(Target, false, nullptr, ETeleportType::TeleportPhysics);
			AController* Controller = Pawn.GetController();
			if (Controller != nullptr && !Controller->IsLocalController())
			{
				Controller->ClientSetLocation(Target, Pawn.GetActorRotation());
			}
		}

		/** A character may settle a little on the floor: positions are compared on the ground plane within this distance. */
		constexpr double Tolerance = 30.0;

		UEnhancedInputLocalPlayerSubsystem* GetHostInput(const UWorld* Host)
		{
			const APlayerController* Controller = Host != nullptr ? Host->GetFirstPlayerController() : nullptr;
			return Controller != nullptr ? ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(Controller->GetLocalPlayer()) : nullptr;
		}

		/** Pawn location of every player, by player id. */
		TMap<int32, FVector> GetPawnLocations(const UWorld* World)
		{
			TMap<int32, FVector> Locations;
			const AGameStateBase* GameState = World != nullptr ? World->GetGameState() : nullptr;
			if (GameState != nullptr)
			{
				for (const APlayerState* Player : GameState->PlayerArray)
				{
					if (Player != nullptr && Player->GetPawn() != nullptr)
					{
						Locations.Add(Player->GetPlayerId(), Player->GetPawn()->GetActorLocation());
					}
				}
			}
			return Locations;
		}

		/** Every player's pawn stands (on the ground plane) where Expected says. */
		bool PawnsAt(const UWorld* World, const TMap<int32, FVector>& Expected)
		{
			const TMap<int32, FVector> Actual = GetPawnLocations(World);
			if (Expected.IsEmpty() || Actual.Num() != Expected.Num())
			{
				return false;
			}
			for (const TPair<int32, FVector>& Pair : Expected)
			{
				const FVector* Location = Actual.Find(Pair.Key);
				if (Location == nullptr || FVector::Dist2D(*Location, Pair.Value) > Tolerance)
				{
					return false;
				}
			}
			return true;
		}

		void DeleteSlotFiles()
		{
			UGameplayStatics::DeleteGameInSlot(QuickSlotFile, 0);
			UGameplayStatics::DeleteGameInSlot(SlotListFile, 0);
		}

		/** Latent command: removes the slot files the test wrote, also after a failure. */
		class FDeleteSlotFilesCommand : public IAutomationLatentCommand
		{
		public:
			virtual bool Update() override
			{
				DeleteSlotFiles();
				return true;
			}
		};

		struct FSaveScenario
		{
			TMap<int32, FVector> Saved;
			TMap<int32, FVector> Moved;
		};
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCKSampleSaveListenTest, "CoopKit.Sample.Save.ListenServer",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCKSampleSaveListenTest::RunTest(const FString& Parameters)
{
	using namespace CKSample::Tests;

	// The host saves with F5, everyone walks away, the host loads with F9: everyone is back on host and client.
	FNetSessionSettings Settings;
	Settings.Host = ENetSessionHost::ListenServer;
	Settings.RemoteClients = 1;
	const TSharedRef<FNetTestSession> Session = MakeShared<FNetTestSession>(Settings);
	const TSharedRef<FSaveScenario> Scenario = MakeShared<FSaveScenario>();
	const TWeakObjectPtr<const UInputAction> SaveAction = LoadObject<UInputAction>(nullptr, SaveActionPath);
	const TWeakObjectPtr<const UInputAction> LoadAction = LoadObject<UInputAction>(nullptr, LoadActionPath);
	if (!TestTrue(TEXT("Input actions of the save bridge exist"), SaveAction.IsValid() && LoadAction.IsValid()))
	{
		return false;
	}
	DeleteSlotFiles();

	TArray<FNetStep> Steps;
	Steps.Add({ TEXT("The host has the save keys, the client has none"), nullptr,
		[Session]()
		{
			const TArray<UWorld*> Clients = Session->GetClientWorlds();
			const APlayerController* Host = Session->GetServerWorld()->GetFirstPlayerController();
			const APlayerController* Client = Clients.IsEmpty() ? nullptr : Clients[0]->GetFirstPlayerController();
			return HasComponentNamed(Host, TEXT("CKSampleSaveInputComponent")) && Client != nullptr && !HasComponentNamed(Client, TEXT("CKSampleSaveInputComponent"))
				&& GetPawnLocations(Session->GetServerWorld()).Num() == 2 && GetPawnLocations(Clients[0]).Num() == 2;
		}, 150 });

	Steps.Add({ TEXT("F5 on the host writes the quick save"),
		[Session, Scenario, SaveAction]()
		{
			Scenario->Saved = GetPawnLocations(Session->GetServerWorld());
			GetHostInput(Session->GetServerWorld())->InjectInputForAction(SaveAction.Get(), FInputActionValue(true), {}, {});
		},
		[]() { return UGameplayStatics::DoesSaveGameExist(QuickSlotFile, 0); }, 300 });

	// The client then takes the host's saved spot. The plugin restores players in PlayerArray order, the host first: the
	// host goes back to a spot the client still holds, which a teleport through APawn::TeleportTo would refuse (TASK-29).
	Steps.Add({ TEXT("The host walks away (the server moves the pawn)"),
		[Session, Scenario]()
		{
			const APlayerController* Host = Session->GetServerWorld()->GetFirstPlayerController();
			APawn* Pawn = Host->GetPawn();
			const FVector Target = Pawn->GetActorLocation() + FVector(0.0, MoveAway, 0.0);
			MovePawn(*Pawn, Target);
			Scenario->Moved = Scenario->Saved;
			Scenario->Moved.Add(Host->PlayerState->GetPlayerId(), Target);
		},
		[Session, Scenario]() { return PawnsAt(Session->GetServerWorld(), Scenario->Moved) && PawnsAt(Session->GetClientWorlds()[0], Scenario->Moved); }, 150 });

	Steps.Add({ TEXT("The client takes the host's saved spot (the server moves the pawn)"),
		[Session, Scenario]()
		{
			const APlayerController* Host = Session->GetServerWorld()->GetFirstPlayerController();
			const FVector Target = Scenario->Saved.FindChecked(Host->PlayerState->GetPlayerId());
			for (const APlayerState* Player : Session->GetServerWorld()->GetGameState()->PlayerArray)
			{
				if (Player != Host->PlayerState)
				{
					MovePawn(*Player->GetPawn(), Target);
					Scenario->Moved.Add(Player->GetPlayerId(), Target);
				}
			}
		},
		[Session, Scenario]() { return PawnsAt(Session->GetServerWorld(), Scenario->Moved) && PawnsAt(Session->GetClientWorlds()[0], Scenario->Moved); }, 150 });

	Steps.Add({ TEXT("F9 on the host loads: every pawn is back where it was saved, on the host and the client"),
		[Session, LoadAction]()
		{
			GetHostInput(Session->GetServerWorld())->InjectInputForAction(LoadAction.Get(), FInputActionValue(true), {}, {});
		},
		[Session, Scenario]() { return PawnsAt(Session->GetServerWorld(), Scenario->Saved) && PawnsAt(Session->GetClientWorlds()[0], Scenario->Saved); }, 300 });

	if (!Session->Start(*this, SaveMap))
	{
		return false;
	}
	AddInfo(FString::Printf(TEXT("Scenario: %s"), *Session->Describe()));
	AddCommand(new FNetScenarioCommand(*this, Session, MoveTemp(Steps)));
	AddCommand(new FDeleteSlotFilesCommand());
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
