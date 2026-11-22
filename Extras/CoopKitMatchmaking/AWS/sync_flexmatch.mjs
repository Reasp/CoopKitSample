#!/usr/bin/env node
// Copyright 2026 Ivan Iaitckikh. MIT License, see LICENSE in the Extras folder.
//
// Sends a queue exported from Unreal (Export tab of the queue Data Asset window, or the commandlet) to Amazon GameLift
// FlexMatch: the rule set, the matchmaking configuration of the CloudFormation stack (FlexMatchStack.json) and, with
// --function, the PLAYER_ATTRIBUTES setting of the stack's API function.
//
//   node sync_flexmatch.mjs <Queue>_FlexMatch.json --check
//       check the file offline: no keys, no network
//   node sync_flexmatch.mjs <Queue>_FlexMatch.json --configuration <name> [--function <name>] [--region <r>] --dry-run
//       check the file and print the requests, send nothing (works without keys)
//   node sync_flexmatch.mjs <Queue>_FlexMatch.json --configuration <name> [--function <name>] [--region <r>] --diff
//       read what is on AWS and print what applying would change (read only: Describe and Get calls)
//   node sync_flexmatch.mjs <Queue>_FlexMatch.json --configuration <name> [--function <name>] [--region <r>]
//       apply: ValidateMatchmakingRuleSet -> CreateMatchmakingRuleSet (when the rule set is not there yet) ->
//       UpdateMatchmakingConfiguration -> Lambda UpdateFunctionConfiguration (PLAYER_ATTRIBUTES, other variables kept)
//
// --configuration is the stack output MatchmakingConfigurationName. --function is the physical name of the stack's
// ApiFunction (see README.md). Rule sets cannot be edited in AWS: the export names the rule set after the queue and a
// checksum of its body, so a changed queue makes a new rule set and the configuration switches to it. Old rule sets stay
// until you delete them (DeleteMatchmakingRuleSet).
//
// Keys come from the environment variables AWS_ACCESS_KEY_ID, AWS_SECRET_ACCESS_KEY, AWS_SESSION_TOKEN (optional) and
// AWS_REGION (or --region), or from a .env.local file with lines KEY=value in the current folder (your project's root, not
// the plugin folder). Never commit that file, and never put AWS keys into the game.
// Exit code: 0 done, 1 the file has errors or AWS refused a call, 2 wrong arguments, no file or no keys.
// Node.js 18 or later (built-in fetch), no packages: requests are signed by aws_sign.mjs.
//
// API references, checked on 2026-11-18 (GameLift JSON 1.1 protocol: POST / with X-Amz-Target GameLift.<Action>):
//   https://docs.aws.amazon.com/gamelift/latest/apireference/API_ValidateMatchmakingRuleSet.html
//   https://docs.aws.amazon.com/gamelift/latest/apireference/API_DescribeMatchmakingRuleSets.html (NotFoundException)
//   https://docs.aws.amazon.com/gamelift/latest/apireference/API_CreateMatchmakingRuleSet.html
//   https://docs.aws.amazon.com/gamelift/latest/apireference/API_DescribeMatchmakingConfigurations.html
//   https://docs.aws.amazon.com/gamelift/latest/apireference/API_UpdateMatchmakingConfiguration.html
//   https://docs.aws.amazon.com/lambda/latest/api/API_GetFunctionConfiguration.html
//   https://docs.aws.amazon.com/lambda/latest/api/API_UpdateFunctionConfiguration.html (Environment replaces every
//   variable, so the script sends the old ones too; RevisionId refuses the update if the function changed meanwhile)

import { existsSync, readFileSync } from 'node:fs';
import { join, resolve } from 'node:path';
import { pathToFileURL } from 'node:url';
import { readExport, describeExport, diffValues, formatValue, parseEnvFile } from './read_export.mjs';
import { validateFlexMatchPayload, CONFIGURATION_NAME_PATTERN } from './rule_set.mjs';
import { signRequest, encodePathSegment } from './aws_sign.mjs';

export const USAGE = [
  'Usage:',
  '  node sync_flexmatch.mjs <Queue>_FlexMatch.json --check',
  '  node sync_flexmatch.mjs <Queue>_FlexMatch.json --configuration <MatchmakingConfigurationName> [--function <ApiFunctionName>] [--region <region>] [--dry-run | --diff]',
  'Keys: AWS_ACCESS_KEY_ID, AWS_SECRET_ACCESS_KEY, AWS_SESSION_TOKEN (optional), AWS_REGION in the environment or in .env.local (not needed for --check and --dry-run).',
].join('\n');

/** Lambda function name, partial ARN or ARN (UpdateFunctionConfiguration, FunctionName). */
export const FUNCTION_NAME_PATTERN = /^(arn:(aws[a-zA-Z-]*)?:lambda:)?([a-z]{2}((-gov)|(-iso([a-z]?)))?-[a-z]+-\d{1}:)?(\d{12}:)?(function:)?([a-zA-Z0-9-_]+)(:(\$LATEST|[a-zA-Z0-9-_]+))?$/;
/** Region names such as us-east-1, eu-central-1, us-gov-west-1. */
export const isRegion = (region) => /^[a-z]{2}(-[a-z]+)+-\d{1,2}$/.test(region);

/** Reads the command line. */
export function parseArgs(argv) {
  const options = { exportPath: null, configuration: null, functionName: null, region: null, mode: 'apply', help: false, error: null };
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
    else if (arg === '--configuration') options.configuration = valueOf(++index, arg);
    else if (arg === '--function') options.functionName = valueOf(++index, arg);
    else if (arg === '--region') options.region = valueOf(++index, arg);
    else if (arg.startsWith('--')) options.error = `Unknown option ${arg}.`;
    else if (options.exportPath === null) options.exportPath = arg;
    else options.error = `Unexpected argument ${arg}.`;
  }
  if (modes.length > 1) options.error = 'Give only one of --check, --dry-run and --diff.';
  else if (modes.length === 1) options.mode = modes[0];
  if (!options.error && !options.help && !options.exportPath) options.error = 'Give the export file (<Queue>_FlexMatch.json).';
  return options;
}

/** AWS keys and Region: the environment wins over .env.local. */
export function loadAwsKeys(env, envFiles) {
  const keys = {
    accessKeyId: env.AWS_ACCESS_KEY_ID || '',
    secretAccessKey: env.AWS_SECRET_ACCESS_KEY || '',
    sessionToken: env.AWS_SESSION_TOKEN || '',
    region: env.AWS_REGION || env.AWS_DEFAULT_REGION || '',
    source: 'environment',
  };
  for (const { path, text } of envFiles) {
    if (text === undefined || (keys.accessKeyId && keys.secretAccessKey && keys.region)) continue;
    const values = parseEnvFile(text);
    const before = JSON.stringify(keys);
    if (!keys.accessKeyId && !keys.secretAccessKey) {
      // Key pairs come from one place only: an id from one source and a secret from another never match.
      keys.accessKeyId = values.AWS_ACCESS_KEY_ID || '';
      keys.secretAccessKey = values.AWS_SECRET_ACCESS_KEY || '';
      keys.sessionToken ||= values.AWS_SESSION_TOKEN || '';
    }
    keys.region ||= values.AWS_REGION || values.AWS_DEFAULT_REGION || '';
    if (JSON.stringify(keys) !== before) keys.source = path;
  }
  return keys;
}

/** A GameLift call (JSON 1.1 protocol). */
export function gameLiftRequest(region, action, input) {
  return {
    service: 'gamelift',
    name: action,
    method: 'POST',
    url: `https://gamelift.${region}.amazonaws.com/`,
    headers: { 'Content-Type': 'application/x-amz-json-1.1', 'X-Amz-Target': `GameLift.${action}` },
    input,
    body: JSON.stringify(input),
  };
}

/** A Lambda call (REST protocol) on the configuration of a function: GET reads it, PUT changes it. */
export function lambdaRequest(region, method, functionName, input) {
  const request = {
    service: 'lambda',
    name: method === 'GET' ? 'GetFunctionConfiguration' : 'UpdateFunctionConfiguration',
    method,
    url: `https://lambda.${region}.amazonaws.com/2015-03-31/functions/${encodePathSegment(functionName)}/configuration`,
    headers: {},
    input,
    body: '',
  };
  if (method !== 'GET') {
    request.headers['Content-Type'] = 'application/json';
    request.body = JSON.stringify(input);
  }
  return request;
}

/** The Lambda environment after the sync: every variable of the function, PLAYER_ATTRIBUTES set to the export's value. */
export function mergeEnvironment(variables, playerAttributes) {
  return { ...(variables ?? {}), PLAYER_ATTRIBUTES: playerAttributes };
}

/** The input of UpdateMatchmakingConfiguration from the export. */
export function configurationUpdate(configurationName, payload) {
  return {
    Name: configurationName,
    RuleSetName: payload.ruleSet.name,
    RequestTimeoutSeconds: payload.configuration.requestTimeoutSeconds,
    AcceptanceRequired: payload.configuration.acceptanceRequired,
  };
}

/** Fields of UpdateMatchmakingConfiguration that differ between AWS and the export: [{path, before, after}]. */
export function configurationChanges(deployed, update) {
  const fields = ['RuleSetName', 'RequestTimeoutSeconds', 'AcceptanceRequired'];
  const pick = (source) => Object.fromEntries(fields.map((field) => [field, source?.[field]]));
  return diffValues(pick(deployed), pick(update));
}

/** Error name of an AWS answer: header X-Amzn-ErrorType, or __type / code in the body, without namespace. */
export function awsErrorName(headers, body) {
  const header = typeof headers?.get === 'function' ? headers.get('x-amzn-errortype') : headers?.['x-amzn-errortype'];
  const raw = header || (body && typeof body === 'object' ? body.__type || body.code || body.Code : '') || '';
  return String(raw).split(':')[0].split('#').pop();
}

/** "Name: message" of an AWS error answer. */
export function describeAwsError(answer) {
  const message = answer.body && typeof answer.body === 'object' ? answer.body.message || answer.body.Message : answer.text;
  return `HTTP ${answer.status}${answer.errorName ? ` ${answer.errorName}` : ''}${message ? `: ${message}` : ''}`;
}

/** Signs and sends one request. */
async function send(context, request) {
  const signed = signRequest({
    method: request.method,
    url: request.url,
    headers: request.headers,
    body: request.body,
    region: context.region,
    service: request.service,
    credentials: context.credentials,
    date: context.now(),
  });
  const init = { method: request.method, headers: signed.headers };
  if (request.method !== 'GET') init.body = request.body;
  const response = await context.fetchImpl(request.url, init);
  let text = '';
  try {
    text = await response.text();
  } catch {
    text = '';
  }
  let body = null;
  try {
    body = text ? JSON.parse(text) : {};
  } catch {
    body = null;
  }
  return { status: response.status, ok: response.ok, body, text, errorName: response.ok ? '' : awsErrorName(response.headers, body) };
}

/** Plain-text label of a request for logs and the dry run. */
function requestLabel(request) {
  return request.service === 'gamelift' ? `POST ${request.url} (${request.headers['X-Amz-Target']})` : `${request.method} ${request.url}`;
}

/** The requests of an apply, for the dry run: reads first, then the changes that apply makes when they are needed. */
export function planRequests({ payload, configurationName, functionName, region }) {
  const ruleSet = payload.ruleSet;
  const requests = [
    gameLiftRequest(region, 'ValidateMatchmakingRuleSet', { RuleSetBody: JSON.stringify(ruleSet.body) }),
    gameLiftRequest(region, 'DescribeMatchmakingRuleSets', { Names: [ruleSet.name] }),
    gameLiftRequest(region, 'DescribeMatchmakingConfigurations', { Names: [configurationName] }),
  ];
  if (functionName) requests.push(lambdaRequest(region, 'GET', functionName));
  requests.push(gameLiftRequest(region, 'CreateMatchmakingRuleSet', { Name: ruleSet.name, RuleSetBody: JSON.stringify(ruleSet.body) }));
  requests.push(gameLiftRequest(region, 'UpdateMatchmakingConfiguration', configurationUpdate(configurationName, payload)));
  if (functionName) {
    requests.push(lambdaRequest(region, 'PUT', functionName, {
      Environment: { Variables: { '<every other variable of the function, unchanged>': '...', PLAYER_ATTRIBUTES: payload.playerAttributes } },
      RevisionId: '<RevisionId from GetFunctionConfiguration>',
    }));
  }
  return requests;
}

/**
 * Reads what is on AWS now: the rule set, the configuration and the function's variables.
 * @returns {Promise<{error?: string, ruleSet?: object, configuration?: object, lambda?: object}>}
 */
async function inspect(context, { payload, configurationName, functionName }) {
  const { region } = context;
  const ruleSetName = payload.ruleSet.name;
  const state = {};

  let answer = await context.call(gameLiftRequest(region, 'DescribeMatchmakingRuleSets', { Names: [ruleSetName] }));
  if (!answer.ok && answer.errorName !== 'NotFoundException') return { error: `DescribeMatchmakingRuleSets: ${describeAwsError(answer)}` };
  const found = answer.ok ? (answer.body?.RuleSets ?? []).find((item) => item.RuleSetName === ruleSetName) : undefined;
  if (!found) state.ruleSet = { exists: false };
  else {
    let deployedBody;
    try {
      deployedBody = JSON.parse(found.RuleSetBody);
    } catch {
      deployedBody = found.RuleSetBody;
    }
    state.ruleSet = { exists: true, sameBody: diffValues(deployedBody, payload.ruleSet.body).length === 0 };
  }

  answer = await context.call(gameLiftRequest(region, 'DescribeMatchmakingConfigurations', { Names: [configurationName] }));
  if (!answer.ok && answer.errorName !== 'NotFoundException') return { error: `DescribeMatchmakingConfigurations: ${describeAwsError(answer)}` };
  const configuration = answer.ok ? (answer.body?.Configurations ?? []).find((item) => item.Name === configurationName || item.ConfigurationArn === configurationName) : undefined;
  if (!configuration) {
    return { error: `Matchmaking configuration "${configurationName}" was not found in ${region}. Use the stack output MatchmakingConfigurationName and the stack's Region.` };
  }
  state.configuration = configuration;

  if (functionName) {
    answer = await context.call(lambdaRequest(region, 'GET', functionName));
    if (!answer.ok) return { error: `GetFunctionConfiguration of "${functionName}": ${describeAwsError(answer)}` };
    state.lambda = { variables: answer.body?.Environment?.Variables ?? {}, revisionId: answer.body?.RevisionId };
  }
  return state;
}

/** Lines that say what applying the export changes on AWS. */
function describeChanges(state, { payload, configurationName, functionName }) {
  const lines = [];
  let count = 0;
  if (!state.ruleSet.exists) {
    lines.push(`Rule set "${payload.ruleSet.name}": not on AWS yet; applying creates it.`);
    count++;
  } else if (!state.ruleSet.sameBody) {
    lines.push(`Rule set "${payload.ruleSet.name}" is on AWS with another body. Rule sets cannot change: export the queue again (the name follows the body), or delete that rule set.`);
    count++;
  } else {
    lines.push(`Rule set "${payload.ruleSet.name}": already on AWS with the same body.`);
  }
  if (state.configuration.FlexMatchMode && state.configuration.FlexMatchMode !== payload.configuration.flexMatchMode) {
    lines.push(`Configuration "${configurationName}" runs in ${state.configuration.FlexMatchMode} mode, the export needs ${payload.configuration.flexMatchMode}. The script does not change the mode.`);
  }
  const changes = configurationChanges(state.configuration, configurationUpdate(configurationName, payload));
  if (changes.length === 0) lines.push(`Configuration "${configurationName}": no changes.`);
  else {
    lines.push(`Configuration "${configurationName}" (on AWS -> after applying):`);
    for (const change of changes) lines.push(`  ${change.path}: ${formatValue(change.before)} -> ${formatValue(change.after)}`);
    count += changes.length;
  }
  if (functionName) {
    const before = state.lambda.variables.PLAYER_ATTRIBUTES;
    if (before === payload.playerAttributes) lines.push(`Function "${functionName}": PLAYER_ATTRIBUTES is already ${formatValue(before)}.`);
    else {
      lines.push(`Function "${functionName}": PLAYER_ATTRIBUTES ${formatValue(before)} -> ${formatValue(payload.playerAttributes)} (other variables stay).`);
      count++;
    }
  }
  return { lines, count };
}

/**
 * Runs the script. Every outside effect comes in: argv, env, the .env.local texts, file reading, fetch, the clock and the log.
 * @returns {Promise<number>} the exit code.
 */
export async function run({ argv, env, envFiles, readText, fetchImpl, log, now = () => new Date() }) {
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
  const read = readExport(value, 'flexmatch');
  if (read.error) {
    log(`Error: ${options.exportPath}: ${read.error}`);
    return 1;
  }
  for (const line of describeExport(read)) log(line);
  const payload = read.payload;
  const issues = validateFlexMatchPayload(payload);
  for (const issue of issues) log(`${issue.level === 'error' ? 'Error' : 'Warning'} [${issue.rule}]: ${issue.message}`);
  if (issues.some((issue) => issue.level === 'error')) {
    log('The export file has errors; nothing was sent.');
    return 1;
  }
  log(`Rule set "${payload.ruleSet.name}", request timeout ${payload.configuration.requestTimeoutSeconds} s, PLAYER_ATTRIBUTES "${payload.playerAttributes}".`);
  if (options.mode === 'check') {
    log('Check passed. Nothing was sent.');
    return 0;
  }

  if (!options.configuration) {
    log(`Error: give --configuration <MatchmakingConfigurationName> (an output of the CloudFormation stack).\n${USAGE}`);
    return 2;
  }
  if (!CONFIGURATION_NAME_PATTERN.test(options.configuration)) {
    log('Error: --configuration must be 1-128 letters, digits, hyphens and dots.');
    return 2;
  }
  if (options.functionName && !FUNCTION_NAME_PATTERN.test(options.functionName)) {
    log('Error: --function must be a Lambda function name or ARN.');
    return 2;
  }
  const keys = loadAwsKeys(env, envFiles);
  const region = options.region || keys.region;
  if (region && !isRegion(region)) {
    log(`Error: "${region}" is not an AWS Region name such as us-east-1.`);
    return 2;
  }
  const plan = { payload, configurationName: options.configuration, functionName: options.functionName };

  if (options.mode === 'dry-run') {
    log('Dry run: nothing is sent. Applying makes these requests, signed with AWS Signature Version 4 (Create and Update only when something differs):');
    for (const request of planRequests({ ...plan, region: region || '<region>' })) {
      log(request.method === 'GET' ? requestLabel(request) : `${requestLabel(request)}\n${JSON.stringify(request.input, null, 2)}`);
    }
    if (!options.functionName) log(`Note: without --function the backend's PLAYER_ATTRIBUTES is not changed; it must be "${payload.playerAttributes}".`);
    if (!keys.accessKeyId || !keys.secretAccessKey || !region) log('Note: set AWS_ACCESS_KEY_ID, AWS_SECRET_ACCESS_KEY and AWS_REGION (or --region) before a real run.');
    return 0;
  }

  if (!region) {
    log('Error: set AWS_REGION or pass --region (the Region of the stack).');
    return 2;
  }
  if (!keys.accessKeyId || !keys.secretAccessKey) {
    log('Error: set AWS_ACCESS_KEY_ID and AWS_SECRET_ACCESS_KEY in the environment or in .env.local.');
    return 2;
  }
  log(`Region ${region}, keys from ${keys.source}.`);
  const context = {
    region,
    credentials: { accessKeyId: keys.accessKeyId, secretAccessKey: keys.secretAccessKey, sessionToken: keys.sessionToken || undefined },
    fetchImpl,
    now,
  };
  context.call = async (request) => {
    try {
      return await send(context, request);
    } catch (e) {
      return { status: 0, ok: false, body: null, text: e.message, errorName: 'NetworkError' };
    }
  };

  if (options.mode === 'diff') {
    const state = await inspect(context, plan);
    if (state.error) {
      log(`Error: ${state.error}`);
      return 1;
    }
    const { lines, count } = describeChanges(state, plan);
    for (const line of lines) log(line);
    if (!options.functionName) log(`Note: without --function the backend's PLAYER_ATTRIBUTES is not compared; it must be "${payload.playerAttributes}".`);
    log(count === 0 ? 'No changes. Nothing was changed on AWS.' : 'Nothing was changed on AWS.');
    return 0;
  }

  // Apply: validate, read everything (so a wrong name fails before anything is created), then change in order.
  const bodyText = JSON.stringify(payload.ruleSet.body);
  let answer = await context.call(gameLiftRequest(region, 'ValidateMatchmakingRuleSet', { RuleSetBody: bodyText }));
  if (!answer.ok || answer.body?.Valid !== true) {
    log(`Error: ValidateMatchmakingRuleSet: ${answer.ok ? 'the rule set is not valid' : describeAwsError(answer)}.`);
    return 1;
  }
  log('Rule set body is valid (ValidateMatchmakingRuleSet).');
  const state = await inspect(context, plan);
  if (state.error) {
    log(`Error: ${state.error}`);
    return 1;
  }
  if (state.ruleSet.exists && !state.ruleSet.sameBody) {
    log(`Error: rule set "${payload.ruleSet.name}" is on AWS with another body. Rule sets cannot change: export the queue again, or delete that rule set.`);
    return 1;
  }
  if (state.configuration.FlexMatchMode && state.configuration.FlexMatchMode !== payload.configuration.flexMatchMode) {
    log(`Error: configuration "${options.configuration}" runs in ${state.configuration.FlexMatchMode} mode; the export needs ${payload.configuration.flexMatchMode}.`);
    return 1;
  }

  if (!state.ruleSet.exists) {
    answer = await context.call(gameLiftRequest(region, 'CreateMatchmakingRuleSet', { Name: payload.ruleSet.name, RuleSetBody: bodyText }));
    if (!answer.ok) {
      log(`Error: CreateMatchmakingRuleSet: ${describeAwsError(answer)}.`);
      return 1;
    }
    log(`Rule set "${payload.ruleSet.name}" created.`);
  } else {
    log(`Rule set "${payload.ruleSet.name}" is already on AWS.`);
  }

  const update = configurationUpdate(options.configuration, payload);
  const changes = configurationChanges(state.configuration, update);
  if (changes.length === 0) log(`Configuration "${options.configuration}" is up to date.`);
  else {
    answer = await context.call(gameLiftRequest(region, 'UpdateMatchmakingConfiguration', update));
    if (!answer.ok) {
      log(`Error: UpdateMatchmakingConfiguration: ${describeAwsError(answer)}.`);
      return 1;
    }
    log(`Configuration "${options.configuration}" updated: ${changes.map((change) => change.path).join(', ')}.`);
  }

  if (options.functionName) {
    if (state.lambda.variables.PLAYER_ATTRIBUTES === payload.playerAttributes) {
      log(`Function "${options.functionName}": PLAYER_ATTRIBUTES is already "${payload.playerAttributes}".`);
    } else {
      const input = { Environment: { Variables: mergeEnvironment(state.lambda.variables, payload.playerAttributes) } };
      if (state.lambda.revisionId) input.RevisionId = state.lambda.revisionId;
      answer = await context.call(lambdaRequest(region, 'PUT', options.functionName, input));
      if (!answer.ok) {
        log(`Error: UpdateFunctionConfiguration of "${options.functionName}": ${describeAwsError(answer)}. The rule set and the configuration are already changed: run the script again.`);
        return 1;
      }
      log(`Function "${options.functionName}": PLAYER_ATTRIBUTES set to "${payload.playerAttributes}".`);
    }
  } else {
    log(`Note: without --function the backend's PLAYER_ATTRIBUTES was not changed; it must be "${payload.playerAttributes}".`);
  }
  log('Done. A later update of the CloudFormation stack can switch the configuration and PLAYER_ATTRIBUTES back: run this script again after each stack update.');
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
