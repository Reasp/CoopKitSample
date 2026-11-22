// Copyright 2026 Ivan Iaitckikh. All Rights Reserved.

// CoopKit Revive in the sample project through the bridge CKSampleReviveBridge (module CoopKitSampleBridges):
// the bridge adds the Revive match component to the game state of L_ProvingGround and the Revive keys to every local
// player controller. The test does not link against the plugin or the bridges: it finds their components by class
// name, downs a player with the plugin's console command, reads states through Gameplay Tags and presses the keys by
// injecting the bridge's Enhanced Input actions on the client, the way a player does.

#include "CKSampleNetTestSession.h"
#include "CKSampleTestHelpers.h"
#include "Components/ActorComponent.h"
#include "Engine/Engine.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "EnhancedInputSubsystems.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "GameplayTagAssetInterface.h"
#include "GameplayTagContainer.h"
#include "InputAction.h"
#include "InputActionValue.h"
#include "Misc/AutomationTest.h"
#include "UObject/Package.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace CKSample::Tests
{
	namespace
	{
		const TCHAR* const ReviveMap = TEXT("/Game/CoopKitSample/Maps/L_ProvingGround");
		const TCHAR* const ReviveActionPath = TEXT("/Game/CoopKitSample/Input/IA_Revive.IA_Revive");
		const TCHAR* const GiveUpActionPath = TEXT("/Game/CoopKitSample/Input/IA_ReviveGiveUp.IA_ReviveGiveUp");

		/** Distance between the downed pawn and its reviver after the test moves them together; below the default revive range of 200. */
		constexpr double ReviveDistance = 110.0;

		/** Tags published by the actor's components (the Revive body publishes CK.Revive.State.*). */
		FGameplayTagContainer GetComponentTags(const AActor* Actor)
		{
			FGameplayTagContainer Tags;
			if (Actor == nullptr)
			{
				return Tags;
			}
			for (UActorComponent* Component : Actor->GetComponentsByInterface(UGameplayTagAssetInterface::StaticClass()))
			{
				FGameplayTagContainer ComponentTags;
				Cast<IGameplayTagAssetInterface>(Component)->GetOwnedGameplayTags(ComponentTags);
				Tags.AppendTags(ComponentTags);
			}
			return Tags;
		}

		bool HasTag(const AActor* Actor, const TCHAR* TagName)
		{
			const FGameplayTag Tag = FGameplayTag::RequestGameplayTag(FName(TagName), false);
			return Tag.IsValid() && GetComponentTags(Actor).HasTagExact(Tag);
		}

		APlayerController* GetLocalController(const UWorld* World)
		{
			return World != nullptr ? World->GetFirstPlayerController() : nullptr;
		}

		/** Player id of the client's own player, or INDEX_NONE before its player state arrives. */
		int32 GetLocalPlayerId(const UWorld* World)
		{
			const APlayerController* Controller = GetLocalController(World);
			return Controller != nullptr && Controller->PlayerState != nullptr ? Controller->PlayerState->GetPlayerId() : INDEX_NONE;
		}

		/** The pawn of the player with this id as this world sees it (an autonomous or simulated proxy on a client). */
		APawn* FindPawnOfPlayer(UWorld* World, const int32 PlayerId)
		{
			if (World == nullptr || PlayerId == INDEX_NONE)
			{
				return nullptr;
			}
			for (TActorIterator<APawn> It(World); It; ++It)
			{
				const APlayerState* PlayerState = It->GetPlayerState();
				if (PlayerState != nullptr && PlayerState->GetPlayerId() == PlayerId && !It->IsPendingKillPending())
				{
					return *It;
				}
			}
			return nullptr;
		}

		/** Index of the player in the server's PlayerArray, the argument of the ck.revive.* console commands. */
		int32 GetServerPlayerIndex(const UWorld* ServerWorld, const int32 PlayerId)
		{
			const AGameStateBase* GameState = ServerWorld != nullptr ? ServerWorld->GetGameState() : nullptr;
			if (GameState == nullptr)
			{
				return INDEX_NONE;
			}
			return GameState->PlayerArray.IndexOfByPredicate([PlayerId](const APlayerState* PlayerState)
			{
				return PlayerState != nullptr && PlayerState->GetPlayerId() == PlayerId;
			});
		}

		/** True when every client sees the pawn of the player with this id carrying the tag (or not, when bExpected is false). */
		bool AllClientsSeePlayerTag(const FNetTestSession& Session, const int32 PlayerId, const TCHAR* TagName, const bool bExpected = true)
		{
			const TArray<UWorld*> Clients = Session.GetClientWorlds();
			if (Clients.Num() != Session.GetSettings().RemoteClients)
			{
				return false;
			}
			for (UWorld* World : Clients)
			{
				const APawn* Pawn = FindPawnOfPlayer(World, PlayerId);
				if (Pawn == nullptr || HasTag(Pawn, TagName) != bExpected)
				{
					return false;
				}
			}
			return true;
		}

		UEnhancedInputLocalPlayerSubsystem* GetInputSubsystem(const UWorld* World)
		{
			const APlayerController* Controller = GetLocalController(World);
			return Controller != nullptr ? ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(Controller->GetLocalPlayer()) : nullptr;
		}

		/** Players of the scenario: Downed is client 0's player, Reviver is client 1's player. */
		struct FRevivePlayers
		{
			int32 Downed = INDEX_NONE;
			int32 Reviver = INDEX_NONE;
		};

		/** Server: downs the player with the console command and moves the pawn next to the reviver's pawn. */
		void DownNextToReviver(const FNetTestSession& Session, const FRevivePlayers& Players, FAutomationTestBase& Test)
		{
			UWorld* Server = Session.GetServerWorld();
			const int32 Index = GetServerPlayerIndex(Server, Players.Downed);
			APawn* Downed = FindPawnOfPlayer(Server, Players.Downed);
			const APawn* Reviver = FindPawnOfPlayer(Server, Players.Reviver);
			if (Index == INDEX_NONE || Downed == nullptr || Reviver == nullptr)
			{
				Test.AddError(TEXT("The server has no pawn for the downed player or the reviver."));
				return;
			}
			GEngine->Exec(Server, *FString::Printf(TEXT("ck.revive.Down %d"), Index));
			Downed->SetActorLocation(Reviver->GetActorLocation() + FVector(ReviveDistance, 0.0, 0.0), false, nullptr, ETeleportType::TeleportPhysics);
		}

		bool RunReviveScenario(FAutomationTestBase& Test, const ENetSessionHost Host)
		{
			FNetSessionSettings Settings;
			Settings.Host = Host;
			Settings.RemoteClients = 2;
			const TSharedRef<FNetTestSession> Session = MakeShared<FNetTestSession>(Settings);
			const TSharedRef<FRevivePlayers> Players = MakeShared<FRevivePlayers>();
			FAutomationTestBase* TestPtr = &Test;

			const UInputAction* ReviveAction = LoadObject<UInputAction>(nullptr, ReviveActionPath);
			const UInputAction* GiveUpAction = LoadObject<UInputAction>(nullptr, GiveUpActionPath);
			if (!Test.TestNotNull(TEXT("IA_Revive exists"), ReviveAction) || !Test.TestNotNull(TEXT("IA_ReviveGiveUp exists"), GiveUpAction))
			{
				return false;
			}
			const TWeakObjectPtr<const UInputAction> WeakRevive(ReviveAction);
			const TWeakObjectPtr<const UInputAction> WeakGiveUp(GiveUpAction);

			TArray<FNetStep> Steps;
			Steps.Add({ TEXT("The bridge adds Revive: match component on the game state, body on every pawn, keys on every client"), nullptr,
				[Session, Players]()
				{
					const UWorld* Server = Session->GetServerWorld();
					if (Server == nullptr || !HasComponentNamed(Server->GetGameState(), TEXT("CKReviveMatchComponent")))
					{
						return false;
					}
					const TArray<UWorld*> Clients = Session->GetClientWorlds();
					for (const UWorld* World : Clients)
					{
						const APlayerController* Controller = GetLocalController(World);
						if (!HasComponentNamed(World->GetGameState(), TEXT("CKReviveMatchComponent"))
							|| !HasComponentNamed(Controller, TEXT("CKSampleReviveInputComponent"))
							|| !HasComponentNamed(Controller, TEXT("CKReviveControllerComponent"))
							|| Controller->GetPawn() == nullptr || !HasComponentNamed(Controller->GetPawn(), TEXT("CKReviveBodyComponent")))
						{
							return false;
						}
					}
					Players->Downed = GetLocalPlayerId(Clients[0]);
					Players->Reviver = GetLocalPlayerId(Clients[1]);
					return Players->Downed != INDEX_NONE && Players->Reviver != INDEX_NONE;
				},
				150 });

			Steps.Add({ TEXT("Server: ck.revive.Down next to the reviver; every client sees the player down within reach"),
				[Session, Players, TestPtr]() { DownNextToReviver(*Session, *Players, *TestPtr); },
				[Session, Players]()
				{
					if (!AllClientsSeePlayerTag(*Session, Players->Downed, TEXT("CK.Revive.State.Downed")))
					{
						return false;
					}
					UWorld* ReviverWorld = Session->GetClientWorlds()[1];
					const APawn* Downed = FindPawnOfPlayer(ReviverWorld, Players->Downed);
					const APawn* Reviver = FindPawnOfPlayer(ReviverWorld, Players->Reviver);
					return Downed != nullptr && Reviver != nullptr && FVector::Dist(Downed->GetActorLocation(), Reviver->GetActorLocation()) < 150.0;
				},
				90 });

			Steps.Add({ TEXT("Client 1 holds the revive key (IA_Revive): every client sees the revive running"),
				[Session, WeakRevive]()
				{
					if (UEnhancedInputLocalPlayerSubsystem* Input = GetInputSubsystem(Session->GetClientWorlds()[1]))
					{
						Input->StartContinuousInputInjectionForAction(WeakRevive.Get(), FInputActionValue(true), {}, {});
					}
				},
				[Session, Players]() { return AllClientsSeePlayerTag(*Session, Players->Downed, TEXT("CK.Revive.State.BeingRevived")); },
				90 });

			// Default body data (no Default Body Data in the project settings): a 4 s revive.
			Steps.Add({ TEXT("The revive completes while the key is held: every client sees the player standing"), nullptr,
				[Session, Players]() { return AllClientsSeePlayerTag(*Session, Players->Downed, TEXT("CK.Revive.State.Downed"), false); },
				210 });

			Steps.Add({ TEXT("Server: ck.revive.Down again; client 0 presses give up (IA_ReviveGiveUp): every client sees the player dead"),
				[Session, Players, TestPtr, WeakRevive, WeakGiveUp]()
				{
					if (UEnhancedInputLocalPlayerSubsystem* ReviverInput = GetInputSubsystem(Session->GetClientWorlds()[1]))
					{
						ReviverInput->StopContinuousInputInjectionForAction(WeakRevive.Get());
					}
					DownNextToReviver(*Session, *Players, *TestPtr);
				},
				[Session, Players, WeakGiveUp]()
				{
					// Pressed every frame until the client itself sees its body down: the press must arrive after the down.
					UWorld* DownedWorld = Session->GetClientWorlds()[0];
					if (HasTag(FindPawnOfPlayer(DownedWorld, Players->Downed), TEXT("CK.Revive.State.Downed")))
					{
						if (UEnhancedInputLocalPlayerSubsystem* Input = GetInputSubsystem(DownedWorld))
						{
							Input->InjectInputForAction(WeakGiveUp.Get(), FInputActionValue(true), {}, {});
						}
					}
					return AllClientsSeePlayerTag(*Session, Players->Downed, TEXT("CK.Revive.State.Dead"));
				},
				120 });

			if (!Session->Start(Test, ReviveMap))
			{
				Session->Stop();
				return false;
			}
			Test.AddInfo(FString::Printf(TEXT("Scenario: %s on %s"), *Session->Describe(), ReviveMap));
			Test.AddCommand(new FNetScenarioCommand(Test, Session, MoveTemp(Steps)));
			return true;
		}
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCKSampleReviveDedicatedTest, "CoopKit.Sample.Revive.DedicatedServer",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCKSampleReviveDedicatedTest::RunTest(const FString& Parameters)
{
	return CKSample::Tests::RunReviveScenario(*this, CKSample::Tests::ENetSessionHost::DedicatedServer);
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCKSampleReviveListenTest, "CoopKit.Sample.Revive.ListenServer",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCKSampleReviveListenTest::RunTest(const FString& Parameters)
{
	return CKSample::Tests::RunReviveScenario(*this, CKSample::Tests::ENetSessionHost::ListenServer);
}

#endif // WITH_DEV_AUTOMATION_TESTS
