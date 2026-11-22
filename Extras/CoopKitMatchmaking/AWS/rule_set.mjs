// Copyright 2026 Ivan Iaitckikh. MIT License, see LICENSE in the Extras folder.
//
// Offline check of the "flexmatch" section of a CoopKit Matchmaking export file: the FlexMatch rule set (name and body),
// the matchmaking configuration fields that sync_flexmatch.mjs sets, and the PLAYER_ATTRIBUTES value of the backend Lambda.
// It finds mistakes before a call to AWS. GameLift checks the body again with ValidateMatchmakingRuleSet before anything is
// created, so this check covers the structure, not every rule type of the rule language.
//
// Sources, checked on 2026-11-18:
//   rule set schema   - https://docs.aws.amazon.com/gamelift/latest/flexmatchguide/match-rulesets-schema.html
//   rule set names    - https://docs.aws.amazon.com/gamelift/latest/apireference/API_CreateMatchmakingRuleSet.html
//                       (Name: 1-128 characters, pattern ^[a-zA-Z0-9-\.]*$)
//   configuration     - https://docs.aws.amazon.com/gamelift/latest/apireference/API_UpdateMatchmakingConfiguration.html
//                       (RequestTimeoutSeconds: 1-43200)
//   PLAYER_ATTRIBUTES - backend/api.js, parseAttributeTypes ("Mode:S,Skill:N")
//
// No I/O: the functions take the parsed JSON and return a list of issues. Node.js 18 or later, no packages.

/** Names of rule sets and matchmaking configurations. */
export const RULE_SET_NAME_PATTERN = /^[A-Za-z0-9.-]{1,128}$/;
export const CONFIGURATION_NAME_PATTERN = /^[A-Za-z0-9.-]{1,128}$/;
export const MIN_REQUEST_TIMEOUT = 1;
export const MAX_REQUEST_TIMEOUT = 43200;
/** Attribute types that the backend can send: string (S) and number (N). */
export const ATTRIBUTE_TYPES = { string: 'S', number: 'N' };
/** Attribute types of the rule language that the backend does not send. */
const OTHER_ATTRIBUTE_TYPES = ['string_list', 'string_number_map'];

const isObject = (value) => value !== null && typeof value === 'object' && !Array.isArray(value);
const isInteger = (value) => typeof value === 'number' && Number.isInteger(value);

/** Reads "Mode:S,Skill:N" into [["Mode", "S"], ["Skill", "N"]]; null when a part is malformed. */
export function parsePlayerAttributes(text) {
  if (typeof text !== 'string') return null;
  if (text.trim() === '') return [];
  const pairs = [];
  for (const part of text.split(',')) {
    const match = /^\s*([A-Za-z0-9_]+)\s*:\s*([SN])\s*$/.exec(part);
    if (!match) return null;
    pairs.push([match[1], match[2]]);
  }
  return pairs;
}

/** The PLAYER_ATTRIBUTES value that matches the playerAttributes of a rule set body. */
export function playerAttributesOf(body) {
  if (!isObject(body) || !Array.isArray(body.playerAttributes)) return '';
  return body.playerAttributes
    .filter((attribute) => isObject(attribute) && ATTRIBUTE_TYPES[attribute.type])
    .map((attribute) => `${attribute.name}:${ATTRIBUTE_TYPES[attribute.type]}`)
    .join(',');
}

/**
 * Checks a FlexMatch rule set body (the object, not the JSON text).
 * @returns {{level: 'error'|'warning', rule: string, message: string}[]}
 */
export function validateRuleSetBody(body) {
  const issues = [];
  const error = (rule, message) => issues.push({ level: 'error', rule, message });
  const warning = (rule, message) => issues.push({ level: 'warning', rule, message });
  if (!isObject(body)) {
    error('type', 'ruleSet.body must be a JSON object (the rule set body).');
    return issues;
  }
  if (body.ruleLanguageVersion !== '1.0') error('version', 'ruleSet.body.ruleLanguageVersion must be "1.0".');
  if ('name' in body && typeof body.name !== 'string') error('type', 'ruleSet.body.name must be a string.');

  const attributes = new Map();
  if (!Array.isArray(body.playerAttributes)) {
    error('attributes', 'ruleSet.body.playerAttributes must be a list (it may be empty).');
  } else {
    body.playerAttributes.forEach((attribute, index) => {
      const path = `ruleSet.body.playerAttributes[${index}]`;
      if (!isObject(attribute) || typeof attribute.name !== 'string' || attribute.name === '') {
        error('attributes', `${path} needs a name.`);
        return;
      }
      if (attributes.has(attribute.name)) error('attributes', `${path}: "${attribute.name}" is declared twice.`);
      if (!ATTRIBUTE_TYPES[attribute.type] && !OTHER_ATTRIBUTE_TYPES.includes(attribute.type)) {
        error('attributes', `${path}.type must be string or number (or string_list, string_number_map).`);
      } else if (OTHER_ATTRIBUTE_TYPES.includes(attribute.type)) {
        warning('attributes', `${path}: the backend sends only string and number attributes, so "${attribute.name}" (${attribute.type}) always takes its default.`);
      }
      if ('default' in attribute) {
        if (attribute.type === 'string' && typeof attribute.default !== 'string') error('attributes', `${path}.default must be a string.`);
        if (attribute.type === 'number' && typeof attribute.default !== 'number') error('attributes', `${path}.default must be a number.`);
      }
      attributes.set(attribute.name, attribute.type);
    });
  }

  if ('algorithm' in body && !isObject(body.algorithm)) error('type', 'ruleSet.body.algorithm must be an object.');

  const teams = new Set();
  if (!Array.isArray(body.teams) || body.teams.length === 0) {
    error('teams', 'ruleSet.body.teams must list at least one team.');
  } else {
    body.teams.forEach((team, index) => {
      const path = `ruleSet.body.teams[${index}]`;
      if (!isObject(team) || typeof team.name !== 'string' || team.name === '') {
        error('teams', `${path} needs a name.`);
        return;
      }
      if (teams.has(team.name)) error('teams', `${path}: team "${team.name}" is declared twice.`);
      teams.add(team.name);
      if (!(isInteger(team.minPlayers) && isInteger(team.maxPlayers) && team.minPlayers >= 1 && team.minPlayers <= team.maxPlayers)) {
        error('teams', `${path} needs whole minPlayers and maxPlayers with 1 <= minPlayers <= maxPlayers.`);
      }
    });
  }

  const ruleNames = new Set();
  if (!Array.isArray(body.rules)) {
    error('rules', 'ruleSet.body.rules must be a list (it may be empty).');
  } else {
    body.rules.forEach((rule, index) => {
      const path = `ruleSet.body.rules[${index}]`;
      if (!isObject(rule) || typeof rule.name !== 'string' || rule.name === '') {
        error('rules', `${path} needs a name.`);
        return;
      }
      if (ruleNames.has(rule.name)) error('rules', `${path}: rule "${rule.name}" is declared twice.`);
      ruleNames.add(rule.name);
      if (typeof rule.type !== 'string') error('rules', `${path}.type is required.`);
      if ('description' in rule && typeof rule.description !== 'string') error('rules', `${path}.description must be a string.`);
      if (rule.type === 'batchDistance') {
        if (typeof rule.batchAttribute !== 'string' || !attributes.has(rule.batchAttribute)) {
          error('rules', `${path}.batchAttribute must name an attribute declared in playerAttributes.`);
        } else if (attributes.get(rule.batchAttribute) === 'number' && !('maxDistance' in rule)) {
          // FlexMatch rule types: a batch distance rule on numbers compares them with maxDistance.
          warning('rules', `${path}: "${rule.batchAttribute}" is a number; FlexMatch compares numbers with maxDistance (0 for equal values). ValidateMatchmakingRuleSet decides whether the rule works without it.`);
        }
      }
    });
  }

  if (!Array.isArray(body.expansions)) {
    error('expansions', 'ruleSet.body.expansions must be a list (it may be empty).');
  } else {
    body.expansions.forEach((expansion, index) => {
      const path = `ruleSet.body.expansions[${index}]`;
      if (!isObject(expansion) || typeof expansion.target !== 'string') {
        error('expansions', `${path}.target is required.`);
        return;
      }
      const teamTarget = /^teams\[([^\]]+)\]\.(minPlayers|maxPlayers)$/.exec(expansion.target);
      if (teamTarget && !teams.has(teamTarget[1])) error('expansions', `${path}.target names team "${teamTarget[1]}", which is not declared.`);
      const ruleTarget = /^rules\[([^\]]+)\]\./.exec(expansion.target);
      if (ruleTarget && !ruleNames.has(ruleTarget[1])) error('expansions', `${path}.target names rule "${ruleTarget[1]}", which is not declared.`);
      if (!Array.isArray(expansion.steps) || expansion.steps.length === 0) {
        error('expansions', `${path}.steps must list at least one step.`);
        return;
      }
      let last = 0;
      expansion.steps.forEach((step, stepIndex) => {
        const stepPath = `${path}.steps[${stepIndex}]`;
        if (!isObject(step) || !isInteger(step.waitTimeSeconds) || step.waitTimeSeconds < 1) {
          error('expansions', `${stepPath}.waitTimeSeconds must be a whole number of at least 1.`);
          return;
        }
        if (step.waitTimeSeconds <= last) error('expansions', `${stepPath}.waitTimeSeconds must be later than the step before.`);
        last = step.waitTimeSeconds;
        if (!('value' in step)) error('expansions', `${stepPath}.value is required.`);
      });
    });
  }
  return issues;
}

/**
 * Checks the "flexmatch" section of an export file: rule set name and body, configuration, PLAYER_ATTRIBUTES.
 * @returns {{level: 'error'|'warning', rule: string, message: string}[]} empty when everything is fine.
 */
export function validateFlexMatchPayload(payload) {
  const issues = [];
  const error = (rule, message) => issues.push({ level: 'error', rule, message });
  const warning = (rule, message) => issues.push({ level: 'warning', rule, message });
  if (!isObject(payload)) {
    error('type', 'The "flexmatch" section must be an object.');
    return issues;
  }
  const ruleSet = payload.ruleSet;
  if (!isObject(ruleSet)) {
    error('type', 'flexmatch.ruleSet must be an object with name and body.');
  } else {
    if (typeof ruleSet.name !== 'string' || !RULE_SET_NAME_PATTERN.test(ruleSet.name)) {
      error('name', 'ruleSet.name must be 1-128 letters, digits, hyphens and dots.');
    }
    issues.push(...validateRuleSetBody(ruleSet.body));
  }

  const configuration = payload.configuration;
  if (!isObject(configuration)) {
    error('type', 'flexmatch.configuration must be an object.');
  } else {
    const timeout = configuration.requestTimeoutSeconds;
    if (!(isInteger(timeout) && timeout >= MIN_REQUEST_TIMEOUT && timeout <= MAX_REQUEST_TIMEOUT)) {
      error('configuration', `configuration.requestTimeoutSeconds must be a whole number from ${MIN_REQUEST_TIMEOUT} to ${MAX_REQUEST_TIMEOUT}.`);
    } else if (isObject(ruleSet) && isObject(ruleSet.body) && Array.isArray(ruleSet.body.expansions)) {
      for (const expansion of ruleSet.body.expansions) {
        for (const step of Array.isArray(expansion?.steps) ? expansion.steps : []) {
          if (isInteger(step?.waitTimeSeconds) && step.waitTimeSeconds >= timeout) {
            warning('configuration', `An expansion waits ${step.waitTimeSeconds} s, not less than the request timeout of ${timeout} s: tickets time out before it applies.`);
          }
        }
      }
    }
    if (typeof configuration.acceptanceRequired !== 'boolean') error('configuration', 'configuration.acceptanceRequired must be true or false.');
    if (configuration.flexMatchMode !== 'STANDALONE') error('configuration', 'configuration.flexMatchMode must be STANDALONE: the CoopKit backend uses FlexMatch without GameLift servers.');
  }

  const pairs = parsePlayerAttributes(payload.playerAttributes);
  if (pairs === null) {
    error('attributes', 'flexmatch.playerAttributes must look like "Mode:S,Skill:N" (S string, N number).');
  } else if (isObject(ruleSet) && isObject(ruleSet.body) && Array.isArray(ruleSet.body.playerAttributes)) {
    const declared = new Map(ruleSet.body.playerAttributes.filter(isObject).map((attribute) => [attribute.name, ATTRIBUTE_TYPES[attribute.type]]));
    const listed = new Map(pairs);
    for (const [name, type] of pairs) {
      if (!declared.has(name)) error('attributes', `flexmatch.playerAttributes lists "${name}", which the rule set does not declare.`);
      else if (declared.get(name) !== type) error('attributes', `flexmatch.playerAttributes gives "${name}" type ${type}, the rule set declares ${declared.get(name) ?? 'another type'}.`);
    }
    for (const [name, type] of declared) {
      if (type && !listed.has(name)) error('attributes', `flexmatch.playerAttributes misses "${name}" of the rule set: the backend would not send it.`);
    }
  }
  return issues;
}
