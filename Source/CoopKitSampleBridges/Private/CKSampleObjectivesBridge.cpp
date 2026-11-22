// Copyright 2026 Ivan Iaitckikh. All Rights Reserved.

#include "CKSampleObjectivesBridge.h"
#include "CKObjectivesArea.h"
#include "CKObjectivesDemoQuests.h"
#include "CKObjectivesGameplayTags.h"
#include "CKObjectivesPlayerComponent.h"
#include "CKObjectivesQuestData.h"
#include "CKObjectivesQuestLogComponent.h"
#include "CKObjectivesRewardBagComponent.h"
#include "CKPingMatchComponent.h"
#include "CKPingRulesData.h"
#include "CKPingTypeData.h"
#include "CKPingTypes.h"
#include "CKSamplePlayerController.h"
#include "CKSamplePlayerState.h"
#include "CKSaveableComponent.h"
#include "CoopPlugins.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerStart.h"
#include "GameFramework/PlayerState.h"
#include "UObject/Package.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(CKSampleObjectivesBridge)

namespace CKSample::ObjectivesBridge
{
	/** Actor tag of the sample's patrol point: the sample quest's objective points at it, the ping bridge marks it. */
	const FName TargetTag(TEXT("CKSampleObjectiveTarget"));

	const TCHAR* BuiltInQuestName = TEXT("CKSampleObjectivesPatrol");

	/** The sample's patrol, built in code: reach the patrol point (any player), then two kills together. */
	UCKObjectivesQuestData* GetBuiltInPatrol()
	{
		if (UCKObjectivesQuestData* Existing = FindObject<UCKObjectivesQuestData>(GetTransientPackage(), BuiltInQuestName))
		{
			return Existing;
		}
		using namespace CKObjectives::Demo;
		FQuestValues Values;
		Values.Id = TEXT("SamplePatrol");
		Values.Name = TEXT("Patrol the Proving Ground");
		Values.bRepeatable = true;
		FCKObjectivesObjective Point = MakeObjective(TEXT("Point"), TEXT("Reach the patrol point"), ECKObjectivesKind::Reach, CKObjectives::Tags::Event_Reach, 1,
			ECKObjectivesScope::AnyPlayer);
		Point.TargetActorTag = TargetTag;
		Values.Steps = { MakeStep(TEXT("Patrol"), TEXT("Patrol"), { Point }, TEXT("Hold")),
			MakeStep(TEXT("Hold"), TEXT("Hold the point"), { MakeObjective(TEXT("Enemies"), TEXT("Defeat enemies"), ECKObjectivesKind::Kill, CKObjectives::Tags::Event_Kill, 2) },
				NAME_None) };
		Values.Rewards = { MakeReward(TEXT("Credits"), 50) };
		UCKObjectivesQuestData* Quest = NewObject<UCKObjectivesQuestData>(GetTransientPackage(), BuiltInQuestName, RF_Transient);
		FillQuest(*Quest, Values);
		Quest->AddToRoot();
		return Quest;
	}
}

ACKSampleObjectivesTracker::ACKSampleObjectivesTracker()
{
	Saveable = CreateDefaultSubobject<UCKSaveableComponent>(TEXT("Saveable"));
}

void ACKSampleObjectivesTracker::OnBeforeSave_Implementation()
{
	GetQuestLog()->PrepareForSave();
}

void ACKSampleObjectivesTracker::OnAfterLoad_Implementation(const int32 SavedGameDataVersion)
{
	GetQuestLog()->RestoreAfterLoad();
}

UCKSampleObjectivesBridge::UCKSampleObjectivesBridge()
{
	// Defaults without the ini section: the sample's proving ground and the CoopKit Ping demo's Go type.
	ObjectivesMaps = { FSoftObjectPath(TEXT("/Game/CoopKitSample/Maps/L_ProvingGround.L_ProvingGround")) };
	MarkerType = TSoftObjectPtr<UCKPingTypeData>(FSoftObjectPath(TEXT("/CoopKitPing/Data/DA_PingType_Go.DA_PingType_Go")));
}

bool UCKSampleObjectivesBridge::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

void UCKSampleObjectivesBridge::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	// Every machine makes the built-in quest, so the replicated log names a quest each machine knows.
	GetSampleQuest();
	if (!IsObjectivesMap(InWorld) || InWorld.GetNetMode() == NM_Client)
	{
		return;
	}
	for (TActorIterator<AActor> It(&InWorld); It; ++It)
	{
		Equip(**It);
	}
	ActorSpawnedHandle = InWorld.AddOnActorSpawnedHandler(FOnActorSpawned::FDelegate::CreateUObject(this, &UCKSampleObjectivesBridge::HandleActorSpawned));
	FActorSpawnParameters Parameters;
	Parameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	InWorld.SpawnActor<ACKSampleObjectivesTracker>(FVector::ZeroVector, FRotator::ZeroRotator, Parameters);
	SpawnTargets(InWorld);
}

void UCKSampleObjectivesBridge::Deinitialize()
{
	if (UWorld* World = GetWorld())
	{
		World->RemoveOnActorSpawnedHandler(ActorSpawnedHandle);
	}
	Super::Deinitialize();
}

UCKObjectivesQuestData* UCKSampleObjectivesBridge::GetSampleQuest() const
{
	UCKObjectivesQuestData* Quest = SampleQuest.IsNull() ? nullptr : SampleQuest.LoadSynchronous();
	return Quest != nullptr ? Quest : CKSample::ObjectivesBridge::GetBuiltInPatrol();
}

bool UCKSampleObjectivesBridge::StartSampleQuest()
{
	UCKObjectivesQuestLogComponent* QuestLog = UCKObjectivesQuestLogComponent::GetOrCreateQuestLog(this);
	const ECKObjectivesResult Result = QuestLog != nullptr ? QuestLog->StartQuest(nullptr, GetSampleQuest()) : ECKObjectivesResult::NoQuestLog;
	UE_LOG(LogCKSample, Log, TEXT("CoopKit Objectives bridge: Start Sample Quest: %s."), *StaticEnum<ECKObjectivesResult>()->GetNameStringByValue(static_cast<int64>(Result)));
	return Result == ECKObjectivesResult::Success;
}

int32 UCKSampleObjectivesBridge::ReportSampleEvent(APlayerState* Player, const FName EventTagName)
{
	return UCKObjectivesQuestLogComponent::ReportObjectiveEvent(this, Player, FGameplayTag::RequestGameplayTag(EventTagName, false), 1);
}

FName UCKSampleObjectivesBridge::GetSampleQuestStep() const
{
	const UCKObjectivesQuestLogComponent* QuestLog = UCKObjectivesQuestLogComponent::FindQuestLog(this);
	FCKObjectivesQuestProgress Progress;
	if (QuestLog == nullptr || !QuestLog->GetQuestProgress(GetSampleQuest(), Progress) || Progress.State != ECKObjectivesQuestState::Active)
	{
		return NAME_None;
	}
	return Progress.StepId;
}

bool UCKSampleObjectivesBridge::HasQuestLog() const
{
	return UCKObjectivesQuestLogComponent::FindQuestLog(this) != nullptr;
}

bool UCKSampleObjectivesBridge::IsObjectivesMap(const UWorld& World) const
{
	const FString MapPackage = UWorld::RemovePIEPrefix(World.GetOutermost()->GetName());
	return ObjectivesMaps.ContainsByPredicate([&MapPackage](const FSoftObjectPath& Map)
	{
		return Map.GetLongPackageName() == MapPackage;
	});
}

void UCKSampleObjectivesBridge::HandleActorSpawned(AActor* Actor)
{
	if (Actor != nullptr)
	{
		Equip(*Actor);
	}
}

void UCKSampleObjectivesBridge::Equip(AActor& Actor)
{
	if (!Actor.HasAuthority())
	{
		return;
	}
	if (const ACKObjectivesTracker* Tracker = Cast<ACKObjectivesTracker>(&Actor))
	{
		// The tracker CoopKit Save spawns again after a load is a new actor: bind its log too.
		Bind(Tracker->GetQuestLog());
	}
	else if (Actor.IsA<ACKSamplePlayerState>() && Actor.FindComponentByClass<UCKObjectivesRewardBagComponent>() == nullptr)
	{
		UCKObjectivesRewardBagComponent* Bag = NewObject<UCKObjectivesRewardBagComponent>(&Actor, TEXT("CKObjectivesRewardBag"));
		Actor.AddInstanceComponent(Bag);
		Bag->RegisterComponent();
	}
	else if (Actor.IsA<ACKSamplePlayerController>() && Actor.FindComponentByClass<UCKObjectivesPlayerComponent>() == nullptr)
	{
		// Replicated: the owning client gets the component and sends its requests through it.
		UCKObjectivesPlayerComponent* Voice = NewObject<UCKObjectivesPlayerComponent>(&Actor, TEXT("CKObjectivesPlayer"));
		Actor.AddInstanceComponent(Voice);
		Voice->RegisterComponent();
	}
}

void UCKSampleObjectivesBridge::Bind(UCKObjectivesQuestLogComponent* QuestLog)
{
	if (QuestLog != nullptr)
	{
		QuestLog->OnStepStarted.AddUniqueDynamic(this, &UCKSampleObjectivesBridge::HandleStepStarted);
	}
}

void UCKSampleObjectivesBridge::SpawnTargets(UWorld& World)
{
	FVector Origin = FVector::ZeroVector;
	for (TActorIterator<APlayerStart> It(&World); It; ++It)
	{
		Origin = It->GetActorLocation();
		break;
	}
	FActorSpawnParameters Parameters;
	Parameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	if (ACKObjectivesArea* Point = World.SpawnActor<ACKObjectivesArea>(Origin + TargetOffset, FRotator::ZeroRotator, Parameters))
	{
		Point->Tags.Add(CKSample::ObjectivesBridge::TargetTag);
		UE_LOG(LogCKSample, Log, TEXT("CoopKit Objectives bridge: patrol point placed on %s."), *World.GetMapName());
	}
}

void UCKSampleObjectivesBridge::HandleStepStarted(UCKObjectivesQuestLogComponent* QuestLog, UCKObjectivesQuestData* Quest, const FName StepId)
{
	UWorld* World = GetWorld();
	UCKPingMatchComponent* Match = UCKPingMatchComponent::FindMatchComponent(World);
	if (World == nullptr || World->GetNetMode() == NM_Client || Match == nullptr || QuestLog == nullptr)
	{
		return;
	}
	// The first player with a player state authors the markers: a ping needs an author, and every player sees it.
	APlayerController* Author = nullptr;
	for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
	{
		if (It->IsValid() && (*It)->PlayerState != nullptr)
		{
			Author = It->Get();
			break;
		}
	}
	const UCKPingRulesData* Rules = Match->GetRules();
	UCKPingTypeData* Type = MarkerType.LoadSynchronous();
	if (Type == nullptr || Rules == nullptr || !Rules->AllowsType(Type))
	{
		Type = Rules != nullptr ? Rules->GetQuickPingType() : nullptr;
	}
	if (Author == nullptr || Type == nullptr)
	{
		UE_LOG(LogCKSample, Log, TEXT("CoopKit Objectives -> Ping bridge: no marker for step %s (%s)."), *StepId.ToString(),
			Author == nullptr ? TEXT("no player to author it") : TEXT("no ping type the rules allow"));
		return;
	}
	for (AActor* Target : QuestLog->GetObjectiveTargetActors(Quest))
	{
		ECKPingRejectReason Reason = ECKPingRejectReason::NoMatch;
		if (Match->CreatePing(Author, Target->GetActorLocation(), Target, Type, Reason) != 0)
		{
			++MarkersPlaced;
		}
		else
		{
			UE_LOG(LogCKSample, Log, TEXT("CoopKit Objectives -> Ping bridge: no marker on %s (%s)."), *Target->GetName(),
				*StaticEnum<ECKPingRejectReason>()->GetNameStringByValue(static_cast<int64>(Reason)));
		}
	}
}
