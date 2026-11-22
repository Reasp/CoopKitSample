// Copyright 2026 Ivan Iaitckikh. MIT License, see LICENSE in the Extras folder.
//
// CoopKit Matchmaking server module for Nakama (JavaScript runtime).
//
// What it does:
//   - reads the queue file coopkit_queues.json from the module folder: {"version": 1, "queues": {"<queue>": {...}}},
//     written by sync_queue.mjs from a queue exported in Unreal;
//   - checks it lightly and writes the queues (or the problems) to the server log at start-up;
//   - registers the RPC "coopkit_queues", which returns the file as the server reads it, plus a list of problems, so tools
//     (sync_queue.mjs --diff --server) and game clients can see which queues the server runs;
//   - group tickets (parties): a before hook of party_matchmaker_add gives the party ticket of a queue in the file the queue's
//     minCount and maxCount, so the server, not the game build, decides the match size of parties.
// The matchmaking itself runs in Nakama's matchmaker with the ticket the game sends. Single-player tickets (matchmaker_add)
// are not changed. Keep new code in top-level functions like the ones below.
//
// The party matchmaker needs no other server code (Nakama 3.37.0, checked 2026-11-18): the leader's client creates a closed
// party (party_create), every member joins it on its own socket (party_join, the leader's party_accept), and the leader adds
// the party to the matchmaker (party_matchmaker_add); every member gets matchmaker_matched. There is no runtime function that
// puts a player's socket in a party, so creating and joining stay on the clients (server/pipeline_party.go,
// server/party_handler.go; https://heroiclabs.com/docs/nakama/concepts/parties/).
//
// Nakama JavaScript runtime rules this file follows (Nakama 3.37.0, checked 2026-11-18):
//   - Nakama loads one file, set by --runtime.js_entrypoint, relative to runtime.path (/nakama/data/modules in Docker), and
//     calls its top-level function InitModule(ctx, logger, nk, initializer).
//     https://heroiclabs.com/docs/nakama/server-framework/typescript-runtime/
//   - nk.fileRead(relPath) returns the text of a file; relPath is relative to runtime.path, and a missing file throws.
//     https://heroiclabs.com/docs/nakama/server-framework/typescript-runtime/function-reference/ (fileRead), and
//     server/runtime_javascript_nakama.go (fileRead joins runtime.path and relPath)
//   - initializer.registerRpc(id, fn): fn(ctx, logger, nk, payload) returns a string. Nakama finds fn by reading the source
//     of InitModule, so the call must be a statement of InitModule itself with a top-level function name, not an inline
//     function. Other statements of InitModule that are calls must not look like "a.b.c()" or "f().g()": put such code in
//     helper functions (server/runtime_javascript_init.go, getRegisteredFnIdentifier).
//     https://heroiclabs.com/docs/nakama/server-framework/runtime-examples/server-to-server/
//   - Every RPC call can run in another JavaScript VM, and globals are read-only by default (runtime.js_read_only_globals),
//     so the module keeps no state: it reads the file again on every call.
//   - initializer.registerRtBefore(name, fn) hooks a realtime message by the name of its Envelope field type
//     ("PartyMatchmakerAdd"); the same "statement of InitModule with a top-level function name" rule applies
//     (runtime_javascript_init.go, registerRtBefore and getRtHookFnIdentifier). fn(ctx, logger, nk, envelope) gets the
//     envelope as protojson with JSON names (envelope.partyMatchmakerAdd.minCount, stringProperties) and every field present,
//     and returns the envelope to go on with; returning null drops the message, throwing sends the client an error
//     (runtime_javascript.go BeforeRt, pipeline.go ProcessRequest).
//     https://heroiclabs.com/docs/nakama/server-framework/introduction/hooks/ (PartyMatchmakerAdd)
//   - Plain ES5 style (var, function), like the TypeScript runtime's compiled output.

var CK_QUEUES_FILE = 'coopkit_queues.json';
var CK_QUEUES_VERSION = 1;
var CK_MIN_PLAYERS = 2;
var CK_MAX_PLAYERS = 4;

function ckIsObject(value) {
  return value !== null && typeof value === 'object' && !Array.isArray(value);
}

/** Attribute keys of the plugin: letters, digits and _. The pattern lives inside the function: globals are frozen. */
function ckIsKey(value) {
  return typeof value === 'string' && /^[A-Za-z0-9_]+$/.test(value);
}

function ckIsWholeNumber(value) {
  return typeof value === 'number' && value % 1 === 0;
}

/** Problems of one queue entry, as texts; empty when it is fine. */
function ckCheckQueue(name, queue) {
  var problems = [];
  if (!ckIsObject(queue)) {
    return ['queue "' + name + '" must be an object'];
  }
  if (queue.queue !== name) {
    problems.push('queue "' + name + '": field "queue" must be "' + name + '"');
  }
  if (!(ckIsWholeNumber(queue.minCount) && ckIsWholeNumber(queue.maxCount) && queue.minCount >= CK_MIN_PLAYERS &&
      queue.minCount <= queue.maxCount && queue.maxCount <= CK_MAX_PLAYERS)) {
    problems.push('queue "' + name + '": minCount and maxCount must be whole numbers from ' + CK_MIN_PLAYERS + ' to ' + CK_MAX_PLAYERS);
  }
  var lists = ['matchKeys', 'numericKeys'];
  for (var i = 0; i < lists.length; i++) {
    var keys = queue[lists[i]];
    if (!Array.isArray(keys)) {
      problems.push('queue "' + name + '": ' + lists[i] + ' must be a list');
      continue;
    }
    for (var k = 0; k < keys.length; k++) {
      if (!ckIsKey(keys[k])) {
        problems.push('queue "' + name + '": ' + lists[i] + ' has a key that is not letters, digits and _');
      }
    }
  }
  if (!ckIsObject(queue.defaults)) {
    problems.push('queue "' + name + '": defaults must be an object');
  }
  if (!ckIsKey(queue.queueProperty)) {
    problems.push('queue "' + name + '": queueProperty must be the ticket property with the queue name');
  }
  return problems;
}

/**
 * Reads and checks the queue file. Returns {version, queues, problems}: queues holds only the entries without problems.
 * Never throws: a missing or broken file gives no queues and a problem.
 */
function ckLoadQueues(nk) {
  var result = { version: CK_QUEUES_VERSION, queues: {}, problems: [] };
  var text;
  try {
    text = nk.fileRead(CK_QUEUES_FILE);
  } catch (error) {
    result.problems.push('cannot read ' + CK_QUEUES_FILE + ' in the module folder: ' + error);
    return result;
  }
  var file;
  try {
    file = JSON.parse(text);
  } catch (error) {
    result.problems.push(CK_QUEUES_FILE + ' is not JSON: ' + error);
    return result;
  }
  if (!ckIsObject(file) || file.version !== CK_QUEUES_VERSION || !ckIsObject(file.queues)) {
    result.problems.push(CK_QUEUES_FILE + ' must be {"version": ' + CK_QUEUES_VERSION + ', "queues": {...}}');
    return result;
  }
  var names = Object.keys(file.queues);
  for (var i = 0; i < names.length; i++) {
    var problems = ckCheckQueue(names[i], file.queues[names[i]]);
    if (problems.length === 0) {
      result.queues[names[i]] = file.queues[names[i]];
    } else {
      result.problems = result.problems.concat(problems);
    }
  }
  return result;
}

/** Writes the loaded queues and the problems to the server log. */
function ckLogQueues(logger, loaded) {
  var names = Object.keys(loaded.queues);
  for (var i = 0; i < names.length; i++) {
    var queue = loaded.queues[names[i]];
    logger.info('%s', 'CoopKit queue "' + names[i] + '": ' + queue.minCount + '-' + queue.maxCount + ' players, match keys: ' +
      (queue.matchKeys.length ? queue.matchKeys.join(', ') : '(none)'));
  }
  if (names.length === 0) {
    logger.warn('%s', 'CoopKit: no queues loaded from ' + CK_QUEUES_FILE + '.');
  }
  for (var p = 0; p < loaded.problems.length; p++) {
    logger.warn('%s', 'CoopKit: ' + loaded.problems[p]);
  }
}

/** RPC coopkit_queues: the queue file as the server reads it, with the problems found. The payload is not used. */
function rpcCoopKitQueues(ctx, logger, nk, payload) {
  return JSON.stringify(ckLoadQueues(nk));
}

/**
 * The queue of a ticket: the file's queue whose queueProperty (CKMM_Queue) the ticket carries with the queue's name.
 * Returns null for a ticket of another game or of a queue the file does not hold.
 */
function ckFindTicketQueue(loaded, stringProperties) {
  if (!ckIsObject(stringProperties)) {
    return null;
  }
  var names = Object.keys(loaded.queues);
  for (var i = 0; i < names.length; i++) {
    var queue = loaded.queues[names[i]];
    if (stringProperties[queue.queueProperty] === names[i]) {
      return queue;
    }
  }
  return null;
}

/**
 * Applies the queue file to a party ticket (the partyMatchmakerAdd field of the envelope): a queue of the file sets minCount and
 * maxCount. Returns a line for the server log, or '' when nothing changed.
 */
function ckApplyQueueToPartyTicket(loaded, add) {
  if (!ckIsObject(add)) {
    return '';
  }
  var queue = ckFindTicketQueue(loaded, add.stringProperties);
  if (queue === null) {
    var named = ckIsObject(add.stringProperties) ? add.stringProperties.CKMM_Queue : undefined;
    return typeof named === 'string' ? 'CoopKit: party ticket of queue "' + named + '" is not in ' + CK_QUEUES_FILE + '; sent as the game built it.' : '';
  }
  if (add.minCount === queue.minCount && add.maxCount === queue.maxCount) {
    return '';
  }
  var line = 'CoopKit: party ticket of queue "' + queue.queue + '": ' + add.minCount + '-' + add.maxCount + ' players from the game, ' +
    queue.minCount + '-' + queue.maxCount + ' from ' + CK_QUEUES_FILE + '.';
  add.minCount = queue.minCount;
  add.maxCount = queue.maxCount;
  return line;
}

/** Before hook of party_matchmaker_add: the party ticket of a CoopKit queue gets the queue's player counts. */
function rtBeforePartyMatchmakerAdd(ctx, logger, nk, envelope) {
  var line = ckApplyQueueToPartyTicket(ckLoadQueues(nk), envelope.partyMatchmakerAdd);
  if (line !== '') {
    logger.info('%s', line);
  }
  return envelope;
}

function InitModule(ctx, logger, nk, initializer) {
  var loaded = ckLoadQueues(nk);
  ckLogQueues(logger, loaded);
  initializer.registerRpc('coopkit_queues', rpcCoopKitQueues);
  initializer.registerRtBefore('PartyMatchmakerAdd', rtBeforePartyMatchmakerAdd);
  logger.info('%s', 'CoopKit Matchmaking module loaded: RPC coopkit_queues, party ticket hook.');
}
