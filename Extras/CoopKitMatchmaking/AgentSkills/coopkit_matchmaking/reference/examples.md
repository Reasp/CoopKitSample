# Examples

Replace `Sleepwalker`/`SLEEPWALKER_API` with the project's module name and API macro (module `Sleepwalker` gives `SLEEPWALKER_API`) and the copyright line with the project's. The plugin's classes are `final` except the provider bases `UCKMatchmakingProvider` and `UCKMatchmakingRestProvider`: a new matchmaking service is a provider subclass in the project; everything else uses the subsystem, the queue assets and the match result.

## 1. "Our studio runs its own matchmaking server" (a provider of the project)

The studio's server speaks a small REST contract: `POST /tickets` with `{"queue", "players", "attributes"}` answers `{"ticketId"}`; `GET /tickets/<id>` answers `{"status": "searching" | "matched" | "failed", "match": {"matchId", "players": [{"playerId"}], "connectionInfo"}, "reason"}`; `DELETE /tickets/<id>` removes the ticket. The provider derives from `UCKMatchmakingRestProvider`, which already sends the requests, applies the timeout and has the *Base Url* field.

`Source/Sleepwalker/Public/SleepwalkerStudioProvider.h`

```cpp
// Copyright 2026 Example Games. All Rights Reserved.

#pragma once

#include "CKMatchmakingRestProvider.h"
#include "CoreMinimal.h"
#include "SleepwalkerStudioProvider.generated.h"

class FJsonObject;

/**
 * Matchmaking on the studio's own server (REST, polling). Pick it in the Provider field of a Matchmaking Queue card or in
 * the New Matchmaking Queue wizard.
 */
UCLASS(meta = (DisplayName = "Sleepwalker Studio Server"))
class SLEEPWALKER_API USleepwalkerStudioProvider final : public UCKMatchmakingRestProvider
{
	GENERATED_BODY()

public:
	virtual void SubmitTicket(const FCKMatchmakingTicket& Ticket) override;
	virtual void PollTicket(const FString& TicketId) override;
	virtual void CancelTicket(const FString& TicketId) override;
	virtual float GetPollIntervalSeconds() const override;
	virtual FText GetProviderDisplayName() const override;

#if WITH_EDITOR
	virtual void ValidateProviderSettings(FDataValidationContext& Context, EDataValidationResult& InOutResult) const override;
#endif

	/** Reads the "match" object of the server's answer; false when it has no match id or no players. */
	static bool ParseMatch(const FJsonObject& MatchObject, FCKMatchResult& OutMatch);

protected:
	virtual void AddRequestHeaders(IHttpRequest& Request) const override;

private:
	/** Key that tells the studio server which game asks (not a secret: it ships with the game). */
	UPROPERTY(EditAnywhere, Category = "Studio Server")
	FString GameKey;

	/** Seconds between two questions about a waiting ticket. */
	UPROPERTY(EditAnywhere, Category = "Studio Server", meta = (ClampMin = "0.5", Units = "s"))
	float PollSeconds = 2.0f;

	/** True while a poll request is on its way; the next poll waits for it. */
	bool bPollInFlight = false;
};
```

`Source/Sleepwalker/Private/SleepwalkerStudioProvider.cpp`

```cpp
// Copyright 2026 Example Games. All Rights Reserved.

#include "SleepwalkerStudioProvider.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Interfaces/IHttpRequest.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

#define LOCTEXT_NAMESPACE "SleepwalkerStudioProvider"

void USleepwalkerStudioProvider::SubmitTicket(const FCKMatchmakingTicket& Ticket)
{
	const TSharedRef<FJsonObject> Body = MakeShared<FJsonObject>();
	Body->SetStringField(TEXT("queue"), Ticket.QueueName.ToString());
	TArray<TSharedPtr<FJsonValue>> Players;
	for (const FString& PlayerId : Ticket.PlayerIds)
	{
		Players.Add(MakeShared<FJsonValueString>(PlayerId));
	}
	Body->SetArrayField(TEXT("players"), Players);
	const TSharedRef<FJsonObject> Attributes = MakeShared<FJsonObject>();
	for (const TPair<FName, FString>& Attribute : Ticket.Attributes)
	{
		Attributes->SetStringField(Attribute.Key.ToString(), Attribute.Value);
	}
	Body->SetObjectField(TEXT("attributes"), Attributes);

	SendJsonRequest(TEXT("POST"), TEXT("/tickets"), Body, [this](const FCKMatchmakingRestResponse& Response)
	{
		FString TicketId;
		if (!Response.IsSuccess() || !Response.Body.IsValid() || !Response.Body->TryGetStringField(TEXT("ticketId"), TicketId))
		{
			// Exactly one Submitted or Failed answers a submit; the subsystem retries by the queue's Max Ticket Retries.
			ReportFailed(FString(), ECKMatchmakingFailure::ProviderError, FString::Printf(TEXT("POST /tickets returned %d"), Response.StatusCode));
			return;
		}
		ReportSubmitted(TicketId);
	});
}

void USleepwalkerStudioProvider::PollTicket(const FString& TicketId)
{
	if (bPollInFlight)
	{
		return;
	}
	bPollInFlight = true;
	SendJsonRequest(TEXT("GET"), TEXT("/tickets/") + TicketId, nullptr, [this, TicketId](const FCKMatchmakingRestResponse& Response)
	{
		bPollInFlight = false;
		FString Status;
		if (!Response.IsSuccess() || !Response.Body.IsValid() || !Response.Body->TryGetStringField(TEXT("status"), Status))
		{
			ReportFailed(TicketId, ECKMatchmakingFailure::ProviderError, FString::Printf(TEXT("GET ticket returned %d"), Response.StatusCode));
			return;
		}
		if (Status == TEXT("failed"))
		{
			FString Reason;
			Response.Body->TryGetStringField(TEXT("reason"), Reason);
			ReportFailed(TicketId, ECKMatchmakingFailure::ProviderError, Reason);
			return;
		}
		const TSharedPtr<FJsonObject>* MatchObject = nullptr;
		FCKMatchResult Match;
		if (Status == TEXT("matched") && Response.Body->TryGetObjectField(TEXT("match"), MatchObject) && ParseMatch(**MatchObject, Match))
		{
			// The same match id and players on every machine: the subsystem picks the listen server host from them.
			ReportMatched(TicketId, Match);
		}
		// "searching": nothing to report, the subsystem polls again after Get Poll Interval Seconds.
	});
}

void USleepwalkerStudioProvider::CancelTicket(const FString& TicketId)
{
	SendJsonRequest(TEXT("DELETE"), TEXT("/tickets/") + TicketId, nullptr, [this, TicketId](const FCKMatchmakingRestResponse& Response)
	{
		if (Response.IsSuccess())
		{
			ReportCancelled(TicketId);
		}
		else
		{
			ReportFailed(TicketId, ECKMatchmakingFailure::ProviderError, FString::Printf(TEXT("DELETE ticket returned %d"), Response.StatusCode));
		}
	});
}

float USleepwalkerStudioProvider::GetPollIntervalSeconds() const
{
	return PollSeconds;
}

FText USleepwalkerStudioProvider::GetProviderDisplayName() const
{
	return LOCTEXT("DisplayName", "Studio Server");
}

bool USleepwalkerStudioProvider::ParseMatch(const FJsonObject& MatchObject, FCKMatchResult& OutMatch)
{
	OutMatch = FCKMatchResult();
	MatchObject.TryGetStringField(TEXT("matchId"), OutMatch.MatchId);
	MatchObject.TryGetStringField(TEXT("connectionInfo"), OutMatch.ConnectionInfo);
	const TArray<TSharedPtr<FJsonValue>>* Players = nullptr;
	if (MatchObject.TryGetArrayField(TEXT("players"), Players))
	{
		for (const TSharedPtr<FJsonValue>& Value : *Players)
		{
			const TSharedPtr<FJsonObject>* PlayerObject = nullptr;
			FString PlayerId;
			if (Value.IsValid() && Value->TryGetObject(PlayerObject) && (*PlayerObject)->TryGetStringField(TEXT("playerId"), PlayerId))
			{
				OutMatch.Players.AddDefaulted_GetRef().PlayerId = PlayerId;
			}
		}
	}
	return OutMatch.IsValid();
}

void USleepwalkerStudioProvider::AddRequestHeaders(IHttpRequest& Request) const
{
	Super::AddRequestHeaders(Request);
	Request.SetHeader(TEXT("X-Game-Key"), GameKey);
}

#if WITH_EDITOR
void USleepwalkerStudioProvider::ValidateProviderSettings(FDataValidationContext& Context, EDataValidationResult& InOutResult) const
{
	// The base reports a bad Base Url; shown in the queue card's Validation tab and by the hub's Check Project.
	Super::ValidateProviderSettings(Context, InOutResult);
	if (GameKey.IsEmpty())
	{
		Context.AddError(LOCTEXT("NoGameKey", "Game Key is empty: the studio server refuses tickets without it."));
		InOutResult = EDataValidationResult::Invalid;
	}
}
#endif

#undef LOCTEXT_NAMESPACE
```

`Build.cs` of the project module: `PublicDependencyModuleNames.AddRange(new[] { "CoopKitMatchmaking", "HTTP", "Json" });` (the class derives from a plugin class, and its header uses `IHttpRequest`).

How the user uses it: open the queue in the CoopKit Hub (*Tools > CoopKit Hub*, category *Matchmaking Queue*), pick **Sleepwalker Studio Server** in the card's **Provider** field (or in the **Provider** option of **New Matchmaking Queue**), fill **Base Url** and **Game Key**; the Validation tab names what is missing. Keep a second queue with the **Local** provider for tests and PIE: Local needs no server.

## 2. "Players of one region only" (attributes, no plugin edit)

The rule itself needs no C++: in the queue card add `Region` to **Match Keys** (every ticket of a match must have the same value). A value every ticket shares goes to **Default Attributes**. The game passes the player's choice when it starts matchmaking; a key must not be empty or start with `CKMM_`.

`Source/Sleepwalker/Private/SleepwalkerMatchmakingMenu.cpp` (a part of the project's menu widget)

```cpp
// Copyright 2026 Example Games. All Rights Reserved.

#include "SleepwalkerMatchmakingMenu.h"
#include "CKMatchmakingQueueData.h"
#include "CKMatchmakingSubsystem.h"

void USleepwalkerMatchmakingMenu::FindMatch(const FString& Region)
{
	UCKMatchmakingSubsystem* Matchmaking = UCKMatchmakingSubsystem::Get(this);
	const UCKMatchmakingQueueData* LoadedQueue = Queue.LoadSynchronous();
	if (Matchmaking == nullptr || LoadedQueue == nullptr)
	{
		return;
	}
	Matchmaking->OnMatchFound.AddUniqueDynamic(this, &USleepwalkerMatchmakingMenu::HandleMatchFound);
	Matchmaking->OnMatchmakingFailed.AddUniqueDynamic(this, &USleepwalkerMatchmakingMenu::HandleFailed);
	TMap<FName, FString> Attributes;
	Attributes.Add(TEXT("Region"), Region);
	if (!Matchmaking->StartMatchmaking(LoadedQueue, Attributes))
	{
		// Busy, a bad queue or a reserved key: no event follows, the reason is here.
		ShowStatus(UEnum::GetDisplayValueAsText(Matchmaking->GetLastFailure()));
	}
}

void USleepwalkerMatchmakingMenu::HandleMatchFound(const FCKMatchResult& Match)
{
	ShowStatus(FText::Format(NSLOCTEXT("Sleepwalker", "MatchFound", "Match of {0} players found"), Match.Players.Num()));
}

void USleepwalkerMatchmakingMenu::HandleFailed(const ECKMatchmakingFailure Failure, const FString& Message)
{
	ShowStatus(UEnum::GetDisplayValueAsText(Failure));
}
```

The header declares `TSoftObjectPtr<UCKMatchmakingQueueData> Queue` as an `EditAnywhere` property (the user picks the queue asset), `FindMatch`, `ShowStatus` and the two handlers as `UFUNCTION()` (dynamic delegates bind only to `UFUNCTION`s). In Blueprint the same is the **Find Match** node with its pins.

## 3. "We join the match ourselves" (Join Mode Manual)

When the game has its own way to bring players together (a dedicated server address from the provider, a party system, CoopKit Sessions), set **Join Mode** of the queue to **Manual**. The plugin then only reports the match; the game joins it and tells the result:

```cpp
void USleepwalkerMatchJoiner::HandleMatchFound(const FCKMatchResult& Match)
{
	UCKMatchmakingSubsystem* Matchmaking = UCKMatchmakingSubsystem::Get(this);
	APlayerController* Controller = GetWorld()->GetFirstPlayerController();
	if (Matchmaking == nullptr || Controller == nullptr)
	{
		return;
	}
	if (!Match.ConnectionInfo.IsEmpty())
	{
		// A dedicated server placed by the service: travel there.
		Controller->ClientTravel(Match.ConnectionInfo, TRAVEL_Absolute);
		Matchmaking->ReportJoinResult(true);
		return;
	}
	// A listen server match: the host is the same player on every machine (Match.HostPlayerId).
	JoinThroughOwnSessions(Match, [Matchmaking](const bool bJoined)
	{
		// A failure sends the ticket again while the queue allows retries.
		Matchmaking->ReportJoinResult(bJoined, ECKMatchmakingFailure::JoinFailed);
	});
}
```

`JoinThroughOwnSessions` is the game's own code. After the game ends, call `LeaveMatch()` so that matchmaking can start again.

## 4. The automation test of example 1

`Source/Sleepwalker/Private/SleepwalkerStudioProviderTest.cpp`

```cpp
// Copyright 2026 Example Games. All Rights Reserved.

#include "Dom/JsonObject.h"
#include "Misc/AutomationTest.h"
#include "Misc/DataValidation.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "SleepwalkerStudioProvider.h"
#include "UObject/Package.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSleepwalkerStudioProviderTest, "Sleepwalker.Matchmaking.StudioProvider",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FSleepwalkerStudioProviderTest::RunTest(const FString& Parameters)
{
	// The server's answer as a string: no server, no network.
	TSharedPtr<FJsonObject> Answer;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(TEXT("{\"matchId\":\"m1\",\"players\":[{\"playerId\":\"Ann\"},{\"playerId\":\"Bob\"}]}"));
	if (!TestTrue(TEXT("The answer parses"), FJsonSerializer::Deserialize(Reader, Answer) && Answer.IsValid()))
	{
		return false;
	}
	FCKMatchResult Match;
	TestTrue(TEXT("A match with players is read"), USleepwalkerStudioProvider::ParseMatch(*Answer, Match));
	TestEqual(TEXT("Match id"), Match.MatchId, FString(TEXT("m1")));
	TestEqual(TEXT("Both players"), Match.Players.Num(), 2);
	TestFalse(TEXT("A match without players is refused"), USleepwalkerStudioProvider::ParseMatch(FJsonObject(), Match));

#if WITH_EDITOR
	// Missing settings are reported in the queue card's Validation tab.
	const USleepwalkerStudioProvider* Provider = NewObject<USleepwalkerStudioProvider>(GetTransientPackage());
	FDataValidationContext Context;
	EDataValidationResult Result = EDataValidationResult::Valid;
	Provider->ValidateProviderSettings(Context, Result);
	TArray<FText> Warnings;
	TArray<FText> Errors;
	Context.SplitIssues(Warnings, Errors);
	TestTrue(TEXT("A provider without settings is invalid"), Result == EDataValidationResult::Invalid);
	TestTrue(TEXT("The empty Game Key is named"), Errors.ContainsByPredicate([](const FText& Error) { return Error.ToString().Contains(TEXT("Game Key")); }));
#endif
	return true;
}

#endif
```

The match flow itself (tickets, retries, the host, the join) is the plugin's and the plugin's tests cover it; to try the project's menu without a server, give the menu a queue with the **Local** provider and play two PIE windows.

### Testing a validation

A provider of the project adds its checks in `ValidateProviderSettings(Context, InOutResult)`, which the queue's `IsDataValid` calls, so the queue's validation shows them. A data asset of your own (a `UPrimaryDataAsset` subclass with `IsDataValid`) is tested like this, inside `#if WITH_EDITOR`, because `IsDataValid` exists only in the editor: `USleepwalkerSquadData` and its `SquadSize` stand for your own class and property.

```cpp
#include "Misc/DataValidation.h"
#include "UObject/UnrealType.h"

// Bad data: the asset must report it.
USleepwalkerSquadData* Data = NewObject<USleepwalkerSquadData>(GetTransientPackage());
FIntProperty* Size = FindFProperty<FIntProperty>(Data->GetClass(), TEXT("SquadSize"));
if (!TestNotNull(TEXT("The property exists"), Size))
{
	return false;
}
Size->SetPropertyValue_InContainer(Data, 0);
FDataValidationContext Bad;
TestEqual(TEXT("A squad of 0 is invalid"), Data->IsDataValid(Bad), EDataValidationResult::Invalid);
TestTrue(TEXT("The error is reported"), Bad.GetNumErrors() > 0);

// Good data: no errors.
Size->SetPropertyValue_InContainer(Data, 4);
FDataValidationContext Good;
Data->IsDataValid(Good);
TestTrue(TEXT("A squad of 4 has no errors"), Good.GetNumErrors() == 0);
```

## 5. Add a C++ module to a Blueprint-only project

The build script reports "the project has no C++ target". Fix: in the editor, Tools > New C++ Class, pick **None** as the parent, name it, and finish. The editor creates `Source/<Project>/` and target files and offers to restart. Then run the build script again.

Without the editor (what an agent can do): create `Source/<Project>.Target.cs` and `Source/<Project>Editor.Target.cs` (`TargetRules` with `Type = TargetType.Game` or `TargetType.Editor`, `DefaultBuildSettings = BuildSettingsVersion.Latest`, `IncludeOrderVersion = EngineIncludeOrderVersion.Latest`, `ExtraModuleNames.Add("<Project>")`), `Source/<Project>/<Project>.Build.cs`, `Source/<Project>/<Project>.cpp` with `IMPLEMENT_PRIMARY_GAME_MODULE(FDefaultGameModuleImpl, <Project>, "<Project>");`, and add a `Modules` entry (`Name`, `Type: Runtime`, `LoadingPhase: Default`) to the `.uproject`. Ask the user to close the editor first.
