// Copyright 2026 Ivan Iaitckikh. All Rights Reserved.

#include "CKSampleTraversalBridge.h"
#include "CKTraversalMovementComponent.h"
#include "CoopPlugins.h"
#include "Engine/World.h"
#include "GameFramework/GameModeBase.h"
#include "GameplayTagContainer.h"
#include "UObject/UnrealType.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(CKSampleTraversalBridge)

ACKSampleTraversalCharacter::ACKSampleTraversalCharacter(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer.SetDefaultSubobjectClass<UCKTraversalMovementComponent>(ACharacter::CharacterMovementComponentName))
{
}

void ACKSampleTraversalCharacter::PostInitializeComponents()
{
	CopySampleDefaults();
	Super::PostInitializeComponents();

	// Revive -> Traversal, by tag names only: the same tags on every machine, so the server checks what the client predicts.
	if (UCKTraversalMovementComponent* Movement = GetTraversalMovement())
	{
		FGameplayTagContainer Blocked;
		for (const TCHAR* TagName : { TEXT("CK.Revive.State.Downed"), TEXT("CK.Revive.State.Dead") })
		{
			const FGameplayTag Tag = FGameplayTag::RequestGameplayTag(FName(TagName), false);
			if (Tag.IsValid())
			{
				Blocked.AddTag(Tag);
			}
		}
		Movement->SetBlockedTags(Blocked);
	}
}

void ACKSampleTraversalCharacter::Jump()
{
	// At an obstacle (or by a friend's open offer) the jump key asks for a move; with no move to make it jumps.
	UCKTraversalMovementComponent* Movement = GetTraversalMovement();
	if (Movement != nullptr && Movement->IsMovingOnGround() && Movement->RequestTraverse() == ECKTraversalResult::Started)
	{
		return;
	}
	Super::Jump();
}

UCKTraversalMovementComponent* ACKSampleTraversalCharacter::GetTraversalMovement() const
{
	return Cast<UCKTraversalMovementComponent>(GetCharacterMovement());
}

void ACKSampleTraversalCharacter::CopySampleDefaults()
{
	const UClass* Source = GetDefault<UCKSampleTraversalBridge>()->GetCharacterDefaults().TryLoadClass<ACKSampleCharacter>();
	if (Source == nullptr || Source == ACKSampleCharacter::StaticClass())
	{
		return;
	}
	// The editable defaults of the sample's character (input actions, body paths); components and visible-only fields stay.
	const UObject* SourceDefaults = Source->GetDefaultObject();
	for (TFieldIterator<FProperty> It(ACKSampleCharacter::StaticClass(), EFieldIteratorFlags::ExcludeSuper); It; ++It)
	{
		if (It->HasAnyPropertyFlags(CPF_Edit) && !It->HasAnyPropertyFlags(CPF_EditConst | CPF_InstancedReference | CPF_Transient))
		{
			It->CopyCompleteValue_InContainer(this, SourceDefaults);
		}
	}
}

UCKSampleTraversalBridge::UCKSampleTraversalBridge()
{
	// The maps come only from the ini section (TraversalMaps): an empty list there turns the bridge off. The character
	// Blueprint default works without the section.
	CharacterDefaults = FSoftClassPath(TEXT("/Game/CoopKitSample/Blueprints/BP_CKSampleCharacter.BP_CKSampleCharacter_C"));
}

bool UCKSampleTraversalBridge::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

void UCKSampleTraversalBridge::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	// The game mode is made after the world's subsystems and before any player logs in: the pawn class can still change.
	InitializedHandle = FGameModeEvents::GameModeInitializedEvent.AddUObject(this, &UCKSampleTraversalBridge::HandleGameModeInitialized);
}

void UCKSampleTraversalBridge::Deinitialize()
{
	FGameModeEvents::GameModeInitializedEvent.Remove(InitializedHandle);
	Super::Deinitialize();
}

void UCKSampleTraversalBridge::HandleGameModeInitialized(AGameModeBase* GameMode)
{
	const UWorld* World = GetWorld();
	if (GameMode == nullptr || World == nullptr || GameMode->GetWorld() != World || !IsTraversalMap(*World))
	{
		return;
	}
	// Only the sample's own character is replaced; a map that gives another pawn keeps it.
	const UClass* PawnClass = GameMode->DefaultPawnClass.Get();
	if (PawnClass == nullptr || !PawnClass->IsChildOf(ACKSampleCharacter::StaticClass()) || PawnClass->IsChildOf(ACKSampleTraversalCharacter::StaticClass()))
	{
		return;
	}
	GameMode->DefaultPawnClass = ACKSampleTraversalCharacter::StaticClass();
	bActive = true;
	UE_LOG(LogCKSample, Log, TEXT("CoopKit Traversal bridge: players get the traversal character instead of %s."), *PawnClass->GetName());
}

bool UCKSampleTraversalBridge::IsTraversalMap(const UWorld& World) const
{
	const FString MapPackage = UWorld::RemovePIEPrefix(World.GetOutermost()->GetName());
	return TraversalMaps.ContainsByPredicate([&MapPackage](const FSoftObjectPath& Map)
	{
		return Map.GetLongPackageName() == MapPackage;
	});
}
