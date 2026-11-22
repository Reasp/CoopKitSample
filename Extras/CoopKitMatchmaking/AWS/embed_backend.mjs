// Copyright 2026 Ivan Iaitckikh. MIT License, see LICENSE in the Extras folder.
//
// Copies the backend code (backend/*.js) into the CloudFormation template as inline Lambda code, so the template deploys
// as one file: no build step, no S3 bucket. Every AWS::Lambda::Function with Metadata "CoopKit::Source" gets that file
// as Code.ZipFile. Run it after you change the backend code:
//
//   node embed_backend.mjs                (rewrites FlexMatchStack.json next to this script)
//   node embed_backend.mjs --check        (exit code 1 when the template is out of date)
//
// Node.js 18 or later, no packages.

import { readFileSync, writeFileSync } from 'node:fs';
import { dirname, join } from 'node:path';
import { fileURLToPath } from 'node:url';

const here = dirname(fileURLToPath(import.meta.url));
const templatePath = join(here, 'FlexMatchStack.json');
const check = process.argv.includes('--check');

const readText = (path) => readFileSync(path, 'utf8').replace(/\r\n/g, '\n');
const template = JSON.parse(readText(templatePath));
let stale = 0;
for (const [name, resource] of Object.entries(template.Resources || {})) {
  const source = resource.Metadata && resource.Metadata['CoopKit::Source'];
  if (resource.Type !== 'AWS::Lambda::Function' || !source) continue;
  const code = readText(join(here, source));
  if (resource.Properties.Code.ZipFile !== code) {
    stale += 1;
    console.log(`${check ? 'Out of date' : 'Updated'}: ${name} <- ${source}`);
    resource.Properties.Code.ZipFile = code;
  }
}

if (check) {
  process.exit(stale > 0 ? 1 : 0);
}
if (stale > 0) {
  writeFileSync(templatePath, `${JSON.stringify(template, null, 2)}\n`);
}
console.log(stale > 0 ? `FlexMatchStack.json: ${stale} function(s) updated.` : 'FlexMatchStack.json is up to date.');
