// Copyright 2026 Ivan Iaitckikh. MIT License, see LICENSE in the Extras folder.
'use strict';

/*
 * CoopKit Matchmaking - FlexMatch event handler of the standalone backend (AWS Lambda subscribed to the SNS topic that
 * the matchmaking configuration notifies).
 *
 * FlexMatch publishes one event per ticket change ("GameLift Matchmaking Event", detail.type MatchmakingSearching,
 * MatchmakingSucceeded, MatchmakingTimedOut, MatchmakingCancelled, MatchmakingFailed, ...). This handler writes the new
 * status of every ticket in the event to the table that the HTTP API (api.js) reads. In standalone mode the
 * MatchmakingSucceeded event is the only place with the full player list of the match, so the API never needs
 * DescribeMatchmaking. Updates are idempotent (SNS may deliver an event twice) and never move a finished ticket.
 *
 * Code inline in the CloudFormation template (FlexMatchStack.json); after a change run "node embed_backend.mjs" in the
 * AWS folder to copy it into the template.
 */

const TICKET_PREFIX = 't#';

/** detail.type -> the FlexMatch ticket status it sets; types not listed do not change the ticket. */
const STATUS_BY_EVENT = {
  MatchmakingSearching: 'SEARCHING',
  PotentialMatchCreated: 'PLACING',
  MatchmakingSucceeded: 'COMPLETED',
  MatchmakingTimedOut: 'TIMED_OUT',
  MatchmakingCancelled: 'CANCELLED',
  MatchmakingFailed: 'FAILED',
};

/**
 * What one FlexMatch event changes: a list of {ticketId, status, matchId?, players?, statusReason?, statusMessage?}.
 * Players of a match: detail.gameSessionInfo.players (playerId, team), each with the ticket that brought it.
 */
function ticketUpdatesFromEvent(event) {
  const detail = (event && event.detail) || {};
  const status = STATUS_BY_EVENT[detail.type];
  if (!status || !Array.isArray(detail.tickets)) return [];

  let players;
  if (status === 'COMPLETED') {
    const ticketOf = {};
    for (const ticket of detail.tickets) {
      for (const player of ticket.players || []) ticketOf[player.playerId] = ticket.ticketId;
    }
    const source = (detail.gameSessionInfo && detail.gameSessionInfo.players) || detail.tickets.flatMap((t) => t.players || []);
    players = source
      .filter((p) => p && p.playerId)
      .map((p) => ({ playerId: p.playerId, team: p.team || '', ticketId: ticketOf[p.playerId] || '' }));
  }

  return detail.tickets
    .filter((ticket) => ticket && ticket.ticketId)
    .map((ticket) => {
      const update = { ticketId: ticket.ticketId, status };
      if (status === 'COMPLETED') {
        update.matchId = detail.matchId || '';
        update.players = players;
      }
      if (detail.reason) update.statusReason = String(detail.reason);
      if (detail.message) update.statusMessage = String(detail.message);
      return update;
    });
}

/** The FlexMatch events inside an SNS delivery to Lambda (Records[].Sns.Message is the event as JSON text). */
function eventsFromSnsDelivery(delivery) {
  const events = [];
  for (const record of (delivery && delivery.Records) || []) {
    const message = record && record.Sns && record.Sns.Message;
    if (!message) continue;
    try {
      events.push(JSON.parse(message));
    } catch {
      console.warn('Skipping an SNS message that is not JSON');
    }
  }
  return events;
}

/** UpdateItem input for one ticket update: only an existing ticket, never one that has already finished. */
function makeUpdateInput(table, update, nowSeconds) {
  const names = { '#s': 'status' };
  const values = {
    ':s': { S: update.status }, ':u': { N: String(nowSeconds) },
    ':c': { S: 'COMPLETED' }, ':f': { S: 'FAILED' }, ':x': { S: 'CANCELLED' }, ':t': { S: 'TIMED_OUT' },
  };
  const sets = ['#s = :s', 'updatedAt = :u'];
  if (update.matchId !== undefined) {
    sets.push('matchId = :m');
    values[':m'] = { S: update.matchId };
  }
  if (update.players !== undefined) {
    sets.push('players = :p');
    values[':p'] = { S: JSON.stringify(update.players) };
  }
  if (update.statusReason !== undefined) {
    sets.push('statusReason = :r');
    values[':r'] = { S: update.statusReason };
  }
  if (update.statusMessage !== undefined) {
    sets.push('statusMessage = :g');
    values[':g'] = { S: update.statusMessage };
  }
  return {
    TableName: table,
    Key: { pk: { S: TICKET_PREFIX + update.ticketId } },
    UpdateExpression: `SET ${sets.join(', ')}`,
    ConditionExpression: 'attribute_exists(pk) AND NOT (#s IN (:c, :f, :x, :t))',
    ExpressionAttributeNames: names,
    ExpressionAttributeValues: values,
  };
}

/**
 * The SNS handler with its dependencies injected.
 * @param {object} deps
 * @param {{send: Function}} deps.dynamo  DynamoDB client.
 * @param {object} deps.commands  UpdateItemCommand.
 * @param {object} deps.env       TABLE_NAME.
 * @param {() => number} [deps.now]  Milliseconds since the epoch.
 */
function createEventHandler(deps) {
  const now = deps.now || (() => Date.now());
  return async function handler(delivery) {
    let applied = 0;
    for (const event of eventsFromSnsDelivery(delivery)) {
      for (const update of ticketUpdatesFromEvent(event)) {
        try {
          await deps.dynamo.send(new deps.commands.UpdateItemCommand(makeUpdateInput(deps.env.TABLE_NAME, update, Math.floor(now() / 1000))));
          applied += 1;
        } catch (error) {
          // Unknown ticket (another backend, an expired item) or a ticket that has already finished: nothing to do.
          if (!error || error.name !== 'ConditionalCheckFailedException') throw error;
        }
      }
    }
    return { applied };
  };
}

let defaultHandler = null;

/** Lambda entry point: the AWS SDK from the runtime, the table from the environment of the function. */
exports.handler = async (delivery) => {
  if (!defaultHandler) {
    const dynamoSdk = require('@aws-sdk/client-dynamodb');
    defaultHandler = createEventHandler({
      dynamo: new dynamoSdk.DynamoDBClient({}),
      commands: { UpdateItemCommand: dynamoSdk.UpdateItemCommand },
      env: process.env,
    });
  }
  return defaultHandler(delivery);
};

exports.createEventHandler = createEventHandler;
exports.ticketUpdatesFromEvent = ticketUpdatesFromEvent;
exports.eventsFromSnsDelivery = eventsFromSnsDelivery;
exports.makeUpdateInput = makeUpdateInput;
