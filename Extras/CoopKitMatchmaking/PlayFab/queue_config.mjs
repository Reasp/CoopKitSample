// Copyright 2026 Ivan Iaitckikh. MIT License, see LICENSE in the Extras folder.
//
// Offline check of a PlayFab matchmaking queue configuration (MatchmakingQueueConfig), the body that set_queue.mjs sends
// to Match/SetMatchmakingQueue. It finds mistakes before a paid or failed call: unknown or misspelled fields (a key or a
// secret pasted into the file is one of them), wrong types and enum values, queue and rule names PlayFab refuses, match
// sizes out of range, more than 20 rules, two expansions on one rule, team rules without teams, server allocation without
// a build.
//
// Sources, checked on 2026-11-14:
//   fields and enums  - Matchmaking Admin - Set Matchmaking Queue, REST API version 260922
//                       https://learn.microsoft.com/en-us/rest/api/playfab/multiplayer/matchmaking-admin/set-matchmaking-queue
//   names and limits  - Configuring matchmaking queues
//                       https://learn.microsoft.com/en-us/gaming/playfab/multiplayer/matchmaking/config-queues
//   attribute paths   - Specifying attributes with your tickets
//                       https://learn.microsoft.com/en-us/gaming/playfab/multiplayer/matchmaking/ticket-attributes
//
// No I/O: validateQueueConfig takes the parsed JSON and returns a list of issues. Node.js 18 or later, no packages.

/** Queue and team names: 1-64 letters, digits, underscores and hyphens, starting with a letter or a digit. */
export const QUEUE_NAME_PATTERN = /^[A-Za-z0-9][A-Za-z0-9_-]{0,63}$/;
/** Rule names: 1-255 letters, digits, underscores and hyphens, starting with a letter or a digit. */
export const RULE_NAME_PATTERN = /^[A-Za-z0-9][A-Za-z0-9_-]{0,254}$/;
/** Smallest MinMatchSize. */
export const MIN_MATCH_SIZE = 2;
/** Largest MaxMatchSize without teams, and with teams. */
export const MAX_MATCH_SIZE = 100;
export const MAX_MATCH_SIZE_WITH_TEAMS = 32;
/** Rules per queue. */
export const MAX_RULES = 20;

const ATTRIBUTE_SOURCES = ['User', 'PlayerEntity'];
const NOT_SPECIFIED_BEHAVIORS = ['UseDefault', 'MatchAny'];
const MERGE_FUNCTIONS = ['Min', 'Max', 'Average'];

/** Field types: 'string', 'number', 'boolean', 'object:<Type>', 'array:<type>'. */
const TYPES = {
  MatchmakingQueueConfig: {
    BuildAliasParams: 'object:BuildAliasParams',
    BuildId: 'string',
    DifferenceRules: 'array:object:DifferenceRule',
    MatchTotalRules: 'array:object:MatchTotalRule',
    MaxMatchSize: 'number',
    MaxTicketSize: 'number',
    MinMatchSize: 'number',
    Name: 'string',
    RegionSelectionRule: 'object:RegionSelectionRule',
    ServerAllocationEnabled: 'boolean',
    SetIntersectionRules: 'array:object:SetIntersectionRule',
    StatisticsVisibilityToPlayers: 'object:StatisticsVisibilityToPlayers',
    StringEqualityRules: 'array:object:StringEqualityRule',
    TeamDifferenceRules: 'array:object:TeamDifferenceRule',
    TeamSizeBalanceRule: 'object:TeamSizeBalanceRule',
    TeamTicketSizeSimilarityRule: 'object:TeamTicketSizeSimilarityRule',
    Teams: 'array:object:MatchmakingQueueTeam',
  },
  BuildAliasParams: { AliasId: 'string' },
  StatisticsVisibilityToPlayers: { ShowNumberOfPlayersMatching: 'boolean', ShowTimeToMatch: 'boolean' },
  MatchmakingQueueTeam: { MaxTeamSize: 'number', MinTeamSize: 'number', Name: 'string' },
  QueueRuleAttribute: { Path: 'string', Source: 'string' },
  OverrideDouble: { Value: 'number' },
  OverrideUnsignedInt: { Value: 'number' },
  StringEqualityRule: {
    Attribute: 'object:QueueRuleAttribute', AttributeNotSpecifiedBehavior: 'string', DefaultAttributeValue: 'string',
    Expansion: 'object:StringEqualityRuleExpansion', Name: 'string', SecondsUntilOptional: 'number', Weight: 'number',
  },
  StringEqualityRuleExpansion: { EnabledOverrides: 'array:boolean', SecondsBetweenExpansions: 'number' },
  DifferenceRule: {
    Attribute: 'object:QueueRuleAttribute', AttributeNotSpecifiedBehavior: 'string', CustomExpansion: 'object:CustomDifferenceRuleExpansion',
    DefaultAttributeValue: 'number', Difference: 'number', LinearExpansion: 'object:LinearDifferenceRuleExpansion', MergeFunction: 'string',
    Name: 'string', SecondsUntilOptional: 'number', Weight: 'number',
  },
  CustomDifferenceRuleExpansion: { DifferenceOverrides: 'array:object:OverrideDouble', SecondsBetweenExpansions: 'number' },
  LinearDifferenceRuleExpansion: { Delta: 'number', Limit: 'number', SecondsBetweenExpansions: 'number' },
  SetIntersectionRule: {
    Attribute: 'object:QueueRuleAttribute', AttributeNotSpecifiedBehavior: 'string', CustomExpansion: 'object:CustomSetIntersectionRuleExpansion',
    DefaultAttributeValue: 'array:string', LinearExpansion: 'object:LinearSetIntersectionRuleExpansion', MinIntersectionSize: 'number',
    Name: 'string', SecondsUntilOptional: 'number', Weight: 'number',
  },
  CustomSetIntersectionRuleExpansion: { MinIntersectionSizeOverrides: 'array:object:OverrideUnsignedInt', SecondsBetweenExpansions: 'number' },
  LinearSetIntersectionRuleExpansion: { Delta: 'number', SecondsBetweenExpansions: 'number' },
  MatchTotalRule: {
    Attribute: 'object:QueueRuleAttribute', Expansion: 'object:MatchTotalRuleExpansion', Max: 'number', Min: 'number', Name: 'string',
    SecondsUntilOptional: 'number', Weight: 'number',
  },
  MatchTotalRuleExpansion: { MaxOverrides: 'array:object:OverrideDouble', MinOverrides: 'array:object:OverrideDouble', SecondsBetweenExpansions: 'number' },
  RegionSelectionRule: {
    CustomExpansion: 'object:CustomRegionSelectionRuleExpansion', LinearExpansion: 'object:LinearRegionSelectionRuleExpansion', MaxLatency: 'number',
    Name: 'string', Path: 'string', SecondsUntilOptional: 'number', Weight: 'number',
  },
  CustomRegionSelectionRuleExpansion: { MaxLatencyOverrides: 'array:object:OverrideUnsignedInt', SecondsBetweenExpansions: 'number' },
  LinearRegionSelectionRuleExpansion: { Delta: 'number', Limit: 'number', SecondsBetweenExpansions: 'number' },
  TeamDifferenceRule: {
    Attribute: 'object:QueueRuleAttribute', CustomExpansion: 'object:CustomTeamDifferenceRuleExpansion', DefaultAttributeValue: 'number',
    Difference: 'number', LinearExpansion: 'object:LinearTeamDifferenceRuleExpansion', Name: 'string', SecondsUntilOptional: 'number',
  },
  CustomTeamDifferenceRuleExpansion: { DifferenceOverrides: 'array:object:OverrideDouble', SecondsBetweenExpansions: 'number' },
  LinearTeamDifferenceRuleExpansion: { Delta: 'number', Limit: 'number', SecondsBetweenExpansions: 'number' },
  TeamSizeBalanceRule: {
    CustomExpansion: 'object:CustomTeamSizeBalanceRuleExpansion', Difference: 'number', LinearExpansion: 'object:LinearTeamSizeBalanceRuleExpansion',
    Name: 'string', SecondsUntilOptional: 'number',
  },
  CustomTeamSizeBalanceRuleExpansion: { DifferenceOverrides: 'array:object:OverrideUnsignedInt', SecondsBetweenExpansions: 'number' },
  LinearTeamSizeBalanceRuleExpansion: { Delta: 'number', Limit: 'number', SecondsBetweenExpansions: 'number' },
  TeamTicketSizeSimilarityRule: { Name: 'string', SecondsUntilOptional: 'number' },
};

/** Rule lists and single rules of a queue: field -> [rule type, required fields besides Name]. */
const RULE_FIELDS = {
  StringEqualityRules: ['StringEqualityRule', ['Attribute']],
  DifferenceRules: ['DifferenceRule', ['Attribute', 'Difference']],
  SetIntersectionRules: ['SetIntersectionRule', ['Attribute', 'MinIntersectionSize']],
  MatchTotalRules: ['MatchTotalRule', ['Attribute', 'Min', 'Max']],
  RegionSelectionRule: ['RegionSelectionRule', ['Path', 'MaxLatency']],
  TeamDifferenceRules: ['TeamDifferenceRule', ['Attribute', 'Difference']],
  TeamSizeBalanceRule: ['TeamSizeBalanceRule', ['Difference']],
  TeamTicketSizeSimilarityRule: ['TeamTicketSizeSimilarityRule', []],
};
const TEAM_RULE_FIELDS = ['TeamDifferenceRules', 'TeamSizeBalanceRule', 'TeamTicketSizeSimilarityRule'];

const isObject = (value) => value !== null && typeof value === 'object' && !Array.isArray(value);
const isInteger = (value) => typeof value === 'number' && Number.isInteger(value);

/** Checks the JSON types and the known fields of value against type typeName; reports at path. */
function checkShape(value, typeName, path, issues) {
  const fields = TYPES[typeName];
  if (!isObject(value)) {
    issues.push({ level: 'error', rule: 'type', message: `${path} must be an object (${typeName}).` });
    return;
  }
  for (const [key, child] of Object.entries(value)) {
    const type = fields[key];
    if (!type) {
      issues.push({ level: 'error', rule: 'field', message: `${path}.${key} is not a field of ${typeName} (known: ${Object.keys(fields).join(', ')}).` });
      continue;
    }
    checkValue(child, type, `${path}.${key}`, issues);
  }
}

function checkValue(value, type, path, issues) {
  if (type.startsWith('array:')) {
    if (!Array.isArray(value)) {
      issues.push({ level: 'error', rule: 'type', message: `${path} must be an array.` });
      return;
    }
    value.forEach((item, index) => checkValue(item, type.slice('array:'.length), `${path}[${index}]`, issues));
    return;
  }
  if (type.startsWith('object:')) {
    checkShape(value, type.slice('object:'.length), path, issues);
    return;
  }
  if (typeof value !== type) {
    issues.push({ level: 'error', rule: 'type', message: `${path} must be a ${type}, not ${Array.isArray(value) ? 'array' : typeof value}.` });
  }
}

function checkNonNegative(rule, fields, path, issues) {
  for (const field of fields) {
    if (typeof rule[field] === 'number' && rule[field] < 0) {
      issues.push({ level: 'error', rule: 'range', message: `${path}.${field} must not be negative.` });
    }
  }
}

function checkExpansion(rule, path, issues) {
  if (rule.LinearExpansion && rule.CustomExpansion) {
    issues.push({ level: 'error', rule: 'expansion', message: `${path} has LinearExpansion and CustomExpansion; only one expansion can be set per rule.` });
  }
  for (const key of ['LinearExpansion', 'CustomExpansion', 'Expansion']) {
    const expansion = rule[key];
    if (isObject(expansion) && !(typeof expansion.SecondsBetweenExpansions === 'number' && expansion.SecondsBetweenExpansions > 0)) {
      issues.push({ level: 'error', rule: 'expansion', message: `${path}.${key}.SecondsBetweenExpansions must be a number above 0.` });
    }
  }
}

function checkAttribute(rule, path, issues) {
  const attribute = rule.Attribute;
  if (!isObject(attribute)) {
    return;
  }
  if (typeof attribute.Path !== 'string' || attribute.Path.length === 0) {
    issues.push({ level: 'error', rule: 'attribute', message: `${path}.Attribute.Path is required.` });
  }
  if (!ATTRIBUTE_SOURCES.includes(attribute.Source)) {
    issues.push({ level: 'error', rule: 'attribute', message: `${path}.Attribute.Source must be ${ATTRIBUTE_SOURCES.join(' or ')}.` });
  } else if (attribute.Source === 'PlayerEntity' && typeof attribute.Path === 'string' && !attribute.Path.startsWith('$.')) {
    issues.push({ level: 'warning', rule: 'attribute', message: `${path}.Attribute.Path of a PlayerEntity attribute is a JSONPath such as $.playerSkill.Mean.` });
  } else if (attribute.Source === 'User' && typeof attribute.Path === 'string' && attribute.Path.startsWith('$')) {
    issues.push({ level: 'warning', rule: 'attribute', message: `${path}.Attribute.Path of a User attribute is the attribute's name in the ticket, not a JSONPath.` });
  }
  if ('AttributeNotSpecifiedBehavior' in rule && !NOT_SPECIFIED_BEHAVIORS.includes(rule.AttributeNotSpecifiedBehavior)) {
    issues.push({ level: 'error', rule: 'enum', message: `${path}.AttributeNotSpecifiedBehavior must be ${NOT_SPECIFIED_BEHAVIORS.join(' or ')}.` });
  }
  if (rule.AttributeNotSpecifiedBehavior === 'UseDefault' && !('DefaultAttributeValue' in rule)) {
    issues.push({ level: 'error', rule: 'attribute', message: `${path} uses UseDefault but has no DefaultAttributeValue.` });
  }
}

/** Every rule of the queue as [field path, rule type, required fields, rule]. */
export function listRules(config) {
  const rules = [];
  for (const [field, [type, required]] of Object.entries(RULE_FIELDS)) {
    const value = config[field];
    if (Array.isArray(value)) {
      value.forEach((rule, index) => rules.push([`${field}[${index}]`, type, required, rule]));
    } else if (value !== undefined && !field.endsWith('s')) {
      rules.push([field, type, required, value]);
    }
  }
  return rules;
}

/**
 * Checks a MatchmakingQueueConfig (the parsed JSON object).
 * @returns {{level: 'error'|'warning', rule: string, message: string}[]} empty when the configuration is fine.
 */
export function validateQueueConfig(config) {
  const issues = [];
  if (!isObject(config)) {
    return [{ level: 'error', rule: 'type', message: 'The queue configuration must be a JSON object (MatchmakingQueueConfig).' }];
  }
  if ('MatchmakingQueue' in config) {
    return [{ level: 'error', rule: 'field', message: 'Write the MatchmakingQueueConfig itself, without the MatchmakingQueue wrapper: set_queue.mjs adds it.' }];
  }
  checkShape(config, 'MatchmakingQueueConfig', 'queue', issues);

  for (const field of ['Name', 'MinMatchSize', 'MaxMatchSize']) {
    if (!(field in config)) {
      issues.push({ level: 'error', rule: 'required', message: `queue.${field} is required.` });
    }
  }
  if (typeof config.Name === 'string' && !QUEUE_NAME_PATTERN.test(config.Name)) {
    issues.push({ level: 'error', rule: 'name', message: `queue.Name "${config.Name}" must be 1-64 letters, digits, '_' or '-', starting with a letter or a digit.` });
  }

  const hasTeams = Array.isArray(config.Teams) && config.Teams.length > 0;
  const maxSize = hasTeams ? MAX_MATCH_SIZE_WITH_TEAMS : MAX_MATCH_SIZE;
  if ('MinMatchSize' in config && !(isInteger(config.MinMatchSize) && config.MinMatchSize >= MIN_MATCH_SIZE)) {
    issues.push({ level: 'error', rule: 'size', message: `queue.MinMatchSize must be a whole number of at least ${MIN_MATCH_SIZE}.` });
  }
  if ('MaxMatchSize' in config && !(isInteger(config.MaxMatchSize) && config.MaxMatchSize <= maxSize)) {
    issues.push({ level: 'error', rule: 'size', message: `queue.MaxMatchSize must be a whole number of at most ${maxSize}${hasTeams ? ' with teams' : ''}.` });
  }
  if (isInteger(config.MinMatchSize) && isInteger(config.MaxMatchSize) && config.MinMatchSize > config.MaxMatchSize) {
    issues.push({ level: 'error', rule: 'size', message: 'queue.MinMatchSize must not be above MaxMatchSize.' });
  }
  if ('MaxTicketSize' in config && !(isInteger(config.MaxTicketSize) && config.MaxTicketSize >= 1 && (!isInteger(config.MaxMatchSize) || config.MaxTicketSize <= config.MaxMatchSize))) {
    issues.push({ level: 'error', rule: 'size', message: 'queue.MaxTicketSize must be a whole number from 1 to MaxMatchSize.' });
  }

  if (config.ServerAllocationEnabled === true) {
    if (!config.BuildId && !config.BuildAliasParams) {
      issues.push({ level: 'error', rule: 'server', message: 'queue.ServerAllocationEnabled needs BuildId or BuildAliasParams of a PlayFab Multiplayer Servers build.' });
    }
    if (!config.RegionSelectionRule) {
      issues.push({ level: 'warning', rule: 'server', message: 'A queue that allocates servers needs a RegionSelectionRule (PlayFab: "Required for multiplayer server integration").' });
    }
  }

  if (Array.isArray(config.Teams)) {
    if (config.Teams.length === 1) {
      issues.push({ level: 'error', rule: 'teams', message: 'queue.Teams needs two or more teams (or none).' });
    }
    const seen = new Set();
    config.Teams.forEach((team, index) => {
      if (!isObject(team)) {
        return;
      }
      const path = `queue.Teams[${index}]`;
      if (typeof team.Name !== 'string' || !QUEUE_NAME_PATTERN.test(team.Name)) {
        issues.push({ level: 'error', rule: 'name', message: `${path}.Name must be 1-64 letters, digits, '_' or '-', starting with a letter or a digit.` });
      } else if (seen.has(team.Name.toLowerCase())) {
        issues.push({ level: 'error', rule: 'name', message: `${path}.Name "${team.Name}" is used by another team.` });
      } else {
        seen.add(team.Name.toLowerCase());
      }
      if (!(isInteger(team.MinTeamSize) && isInteger(team.MaxTeamSize) && team.MinTeamSize >= 1 && team.MinTeamSize <= team.MaxTeamSize)) {
        issues.push({ level: 'error', rule: 'teams', message: `${path} needs whole MinTeamSize and MaxTeamSize with 1 <= MinTeamSize <= MaxTeamSize.` });
      }
    });
  }
  for (const field of TEAM_RULE_FIELDS) {
    if (config[field] !== undefined && !hasTeams) {
      issues.push({ level: 'error', rule: 'teams', message: `queue.${field} is a team rule; it needs Teams.` });
    }
  }

  const rules = listRules(config);
  if (rules.length > MAX_RULES) {
    issues.push({ level: 'error', rule: 'rules', message: `The queue has ${rules.length} rules; PlayFab allows ${MAX_RULES}.` });
  }
  const ruleNames = new Set();
  for (const [field, , required, rule] of rules) {
    if (!isObject(rule)) {
      continue;
    }
    const path = `queue.${field}`;
    if (typeof rule.Name !== 'string' || !RULE_NAME_PATTERN.test(rule.Name)) {
      issues.push({ level: 'error', rule: 'name', message: `${path}.Name must be 1-255 letters, digits, '_' or '-', starting with a letter or a digit.` });
    } else if (ruleNames.has(rule.Name)) {
      issues.push({ level: 'error', rule: 'name', message: `${path}.Name "${rule.Name}" is used by another rule; rule names are unique within a queue.` });
    } else {
      ruleNames.add(rule.Name);
    }
    for (const name of required) {
      if (!(name in rule)) {
        issues.push({ level: 'error', rule: 'required', message: `${path}.${name} is required.` });
      }
    }
    checkAttribute(rule, path, issues);
    checkExpansion(rule, path, issues);
    checkNonNegative(rule, ['Weight', 'SecondsUntilOptional', 'Difference', 'MinIntersectionSize', 'MaxLatency'], path, issues);
    if ('MergeFunction' in rule && !MERGE_FUNCTIONS.includes(rule.MergeFunction)) {
      issues.push({ level: 'error', rule: 'enum', message: `${path}.MergeFunction must be ${MERGE_FUNCTIONS.join(', ')}.` });
    }
    if (typeof rule.Min === 'number' && typeof rule.Max === 'number' && rule.Max < rule.Min) {
      issues.push({ level: 'error', rule: 'range', message: `${path}.Max must be >= Min.` });
    }
    if (field.startsWith('MatchTotalRules') && typeof rule.Min === 'number' && rule.Min < 2) {
      issues.push({ level: 'warning', rule: 'range', message: `${path}.Min: PlayFab's reference says "Must be >=2".` });
    }
  }
  return issues;
}

/** User attributes the queue's rules read, by name (what the game must put into its tickets). */
export function userAttributes(config) {
  const names = new Set();
  for (const [, , , rule] of isObject(config) ? listRules(config) : []) {
    if (isObject(rule) && isObject(rule.Attribute) && rule.Attribute.Source === 'User' && typeof rule.Attribute.Path === 'string') {
      names.add(rule.Attribute.Path);
    }
    if (isObject(rule) && typeof rule.Path === 'string') {
      names.add(rule.Path);
    }
  }
  return [...names].sort();
}
