// Copyright 2026 Ivan Iaitckikh. MIT License, see LICENSE in the Extras folder.
//
// Helpers shared by the sync scripts of CoopKit Matchmaking: reading the queue export file that the Unreal editor writes
// (button Export in the queue Data Asset window, or the commandlet -run=CKMatchmakingExport), comparing two JSON values,
// and reading KEY=value lines of a .env.local file.
//
// The same file sits in the AWS, Nakama and PlayFab folders, so each folder works on its own when you copy it alone.
//
// Export file, version 1:
//   { "format": "coopkit-matchmaking-export", "version": 1, "platform": "flexmatch" | "playfab" | "nakama",
//     "queueAsset": "/Game/...", "queueName": "Coop", "notes": ["..."], "<platform>": { ...payload... } }
//
// No I/O: every function takes values and returns values. Node.js 18 or later, no packages.

export const EXPORT_FORMAT = 'coopkit-matchmaking-export';
export const EXPORT_VERSION = 1;
export const PLATFORMS = ['flexmatch', 'playfab', 'nakama'];

const isObject = (value) => value !== null && typeof value === 'object' && !Array.isArray(value);

/** True when the parsed JSON looks like an export file (it has a "format" field), not a bare platform file. */
export function isExportEnvelope(value) {
  return isObject(value) && 'format' in value;
}

/**
 * Checks the envelope of a parsed export file for one platform.
 * @param {unknown} value the parsed JSON.
 * @param {'flexmatch'|'playfab'|'nakama'} platform the platform of the calling script.
 * @returns {{error: string} | {envelope: object, payload: object, notes: string[], queueName: string, queueAsset: string}}
 */
export function readExport(value, platform) {
  if (!isObject(value)) {
    return { error: 'The file must be a JSON object: a CoopKit Matchmaking export file.' };
  }
  if (value.format !== EXPORT_FORMAT) {
    return { error: `The file is not a CoopKit Matchmaking export file: "format" must be "${EXPORT_FORMAT}".` };
  }
  if (value.version !== EXPORT_VERSION) {
    return { error: `Export version ${JSON.stringify(value.version)} is not supported: this script reads version ${EXPORT_VERSION}. Use the scripts from the same plugin version as the editor that wrote the file.` };
  }
  if (!PLATFORMS.includes(value.platform)) {
    return { error: `"platform" must be one of ${PLATFORMS.join(', ')}.` };
  }
  if (value.platform !== platform) {
    return { error: `The file was exported for ${value.platform}, and this script is for ${platform}. Use the script in the Extras folder of that platform, or export the queue again with the right provider.` };
  }
  const payload = value[platform];
  if (!isObject(payload)) {
    return { error: `The file has no "${platform}" section. Export the queue again.` };
  }
  const notes = value.notes ?? [];
  if (!Array.isArray(notes) || notes.some((note) => typeof note !== 'string')) {
    return { error: '"notes" must be a list of texts.' };
  }
  if (typeof value.queueName !== 'string' || value.queueName === '') {
    return { error: '"queueName" must be the name of the queue.' };
  }
  return { envelope: value, payload, notes, queueName: value.queueName, queueAsset: typeof value.queueAsset === 'string' ? value.queueAsset : '' };
}

/** The lines a script prints about an export: where it comes from and the notes on fields the platform cannot take. */
export function describeExport(read) {
  const lines = [`Export of queue "${read.queueName}"${read.queueAsset ? ` (${read.queueAsset})` : ''}.`];
  for (const note of read.notes) lines.push(`Note: ${note}`);
  return lines;
}

/** Reads KEY=value lines; # starts a comment line; quotes around the value are removed. */
export function parseEnvFile(text) {
  const values = {};
  for (const line of String(text ?? '').split(/\r?\n/)) {
    const match = /^\s*(?:export\s+)?([A-Za-z_][A-Za-z0-9_]*)\s*=\s*(.*?)\s*$/.exec(line);
    if (!match || line.trim().startsWith('#')) continue;
    values[match[1]] = match[2].replace(/^(['"])(.*)\1$/, '$2');
  }
  return values;
}

const isUnset = (value) => value === undefined || value === null;

/** A short one-line text of a JSON value for a diff line. */
export function formatValue(value) {
  return isUnset(value) ? '(not set)' : JSON.stringify(value);
}

/** Items of two arrays get matched by this field when every item of both arrays is an object that has it. */
const MATCH_FIELDS = ['Name', 'name'];

function matchField(left, right) {
  for (const field of MATCH_FIELDS) {
    const all = [...left, ...right];
    if (all.length > 0 && all.every((item) => isObject(item) && typeof item[field] === 'string')) {
      const names = (list) => list.map((item) => item[field]);
      if (new Set(names(left)).size === left.length && new Set(names(right)).size === right.length) return field;
    }
  }
  return null;
}

/**
 * Differences between two JSON values, field by field. Arrays of named objects (rules, teams) are matched by their name,
 * other arrays by position. A missing field and null count as the same ("not set").
 * @returns {{path: string, before: unknown, after: unknown}[]} empty when the values are equal.
 */
export function diffValues(before, after, path = '') {
  if (isUnset(before) && isUnset(after)) return [];
  if (isObject(before) && isObject(after)) {
    const changes = [];
    const keys = [...new Set([...Object.keys(before), ...Object.keys(after)])];
    for (const key of keys) changes.push(...diffValues(before[key], after[key], path ? `${path}.${key}` : key));
    return changes;
  }
  if (Array.isArray(before) && Array.isArray(after)) {
    const field = matchField(before, after);
    const changes = [];
    if (field) {
      const byName = (list) => new Map(list.map((item) => [item[field], item]));
      const left = byName(before);
      const right = byName(after);
      for (const name of new Set([...left.keys(), ...right.keys()])) changes.push(...diffValues(left.get(name), right.get(name), `${path}[${name}]`));
      // Same items in another order: report the order, because some platforms read it.
      if (changes.length === 0 && before.some((item, index) => item[field] !== after[index][field])) {
        changes.push({ path: `${path} (order)`, before: before.map((item) => item[field]), after: after.map((item) => item[field]) });
      }
      return changes;
    }
    const length = Math.max(before.length, after.length);
    for (let index = 0; index < length; index++) changes.push(...diffValues(before[index], after[index], `${path}[${index}]`));
    return changes;
  }
  if (JSON.stringify(before) === JSON.stringify(after)) return [];
  return [{ path: path || '(value)', before, after }];
}

/** "  path: before -> after" lines of a diff. */
export function formatDiff(changes) {
  return changes.map((change) => `  ${change.path}: ${formatValue(change.before)} -> ${formatValue(change.after)}`);
}
