// Copyright 2026 Ivan Iaitckikh. All Rights Reserved.

// CoopKit Voice in the sample project, with CoopKit Revive linked by tags only: the bridge sets the sample's profile, the
// server routes by CoopKit Revive's states (a downed player is heard only close by, a dead one not at all by the living),
// at 150 ms lag and 2% loss on a listen server. The plugins stay unlinked from the tests: they are reached by name, the
// engine's own mute list tells the routing's decision.

#include "CKSampleNetTestSession.h"
#include "CKSampleTestHelpers.h"
#include "CollisionQueryParams.h"
#include "Engine/Engine.h"
#include "Engine/HitResult.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "GameplayTagAssetInterface.h"
#include "GameplayTagContainer.h"
#include "Misc/AutomationTest.h"
#include "Subsystems/WorldSubsystem.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace CKSample::Tests
{
	namespace
	{
		const TCHAR* const VoiceMap = TEXT("/Game/CoopKitSample/Maps/L_ProvingGround");

		APlayerController* VoiceLocalController(const UWorld* World)
		{
			return World != nullptr ? World->GetFirstPlayerController() : nullptr;
		}

		/** The server's controller of the player with this unique id. */
		APlayerController* VoiceServerController(UWorld* Server, const FUniqueNetIdRepl& Id)
		{
			for (FConstPlayerControllerIterator It = Server->GetPlayerControllerIterator(); It; ++It)
			{
				if (It->Get() != nullptr && It->Get()->PlayerState != nullptr && It->Get()->PlayerState->GetUniqueId() == Id)
				{
					return It->Get();
				}
			}
			return nullptr;
		}

		/** Calls the bridge's Activate on the server world (by name: the tests do not link the bridges). */
		bool ActivateVoiceBridge(UWorld* Server)
		{
			UClass* BridgeClass = FindObject<UClass>(nullptr, TEXT("/Script/CoopKitSampleBridges.CKSampleVoiceBridge"));
			UWorldSubsystem* Bridge = BridgeClass != nullptr && Server != nullptr ? Server->GetSubsystemBase(BridgeClass) : nullptr;
			UFunction* Activate = Bridge != nullptr ? Bridge->FindFunction(TEXT("Activate")) : nullptr;
			if (Activate == nullptr)
			{
				return false;
			}
			Bridge->ProcessEvent(Activate, nullptr);
			return true;
		}

		/** Index of the player in the server's player array, for the ck.revive console commands. */
		int32 VoiceServerIndex(const UWorld* Server, const FUniqueNetIdRepl& Id)
		{
			const AGameStateBase* GameState = Server->GetGameState();
			return GameState != nullptr ? GameState->PlayerArray.IndexOfByPredicate([&Id](const APlayerState* State) { return State != nullptr && State->GetUniqueId() == Id; }) : INDEX_NONE;
		}

		struct FVoiceSamplePlayers
		{
			FUniqueNetIdRepl Host;
			FUniqueNetIdRepl Client;
			int32 WaitTicks = 0;
		};

		/** Tags the pawn publishes (CoopKit Revive's state among them): the actor and its components with the engine interface. */
		FGameplayTagContainer VoicePawnTags(const APawn* Pawn)
		{
			FGameplayTagContainer Tags;
			if (Pawn == nullptr)
			{
				return Tags;
			}
			if (const IGameplayTagAssetInterface* Own = Cast<IGameplayTagAssetInterface>(Pawn))
			{
				Own->GetOwnedGameplayTags(Tags);
			}
			for (const UActorComponent* Component : Pawn->GetComponentsByInterface(UGameplayTagAssetInterface::StaticClass()))
			{
				FGameplayTagContainer ComponentTags;
				Cast<IGameplayTagAssetInterface>(Component)->GetOwnedGameplayTags(ComponentTags);
				Tags.AppendTags(ComponentTags);
			}
			return Tags;
		}
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCKSampleVoiceListenServerTest, "CoopKit.Sample.Voice.ListenServer",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCKSampleVoiceListenServerTest::RunTest(const FString& Parameters)
{
	using namespace CKSample::Tests;

	FNetSessionSettings Settings;
	Settings.Host = ENetSessionHost::ListenServer;
	Settings.RemoteClients = 1;
	const TSharedRef<FNetTestSession> Session = MakeShared<FNetTestSession>(Settings);
	const TSharedRef<FVoiceSamplePlayers> Players = MakeShared<FVoiceSamplePlayers>();

	// The host hears the client: the host's own controller has no engine mute for the client.
	const TFunction<bool()> HostHearsClient = [Session, Players]()
	{
		APlayerController* Host = VoiceLocalController(Session->GetServerWorld());
		return Host != nullptr && Players->Client.IsValid() && !Host->IsPlayerMuted(*Players->Client.GetUniqueNetId());
	};
	// Moves the host (the server's own local pawn) to the distance from the client: the client's own movement never fights
	// it. The map has walls and props: of four directions the first one with a free line of sight is taken, so the test
	// measures distance and state, not the level's geometry (walls have their own tests).
	const TFunction<void(float)> PlaceClient = [Session, Players](const float Distance)
	{
		UWorld* Server = Session->GetServerWorld();
		APlayerController* Host = VoiceLocalController(Server);
		APlayerController* Client = VoiceServerController(Server, Players->Client);
		APawn* HostPawn = Host != nullptr ? Host->GetPawn() : nullptr;
		const APawn* ClientPawn = Client != nullptr ? Client->GetPawn() : nullptr;
		if (HostPawn == nullptr || ClientPawn == nullptr)
		{
			return;
		}
		const FVector From = ClientPawn->GetActorLocation();
		FCollisionQueryParams Params(SCENE_QUERY_STAT(CKSampleVoicePlace), false);
		Params.AddIgnoredActor(HostPawn);
		Params.AddIgnoredActor(ClientPawn);
		const FVector Directions[] = { FVector(1.0, 0.0, 0.0), FVector(-1.0, 0.0, 0.0), FVector(0.0, 1.0, 0.0), FVector(0.0, -1.0, 0.0) };
		FVector Target = From + Directions[0] * Distance;
		for (const FVector& Direction : Directions)
		{
			FHitResult Hit;
			// A little farther than the spot: the host's own body needs room too.
			if (!Server->LineTraceSingleByChannel(Hit, From, From + Direction * (Distance + 100.0f), ECC_Visibility, Params))
			{
				Target = From + Direction * Distance;
				break;
			}
		}
		HostPawn->SetActorLocation(Target, false, nullptr, ETeleportType::TeleportPhysics);
	};
	// Every second while waiting: the distance and the client's tags, so a failure says what the server saw.
	const TFunction<void(const TCHAR*)> Explain = [this, Session, Players](const TCHAR* What)
	{
		if (++Players->WaitTicks % 30 != 0)
		{
			return;
		}
		UWorld* Server = Session->GetServerWorld();
		const APlayerController* Host = VoiceLocalController(Server);
		const APlayerController* Client = VoiceServerController(Server, Players->Client);
		const APawn* HostPawn = Host != nullptr ? Host->GetPawn() : nullptr;
		const APawn* ClientPawn = Client != nullptr ? Client->GetPawn() : nullptr;
		AddInfo(FString::Printf(TEXT("%s: distance %.0f cm, client tags %s"), What,
			HostPawn != nullptr && ClientPawn != nullptr ? FVector::Dist(HostPawn->GetActorLocation(), ClientPawn->GetActorLocation()) : -1.0f,
			*VoicePawnTags(ClientPawn).ToStringSimple()));
	};
	const TFunction<bool(const TCHAR*)> ClientHasTag = [Session, Players](const TCHAR* TagName)
	{
		const APlayerController* Client = VoiceServerController(Session->GetServerWorld(), Players->Client);
		const FGameplayTag Tag = FGameplayTag::RequestGameplayTag(FName(TagName), false);
		return Client != nullptr && Tag.IsValid() && VoicePawnTags(Client->GetPawn()).HasTag(Tag);
	};
	const TFunction<void(const TCHAR*)> ReviveCommand = [Session, Players](const TCHAR* Command)
	{
		UWorld* Server = Session->GetServerWorld();
		GEngine->Exec(Server, *FString::Printf(TEXT("%s %d"), Command, VoiceServerIndex(Server, Players->Client)));
	};

	TArray<FNetStep> Steps;
	Steps.Add({ TEXT("The bridge switches the sample's voice on: every player gets the voice component, on the client too"),
		[Session]() { ActivateVoiceBridge(Session->GetServerWorld()); },
		[Session, Players]()
		{
			const UWorld* Client = Session->GetClientWorlds().IsEmpty() ? nullptr : Session->GetClientWorlds()[0];
			const APlayerController* Host = VoiceLocalController(Session->GetServerWorld());
			const APlayerController* Remote = VoiceLocalController(Client);
			if (Host == nullptr || Remote == nullptr || Host->PlayerState == nullptr || Remote->PlayerState == nullptr)
			{
				return false;
			}
			Players->Host = Host->PlayerState->GetUniqueId();
			Players->Client = Remote->PlayerState->GetUniqueId();
			return HasComponentNamed(Remote->PlayerState, TEXT("CKVoicePlayerComponent")) && HasComponentNamed(Host->PlayerState, TEXT("CKVoicePlayerComponent"))
				&& HasComponentNamed(Host->PlayerState, TEXT("CKSampleVoiceRadioTags"));
		},
		150 });

	Steps.Add({ TEXT("At 30 m the host does not hear the client (the server mutes the voice)"), [PlaceClient]() { PlaceClient(3000.0f); },
		[HostHearsClient]() { return !HostHearsClient(); }, 120 });
	Steps.Add({ TEXT("At 3 m the host hears the client"),
		[PlaceClient, Players]() { Players->WaitTicks = 0; PlaceClient(300.0f); },
		[HostHearsClient, Explain]()
		{
			Explain(TEXT("Waiting for the voice at 3 m"));
			return HostHearsClient();
		},
		120 });

	Steps.Add({ TEXT("CoopKit Revive downs the client; at 10 m the whisper does not reach the host"),
		[PlaceClient, ReviveCommand]() { ReviveCommand(TEXT("ck.revive.Down")); PlaceClient(1000.0f); },
		[HostHearsClient, ClientHasTag]() { return ClientHasTag(TEXT("CK.Revive.State.Downed")) && !HostHearsClient(); },
		120 });

	Steps.Add({ TEXT("The downed client at 3 m: the whisper is heard"),
		[PlaceClient, Players]() { Players->WaitTicks = 0; PlaceClient(300.0f); },
		[HostHearsClient, ClientHasTag, Explain]()
		{
			Explain(TEXT("Waiting for the whisper"));
			return ClientHasTag(TEXT("CK.Revive.State.Downed")) && HostHearsClient();
		},
		120 });

	Steps.Add({ TEXT("CoopKit Revive kills the client: the living host does not hear the dead even at 3 m"),
		[ReviveCommand]() { ReviveCommand(TEXT("ck.revive.Kill")); },
		[HostHearsClient, ClientHasTag]() { return ClientHasTag(TEXT("CK.Revive.State.Dead")) && !HostHearsClient(); },
		120 });

	if (!Session->Start(*this, VoiceMap))
	{
		return false;
	}
	AddInfo(FString::Printf(TEXT("Scenario: %s on L_ProvingGround"), *Session->Describe()));
	AddCommand(new FNetScenarioCommand(*this, Session, MoveTemp(Steps)));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
