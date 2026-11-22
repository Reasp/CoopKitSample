// Copyright 2026 Ivan Iaitckikh. All Rights Reserved.

#include "CKSampleCombatBridge.h"
#include "CKCombatComponent.h"
#include "CKCombatProfileData.h"
#include "CKSampleCharacter.h"
#include "CKSamplePlayerController.h"
#include "CoopPlugins.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "EngineUtils.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "InputAction.h"
#include "InputActionValue.h"
#include "InputMappingContext.h"
#include "TimerManager.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(CKSampleCombatBridge)

namespace CKSample::CombatBridge
{
	/** How often the bridge looks for local player controllers, in seconds. */
	constexpr float InputScanInterval = 0.25f;

	/** Name of the combat component on every fighter. */
	const TCHAR* const CombatComponentName = TEXT("CKCombat");
}

bool UCKSampleCombatBridge::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

void UCKSampleCombatBridge::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	if (!IsCombatMap(InWorld))
	{
		return;
	}

	if (InWorld.GetNetMode() != NM_Client)
	{
		// Pawns placed in the map or already spawned (the listen host's), then every new one the moment it spawns.
		for (TActorIterator<APawn> It(&InWorld); It; ++It)
		{
			EquipPawn(**It);
		}
		ActorSpawnedHandle = InWorld.AddOnActorSpawnedHandler(FOnActorSpawned::FDelegate::CreateUObject(this, &UCKSampleCombatBridge::HandleActorSpawned));
	}
	if (InWorld.GetNetMode() != NM_DedicatedServer)
	{
		AddInputComponents();
		InWorld.GetTimerManager().SetTimer(InputTimer, FTimerDelegate::CreateUObject(this, &UCKSampleCombatBridge::AddInputComponents),
			CKSample::CombatBridge::InputScanInterval, true);
	}
}

void UCKSampleCombatBridge::Deinitialize()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(InputTimer);
		World->RemoveOnActorSpawnedHandler(ActorSpawnedHandle);
	}
	Super::Deinitialize();
}

bool UCKSampleCombatBridge::IsCombatMap(const UWorld& World) const
{
	const FString MapPackage = UWorld::RemovePIEPrefix(World.GetOutermost()->GetName());
	return CombatMaps.ContainsByPredicate([&MapPackage](const FSoftObjectPath& Map)
	{
		return Map.GetLongPackageName() == MapPackage;
	});
}

void UCKSampleCombatBridge::HandleActorSpawned(AActor* Actor)
{
	if (APawn* Pawn = Cast<APawn>(Actor))
	{
		EquipPawn(*Pawn);
	}
}

void UCKSampleCombatBridge::EquipPawn(APawn& Pawn)
{
	if (!Pawn.HasAuthority() || UCKCombatComponent::FindCombatComponent(&Pawn) != nullptr)
	{
		return;
	}
	const bool bPlayer = Pawn.IsA<ACKSampleCharacter>();
	const bool bEnemy = !bPlayer && EnemyClasses.ContainsByPredicate([&Pawn](const TSoftClassPtr<APawn>& EnemyClass)
	{
		const UClass* Class = EnemyClass.LoadSynchronous();
		return Class != nullptr && Pawn.IsA(Class);
	});
	if (!bPlayer && !bEnemy)
	{
		return;
	}

	// Profile before registration: registering on an actor that has begun play runs the component's BeginPlay.
	UCKCombatComponent* Combat = NewObject<UCKCombatComponent>(&Pawn, CKSample::CombatBridge::CombatComponentName);
	if (UCKCombatProfileData* Profile = (bPlayer ? PlayerProfile : EnemyProfile).LoadSynchronous())
	{
		Combat->SetProfile(Profile);
	}
	Pawn.AddInstanceComponent(Combat);
	Combat->RegisterComponent();
	UE_LOG(LogCKSample, Log, TEXT("CoopKit Combat bridge: %s fights (%s)."), *Pawn.GetName(), bPlayer ? TEXT("player") : TEXT("enemy"));
}

void UCKSampleCombatBridge::AddInputComponents()
{
	UWorld* World = GetWorld();
	if (World == nullptr)
	{
		return;
	}
	for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
	{
		APlayerController* Controller = It->Get();
		// Input needs a local player and the input component the engine creates when the player is assigned.
		if (Controller == nullptr || !Controller->IsLocalController() || !Controller->IsA<ACKSamplePlayerController>()
			|| Controller->GetLocalPlayer() == nullptr || Controller->InputComponent == nullptr
			|| Controller->FindComponentByClass<UCKSampleCombatInputComponent>() != nullptr)
		{
			continue;
		}
		UCKSampleCombatInputComponent* Input = NewObject<UCKSampleCombatInputComponent>(Controller, TEXT("CKSampleCombatInput"));
		Input->Configure(InputMappingContext.LoadSynchronous(), InputMappingPriority, AttackAction.LoadSynchronous(), BlockAction.LoadSynchronous(),
			DodgeAction.LoadSynchronous());
		Controller->AddInstanceComponent(Input);
		Input->RegisterComponent();
	}
}

UCKSampleCombatInputComponent::UCKSampleCombatInputComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UCKSampleCombatInputComponent::Configure(UInputMappingContext* InMappingContext, const int32 InMappingPriority, UInputAction* InAttack,
	UInputAction* InBlock, UInputAction* InDodge)
{
	MappingContext = InMappingContext;
	MappingPriority = InMappingPriority;
	AttackAction = InAttack;
	BlockAction = InBlock;
	DodgeAction = InDodge;
}

UCKCombatComponent* UCKSampleCombatInputComponent::GetCombat() const
{
	const APlayerController* Controller = Cast<APlayerController>(GetOwner());
	return Controller != nullptr ? UCKCombatComponent::FindCombatComponent(Controller->GetPawn()) : nullptr;
}

void UCKSampleCombatInputComponent::BeginPlay()
{
	Super::BeginPlay();

	APlayerController* Controller = Cast<APlayerController>(GetOwner());
	UEnhancedInputComponent* Input = Controller != nullptr ? Cast<UEnhancedInputComponent>(Controller->InputComponent) : nullptr;
	if (Input == nullptr)
	{
		UE_LOG(LogCKSample, Warning, TEXT("%s: the owner is not a player controller with an Enhanced Input component; combat keys are not bound."), *GetName());
		return;
	}
	if (MappingContext == nullptr || AttackAction == nullptr || BlockAction == nullptr || DodgeAction == nullptr)
	{
		UE_LOG(LogCKSample, Warning, TEXT("%s: input assets are missing in [/Script/CoopKitSampleBridges.CKSampleCombatBridge] of DefaultGame.ini."), *GetName());
		return;
	}
	if (UEnhancedInputLocalPlayerSubsystem* InputSubsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(Controller->GetLocalPlayer()))
	{
		InputSubsystem->AddMappingContext(MappingContext, MappingPriority);
	}
	BindingHandles.Add(Input->BindAction(AttackAction, ETriggerEvent::Started, this, &UCKSampleCombatInputComponent::HandleAttack).GetHandle());
	BindingHandles.Add(Input->BindAction(BlockAction, ETriggerEvent::Started, this, &UCKSampleCombatInputComponent::HandleBlockStarted).GetHandle());
	BindingHandles.Add(Input->BindAction(BlockAction, ETriggerEvent::Completed, this, &UCKSampleCombatInputComponent::HandleBlockStopped).GetHandle());
	BindingHandles.Add(Input->BindAction(BlockAction, ETriggerEvent::Canceled, this, &UCKSampleCombatInputComponent::HandleBlockStopped).GetHandle());
	BindingHandles.Add(Input->BindAction(DodgeAction, ETriggerEvent::Started, this, &UCKSampleCombatInputComponent::HandleDodge).GetHandle());
}

void UCKSampleCombatInputComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	const APlayerController* Controller = Cast<APlayerController>(GetOwner());
	if (UEnhancedInputComponent* Input = Controller != nullptr ? Cast<UEnhancedInputComponent>(Controller->InputComponent) : nullptr)
	{
		for (const uint32 Handle : BindingHandles)
		{
			Input->RemoveBindingByHandle(Handle);
		}
	}
	BindingHandles.Reset();
	if (Controller != nullptr && MappingContext != nullptr)
	{
		if (UEnhancedInputLocalPlayerSubsystem* InputSubsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(Controller->GetLocalPlayer()))
		{
			InputSubsystem->RemoveMappingContext(MappingContext);
		}
	}
	Super::EndPlay(EndPlayReason);
}

void UCKSampleCombatInputComponent::HandleAttack(const FInputActionValue& Value)
{
	if (UCKCombatComponent* Combat = GetCombat())
	{
		Combat->Attack();
	}
}

void UCKSampleCombatInputComponent::HandleBlockStarted(const FInputActionValue& Value)
{
	if (UCKCombatComponent* Combat = GetCombat())
	{
		Combat->StartBlock();
	}
}

void UCKSampleCombatInputComponent::HandleBlockStopped(const FInputActionValue& Value)
{
	if (UCKCombatComponent* Combat = GetCombat())
	{
		Combat->StopBlock();
	}
}

void UCKSampleCombatInputComponent::HandleDodge(const FInputActionValue& Value)
{
	if (UCKCombatComponent* Combat = GetCombat())
	{
		// Zero: the component takes the last movement input, else dodges backwards.
		Combat->Dodge(FVector::ZeroVector);
	}
}
