#!/usr/bin/env node
// Copyright 2026 Ivan Iaitckikh. MIT License, see LICENSE in the Extras folder.
//
// Creates, updates, compares or removes a PlayFab matchmaking queue from a JSON file, with PlayFab's REST API:
//   1. Authentication/GetEntityToken with the title's secret key (X-SecretKey header) -> a title entity token;
//   2. Match/SetMatchmakingQueue {MatchmakingQueue: <the queue>}, Match/GetMatchmakingQueue {QueueName} (--diff) or
//      Match/RemoveMatchmakingQueue {QueueName}, with that token.
// The secret key is a developer key: keep it on your machine. The game never needs it (the provider signs players in
// with the Title Id only).
//
// The file is either a MatchmakingQueueConfig (such as CoopQueue.json) or the export file that the Unreal editor writes
// for a queue with the Azure PlayFab provider (Export tab of the queue Data Asset window): <QueueName>_PlayFab.json.
//
//   node set_queue.mjs [file.json] --check      check the file offline: no keys, nothing sent
//   node set_queue.mjs [file.json] --dry-run    check the file and print the requests, send nothing (works without keys)
//   node set_queue.mjs [file.json] --diff       compare the file with the queue on PlayFab (read only, needs keys)
//   node set_queue.mjs [file.json]              check the file, then set the queue (CoopQueue.json by default)
//   node set_queue.mjs --remove <QueueName> [--dry-run]   remove the queue
//
// Keys come from the environment variables PLAYFAB_TITLE_ID and PLAYFAB_SECRET_KEY, or from a .env.local file with
// lines KEY=value in the current folder (your project's root, not the plugin folder). Never commit that file.
// Exit code: 0 done, 1 the file has errors or PlayFab refused the call, 2 no file or no keys.
// Node.js 18 or later (built-in fetch), no packages.
//
// GetMatchmakingQueue and the error of a queue that does not exist (MatchmakingQueueNotFound, 2016) are from
// https://learn.microsoft.com/en-us/rest/api/playfab/multiplayer/matchmaking-admin/get-matchmaking-queue (checked 2026-11-18).

import { existsSync, readFileSync } from 'node:fs';
import { dirname, join, resolve } from 'node:path';
import { fileURLToPath, pathToFileURL } from 'node:url';
import { validateQueueConfig, userAttributes, QUEUE_NAME_PATTERN } from './queue_config.mjs';
import { isExportEnvelope, readExport, describeExport, diffValues, formatDiff, parseEnvFile } from './read_export.mjs';

const here = dirname(fileURLToPath(import.meta.url));

export const USAGE = [
  'Usage:',
  '  node set_queue.mjs [CoopQueue.json | <Queue>_PlayFab.json] [--check | --dry-run | --diff]',
  '  node set_queue.mjs --remove <QueueName> [--dry-run]',
  'Keys: PLAYFAB_TITLE_ID and PLAYFAB_SECRET_KEY in the environment or in .env.local (not needed for --check and --dry-run).',
].join('\n');

/** PlayFab's error of a queue that does not exist (GetMatchmakingQueue reference, "Error Codes"). */
export const QUEUE_NOT_FOUND_ERROR = 'MatchmakingQueueNotFound';
export const QUEUE_NOT_FOUND_CODE = 2016;

/** Reads the command line. The flags --check and --diff are set on the result only when they are given. */
export function parseArgs(argv) {
  const options = { configPath: null, removeName: null, dryRun: false, help: false, error: null };
  for (let index = 0; index < argv.length; index++) {
    const arg = argv[index];
    if (arg === '--dry-run') options.dryRun = true;
    else if (arg === '--check') options.check = true;
    else if (arg === '--diff') options.diff = true;
    else if (arg === '--help' || arg === '-h') options.help = true;
    else if (arg === '--remove') {
      options.removeName = argv[++index] ?? null;
      if (!options.removeName) options.error = '--remove needs a queue name.';
    } else if (arg.startsWith('--')) options.error = `Unknown option ${arg}.`;
    else if (options.configPath === null) options.configPath = arg;
    else options.error = `Unexpected argument ${arg}.`;
  }
  if (options.removeName && options.configPath) options.error = 'Give a JSON file or --remove, not both.';
  if ([options.dryRun, options.check, options.diff].filter(Boolean).length > 1) options.error = 'Give only one of --check, --dry-run and --diff.';
  if (options.removeName && (options.check || options.diff)) options.error = '--remove works only with --dry-run.';
  return options;
}

/** Reads KEY=value lines of .env.local (kept here for older callers; the code is in read_export.mjs). */
export { parseEnvFile };

/** Title id and secret key: the environment wins over .env.local. */
export function loadKeys(env, envFiles) {
  const keys = { titleId: env.PLAYFAB_TITLE_ID || '', secretKey: env.PLAYFAB_SECRET_KEY || '', source: 'environment' };
  if (keys.titleId && keys.secretKey) return keys;
  for (const { path, text } of envFiles) {
    if (text === undefined) continue;
    const values = parseEnvFile(text);
    keys.titleId ||= values.PLAYFAB_TITLE_ID || '';
    keys.secretKey ||= values.PLAYFAB_SECRET_KEY || '';
    keys.source = path;
    if (keys.titleId && keys.secretKey) break;
  }
  return keys;
}

/** True for a Title Id of letters and digits, as Game Manager shows it. */
export const isTitleId = (titleId) => /^[A-Za-z0-9]{1,32}$/.test(titleId);

/**
 * The two calls of a run: [GetEntityToken, then SetMatchmakingQueue, GetMatchmakingQueue (diff) or RemoveMatchmakingQueue].
 */
export function buildRequests({ titleId, secretKey, config, removeName, diff = false }) {
  const base = `https://${titleId}.playfabapi.com`;
  const token = {
    name: 'GetEntityToken',
    url: `${base}/Authentication/GetEntityToken`,
    headers: { 'Content-Type': 'application/json', 'X-SecretKey': secretKey },
    body: {},
  };
  let change;
  if (removeName) change = { name: 'RemoveMatchmakingQueue', url: `${base}/Match/RemoveMatchmakingQueue`, body: { QueueName: removeName } };
  else if (diff) change = { name: 'GetMatchmakingQueue', url: `${base}/Match/GetMatchmakingQueue`, body: { QueueName: config.Name } };
  else change = { name: 'SetMatchmakingQueue', url: `${base}/Match/SetMatchmakingQueue`, body: { MatchmakingQueue: config } };
  change.headers = { 'Content-Type': 'application/json', 'X-EntityToken': '<entity token from GetEntityToken>' };
  return [token, change];
}

/** "error (errorCode): errorMessage" of a PlayFab error answer. */
export function describePlayFabError(status, body) {
  if (body && typeof body === 'object' && (body.error || body.errorCode)) {
    return `HTTP ${status} ${body.error} (${body.errorCode})${body.errorMessage ? `: ${body.errorMessage}` : ''}`;
  }
  return `HTTP ${status}`;
}

/**
 * True when a GetMatchmakingQueue error says that the queue does not exist. The reference lists MatchmakingQueueNotFound
 * (2016). Assumption, not checked against a live title: a message such as "queue ... not found" means the same, in case
 * PlayFab answers with a generic error code.
 */
export function isQueueNotFound(body) {
  if (!body || typeof body !== 'object') return false;
  if (body.error === QUEUE_NOT_FOUND_ERROR || body.errorCode === QUEUE_NOT_FOUND_CODE) return true;
  return typeof body.errorMessage === 'string' && /queue/i.test(body.errorMessage) && /not\s+(be\s+)?found|does not exist/i.test(body.errorMessage);
}

async function postJson(fetchImpl, request, headers) {
  const response = await fetchImpl(request.url, { method: 'POST', headers, body: JSON.stringify(request.body) });
  let body = null;
  try {
    body = await response.json();
  } catch {
    body = null;
  }
  return { status: response.status, ok: response.ok, body };
}

/**
 * Reads the queue from the text of a file: a MatchmakingQueueConfig, or a PlayFab export file of the Unreal editor.
 * @returns {{config?: object, exportLines?: string[], error?: string}}
 */
export function readQueueText(text, path) {
  let value;
  try {
    value = JSON.parse(text);
  } catch (e) {
    return { error: `${path} is not JSON: ${e.message}` };
  }
  if (!isExportEnvelope(value)) return { config: value };
  const read = readExport(value, 'playfab');
  if (read.error) return { error: `${path}: ${read.error}` };
  const queue = read.payload.queue;
  if (queue === null || typeof queue !== 'object' || Array.isArray(queue)) {
    return { error: `${path}: the "playfab" section has no "queue" object. Export the queue again.` };
  }
  const exportLines = describeExport(read);
  if (queue.Name !== read.queueName) {
    exportLines.push(`Note: the PlayFab queue name is "${queue.Name}" (Queue Name Override of the provider), the queue data's name is "${read.queueName}".`);
  }
  return { config: queue, exportLines };
}

/**
 * Runs the script. Every outside effect comes in: argv, env, the .env.local texts, file reading, fetch and the log.
 * @returns {Promise<number>} the exit code.
 */
export async function run({ argv, env, envFiles, readText, fetchImpl, log }) {
  const options = parseArgs(argv);
  if (options.help) {
    log(USAGE);
    return 0;
  }
  if (options.error) {
    log(`Error: ${options.error}\n${USAGE}`);
    return 2;
  }

  let config = null;
  if (options.removeName) {
    if (!QUEUE_NAME_PATTERN.test(options.removeName)) {
      log(`Error: "${options.removeName}" is not a PlayFab queue name.`);
      return 1;
    }
  } else {
    const configPath = options.configPath ?? join(here, 'CoopQueue.json');
    const text = readText(configPath);
    if (text === undefined) {
      log(`Error: cannot read ${configPath}.`);
      return 2;
    }
    const read = readQueueText(text, configPath);
    if (read.error) {
      log(`Error: ${read.error}`);
      return 1;
    }
    config = read.config;
    for (const line of read.exportLines ?? []) log(line);
    const issues = validateQueueConfig(config);
    for (const issue of issues) log(`${issue.level === 'error' ? 'Error' : 'Warning'} [${issue.rule}]: ${issue.message}`);
    if (issues.some((issue) => issue.level === 'error')) {
      log(`The queue file has errors; nothing was sent.`);
      return 1;
    }
    log(`Queue "${config.Name}": ${config.MinMatchSize}-${config.MaxMatchSize} players, ticket attributes: ${userAttributes(config).join(', ') || '(none)'}.`);
    if (options.check) {
      log('Check passed. Nothing was sent.');
      return 0;
    }
  }

  const keys = loadKeys(env, envFiles);
  if (keys.titleId && !isTitleId(keys.titleId)) {
    log('Error: PLAYFAB_TITLE_ID must be letters and digits (Game Manager > Settings > Game Properties).');
    return 2;
  }
  const requests = buildRequests({ titleId: keys.titleId || '<TitleId>', secretKey: keys.secretKey, config, removeName: options.removeName, diff: options.diff });

  if (options.dryRun) {
    log('Dry run: nothing is sent. The requests would be:');
    for (const request of requests) {
      const headers = { ...request.headers };
      if ('X-SecretKey' in headers) headers['X-SecretKey'] = keys.secretKey ? `<PLAYFAB_SECRET_KEY, ${keys.secretKey.length} characters>` : '<PLAYFAB_SECRET_KEY is not set>';
      log(`POST ${request.url}\n${JSON.stringify({ headers, body: request.body }, null, 2)}`);
    }
    if (!keys.titleId || !keys.secretKey) log('Note: set PLAYFAB_TITLE_ID and PLAYFAB_SECRET_KEY before a real run.');
    return 0;
  }

  if (!keys.titleId || !keys.secretKey) {
    log('Error: set PLAYFAB_TITLE_ID and PLAYFAB_SECRET_KEY in the environment or in .env.local.');
    return 2;
  }
  log(`Title ${keys.titleId}, keys from ${keys.source}.`);
  const [tokenRequest, changeRequest] = requests;
  let answer;
  try {
    answer = await postJson(fetchImpl, tokenRequest, tokenRequest.headers);
  } catch (e) {
    log(`Error: GetEntityToken failed: ${e.message}`);
    return 1;
  }
  const entityToken = answer.body?.data?.EntityToken;
  if (!answer.ok || typeof entityToken !== 'string' || !entityToken) {
    log(`Error: GetEntityToken: ${describePlayFabError(answer.status, answer.body)}. Check the Title Id and the secret key.`);
    return 1;
  }
  try {
    answer = await postJson(fetchImpl, changeRequest, { ...changeRequest.headers, 'X-EntityToken': entityToken });
  } catch (e) {
    log(`Error: ${changeRequest.name} failed: ${e.message}`);
    return 1;
  }

  if (options.diff) {
    if (!answer.ok) {
      if (isQueueNotFound(answer.body)) {
        log(`Queue "${config.Name}" is not deployed yet: applying the file creates it.`);
        return 0;
      }
      log(`Error: GetMatchmakingQueue: ${describePlayFabError(answer.status, answer.body)}.`);
      return 1;
    }
    const deployed = answer.body?.data?.MatchmakingQueue;
    if (!deployed || typeof deployed !== 'object') {
      log(`Queue "${config.Name}" is not deployed yet: applying the file creates it.`);
      return 0;
    }
    const changes = diffValues(deployed, config);
    if (changes.length === 0) {
      log(`No changes: queue "${config.Name}" on PlayFab matches the file.`);
    } else {
      log(`Queue "${config.Name}": applying the file changes ${changes.length} field(s) (on PlayFab -> in the file):`);
      for (const line of formatDiff(changes)) log(line);
    }
    log('Nothing was changed on PlayFab.');
    return 0;
  }

  if (!answer.ok) {
    log(`Error: ${changeRequest.name}: ${describePlayFabError(answer.status, answer.body)}.`);
    return 1;
  }
  log(options.removeName ? `Queue "${options.removeName}" removed.` : `Queue "${config.Name}" set.`);
  return 0;
}

const readOptional = (path) => (existsSync(path) ? readFileSync(path, 'utf8') : undefined);

if (process.argv[1] && import.meta.url === pathToFileURL(resolve(process.argv[1])).href) {
  // Only the current folder: a .env.local next to this script would sit inside the plugin and could be packaged with it.
  const envPath = join(process.cwd(), '.env.local');
  const code = await run({
    argv: process.argv.slice(2),
    env: process.env,
    envFiles: [{ path: envPath, text: readOptional(envPath) }],
    readText: readOptional,
    fetchImpl: globalThis.fetch,
    log: (line) => console.log(line),
  });
  process.exit(code);
}
