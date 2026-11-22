// Copyright 2026 Ivan Iaitckikh. MIT License, see LICENSE in the Extras folder.
//
// AWS Signature Version 4 for the requests of sync_flexmatch.mjs, with node:crypto only (no AWS SDK, no packages).
//
// Steps as in "Create a signed AWS API request" (https://docs.aws.amazon.com/IAM/latest/UserGuide/create-signed-request.html,
// checked 2026-11-18): canonical request -> string to sign -> signing key (HMAC chain over date, Region, service,
// "aws4_request") -> signature -> Authorization header. Paths are URI-encoded twice, as for every service except Amazon S3.
// The tests check the result against the "get-vanilla" example of the AWS Signature Version 4 test suite
// (https://github.com/awslabs/aws-c-auth/tree/main/tests/aws-signing-test-suite/v4/get-vanilla, the same example as in the
// older aws-sig-v4-test-suite): credentials AKIDEXAMPLE / wJalrXUtnFEMI/K7MDENG+bPxRfiCYEXAMPLEKEY, 2015-08-30T12:36:00Z,
// us-east-1, service "service".
//
// No I/O: signRequest returns the headers to send.

import { createHash, createHmac } from 'node:crypto';

export const ALGORITHM = 'AWS4-HMAC-SHA256';

const sha256Hex = (data) => createHash('sha256').update(data, 'utf8').digest('hex');
const hmac = (key, data) => createHmac('sha256', key).update(data, 'utf8').digest();

/** Percent-encoding of RFC 3986: everything except A-Z a-z 0-9 - _ . ~ is encoded. */
export function encodeRfc3986(text) {
  return encodeURIComponent(text).replace(/[!'()*]/g, (c) => `%${c.charCodeAt(0).toString(16).toUpperCase()}`);
}

/** "20150830T123600Z" of a date. */
export function toAmzDate(date) {
  return date.toISOString().replace(/[-:]/g, '').replace(/\.\d{3}/, '');
}

/**
 * The canonical URI: each segment of the path as sent (already encoded once, see encodePathSegment) encoded once more,
 * so "%3A" becomes "%253A". AWS: "Each path segment must be URI-encoded twice" for every service except Amazon S3.
 */
export function canonicalUri(pathname) {
  if (!pathname || pathname === '/') return '/';
  return pathname.split('/').map(encodeRfc3986).join('/');
}

/** One path segment of a request URL, for example a Lambda function name or ARN. */
export const encodePathSegment = encodeRfc3986;

/** The canonical query string: parameters sorted by name, then by value, both encoded. */
export function canonicalQuery(search) {
  const pairs = [...new URLSearchParams(search)].map(([key, value]) => [encodeRfc3986(key), encodeRfc3986(value)]);
  pairs.sort((a, b) => (a[0] === b[0] ? (a[1] < b[1] ? -1 : a[1] > b[1] ? 1 : 0) : a[0] < b[0] ? -1 : 1));
  return pairs.map(([key, value]) => `${key}=${value}`).join('&');
}

/**
 * Signs one request.
 * @param {object} request
 * @param {string} request.method      GET, POST, PUT...
 * @param {string} request.url         the full https URL.
 * @param {Record<string,string>} [request.headers]  headers to send and sign (Host and X-Amz-Date are added).
 * @param {string} [request.body]      the body text ('' for none).
 * @param {string} request.region
 * @param {string} request.service     for example gamelift or lambda.
 * @param {{accessKeyId: string, secretAccessKey: string, sessionToken?: string}} request.credentials
 * @param {Date} request.date
 * @returns {{headers: Record<string,string>, canonicalRequest: string, stringToSign: string, signature: string}}
 */
export function signRequest({ method, url, headers = {}, body = '', region, service, credentials, date }) {
  const target = new URL(url);
  const amzDate = toAmzDate(date);
  const day = amzDate.slice(0, 8);
  const all = { ...headers, host: target.host, 'x-amz-date': amzDate };
  if (credentials.sessionToken) all['x-amz-security-token'] = credentials.sessionToken;

  const canonical = new Map();
  for (const [name, value] of Object.entries(all)) {
    canonical.set(name.toLowerCase(), String(value).trim().replace(/\s+/g, ' '));
  }
  const names = [...canonical.keys()].sort();
  const signedHeaders = names.join(';');
  const canonicalRequest = [
    method.toUpperCase(),
    canonicalUri(target.pathname),
    canonicalQuery(target.search),
    names.map((name) => `${name}:${canonical.get(name)}\n`).join(''),
    signedHeaders,
    sha256Hex(body),
  ].join('\n');

  const scope = `${day}/${region}/${service}/aws4_request`;
  const stringToSign = [ALGORITHM, amzDate, scope, sha256Hex(canonicalRequest)].join('\n');
  const signingKey = hmac(hmac(hmac(hmac(`AWS4${credentials.secretAccessKey}`, day), region), service), 'aws4_request');
  const signature = createHmac('sha256', signingKey).update(stringToSign, 'utf8').digest('hex');

  const out = { ...headers, 'X-Amz-Date': amzDate };
  if (credentials.sessionToken) out['X-Amz-Security-Token'] = credentials.sessionToken;
  out.Authorization = `${ALGORITHM} Credential=${credentials.accessKeyId}/${scope}, SignedHeaders=${signedHeaders}, Signature=${signature}`;
  return { headers: out, canonicalRequest, stringToSign, signature };
}
