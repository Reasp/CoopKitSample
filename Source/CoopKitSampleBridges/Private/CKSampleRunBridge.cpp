// Copyright 2026 Ivan Iaitckikh. All Rights Reserved.

#include "CKSampleRunBridge.h"
#include "CKReviveMatchComponent.h"
#include "CKRunComponent.h"
#include "CKRunData.h"
#include "CKRunDemo.h"
#include "CKSaveableComponent.h"
#include "CoopPlugins.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "TimerManager.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(CKSampleRunBridge)

ACKSampleRunDirector::ACKSampleRunDirector(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	Saveable = CreateDefaultSubobject<UCKSaveableComponent>(TEXT("Saveable"));
}

void ACKSampleRunDirector::OnBeforeSave_Implementation()
{
	if (UCKRunComponent* Run = GetRunComponent())
	{
		Run->PrepareForSave();
	}
}

void ACKSampleRunDirector::OnAfterLoad_Implementation(const int32 SavedGameDataVersion)
{
	if (UCKRunComponent* Run = GetRunComponent())
	{
		Run->RestoreAfterLoad();
	}
}

UCKSampleRunBridge::UCKSampleRunBridge()
{
	// Defaults without the ini section: the sample's proving ground and the plugin's demo run.
	RunMaps = { FSoftObjectPath(TEXT("/Game/CoopKitSample/Maps/L_ProvingGround.L_ProvingGround")) };
}

bool UCKSampleRunBridge::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

void UCKSampleRunBridge::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	// Every machine makes the built-in run, so the replicated run names data each machine knows.
	GetSampleRun();
	if (!IsRunMap(InWorld) || InWorld.GetNetMode() == NM_Client)
	{
		return;
	}
	// The director must exist before anyone asks for one, or the plugin spawns its own without the save component.
	if (TActorIterator<ACKSampleRunDirector>(&InWorld))
	{
		return;
	}
	FActorSpawnParameters Parameters;
	Parameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	InWorld.SpawnActor<ACKSampleRunDirector>(FVector::ZeroVector, FRotator::ZeroRotator, Parameters);
	BindRevive();
	if (!bReviveBound)
	{
		InWorld.GetTimerManager().SetTimer(BindTimer, FTimerDelegate::CreateUObject(this, &UCKSampleRunBridge::BindRevive), 1.0f, true);
	}
}

void UCKSampleRunBridge::Deinitialize()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(BindTimer);
	}
	Super::Deinitialize();
}

UCKRunData* UCKSampleRunBridge::GetSampleRun() const
{
	UCKRunData* Run = SampleRun.IsNull() ? nullptr : SampleRun.LoadSynchronous();
	return Run != nullptr ? Run : CKRun::Demo::GetBuiltInRun();
}

UCKRunComponent* UCKSampleRunBridge::GetDirector() const
{
	return UCKRunComponent::FindRunDirector(GetWorld());
}

bool UCKSampleRunBridge::StartSampleRun()
{
	UCKRunComponent* Director = UCKRunComponent::GetOrCreateRunDirector(GetWorld());
	const ECKRunResult Result = Director != nullptr ? Director->StartRun(GetSampleRun(), 0, TArray<FName>()) : ECKRunResult::NoDirector;
	UE_LOG(LogCKSample, Log, TEXT("CoopKit Run bridge: Start Sample Run: %s."), *StaticEnum<ECKRunResult>()->GetNameStringByValue(static_cast<int64>(Result)));
	return Result == ECKRunResult::Success;
}

bool UCKSampleRunBridge::ClearSampleRoom()
{
	UCKRunComponent* Director = GetDirector();
	return Director != nullptr && Director->CompleteRoom(true);
}

bool UCKSampleRunBridge::EndSampleRun()
{
	UCKRunComponent* Director = GetDirector();
	return Director != nullptr && Director->EndRun(ECKRunEndReason::Abandoned);
}

FName UCKSampleRunBridge::GetSampleRunPhase() const
{
	const UCKRunComponent* Director = GetDirector();
	return Director != nullptr ? FName(*StaticEnum<ECKRunPhase>()->GetNameStringByValue(static_cast<int64>(Director->GetPhase()))) : NAME_None;
}

int64 UCKSampleRunBridge::GetSampleRoomSeed() const
{
	const UCKRunComponent* Director = GetDirector();
	return Director != nullptr && Director->GetPublicState().IsRunning() ? Director->GetRoomSeed() : 0;
}

bool UCKSampleRunBridge::HasSampleRunDirector() const
{
	const UCKRunComponent* Director = GetDirector();
	return Director != nullptr && Director->GetOwner() != nullptr && Director->GetOwner()->IsA<ACKSampleRunDirector>();
}

bool UCKSampleRunBridge::IsRunMap(const UWorld& World) const
{
	const FString MapPackage = UWorld::RemovePIEPrefix(World.GetOutermost()->GetName());
	return RunMaps.ContainsByPredicate([&MapPackage](const FSoftObjectPath& Map)
	{
		return Map.GetLongPackageName() == MapPackage;
	});
}

void UCKSampleRunBridge::BindRevive()
{
	UCKReviveMatchComponent* Match = bReviveBound ? nullptr : UCKReviveMatchComponent::FindMatchComponent(GetWorld());
	if (Match == nullptr)
	{
		return;
	}
	Match->OnTeamWipedChanged.AddUniqueDynamic(this, &UCKSampleRunBridge::HandleTeamWipedChanged);
	bReviveBound = true;
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(BindTimer);
	}
}

void UCKSampleRunBridge::HandleTeamWipedChanged(const bool bTeamWiped)
{
	UCKRunComponent* Director = GetDirector();
	if (!bTeamWiped || Director == nullptr || Director->GetOwner() == nullptr || !Director->GetOwner()->HasAuthority() || Director->GetPhase() != ECKRunPhase::InRoom)
	{
		return;
	}
	// The room failed: the run ends in defeat, the meta reward still counts the rooms cleared.
	if (Director->CompleteRoom(false))
	{
		++TeamWipeDefeats;
		UE_LOG(LogCKSample, Log, TEXT("CoopKit Run bridge: the team was wiped in a room: the run ends in defeat."));
	}
}
