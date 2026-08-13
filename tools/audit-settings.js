#!/usr/bin/env node
// Walks every Clay config item to the C setter that will actually run it:
//   phone value -> wire key -> the key the watch reads -> handler in comm.c
// Run after touching config.js, comm.c or package.json messageKeys:
//   node tools/audit-settings.js
// A '!' row means a setting silently does nothing on the watch.
// Every config item: phone value -> wire key -> the C setter that will run.
// Catches both key drift and "declared in Clay but never parsed on the watch".
const M = require('module'), path = require('path'), fs = require('fs');
const ROOT = '/Users/jaredwuerzburger/Documents/GitHub/TouchyWeather - Watchface';
const oL = M._load;
M._load = function (r) {
  if (r === 'message_keys') return require(path.join(ROOT, 'build/js/message_keys.json'));
  return oL.apply(this, arguments);
};
global.Pebble = { platform: 'pypkjs', addEventListener() {} };
const st = {};
global.localStorage = { getItem: k => (k in st ? st[k] : null), setItem: (k, v) => { st[k] = String(v); }, removeItem: k => { delete st[k]; }, clear() {} };

const Clay = require(path.join(ROOT, 'node_modules/@rebble/clay/dist/js/index.js'));
const config = require(path.join(ROOT, 'src/pkjs/config.js'));
const clay = new Clay(config, require(path.join(ROOT, 'src/pkjs/face_preview.js')), { autoHandleEvents: false });

const declared = [];
(function walk(items) { items.forEach(i => { if (i.messageKey) declared.push(i); if (i.items) walk(i.items); }); })(config);

// A distinct non-default value per item, so a mis-routed value is visible.
const probe = {};
declared.forEach(it => {
  if (it.type === 'toggle') probe[it.messageKey] = true;
  else if (it.type === 'input') probe[it.messageKey] = '12.34,-56.78';
  else if (it.options) {
    const opts = it.options.map(o => o.value);
    probe[it.messageKey] = opts[opts.length - 1];   // last option = clearly not the default
  } else probe[it.messageKey] = '1';
});

const msg = clay.getSettings(encodeURIComponent(JSON.stringify(probe)));
const appKeys = JSON.parse(fs.readFileSync(path.join(ROOT, 'build/appinfo.json'), 'utf8')).appKeys;
const rev = {}; for (const [n, id] of Object.entries(appKeys)) rev[id] = n;

// Which keys comm.c actually parses, and into what.
const comm = fs.readFileSync(path.join(ROOT, 'src/c/comm.c'), 'utf8');
const parsed = {};
const re = /dict_find\(iter,\s*MESSAGE_KEY_(\w+)\)\)\)\s*\{\s*(?:[^}]*?)(settings_set_\w+|theme_set|d->\w+)/g;
let m; while ((m = re.exec(comm))) parsed[m[1]] = m[2];
// The page toggles go through a table-driven loop, not an inline dict_find.
comm.replace(/\{\s*MESSAGE_KEY_(PageEnabled\w+),/g, (_, k) => { parsed[k] = 'settings_set_page_enabled'; return _; });
// Three settings are deliberately phone-side only: PKJS consumes them into
// localStorage to build the weather request, and the watch never needs them.
// WindSpeedUnit is the subtle one — the watch DOES render wind units, but from
// `WindUnits`, an integer index.js derives from this picker. Auditing it as a
// missing C handler is a false positive; the pairing is documented at the
// WindUnits handler in comm.c.
const pkjs = fs.readFileSync(path.join(ROOT, 'src/pkjs/index.js'), 'utf8');
['TimeFormat', 'LocationOverride', 'WindSpeedUnit'].forEach(k => {
  if (pkjs.includes('dict.' + k)) parsed[k] = '(phone-side: index.js localStorage)';
});

let bad = 0;
console.log('setting              value  ->  key   -> watch key      -> C handler');
console.log('-'.repeat(88));
declared.forEach(it => {
  const k = it.messageKey;
  const id = appKeys[k];
  const sent = msg[id];
  const lands = rev[id];
  const handler = parsed[lands] || (lands === k ? '(none — NOT PARSED)' : '(none)');
  const ok = lands === k && sent !== undefined && parsed[lands];
  if (!ok) bad++;
  console.log(`${ok ? ' ' : '!'} ${k.padEnd(20)} ${String(sent).padEnd(6)} ${String(id).padEnd(6)} ${String(lands).padEnd(15)} ${handler}`);
});
console.log(`\n${declared.length} config items, ${bad} problem(s)`);
