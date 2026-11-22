// Copyright 2026 Ivan Iaitckikh. All Rights Reserved.

#include "CKSamplePlayFabMock.h"
#include "Containers/StringConv.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "HttpPath.h"
#include "HttpRequestHandler.h"
#include "HttpResultCallback.h"
#include "HttpRouteHandle.h"
#include "HttpServerConstants.h"
#include "HttpServerModule.h"
#include "HttpServerRequest.h"
#include "HttpServerResponse.h"
#include "IHttpRouter.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace CKSample::Tests
{
	namespace
	{
		const TCHAR* const TitleId = TEXT("C0E7A");
		constexpr int32 MaxTicketSize = 4;

		struct FMockTicket
		{
			FString EntityId;
			FString QueueName;
			FString Status = TEXT("WaitingForMatch");
			FString CancellationReason;
			FString MatchId;

			/** MembersToMatchWith: the entities allowed to join the ticket. */
			TArray<FString> Invited;

			/** Invited entities that joined. */
			TArray<FString> Joined;

			int32 GetPlayers() const
			{
				return 1 + Joined.Num();
			}
		};

		struct FMockResponse
		{
			int32 StatusCode = 200;
			FString Body;
		};

		FMockResponse MakeAnswer(const FString& Data)
		{
			return { 200, FString::Printf(TEXT("{\"code\":200,\"status\":\"OK\",\"data\":%s}"), *Data) };
		}

		FMockResponse MakeError(const int32 StatusCode, const TCHAR* Status, const TCHAR* Error, const int32 ErrorCode, const TCHAR* Message)
		{
			return { StatusCode, FString::Printf(TEXT("{\"code\":%d,\"status\":\"%s\",\"error\":\"%s\",\"errorCode\":%d,\"errorMessage\":\"%s\"}"), StatusCode, Status, Error, ErrorCode, Message) };
		}

		FMockResponse MakeNotAuthenticated()
		{
			return MakeError(401, TEXT("Unauthorized"), TEXT("NotAuthenticated"), 1074, TEXT("This API method does not allow anonymous callers."));
		}

		FString BodyToString(const TArray<uint8>& Body)
		{
			if (Body.IsEmpty())
			{
				return FString();
			}
			const FUTF8ToTCHAR Converted(reinterpret_cast<const ANSICHAR*>(Body.GetData()), Body.Num());
			return FString(Converted.Length(), Converted.Get());
		}

		TSharedPtr<FJsonObject> ParseBody(const FString& Body)
		{
			TSharedPtr<FJsonObject> Json;
			return FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Body), Json) ? Json : nullptr;
		}
	}

	/** PlayFab's tickets, entities and matches of one title. */
	struct FSamplePlayFabMock::FState
	{
		int32 MatchSize = 3;
		TMap<FString, FString> CustomIdEntities;
		TMap<FString, FString> TokenEntities;
		TMap<FString, FMockTicket> Tickets;
		TArray<FString> Waiting;
		TMap<FString, TArray<FString>> MatchTickets;
		int32 NextEntity = 1;
		int32 NextToken = 1;
		int32 NextTicket = 1;
		int32 NextMatch = 1;
		int32 GroupTickets = 0;
		int32 MemberJoins = 0;

		FString EntityOf(const FHttpServerRequest& Request) const
		{
			for (const TPair<FString, TArray<FString>>& Header : Request.Headers)
			{
				if (Header.Key.Equals(TEXT("X-EntityToken"), ESearchCase::IgnoreCase) && !Header.Value.IsEmpty())
				{
					return TokenEntities.FindRef(Header.Value[0]);
				}
			}
			return FString();
		}

		bool HasActiveTicket(const FString& EntityId) const
		{
			for (const TPair<FString, FMockTicket>& Entry : Tickets)
			{
				if ((Entry.Value.EntityId == EntityId || Entry.Value.Joined.Contains(EntityId)) && Entry.Value.Status != TEXT("Canceled") && Entry.Value.Status != TEXT("Matched"))
				{
					return true;
				}
			}
			return false;
		}

		int32 CountWaitingPlayers() const
		{
			int32 Players = 0;
			for (const FString& TicketId : Waiting)
			{
				Players += Tickets[TicketId].GetPlayers();
			}
			return Players;
		}

		/** Matches the oldest waiting tickets once they bring MatchSize players. */
		void MatchWaiting()
		{
			while (CountWaitingPlayers() >= MatchSize)
			{
				const FString MatchId = FString::Printf(TEXT("3d58dc0f-2eb6-4162-a2e0-%012d"), NextMatch++);
				TArray<FString>& Members = MatchTickets.Add(MatchId);
				int32 Players = 0;
				int32 Taken = 0;
				while (Players < MatchSize && Taken < Waiting.Num())
				{
					const FString TicketId = Waiting[Taken++];
					FMockTicket& Ticket = Tickets[TicketId];
					Ticket.Status = TEXT("Matched");
					Ticket.MatchId = MatchId;
					Players += Ticket.GetPlayers();
					Members.Add(TicketId);
				}
				Waiting.RemoveAt(0, Taken);
			}
		}

		void CancelTicket(const FString& TicketId, const TCHAR* Reason)
		{
			FMockTicket& Ticket = Tickets[TicketId];
			Ticket.Status = TEXT("Canceled");
			Ticket.CancellationReason = Reason;
			Waiting.Remove(TicketId);
		}

		FMockResponse SignIn(const FString& RequestBody)
		{
			const TSharedPtr<FJsonObject> Body = ParseBody(RequestBody);
			FString Title;
			FString CustomId;
			if (!Body.IsValid() || !Body->TryGetStringField(TEXT("TitleId"), Title) || Title != TitleId || !Body->TryGetStringField(TEXT("CustomId"), CustomId) || CustomId.IsEmpty())
			{
				return MakeError(400, TEXT("BadRequest"), TEXT("InvalidParams"), 1000, TEXT("Invalid input parameters"));
			}
			FString& EntityId = CustomIdEntities.FindOrAdd(CustomId);
			if (EntityId.IsEmpty())
			{
				// '!' sorts before every digit and letter: that player has the smallest id and hosts the match.
				EntityId = FString::Printf(TEXT("7A0F%012d"), CustomId.StartsWith(TEXT("!")) ? 0 : NextEntity++);
			}
			const FString Token = FString::Printf(TEXT("NHx%sfDB8%d"), *EntityId, NextToken++);
			TokenEntities.Add(Token, EntityId);
			const FString Expiration = (FDateTime::UtcNow() + FTimespan::FromHours(24.0)).ToIso8601();
			return MakeAnswer(FString::Printf(TEXT("{\"PlayFabId\":\"5F2B%s\",\"NewlyCreated\":true,\"SessionTicket\":\"x\",\"EntityToken\":{\"EntityToken\":\"%s\",")
				TEXT("\"TokenExpiration\":\"%s\",\"Entity\":{\"Id\":\"%s\",\"Type\":\"title_player_account\",\"TypeString\":\"title_player_account\"}}}"),
				*EntityId, *Token, *Expiration, *EntityId));
		}

		FMockResponse CreateTicket(const FString& EntityId, const FString& RequestBody)
		{
			const TSharedPtr<FJsonObject> Body = ParseBody(RequestBody);
			const TSharedPtr<FJsonObject>* Creator = nullptr;
			const TSharedPtr<FJsonObject>* Entity = nullptr;
			FString CreatorId;
			FString QueueName;
			double GiveUp = 0.0;
			if (!Body.IsValid() || !Body->TryGetObjectField(TEXT("Creator"), Creator) || !(*Creator)->TryGetObjectField(TEXT("Entity"), Entity)
				|| !(*Entity)->TryGetStringField(TEXT("Id"), CreatorId) || !Body->TryGetStringField(TEXT("QueueName"), QueueName) || !Body->TryGetNumberField(TEXT("GiveUpAfterSeconds"), GiveUp))
			{
				return MakeError(400, TEXT("BadRequest"), TEXT("MatchmakingBadRequest"), 2059, TEXT("Creator, QueueName and GiveUpAfterSeconds are required."));
			}
			if (CreatorId != EntityId)
			{
				return MakeError(400, TEXT("BadRequest"), TEXT("MatchmakingEntityInvalid"), 2001, TEXT("The creator is not the caller."));
			}
			if (HasActiveTicket(EntityId))
			{
				return MakeError(400, TEXT("BadRequest"), TEXT("MatchmakingTicketMembershipLimitExceeded"), 2055, TEXT("The player is already in a ticket."));
			}
			TArray<FString> Invited;
			const TArray<TSharedPtr<FJsonValue>>* MembersToMatchWith = nullptr;
			if (Body->TryGetArrayField(TEXT("MembersToMatchWith"), MembersToMatchWith))
			{
				for (const TSharedPtr<FJsonValue>& Value : *MembersToMatchWith)
				{
					const TSharedPtr<FJsonObject>* Key = nullptr;
					FString MemberId;
					if (!Value.IsValid() || !Value->TryGetObject(Key) || !(*Key)->TryGetStringField(TEXT("Id"), MemberId))
					{
						return MakeError(400, TEXT("BadRequest"), TEXT("MatchmakingEntityInvalid"), 2001, TEXT("A member to match with is not an entity key."));
					}
					Invited.Add(MemberId);
				}
			}
			if (1 + Invited.Num() > MaxTicketSize)
			{
				return MakeError(400, TEXT("BadRequest"), TEXT("MatchmakingNumberOfPlayersInTicketTooLarge"), 2044, TEXT("The ticket has more players than the queue's MaxTicketSize."));
			}
			const FString TicketId = FString::Printf(TEXT("0bf2f6d0-6f4c-4b3a-9c2e-%012d"), NextTicket++);
			FMockTicket& Ticket = Tickets.Add(TicketId);
			Ticket.EntityId = EntityId;
			Ticket.QueueName = QueueName;
			Ticket.Invited = Invited;
			if (Invited.IsEmpty())
			{
				Waiting.Add(TicketId);
			}
			else
			{
				Ticket.Status = TEXT("WaitingForPlayers");
				++GroupTickets;
			}
			MatchWaiting();
			return MakeAnswer(FString::Printf(TEXT("{\"TicketId\":\"%s\"}"), *TicketId));
		}

		FMockResponse JoinTicket(const FString& EntityId, const FString& RequestBody)
		{
			const TSharedPtr<FJsonObject> Body = ParseBody(RequestBody);
			const TSharedPtr<FJsonObject>* Member = nullptr;
			const TSharedPtr<FJsonObject>* Entity = nullptr;
			FString TicketId;
			FString QueueName;
			FString MemberId;
			if (!Body.IsValid() || !Body->TryGetObjectField(TEXT("Member"), Member) || !(*Member)->TryGetObjectField(TEXT("Entity"), Entity)
				|| !(*Entity)->TryGetStringField(TEXT("Id"), MemberId) || !Body->TryGetStringField(TEXT("TicketId"), TicketId) || !Body->TryGetStringField(TEXT("QueueName"), QueueName))
			{
				return MakeError(400, TEXT("BadRequest"), TEXT("MatchmakingBadRequest"), 2059, TEXT("Member, QueueName and TicketId are required."));
			}
			if (MemberId != EntityId)
			{
				return MakeError(400, TEXT("BadRequest"), TEXT("MatchmakingEntityInvalid"), 2001, TEXT("The member is not the caller."));
			}
			FMockTicket* Ticket = Tickets.Find(TicketId);
			if (Ticket == nullptr)
			{
				return MakeError(400, TEXT("BadRequest"), TEXT("MatchmakingTicketNotFound"), 2018, TEXT("Ticket not found."));
			}
			if (Ticket->Joined.Contains(EntityId))
			{
				return MakeError(400, TEXT("BadRequest"), TEXT("MatchmakingAlreadyJoinedTicket"), 2028, TEXT("The player has already joined the ticket."));
			}
			if (Ticket->Status != TEXT("WaitingForPlayers"))
			{
				return MakeError(400, TEXT("BadRequest"), TEXT("MatchmakingTicketAlreadyCompleted"), 2029, TEXT("The ticket has already completed."));
			}
			if (!Ticket->Invited.Contains(EntityId))
			{
				return MakeError(400, TEXT("BadRequest"), TEXT("MatchmakingEntityInvalid"), 2001, TEXT("The player is not listed in the ticket's members."));
			}
			if (HasActiveTicket(EntityId))
			{
				return MakeError(400, TEXT("BadRequest"), TEXT("MatchmakingTicketMembershipLimitExceeded"), 2055, TEXT("The player is already in a ticket."));
			}
			Ticket->Joined.Add(EntityId);
			++MemberJoins;
			if (Ticket->Joined.Num() == Ticket->Invited.Num())
			{
				Ticket->Status = TEXT("WaitingForMatch");
				Waiting.Add(TicketId);
				MatchWaiting();
			}
			return MakeAnswer(TEXT("{}"));
		}

		FMockResponse GetTicket(const FString& EntityId, const FString& RequestBody)
		{
			const TSharedPtr<FJsonObject> Body = ParseBody(RequestBody);
			FString TicketId;
			if (!Body.IsValid() || !Body->TryGetStringField(TEXT("TicketId"), TicketId))
			{
				return MakeError(400, TEXT("BadRequest"), TEXT("MatchmakingBadRequest"), 2059, TEXT("TicketId is required."));
			}
			const FMockTicket* Ticket = Tickets.Find(TicketId);
			if (Ticket == nullptr || (Ticket->EntityId != EntityId && !Ticket->Invited.Contains(EntityId)))
			{
				return MakeError(400, TEXT("BadRequest"), TEXT("MatchmakingTicketNotFound"), 2018, TEXT("Ticket not found."));
			}
			FString Json = FString::Printf(TEXT("{\"TicketId\":\"%s\",\"QueueName\":\"%s\",\"Status\":\"%s\",\"Creator\":{\"Id\":\"%s\",\"Type\":\"title_player_account\"}"),
				*TicketId, *Ticket->QueueName, *Ticket->Status, *Ticket->EntityId);
			if (!Ticket->MatchId.IsEmpty())
			{
				Json += FString::Printf(TEXT(",\"MatchId\":\"%s\""), *Ticket->MatchId);
			}
			if (!Ticket->CancellationReason.IsEmpty())
			{
				Json += FString::Printf(TEXT(",\"CancellationReasonString\":\"%s\""), *Ticket->CancellationReason);
			}
			return MakeAnswer(Json + TEXT("}"));
		}

		FMockResponse GetMatch(const FString& RequestBody)
		{
			const TSharedPtr<FJsonObject> Body = ParseBody(RequestBody);
			FString MatchId;
			const TArray<FString>* Members = Body.IsValid() && Body->TryGetStringField(TEXT("MatchId"), MatchId) ? MatchTickets.Find(MatchId) : nullptr;
			if (Members == nullptr)
			{
				return MakeError(400, TEXT("BadRequest"), TEXT("MatchmakingMatchNotFound"), 2017, TEXT("Match not found."));
			}
			TArray<FString> Json;
			for (const FString& Member : *Members)
			{
				TArray<FString> Entities;
				Entities.Add(Tickets[Member].EntityId);
				Entities.Append(Tickets[Member].Joined);
				for (const FString& Entity : Entities)
				{
					Json.Add(FString::Printf(TEXT("{\"TeamId\":\"\",\"Entity\":{\"Id\":\"%s\",\"Type\":\"title_player_account\",\"TypeString\":\"title_player_account\"}}"), *Entity));
				}
			}
			return MakeAnswer(FString::Printf(TEXT("{\"MatchId\":\"%s\",\"Members\":[%s]}"), *MatchId, *FString::Join(Json, TEXT(","))));
		}

		FMockResponse CancelTicketCall(const FString& EntityId, const FString& RequestBody)
		{
			const TSharedPtr<FJsonObject> Body = ParseBody(RequestBody);
			FString TicketId;
			FMockTicket* Ticket = Body.IsValid() && Body->TryGetStringField(TEXT("TicketId"), TicketId) ? Tickets.Find(TicketId) : nullptr;
			if (Ticket == nullptr)
			{
				return MakeError(400, TEXT("BadRequest"), TEXT("MatchmakingTicketNotFound"), 2018, TEXT("Ticket not found."));
			}
			if (Ticket->EntityId != EntityId && !Ticket->Joined.Contains(EntityId))
			{
				return MakeError(400, TEXT("BadRequest"), TEXT("MatchmakingPlayerHasNotJoinedTicket"), 2053, TEXT("The player has not joined the ticket."));
			}
			if (Ticket->Status == TEXT("Matched") || Ticket->Status == TEXT("Canceled"))
			{
				return MakeError(400, TEXT("BadRequest"), TEXT("MatchmakingTicketAlreadyCompleted"), 2029, TEXT("The ticket has already completed."));
			}
			CancelTicket(TicketId, TEXT("User"));
			return MakeAnswer(TEXT("{}"));
		}

		FMockResponse CancelAll(const FString& EntityId)
		{
			TArray<FString> Active;
			for (const TPair<FString, FMockTicket>& Entry : Tickets)
			{
				if (Entry.Value.EntityId == EntityId && Entry.Value.Status != TEXT("Canceled") && Entry.Value.Status != TEXT("Matched"))
				{
					Active.Add(Entry.Key);
				}
			}
			for (const FString& TicketId : Active)
			{
				CancelTicket(TicketId, TEXT("User"));
			}
			return MakeAnswer(TEXT("{}"));
		}
	};

	namespace
	{
		/** The mock that answers now; the engine keeps a route for the rest of the process, so the routes read it from here. */
		TWeakPtr<FSamplePlayFabMock::FState> ActiveState;

		/** Port the routes are bound on, 0 before the first start. */
		uint32 BoundPort = 0;

		TSharedPtr<IHttpRouter> BoundRouter;

		/** Binds one POST route that runs Handler on the active state, with the caller's entity (none for the sign-in). */
		bool BindRoute(const TSharedPtr<IHttpRouter>& Router, const TCHAR* Path, const bool bNeedsEntity, TFunction<FMockResponse(FSamplePlayFabMock::FState&, const FString&, const FString&)> Handler)
		{
			const FHttpRouteHandle Route = Router->BindRoute(FHttpPath(Path), EHttpServerRequestVerbs::VERB_POST,
				FHttpRequestHandler::CreateLambda([bNeedsEntity, Handler](const FHttpServerRequest& Request, const FHttpResultCallback& OnComplete)
				{
					FMockResponse Answer = { 404, TEXT("{\"error\":\"no mock is running\"}") };
					if (const TSharedPtr<FSamplePlayFabMock::FState> State = ActiveState.Pin())
					{
						const FString EntityId = State->EntityOf(Request);
						if (bNeedsEntity && EntityId.IsEmpty())
						{
							Answer = MakeNotAuthenticated();
						}
						else
						{
							Answer = Handler(*State, EntityId, BodyToString(Request.Body));
						}
					}
					TUniquePtr<FHttpServerResponse> Response = FHttpServerResponse::Create(Answer.Body, TEXT("application/json"));
					Response->Code = static_cast<EHttpServerResponseCodes>(Answer.StatusCode);
					OnComplete(MoveTemp(Response));
					return true;
				}));
			return Route.IsValid();
		}
	}

	FSamplePlayFabMock::~FSamplePlayFabMock()
	{
		Stop();
	}

	bool FSamplePlayFabMock::Start(const int32 InMatchSize, const uint32 FirstPort)
	{
		Stop();
		if (BoundPort == 0)
		{
			FHttpServerModule& Server = FHttpServerModule::Get();
			// Listeners start bound right away once enabled, so a taken port is detected here.
			Server.StartAllListeners();
			for (uint32 Offset = 0; Offset < 10 && BoundPort == 0; ++Offset)
			{
				const TSharedPtr<IHttpRouter> Router = Server.GetHttpRouter(FirstPort + Offset, true);
				if (!Router.IsValid())
				{
					continue;
				}
				// The engine cannot unbind a route reliably: the routes are bound once and stay for the process.
				bool bBound = BindRoute(Router, TEXT("/Client/LoginWithCustomID"), false, [](FState& S, const FString&, const FString& Body) { return S.SignIn(Body); });
				bBound &= BindRoute(Router, TEXT("/Match/CreateMatchmakingTicket"), true, [](FState& S, const FString& Entity, const FString& Body) { return S.CreateTicket(Entity, Body); });
				bBound &= BindRoute(Router, TEXT("/Match/JoinMatchmakingTicket"), true, [](FState& S, const FString& Entity, const FString& Body) { return S.JoinTicket(Entity, Body); });
				bBound &= BindRoute(Router, TEXT("/Match/GetMatchmakingTicket"), true, [](FState& S, const FString& Entity, const FString& Body) { return S.GetTicket(Entity, Body); });
				bBound &= BindRoute(Router, TEXT("/Match/GetMatch"), true, [](FState& S, const FString&, const FString& Body) { return S.GetMatch(Body); });
				bBound &= BindRoute(Router, TEXT("/Match/CancelMatchmakingTicket"), true, [](FState& S, const FString& Entity, const FString& Body) { return S.CancelTicketCall(Entity, Body); });
				bBound &= BindRoute(Router, TEXT("/Match/CancelAllMatchmakingTicketsForPlayer"), true, [](FState& S, const FString& Entity, const FString&) { return S.CancelAll(Entity); });
				if (bBound)
				{
					BoundPort = FirstPort + Offset;
					BoundRouter = Router;
				}
			}
			if (BoundPort == 0)
			{
				return false;
			}
		}
		Port = BoundPort;
		State = MakeShared<FState>();
		State->MatchSize = InMatchSize;
		ActiveState = State;
		return true;
	}

	void FSamplePlayFabMock::Stop()
	{
		if (State.IsValid() && ActiveState.Pin() == State)
		{
			ActiveState.Reset();
		}
		State.Reset();
	}

	const TCHAR* FSamplePlayFabMock::GetTitleId()
	{
		return TitleId;
	}

	FString FSamplePlayFabMock::GetBaseUrl() const
	{
		return FString::Printf(TEXT("http://127.0.0.1:%u"), Port);
	}

	int32 FSamplePlayFabMock::GetTicketCount() const
	{
		return State.IsValid() ? State->Tickets.Num() : 0;
	}

	int32 FSamplePlayFabMock::GetMatchCount() const
	{
		return State.IsValid() ? State->MatchTickets.Num() : 0;
	}

	int32 FSamplePlayFabMock::GetGroupTicketCount() const
	{
		return State.IsValid() ? State->GroupTickets : 0;
	}

	int32 FSamplePlayFabMock::GetMemberJoinCount() const
	{
		return State.IsValid() ? State->MemberJoins : 0;
	}
}

#endif // WITH_DEV_AUTOMATION_TESTS
