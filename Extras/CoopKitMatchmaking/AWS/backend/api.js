// Copyright 2026 Ivan Iaitckikh. MIT License, see LICENSE in the Extras folder.
'use strict';

/*
 * CoopKit Matchmaking - HTTP API of the FlexMatch standalone backend (AWS Lambda behind a function URL).
 *
 * The game talks only to this API; it never holds AWS keys. Routes (JSON in and out):
 *   POST   /v1/sessions      -> 200 {"playerId", "token", "expiresIn"}
 *   POST   /v1/tickets       {"attributes": {"Mode": "coop", "Skill": 12}, "members": ["p-..."]}
 *                            -> 200 {"ticketId", "status": "QUEUED"}
 *                            "members" is optional: the player ids of the rest of a group. The caller and the members become
 *                            the players of one FlexMatch ticket (all with the caller's attributes), so they land in one match.
 *   GET    /v1/tickets/{id}  -> 200 ticket view (below)
 *   DELETE /v1/tickets/{id}  -> 200 ticket view: CANCELLED, or COMPLETED when the match came first
 * Every route except /v1/sessions needs "Authorization: Bearer <token>"; a ticket is visible only to its player.
 *
 * Ticket view: {"ticketId", "status", "statusReason", "statusMessage", "matchId", "players": [{"playerId", "team", "ticketId"}]}
 * with status as in FlexMatch: QUEUED, SEARCHING, REQUIRES_ACCEPTANCE, PLACING, COMPLETED, FAILED, CANCELLED, TIMED_OUT.
 * FlexMatch reports progress as events (SNS -> events.js -> the table); this API reads the table, so polling it does not
 * call DescribeMatchmaking, which AWS says is "not designed to be continually called".
 *
 * Player identity: POST /v1/sessions gives every caller a new random guest id. To keep one id per player across sessions,
 * replace resolvePlayer with a check of your platform's identity token (for example an Epic Online Services ID token) that
 * returns the platform's stable player id.
 *
 * Code inline in the CloudFormation template (FlexMatchStack.json); after a change run "node embed_backend.mjs" in the
 * AWS folder to copy it into the template. Uses the AWS SDK for JavaScript v3 included in the nodejs24.x Lambda runtime.
 */

const crypto = require('node:crypto');

const TERMINAL_STATUSES = ['COMPLETED', 'FAILED', 'CANCELLED', 'TIMED_OUT'];
const SECRET_KEY = 'config#token-secret';
const TICKET_PREFIX = 't#';
const PLAYER_ID_PATTERN = /^[A-Za-z0-9-]{1,128}$/;
const TICKET_ID_PATTERN = /^[A-Za-z0-9-.]{1,128}$/;
/** StartMatchmaking takes at most 10 players per request: the caller and 9 members. */
const MAX_GROUP_MEMBERS = 9;

/** Reads "Mode:S,Skill:N" into {Mode: "S", Skill: "N"}: the player attributes of the rule set and their FlexMatch types. */
function parseAttributeTypes(text) {
  const types = {};
  for (const part of String(text || '').split(',')) {
    const [name, type] = part.split(':').map((s) => (s || '').trim());
    if (name && (type === 'S' || type === 'N')) types[name] = type;
  }
  return types;
}

/**
 * FlexMatch PlayerAttributes from the ticket's attributes. Keys the rule set does not declare are left out: FlexMatch
 * requires "Player attribute keys must match the playerAttributes used in a matchmaking rule set".
 * @returns {{attributes?: object, error?: string}}
 */
function toPlayerAttributes(attributes, types) {
  const out = {};
  for (const [key, value] of Object.entries(attributes || {})) {
    const type = types[key];
    if (!type) continue;
    if (type === 'N') {
      const number = typeof value === 'number' ? value : Number(String(value).trim());
      if (String(value).trim() === '' || !Number.isFinite(number)) return { error: `attribute ${key} must be a number` };
      out[key] = { N: number };
    } else {
      out[key] = { S: String(value) };
    }
  }
  return { attributes: out };
}

/**
 * The ids of the rest of a group from the request body: absent means none.
 * @returns {{members?: string[], error?: string}}
 */
function parseMembers(value, playerId) {
  if (value === undefined) return { members: [] };
  if (!Array.isArray(value)) return { error: 'members must be an array of player ids' };
  if (value.length > MAX_GROUP_MEMBERS) return { error: `a group takes at most ${MAX_GROUP_MEMBERS} members besides its leader` };
  const seen = new Set([playerId]);
  for (const member of value) {
    if (typeof member !== 'string' || !PLAYER_ID_PATTERN.test(member)) return { error: 'a member id must be 1-128 letters, digits and hyphens' };
    if (seen.has(member)) return { error: `member id ${member} is in the group twice` };
    seen.add(member);
  }
  return { members: value };
}

function base64url(buffer) {
  return Buffer.from(buffer).toString('base64').replace(/\+/g, '-').replace(/\//g, '_').replace(/=+$/, '');
}

/** Token "<playerId>.<expiry, unix seconds>.<HMAC-SHA256 of both, base64url>". */
function signToken(secret, playerId, expiresAt) {
  const payload = `${playerId}.${expiresAt}`;
  return `${payload}.${base64url(crypto.createHmac('sha256', secret).update(payload).digest())}`;
}

/** @returns {string|null} the player id of a valid, unexpired token. */
function verifyToken(secret, token, nowSeconds) {
  const parts = String(token || '').split('.');
  if (parts.length !== 3) return null;
  const [playerId, expiry, signature] = parts;
  if (!PLAYER_ID_PATTERN.test(playerId) || !/^\d+$/.test(expiry)) return null;
  const expected = Buffer.from(signToken(secret, playerId, expiry).split('.')[2]);
  const given = Buffer.from(signature);
  if (expected.length !== given.length || !crypto.timingSafeEqual(expected, given)) return null;
  return Number(expiry) > nowSeconds ? playerId : null;
}

/** The ticket as the game sees it, from a table item (DynamoDB attribute values). */
function ticketView(item) {
  const view = { ticketId: item.pk.S.slice(TICKET_PREFIX.length), status: item.status.S };
  if (item.statusReason) view.statusReason = item.statusReason.S;
  if (item.statusMessage) view.statusMessage = item.statusMessage.S;
  if (item.matchId) view.matchId = item.matchId.S;
  if (item.players) view.players = JSON.parse(item.players.S);
  return view;
}

function response(statusCode, body) {
  return { statusCode, headers: { 'content-type': 'application/json' }, body: JSON.stringify(body) };
}

function errorResponse(statusCode, error, message) {
  return response(statusCode, { error, message });
}

function readJsonBody(event) {
  if (!event.body) return {};
  const text = event.isBase64Encoded ? Buffer.from(event.body, 'base64').toString('utf8') : event.body;
  try {
    const value = JSON.parse(text);
    return value && typeof value === 'object' && !Array.isArray(value) ? value : null;
  } catch {
    return null;
  }
}

function isConditionFailure(error) {
  return error && error.name === 'ConditionalCheckFailedException';
}

/**
 * The API handler with its dependencies injected: tests pass fake clients, the Lambda runtime the AWS SDK.
 * @param {object} deps
 * @param {{send: Function}} deps.gamelift   GameLift client.
 * @param {{send: Function}} deps.dynamo     DynamoDB client.
 * @param {object} deps.commands  StartMatchmakingCommand, StopMatchmakingCommand, GetItemCommand, PutItemCommand, UpdateItemCommand.
 * @param {object} deps.env       TABLE_NAME, CONFIGURATION_NAME, PLAYER_ATTRIBUTES, TOKEN_LIFETIME_SECONDS, TICKET_LIFETIME_SECONDS.
 * @param {() => number} [deps.now]  Milliseconds since the epoch.
 */
function createApi(deps) {
  const { gamelift, dynamo, commands, env } = deps;
  const now = deps.now || (() => Date.now());
  const table = env.TABLE_NAME;
  const attributeTypes = parseAttributeTypes(env.PLAYER_ATTRIBUTES);
  const tokenLifetime = Number(env.TOKEN_LIFETIME_SECONDS) || 21600;
  const ticketLifetime = Number(env.TICKET_LIFETIME_SECONDS) || 3600;
  let secret = null;

  /** The signing secret: made on first use and kept in the table, so no secret has to be set at deployment. */
  async function getSecret() {
    if (secret) return secret;
    const key = { pk: { S: SECRET_KEY } };
    const found = await dynamo.send(new commands.GetItemCommand({ TableName: table, Key: key, ConsistentRead: true }));
    if (found.Item) {
      secret = found.Item.value.S;
      return secret;
    }
    const fresh = crypto.randomBytes(32).toString('base64');
    try {
      await dynamo.send(new commands.PutItemCommand({
        TableName: table,
        Item: { ...key, value: { S: fresh } },
        ConditionExpression: 'attribute_not_exists(pk)',
      }));
      secret = fresh;
    } catch (error) {
      if (!isConditionFailure(error)) throw error;
      const again = await dynamo.send(new commands.GetItemCommand({ TableName: table, Key: key, ConsistentRead: true }));
      secret = again.Item.value.S;
    }
    return secret;
  }

  /** Who the caller is. Replace to verify a platform identity token and return the platform's stable player id. */
  async function resolvePlayer() {
    return `p-${crypto.randomBytes(12).toString('hex')}`;
  }

  async function authenticate(event) {
    const header = (event.headers && (event.headers.authorization || event.headers.Authorization)) || '';
    const match = /^Bearer\s+(\S+)$/i.exec(header);
    return match ? verifyToken(await getSecret(), match[1], Math.floor(now() / 1000)) : null;
  }

  async function getTicket(ticketId) {
    const found = await dynamo.send(new commands.GetItemCommand({
      TableName: table,
      Key: { pk: { S: TICKET_PREFIX + ticketId } },
      ConsistentRead: true,
    }));
    return found.Item || null;
  }

  async function createSession(event) {
    const playerId = await resolvePlayer(event);
    const expiresAt = Math.floor(now() / 1000) + tokenLifetime;
    return response(200, { playerId, token: signToken(await getSecret(), playerId, expiresAt), expiresIn: tokenLifetime });
  }

  async function createTicket(event, playerId) {
    const body = readJsonBody(event);
    if (!body) return errorResponse(400, 'BadRequest', 'body must be a JSON object');
    const converted = toPlayerAttributes(body.attributes, attributeTypes);
    if (converted.error) return errorResponse(400, 'InvalidAttribute', converted.error);
    const group = parseMembers(body.members, playerId);
    if (group.error) return errorResponse(400, 'InvalidGroup', group.error);

    const ticketId = `ckmm-${crypto.randomUUID()}`;
    const nowSeconds = Math.floor(now() / 1000);
    // The item exists before FlexMatch can send the first event for the ticket.
    await dynamo.send(new commands.PutItemCommand({
      TableName: table,
      Item: {
        pk: { S: TICKET_PREFIX + ticketId },
        playerId: { S: playerId },
        status: { S: 'QUEUED' },
        updatedAt: { N: String(nowSeconds) },
        expiresAt: { N: String(nowSeconds + ticketLifetime) },
      },
      ConditionExpression: 'attribute_not_exists(pk)',
    }));
    try {
      await gamelift.send(new commands.StartMatchmakingCommand({
        ConfigurationName: env.CONFIGURATION_NAME,
        TicketId: ticketId,
        Players: [playerId, ...group.members].map((id) => ({ PlayerId: id, PlayerAttributes: converted.attributes })),
      }));
    } catch (error) {
      const message = String((error && error.message) || error);
      await dynamo.send(new commands.UpdateItemCommand({
        TableName: table,
        Key: { pk: { S: TICKET_PREFIX + ticketId } },
        UpdateExpression: 'SET #s = :s, statusReason = :r, statusMessage = :m',
        ExpressionAttributeNames: { '#s': 'status' },
        ExpressionAttributeValues: { ':s': { S: 'FAILED' }, ':r': { S: 'StartMatchmakingFailed' }, ':m': { S: message } },
      }));
      const invalid = error && error.name === 'InvalidRequestException';
      return errorResponse(invalid ? 400 : 502, invalid ? 'InvalidRequest' : 'MatchmakingUnavailable', message);
    }
    return response(200, { ticketId, status: 'QUEUED' });
  }

  async function readTicket(ticketId, playerId) {
    const item = await getTicket(ticketId);
    if (!item || item.playerId.S !== playerId) return errorResponse(404, 'NotFound', 'no such ticket');
    return response(200, ticketView(item));
  }

  async function cancelTicket(ticketId, playerId) {
    const item = await getTicket(ticketId);
    if (!item || item.playerId.S !== playerId) return errorResponse(404, 'NotFound', 'no such ticket');
    if (TERMINAL_STATUSES.includes(item.status.S)) return response(200, ticketView(item));
    try {
      await gamelift.send(new commands.StopMatchmakingCommand({ TicketId: ticketId }));
    } catch (error) {
      // The ticket may have ended meanwhile (a match, a time-out): report what the table knows now.
      const latest = await getTicket(ticketId);
      if (latest && TERMINAL_STATUSES.includes(latest.status.S)) return response(200, ticketView(latest));
      return errorResponse(502, 'MatchmakingUnavailable', String((error && error.message) || error));
    }
    try {
      await dynamo.send(new commands.UpdateItemCommand({
        TableName: table,
        Key: { pk: { S: TICKET_PREFIX + ticketId } },
        UpdateExpression: 'SET #s = :s, statusReason = :r',
        ConditionExpression: 'NOT (#s IN (:c, :f, :x, :t))',
        ExpressionAttributeNames: { '#s': 'status' },
        ExpressionAttributeValues: {
          ':s': { S: 'CANCELLED' }, ':r': { S: 'Cancelled' },
          ':c': { S: 'COMPLETED' }, ':f': { S: 'FAILED' }, ':x': { S: 'CANCELLED' }, ':t': { S: 'TIMED_OUT' },
        },
      }));
    } catch (error) {
      if (!isConditionFailure(error)) throw error;
    }
    return response(200, ticketView(await getTicket(ticketId)));
  }

  return async function handler(event) {
    const method = ((event.requestContext && event.requestContext.http && event.requestContext.http.method) || '').toUpperCase();
    const path = (event.rawPath || '/').replace(/\/+$/, '') || '/';
    try {
      if (method === 'POST' && path === '/v1/sessions') return await createSession(event);
      const playerId = await authenticate(event);
      if (path === '/v1/tickets' && method === 'POST') {
        return playerId ? await createTicket(event, playerId) : errorResponse(401, 'Unauthorized', 'sign in with POST /v1/sessions');
      }
      const ticket = /^\/v1\/tickets\/([^/]+)$/.exec(path);
      if (ticket && (method === 'GET' || method === 'DELETE')) {
        if (!playerId) return errorResponse(401, 'Unauthorized', 'sign in with POST /v1/sessions');
        const ticketId = decodeURIComponent(ticket[1]);
        if (!TICKET_ID_PATTERN.test(ticketId)) return errorResponse(404, 'NotFound', 'no such ticket');
        return method === 'GET' ? await readTicket(ticketId, playerId) : await cancelTicket(ticketId, playerId);
      }
      return errorResponse(404, 'NotFound', `no route ${method} ${path}`);
    } catch (error) {
      console.error('Request failed', method, path, error);
      return errorResponse(500, 'InternalError', 'the backend failed; see its log');
    }
  };
}

let defaultHandler = null;

/** Lambda entry point: the AWS SDK from the runtime, settings from the environment of the function. */
exports.handler = async (event) => {
  if (!defaultHandler) {
    const gameLiftSdk = require('@aws-sdk/client-gamelift');
    const dynamoSdk = require('@aws-sdk/client-dynamodb');
    defaultHandler = createApi({
      gamelift: new gameLiftSdk.GameLiftClient({}),
      dynamo: new dynamoSdk.DynamoDBClient({}),
      commands: {
        StartMatchmakingCommand: gameLiftSdk.StartMatchmakingCommand,
        StopMatchmakingCommand: gameLiftSdk.StopMatchmakingCommand,
        GetItemCommand: dynamoSdk.GetItemCommand,
        PutItemCommand: dynamoSdk.PutItemCommand,
        UpdateItemCommand: dynamoSdk.UpdateItemCommand,
      },
      env: process.env,
    });
  }
  return defaultHandler(event);
};

exports.createApi = createApi;
exports.parseAttributeTypes = parseAttributeTypes;
exports.toPlayerAttributes = toPlayerAttributes;
exports.parseMembers = parseMembers;
exports.signToken = signToken;
exports.verifyToken = verifyToken;
