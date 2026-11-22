// Copyright 2026 Ivan Iaitckikh. All Rights Reserved.

// CoopKit Carry with CoopKit Revive in the sample project through the bridges CKSampleCarryBridge and
// CKSampleCarryReviveBridge (module CoopKitSampleBridges): on L_ProvingGround every sample character carries and is a body
// a teammate tows while CoopKit Revive has it down. The listen server host tows its downed client 6 m; the client's own
// movement gets no correction from the server meanwhile; CoopKit Revive gets the client up and the bridge ends the tow on
// every machine. The test does not link against the plugins or the bridges: it finds their components by class name,
// calls their Blueprint functions through reflection, downs and revives with CoopKit Revive's console commands and reads
// states through Gameplay Tags.

#include "CKSampleNetTestSession.h"
#include "CKSampleTestHelpers.h"
#include "Components/ActorComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Controller.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "GameplayTagAssetInterface.h"
#include "GameplayTagContainer.h"
#include "Misc/AutomationTest.h"
#include "UObject/UnrealType.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace CKSample::Tests
{
	namespace CarryTest
	{
		const TCHAR* const Map = TEXT("/Game/CoopKitSample/Maps/L_ProvingGround");

		/** Carry data the CoopKit Carry <-> Revive bridge gives every sample character. */
		const TCHAR* const BodyDataName = TEXT("DA_CKSampleCarry_Body");

		/** Distance from the tower to the tow point of DA_CKSampleCarry_Body (Hold Offset X = -130): the host stands this far in front. */
		constexpr double TowBehindCm = 130.0;

		/** How far the host tows the downed client. */
		constexpr double TowDistanceCm = 600.0;

		/** Frames the downed client rests before the tow, so the down reaches its machine and settles. */
		constexpr int32 SettleTicks = 15;

		/** Frames after the release the report counts the server's corrections of the released client. */
		constexpr int32 ReleaseReportTicks = 30;

		/** Gameplay Tags the actor's components publish (CoopKit Carry and CoopKit Revive publish their states there). */
		bool CarryHasTag(const AActor* Actor, const TCHAR* TagName)
		{
			const FGameplayTag Tag = FGameplayTag::RequestGameplayTag(FName(TagName), false);
			if (Actor == nullptr || !Tag.IsValid())
			{
				return false;
			}
			FGameplayTagContainer Tags;
			for (UActorComponent* Component : Actor->GetComponentsByInterface(UGameplayTagAssetInterface::StaticClass()))
			{
				FGameplayTagContainer ComponentTags;
				Cast<IGameplayTagAssetInterface>(Component)->GetOwnedGameplayTags(ComponentTags);
				Tags.AppendTags(ComponentTags);
			}
			return Tags.HasTagExact(Tag);
		}

		/** Player id of the world's own local player (the host on the listen server), or INDEX_NONE before its player state. */
		int32 CarryLocalPlayerId(const UWorld* World)
		{
			const APlayerController* Controller = World != nullptr ? World->GetFirstPlayerController() : nullptr;
			return Controller != nullptr && Controller->PlayerState != nullptr ? Controller->PlayerState->GetPlayerId() : INDEX_NONE;
		}

		/** The pawn of the player with this id as this world sees it. */
		APawn* CarryPawnOfPlayer(UWorld* World, const int32 PlayerId)
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
		int32 CarryServerPlayerIndex(const UWorld* ServerWorld, const int32 PlayerId)
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

		/** Calls a Blueprint function whose parameters are one actor, through reflection. False when the function is missing. */
		bool CarryCallWithActor(UObject* Object, const TCHAR* FunctionName, AActor* Argument)
		{
			UFunction* Function = Object != nullptr ? Object->FindFunction(FName(FunctionName)) : nullptr;
			if (Function == nullptr)
			{
				return false;
			}
			TArray<uint8> Parameters;
			Parameters.SetNumZeroed(Function->ParmsSize);
			for (TFieldIterator<FProperty> It(Function); It && It->HasAnyPropertyFlags(CPF_Parm); ++It)
			{
				if (const FObjectPropertyBase* ObjectParameter = CastField<FObjectPropertyBase>(*It))
				{
					ObjectParameter->SetObjectPropertyValue_InContainer(Parameters.GetData(), Argument);
				}
			}
			Object->ProcessEvent(Function, Parameters.GetData());
			return true;
		}

		/** Name of the item data of the actor's CoopKit Carry carryable; empty without one. */
		FString CarryItemDataName(const AActor* Actor)
		{
			const UActorComponent* Carryable = FindComponentNamed(Actor, TEXT("CKCarryableComponent"));
			const FObjectPropertyBase* Property = Carryable != nullptr ? FindFProperty<FObjectPropertyBase>(Carryable->GetClass(), TEXT("ItemData")) : nullptr;
			const UObject* Data = Property != nullptr ? Property->GetObjectPropertyValue_InContainer(Carryable) : nullptr;
			return Data != nullptr ? Data->GetName() : FString();
		}

		/** The engine's record of the server's last correction of a client's movement: it changes with every correction sent. */
		const FFloatProperty* CarryAdjustmentTimeProperty()
		{
			return FindFProperty<FFloatProperty>(UCharacterMovementComponent::StaticClass(), TEXT("ServerLastClientAdjustmentTime"));
		}

		/** Server: counts the corrections sent to the pawn's client since the last call (a reset to -1 is not one). */
		void CarryCountCorrections(const APawn* Pawn, float& InOutLastTime, int32& InOutCount)
		{
			const ACharacter* Character = Cast<ACharacter>(Pawn);
			const UCharacterMovementComponent* Movement = Character != nullptr ? Character->GetCharacterMovement() : nullptr;
			const FFloatProperty* Property = CarryAdjustmentTimeProperty();
			if (Movement == nullptr || Property == nullptr)
			{
				return;
			}
			const float Time = Property->GetPropertyValue_InContainer(Movement);
			if (Time > InOutLastTime)
			{
				++InOutCount;
			}
			InOutLastTime = Time;
		}

		/** What the scenario saw. */
		struct FCarryScenario
		{
			int32 HostId = INDEX_NONE;
			int32 ClientId = INDEX_NONE;
			int32 SettledTicks = 0;
			FVector BodyStart = FVector::ZeroVector;
			float LastAdjustmentTime = -1.0f;
			int32 TowCorrections = 0;
			int32 ReleaseCorrections = 0;
			int32 ReleaseTicks = 0;
			bool bClientSawTow = false;
			int32 ClientTowTicks = 0;
			double MaxClientGapCm = 0.0;
		};
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCKSampleCarryListenTest, "CoopKit.Sample.Carry.ListenServer",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCKSampleCarryListenTest::RunTest(const FString& Parameters)
{
	using namespace CKSample::Tests;
	using namespace CKSample::Tests::CarryTest;

	FNetSessionSettings Settings;
	Settings.Host = ENetSessionHost::ListenServer;
	Settings.RemoteClients = 1;
	const TSharedRef<FNetTestSession> Session = MakeShared<FNetTestSession>(Settings);
	const TSharedRef<FCarryScenario> State = MakeShared<FCarryScenario>();
	FAutomationTestBase* TestPtr = this;

	if (!TestNotNull(TEXT("UCharacterMovementComponent has ServerLastClientAdjustmentTime (the test counts corrections with it)"), CarryAdjustmentTimeProperty()))
	{
		return false;
	}

	TArray<FNetStep> Steps;
	Steps.Add({ TEXT("The bridges add Carry: every character carries and is a body to tow (DA_CKSampleCarry_Body) with a Revive body, on every machine"),
		nullptr,
		[Session, State]()
		{
			UWorld* Server = Session->GetServerWorld();
			const TArray<UWorld*> Clients = Session->GetClientWorlds();
			if (Server == nullptr || Clients.Num() != 1)
			{
				return false;
			}
			State->HostId = CarryLocalPlayerId(Server);
			State->ClientId = CarryLocalPlayerId(Clients[0]);
			const APawn* Host = CarryPawnOfPlayer(Server, State->HostId);
			const APawn* Body = CarryPawnOfPlayer(Server, State->ClientId);
			const APawn* HostOnClient = CarryPawnOfPlayer(Clients[0], State->HostId);
			const APawn* BodyOnClient = CarryPawnOfPlayer(Clients[0], State->ClientId);
			return HasComponentNamed(Host, TEXT("CKCarrierComponent")) && HasComponentNamed(HostOnClient, TEXT("CKCarrierComponent"))
				&& HasComponentNamed(BodyOnClient, TEXT("CKCarrierComponent")) && HasComponentNamed(Body, TEXT("CKReviveBodyComponent"))
				&& CarryItemDataName(Body) == BodyDataName && CarryItemDataName(BodyOnClient) == BodyDataName
				&& HasComponentNamed(Server->GetFirstPlayerController(), TEXT("CKSampleCarryInputComponent"))
				&& HasComponentNamed(Clients[0]->GetFirstPlayerController(), TEXT("CKSampleCarryInputComponent"));
		}, 300 });

	Steps.Add({ TEXT("Server: ck.revive.Down downs the client; the host stands 130 cm in front of it; the client sees itself down"),
		[Session, State, TestPtr]()
		{
			UWorld* Server = Session->GetServerWorld();
			APawn* Host = CarryPawnOfPlayer(Server, State->HostId);
			const APawn* Body = CarryPawnOfPlayer(Server, State->ClientId);
			const int32 Index = CarryServerPlayerIndex(Server, State->ClientId);
			if (Host == nullptr || Body == nullptr || Index == INDEX_NONE)
			{
				TestPtr->AddError(TEXT("The server has no pawn for the host or the client."));
				return;
			}
			GEngine->Exec(Server, *FString::Printf(TEXT("ck.revive.Down %d"), Index));
			// The host walks along +X with the body at its tow point behind; only the host moves (it is the server's own
			// player), so the client's position is never corrected by the setup.
			const FVector BodyLocation = Body->GetActorLocation();
			Host->TeleportTo(FVector(BodyLocation.X + TowBehindCm, BodyLocation.Y, Host->GetActorLocation().Z), FRotator::ZeroRotator);
			if (AController* Controller = Host->GetController())
			{
				Controller->SetControlRotation(FRotator::ZeroRotator);
			}
		},
		[Session, State]()
		{
			const TArray<UWorld*> Clients = Session->GetClientWorlds();
			if (Clients.IsEmpty() || !CarryHasTag(CarryPawnOfPlayer(Clients[0], State->ClientId), TEXT("CK.Revive.State.Downed")))
			{
				return false;
			}
			return ++State->SettledTicks >= SettleTicks;
		}, 120 });

	Steps.Add({ TEXT("The host tows the downed client 6 m (Request Tow); the client's movement gets no correction from the server"),
		[Session, State, TestPtr]()
		{
			UWorld* Server = Session->GetServerWorld();
			APawn* Host = CarryPawnOfPlayer(Server, State->HostId);
			APawn* Body = CarryPawnOfPlayer(Server, State->ClientId);
			if (Host == nullptr || Body == nullptr)
			{
				TestPtr->AddError(TEXT("The server has no pawn for the host or the client."));
				return;
			}
			State->BodyStart = Body->GetActorLocation();
			int32 Ignored = 0;
			CarryCountCorrections(Body, State->LastAdjustmentTime, Ignored);
			if (!CarryCallWithActor(FindComponentNamed(Host, TEXT("CKCarrierComponent")), TEXT("RequestTow"), Body))
			{
				TestPtr->AddError(TEXT("The host's carrier component has no Request Tow."));
			}
		},
		[Session, State]()
		{
			UWorld* Server = Session->GetServerWorld();
			const TArray<UWorld*> Clients = Session->GetClientWorlds();
			APawn* Host = CarryPawnOfPlayer(Server, State->HostId);
			const APawn* Body = CarryPawnOfPlayer(Server, State->ClientId);
			if (Host == nullptr || Body == nullptr || Clients.IsEmpty())
			{
				return false;
			}
			CarryCountCorrections(Body, State->LastAdjustmentTime, State->TowCorrections);
			if (!CarryHasTag(Body, TEXT("CK.Carry.State.Towed")))
			{
				return false;
			}
			// The listen server's own player walks on; CoopKit Carry slows it to the body's tow speed.
			Host->AddMovementInput(FVector::ForwardVector, 1.0f);

			const APawn* BodyOnClient = CarryPawnOfPlayer(Clients[0], State->ClientId);
			const APawn* HostOnClient = CarryPawnOfPlayer(Clients[0], State->HostId);
			if (CarryHasTag(BodyOnClient, TEXT("CK.Carry.State.Towed")) && HostOnClient != nullptr)
			{
				State->bClientSawTow = true;
				// For the report: how far from the tow point the client sees its own body (after the attach settles).
				if (++State->ClientTowTicks > 10)
				{
					const double Gap = FMath::Abs(FVector::Dist2D(BodyOnClient->GetActorLocation(), HostOnClient->GetActorLocation()) - TowBehindCm);
					State->MaxClientGapCm = FMath::Max(State->MaxClientGapCm, Gap);
				}
			}
			return State->bClientSawTow && FVector::Dist2D(Body->GetActorLocation(), State->BodyStart) >= TowDistanceCm;
		}, 450 });

	Steps.Add({ TEXT("Server: ck.revive.Revive gets the client up; the bridge ends the tow; every machine sees the client standing and free"),
		[Session, State, TestPtr]()
		{
			UWorld* Server = Session->GetServerWorld();
			const APawn* Host = CarryPawnOfPlayer(Server, State->HostId);
			const APawn* Body = CarryPawnOfPlayer(Server, State->ClientId);
			TestPtr->TestEqual(TEXT("Server corrections of the towed client's movement"), State->TowCorrections, 0);
			TestPtr->TestTrue(TEXT("The body came along behind the host on the server"),
				Host != nullptr && Body != nullptr && FVector::Dist2D(Host->GetActorLocation(), Body->GetActorLocation()) < TowBehindCm + 50.0);
			TestPtr->AddInfo(FString::Printf(TEXT("The towed client saw its body at most %.1f cm off the tow distance from the host."), State->MaxClientGapCm));
			GEngine->Exec(Server, *FString::Printf(TEXT("ck.revive.Revive %d"), CarryServerPlayerIndex(Server, State->ClientId)));
		},
		[Session, State]()
		{
			UWorld* Server = Session->GetServerWorld();
			const TArray<UWorld*> Clients = Session->GetClientWorlds();
			const APawn* Host = CarryPawnOfPlayer(Server, State->HostId);
			const APawn* Body = CarryPawnOfPlayer(Server, State->ClientId);
			if (Host == nullptr || Body == nullptr || Clients.IsEmpty())
			{
				return false;
			}
			CarryCountCorrections(Body, State->LastAdjustmentTime, State->ReleaseCorrections);
			const APawn* HostOnClient = CarryPawnOfPlayer(Clients[0], State->HostId);
			const APawn* BodyOnClient = CarryPawnOfPlayer(Clients[0], State->ClientId);
			return HostOnClient != nullptr && BodyOnClient != nullptr
				&& !CarryHasTag(Body, TEXT("CK.Revive.State.Downed")) && !CarryHasTag(BodyOnClient, TEXT("CK.Revive.State.Downed"))
				&& !CarryHasTag(Body, TEXT("CK.Carry.State.Towed")) && !CarryHasTag(BodyOnClient, TEXT("CK.Carry.State.Towed"))
				&& !CarryHasTag(Host, TEXT("CK.Carry.State.Carrying")) && !CarryHasTag(HostOnClient, TEXT("CK.Carry.State.Carrying"));
		}, 150 });

	Steps.Add({ TEXT("The released client walks on from where its machine put it (server corrections in the next second, for the report)"),
		nullptr,
		[Session, State, TestPtr]()
		{
			CarryCountCorrections(CarryPawnOfPlayer(Session->GetServerWorld(), State->ClientId), State->LastAdjustmentTime, State->ReleaseCorrections);
			if (++State->ReleaseTicks < ReleaseReportTicks)
			{
				return false;
			}
			TestPtr->AddInfo(FString::Printf(TEXT("Server corrections of the client from the release on: %d."), State->ReleaseCorrections));
			return true;
		}, ReleaseReportTicks + 15 });

	if (!Session->Start(*this, Map))
	{
		Session->Stop();
		return false;
	}
	AddInfo(FString::Printf(TEXT("Scenario: %s on %s"), *Session->Describe(), Map));
	AddCommand(new FNetScenarioCommand(*this, Session, MoveTemp(Steps)));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
