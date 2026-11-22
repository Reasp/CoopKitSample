#!/usr/bin/env node
// Copyright 2026 Ivan Iaitckikh. MIT License, see LICENSE in the Extras folder.
//
// Puts a queue exported from Unreal (Export tab of the queue Data Asset window, or the commandlet) into the queue file of
// the CoopKit server module for Nakama: <modules>/coopkit_queues.json = {"version": 1, "queues": {"<queue>": {...}}}.
// Other queues in the file stay. The module (modules/coopkit_matchmaking.js) reads that file and shows it through the RPC
// coopkit_queues.
//
//   node sync_queue.mjs <Queue>_Nakama.json --check                 check the file offline
//   node sync_queue.mjs <Queue>_Nakama.json --dry-run               check and print the queue file it would write
//   node sync_queue.mjs <Queue>_Nakama.json --diff [--server <url>] compare with the queue file and, with --server, with
//                                                                    what the running server reads (read only)
//   node sync_queue.mjs <Queue>_Nakama.json                         write the queue into the queue file
//   --modules <folder>   the server's module folder (default: ./modules, the folder that compose.yaml mounts)
//   --server <url>       Nakama's HTTP address, for example http://127.0.0.1:7350 (used by --diff)
//   --http-key <key>     Nakama's runtime.http_key (default: NAKAMA_HTTP_KEY from the environment or .env.local,
//                        else Nakama's default "defaulthttpkey")
//
// Exit code: 0 done, 1 the file has errors or the server refused the call, 2 wrong arguments or no file.
// Node.js 18 or later (built-in fetch), no packages.
//
// The server call is Nakama's server-to-server RPC over HTTP: POST /v2/rpc/<id>?http_key=<key>&unwrap with a raw JSON body,
// which answers with the RPC's raw result (https://heroiclabs.com/docs/nakama/server-framework/runtime-examples/server-to-server/,
// and server/api_rpc.go of Nakama 3.37.0, checked 2026-11-18).

import { existsSync, readFileSync, writeFileSync, mkdirSync } from 'node:fs';
import { dirname, join, resolve } from 'node:path';
import { pathToFileURL } from 'node:url';
import { readExport, describeExport, diffValues, formatDiff, parseEnvFile } from './read_export.mjs';

export const QUEUES_FILE = 'coopkit_queues.json';
export const QUEUES_VERSION = 1;
export const RPC_ID = 'coopkit_queues';
export const DEFAULT_SERVER = 'http://127.0.0.1:7350';
export const DEFAULT_HTTP_KEY = 'defaulthttpkey';
/** Match size range of a CoopKit queue (Min Players and Max Players of the queue data). */
export const MIN_PLAYERS = 2;
export const MAX_PLAYERS = 4;
/** Attribute keys of the plugin: letters, digits and _. The prefix CKMM_ is the plugin's own. */
export const KEY_PATTERN = /^[A-Za-z0-9_]+$/;
export const RESERVED_PREFIX = 'CKMM_';
/** Queue names that fit into a matchmaker query term (+properties.CKMM_Queue:<queue>) without escaping. */
export const QUEUE_NAME_PATTERN = /^[A-Za-z0-9_-]{1,64}$/;
const QUEUE_FIELDS = ['queue', 'minCount', 'maxCount', 'matchKeys', 'numericKeys', 'defaults', 'queueProperty'];

export const USAGE = [
  'Usage:',
  '  node sync_queue.mjs <Queue>_Nakama.json [--modules <folder>] [--check | --dry-run | --diff [--server <url>] [--http-key <key>]]',
  `Defaults: --modules ./modules, --server ${DEFAULT_SERVER} (only with --diff), --http-key from NAKAMA_HTTP_KEY or "${DEFAULT_HTTP_KEY}".`,
].join('\n');

const isObject = (value) => value !== null && typeof value === 'object' && !Array.isArray(value);
const isInteger = (value) => typeof value === 'number' && Number.isInteger(value);

/** Reads the command line. */
export function parseArgs(argv) {
  const options = { exportPath: null, modules: './modules', server: null, httpKey: null, mode: 'apply', help: false, error: null };
  const modes = [];
  const valueOf = (index, name) => {
    const value = argv[index];
    if (value === undefined || value.startsWith('--')) {
      options.error = `${name} needs a value.`;
      return null;
    }
    return value;
  };
  for (let index = 0; index < argv.length; index++) {
    const arg = argv[index];
    if (arg === '--check') modes.push('check');
    else if (arg === '--dry-run') modes.push('dry-run');
    else if (arg === '--diff') modes.push('diff');
    else if (arg === '--help' || arg === '-h') options.help = true;
    else if (arg === '--modules') options.modules = valueOf(++index, arg);
    else if (arg === '--server') options.server = valueOf(++index, arg);
    else if (arg === '--http-key') options.httpKey = valueOf(++index, arg);
    else if (arg.startsWith('--')) options.error = `Unknown option ${arg}.`;
    else if (options.exportPath === null) options.exportPath = arg;
    else options.error = `Unexpected argument ${arg}.`;
  }
  if (modes.length > 1) options.error = 'Give only one of --check, --dry-run and --diff.';
  else if (modes.length === 1) options.mode = modes[0];
  if (!options.error && options.server && options.mode !== 'diff') options.error = '--server works only with --diff.';
  if (!options.error && options.server && !/^https?:\/\/[^\s/?#]+\/?$/.test(options.server)) options.error = '--server must look like http://127.0.0.1:7350.';
  if (!options.error && !options.help && !options.exportPath) options.error = 'Give the export file (<Queue>_Nakama.json).';
  return options;
}

/**
 * Checks the "nakama" section of an export file (one queue of the queue file).
 * @returns {{level: 'error'|'warning', rule: string, message: string}[]}
 */
export function validateNakamaQueue(queue, path = 'nakama') {
  const issues = [];
  const error = (rule, message) => issues.push({ level: 'error', rule, message });
  if (!isObject(queue)) {
    error('type', `${path} must be an object.`);
    return issues;
  }
  for (const key of Object.keys(queue)) {
    if (!QUEUE_FIELDS.includes(key)) issues.push({ level: 'warning', rule: 'field', message: `${path}.${key} is not a field of a CoopKit queue (known: ${QUEUE_FIELDS.join(', ')}).` });
  }
  if (typeof queue.queue !== 'string' || !QUEUE_NAME_PATTERN.test(queue.queue)) {
    error('name', `${path}.queue must be 1-64 letters, digits, '_' or '-'.`);
  }
  const { minCount, maxCount } = queue;
  if (!(isInteger(minCount) && isInteger(maxCount) && minCount >= MIN_PLAYERS && minCount <= maxCount && maxCount <= MAX_PLAYERS)) {
    error('size', `${path}: minCount and maxCount must be whole numbers with ${MIN_PLAYERS} <= minCount <= maxCount <= ${MAX_PLAYERS}.`);
  }
  const checkKeys = (field) => {
    const keys = queue[field];
    if (!Array.isArray(keys)) {
      error('keys', `${path}.${field} must be a list (it may be empty).`);
      return [];
    }
    const seen = new Set();
    for (const key of keys) {
      if (typeof key !== 'string' || !KEY_PATTERN.test(key) || key.startsWith(RESERVED_PREFIX)) {
        error('keys', `${path}.${field}: "${key}" must be letters, digits and _, without the prefix ${RESERVED_PREFIX}.`);
      } else if (seen.has(key)) {
        error('keys', `${path}.${field}: "${key}" is listed twice.`);
      }
      seen.add(key);
    }
    return keys;
  };
  const matchKeys = checkKeys('matchKeys');
  const numericKeys = checkKeys('numericKeys');
  for (const key of numericKeys) {
    if (typeof key === 'string' && !matchKeys.includes(key)) error('keys', `${path}.numericKeys: "${key}" is not one of the matchKeys.`);
  }
  if (!isObject(queue.defaults)) {
    error('defaults', `${path}.defaults must be an object (it may be empty).`);
  } else {
    for (const [key, value] of Object.entries(queue.defaults)) {
      if (!KEY_PATTERN.test(key) || key.startsWith(RESERVED_PREFIX)) {
        error('defaults', `${path}.defaults: "${key}" must be letters, digits and _, without the prefix ${RESERVED_PREFIX}.`);
      } else if (numericKeys.includes(key) ? typeof value !== 'number' : typeof value !== 'string') {
        error('defaults', `${path}.defaults.${key} must be a ${numericKeys.includes(key) ? 'number (a numeric key)' : 'string'}.`);
      }
    }
  }
  if (typeof queue.queueProperty !== 'string' || !KEY_PATTERN.test(queue.queueProperty)) {
    error('keys', `${path}.queueProperty must be the ticket property with the queue name (CKMM_Queue).`);
  }
  return issues;
}

/**
 * Checks a parsed queue file {"version": 1, "queues": {...}}.
 * @returns {{level: 'error'|'warning', rule: string, message: string}[]}
 */
export function validateQueuesFile(file) {
  if (!isObject(file)) return [{ level: 'error', rule: 'type', message: `${QUEUES_FILE} must be a JSON object.` }];
  const issues = [];
  if (file.version !== QUEUES_VERSION) issues.push({ level: 'error', rule: 'version', message: `${QUEUES_FILE}: "version" must be ${QUEUES_VERSION}.` });
  if (!isObject(file.queues)) {
    issues.push({ level: 'error', rule: 'type', message: `${QUEUES_FILE}: "queues" must be an object.` });
    return issues;
  }
  for (const [name, queue] of Object.entries(file.queues)) {
    issues.push(...validateNakamaQueue(queue, `queues.${name}`));
    if (isObject(queue) && queue.queue !== name) issues.push({ level: 'error', rule: 'name', message: `queues.${name}.queue must be "${name}".` });
  }
  return issues;
}

/** The queue file with the queue added or replaced; the other queues and their order stay. */
export function mergeQueue(file, queue) {
  const queues = isObject(file?.queues) ? { ...file.queues } : {};
  queues[queue.queue] = queue;
  return { ...(isObject(file) ? file : {}), version: QUEUES_VERSION, queues };
}

/** The text written to the queue file. */
export const formatQueuesFile = (file) => `${JSON.stringify(file, null, 2)}\n`;

/** The HTTP key: --http-key, then NAKAMA_HTTP_KEY from the environment, then from .env.local, then Nakama's default. */
export function resolveHttpKey(optionKey, env, envFiles) {
  if (optionKey) return { key: optionKey, source: '--http-key' };
  if (env.NAKAMA_HTTP_KEY) return { key: env.NAKAMA_HTTP_KEY, source: 'environment' };
  for (const { path, text } of envFiles) {
    if (text === undefined) continue;
    const value = parseEnvFile(text).NAKAMA_HTTP_KEY;
    if (value) return { key: value, source: path };
  }
  return { key: DEFAULT_HTTP_KEY, source: 'Nakama default' };
}

/** The URL of the server-to-server RPC call that returns the queue file the server reads. */
export function rpcUrl(server, httpKey) {
  return `${server.replace(/\/+$/, '')}/v2/rpc/${RPC_ID}?http_key=${encodeURIComponent(httpKey)}&unwrap`;
}

/**
 * Asks the running server for its queue file through the RPC.
 * @returns {Promise<{file?: object, error?: string}>}
 */
export async function fetchServerQueues(fetchImpl, server, httpKey) {
  let response;
  try {
    response = await fetchImpl(rpcUrl(server, httpKey), { method: 'POST', headers: { 'Content-Type': 'application/json', Accept: 'application/json' }, body: '' });
  } catch (e) {
    return { error: `cannot reach ${server}: ${e.message}. Is the server running (docker compose -f compose.yaml ps)?` };
  }
  let text = '';
  try {
    text = await response.text();
  } catch {
    text = '';
  }
  if (response.status === 404) {
    return { error: `the server at ${server} has no RPC ${RPC_ID}: it runs without the CoopKit module. Mount ./modules and set --runtime.js_entrypoint as in compose.yaml, then restart it.` };
  }
  if (response.status === 401) return { error: 'the server refused the HTTP key: pass its runtime.http_key with --http-key or NAKAMA_HTTP_KEY.' };
  if (!response.ok) return { error: `the server answered HTTP ${response.status}: ${text.slice(0, 300)}` };
  try {
    return { file: JSON.parse(text) };
  } catch {
    return { error: `the server's answer is not JSON: ${text.slice(0, 300)}` };
  }
}

function logIssues(issues, log) {
  for (const issue of issues) log(`${issue.level === 'error' ? 'Error' : 'Warning'} [${issue.rule}]: ${issue.message}`);
  return issues.some((issue) => issue.level === 'error');
}

/**
 * Runs the script. Every outside effect comes in: argv, env, the .env.local texts, file reading and writing, fetch and the log.
 * @param {object} io
 * @param {(path: string) => string|undefined} io.readText
 * @param {(path: string, text: string) => void} io.writeText
 * @returns {Promise<number>} the exit code.
 */
export async function run({ argv, env, envFiles, readText, writeText, fetchImpl, log }) {
  const options = parseArgs(argv);
  if (options.help) {
    log(USAGE);
    return 0;
  }
  if (options.error) {
    log(`Error: ${options.error}\n${USAGE}`);
    return 2;
  }

  const text = readText(options.exportPath);
  if (text === undefined) {
    log(`Error: cannot read ${options.exportPath}.`);
    return 2;
  }
  let value;
  try {
    value = JSON.parse(text);
  } catch (e) {
    log(`Error: ${options.exportPath} is not JSON: ${e.message}`);
    return 1;
  }
  const read = readExport(value, 'nakama');
  if (read.error) {
    log(`Error: ${options.exportPath}: ${read.error}`);
    return 1;
  }
  for (const line of describeExport(read)) log(line);
  const queue = read.payload;
  if (logIssues(validateNakamaQueue(queue), log)) {
    log('The export file has errors; nothing was written.');
    return 1;
  }
  log(`Queue "${queue.queue}": ${queue.minCount}-${queue.maxCount} players, match keys: ${queue.matchKeys.join(', ') || '(none)'}.`);
  if (options.mode === 'check') {
    log('Check passed. Nothing was written.');
    return 0;
  }

  const filePath = join(options.modules, QUEUES_FILE);
  const existingText = readText(filePath);
  let existing = null;
  if (existingText !== undefined) {
    try {
      existing = JSON.parse(existingText);
    } catch (e) {
      log(`Error: ${filePath} is not JSON: ${e.message}. Fix or delete it; nothing was written.`);
      return 1;
    }
    const fileIssues = validateQueuesFile(existing);
    if (fileIssues.some((issue) => issue.level === 'error' && ['type', 'version'].includes(issue.rule))) {
      logIssues(fileIssues, log);
      log(`Error: ${filePath} is not a CoopKit queue file; nothing was written.`);
      return 1;
    }
  }

  if (options.mode === 'diff') {
    const inFile = existing?.queues?.[queue.queue];
    if (!inFile) log(`${filePath}: queue "${queue.queue}" is not in the file yet; applying adds it.`);
    else {
      const changes = diffValues(inFile, queue);
      if (changes.length === 0) log(`${filePath}: queue "${queue.queue}" matches the export.`);
      else {
        log(`${filePath}: queue "${queue.queue}" (in the file -> in the export):`);
        for (const line of formatDiff(changes)) log(line);
      }
    }
    if (options.server) {
      const { key, source } = resolveHttpKey(options.httpKey, env, envFiles);
      log(`Server ${options.server}, HTTP key from ${source}.`);
      const answer = await fetchServerQueues(fetchImpl, options.server, key);
      if (answer.error) {
        log(`Error: ${answer.error}`);
        return 1;
      }
      for (const problem of Array.isArray(answer.file?.problems) ? answer.file.problems : []) log(`Server warning: ${problem}`);
      const onServer = answer.file?.queues?.[queue.queue];
      if (!onServer) log(`Server: queue "${queue.queue}" is not loaded. Apply, then the server reads the file on the next call.`);
      else {
        const changes = diffValues(onServer, queue);
        if (changes.length === 0) log(`Server: queue "${queue.queue}" matches the export.`);
        else {
          log(`Server: queue "${queue.queue}" (on the server -> in the export):`);
          for (const line of formatDiff(changes)) log(line);
        }
      }
    }
    log('Nothing was written.');
    return 0;
  }

  const merged = mergeQueue(existing, queue);
  const output = formatQueuesFile(merged);
  if (options.mode === 'dry-run') {
    log(`Dry run: nothing is written. ${filePath} would be:\n${output.trimEnd()}`);
    return 0;
  }
  if (existingText === output) {
    log(`${filePath} already has this queue. Nothing to write.`);
    return 0;
  }
  try {
    writeText(filePath, output);
  } catch (e) {
    log(`Error: cannot write ${filePath}: ${e.message}`);
    return 1;
  }
  const others = Object.keys(merged.queues).filter((name) => name !== queue.queue);
  log(`Wrote queue "${queue.queue}" to ${filePath}${others.length ? ` (other queues kept: ${others.join(', ')})` : ''}.`);
  log('The module reads the file on every call of the RPC coopkit_queues. Restart Nakama to see the queues in its start-up log: docker compose -f compose.yaml restart nakama');
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
    writeText: (path, text) => {
      mkdirSync(dirname(path), { recursive: true });
      writeFileSync(path, text, 'utf8');
    },
    fetchImpl: globalThis.fetch,
    log: (line) => console.log(line),
  });
  process.exit(code);
}
