#!/usr/bin/env node
// Guard against the silent killer of this codebase's config pipeline:
// messageKeys are numbered POSITIONALLY from package.json, so inserting or
// removing one renumbers every key after it — and `pebble build` does not
// always regenerate build/js/message_keys.json when package.json changes.
// When the two maps drift, the phone sends each setting under the key the
// watch reads as the NEXT setting: the config still "saves", every value
// lands one slot over, and nothing logs an error. (Seen for real: the Big
// Mode toggle wrote into the Temp field, so the face showed 1°.)
//
// Run after any package.json messageKeys edit:
//   node tools/check-message-keys.js
// Exits non-zero on drift; the fix is always `rm -rf build && pebble build`.
const fs = require('fs');
const path = require('path');

const root = path.resolve(__dirname, '..');
const jsMapPath = path.join(root, 'build/js/message_keys.json');
const appInfoPath = path.join(root, 'build/appinfo.json');

for (const p of [jsMapPath, appInfoPath]) {
  if (!fs.existsSync(p)) {
    console.error(`missing ${path.relative(root, p)} — run \`pebble build\` first`);
    process.exit(2);
  }
}

const jsMap = JSON.parse(fs.readFileSync(jsMapPath, 'utf8'));
const appKeys = JSON.parse(fs.readFileSync(appInfoPath, 'utf8')).appKeys;
const declared = JSON.parse(
    fs.readFileSync(path.join(root, 'package.json'), 'utf8')
).pebble.messageKeys;

// Both build artifacts can be stale TOGETHER (they regenerate on the same
// build), so agreeing with each other is not enough — check them against the
// source of truth. Keys are numbered 10000 + position in package.json.
const stale = declared.filter((name, i) => appKeys[name] !== 10000 + i);
if (stale.length || declared.length !== Object.keys(appKeys).length) {
  console.error('build is STALE vs package.json messageKeys ' +
                `(${declared.length} declared, ${Object.keys(appKeys).length} built).`);
  for (const name of stale.slice(0, 10)) {
    console.error(`  ${name}: expected ${10000 + declared.indexOf(name)}, ` +
                  `built ${appKeys[name]}`);
  }
  console.error('\nFix: rm -rf build && pebble build');
  process.exit(1);
}

const drift = [];
for (const name of Object.keys(jsMap)) {
  if (jsMap[name] !== appKeys[name]) {
    drift.push({ name, phone: jsMap[name], watch: appKeys[name] });
  }
}
for (const name of Object.keys(appKeys)) {
  if (!(name in jsMap)) drift.push({ name, phone: undefined, watch: appKeys[name] });
}

if (!drift.length) {
  console.log(`message keys agree (${Object.keys(jsMap).length} keys)`);
  process.exit(0);
}

const reverse = {};
for (const [name, id] of Object.entries(appKeys)) reverse[id] = name;

console.error(`message key DRIFT — ${drift.length} key(s) disagree.`);
console.error('The phone would send each setting to the wrong watch-side key:\n');
for (const d of drift.slice(0, 15)) {
  const lands = reverse[d.phone] || '(unmapped — silently dropped)';
  console.error(`  ${d.name}: phone sends ${d.phone}, watch reads ${d.watch} ` +
                `-> value applied to ${lands}`);
}
if (drift.length > 15) console.error(`  ... and ${drift.length - 15} more`);
console.error('\nFix: rm -rf build && pebble build');
process.exit(1);
