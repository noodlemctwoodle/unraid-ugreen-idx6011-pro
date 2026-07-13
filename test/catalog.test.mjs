#!/usr/bin/env node
/*
 * plugin/test/catalog.test.mjs
 *
 * Regression test for the dashboard module catalog + editor picker contract.
 *
 * The web layout editor (src/webgui/idxlayout.js) renders, per module row:
 *   - a NAME picker  for every INDEXED ("pick one") module, and
 *   - a STYLE picker whenever a module advertises more than one variant.
 * So a module must carry the variants that make those pickers appear. This test
 * reads the catalog emitted by `panel_dash --modules` and asserts the invariants,
 * so a "pick one" card can never silently lose its style picker (the regression
 * that prompted this test) and no module ends up without a picker where it should
 * have one.
 *
 * Run:  node test/catalog.test.mjs               (builds/uses src/panel/panel_dash)
 *       PANEL_DASH=/path/to/panel_dash node test/catalog.test.mjs
 *       node test/catalog.test.mjs catalog.json  (validate a captured JSON)
 */
import { execFileSync } from 'node:child_process';
import { readFileSync } from 'node:fs';

function loadCatalog() {
  const arg = process.argv[2];
  if (arg && arg.endsWith('.json')) return JSON.parse(readFileSync(arg, 'utf8'));
  const bin = process.env.PANEL_DASH || 'src/panel/panel_dash';
  return JSON.parse(execFileSync(bin, ['--modules'], { encoding: 'utf8' }));
}

const fails = [];
const check = (cond, msg) => { if (!cond) fails.push(msg); };

const cat = loadCatalog();
check(Array.isArray(cat) && cat.length > 0, 'catalog is a non-empty array');

const ids = new Set((cat || []).map(m => m && m.id));
for (const m of cat || []) {
  const id = (m && m.id) || '(no id)';
  check(typeof m.id === 'string' && m.id.length > 0, `${id}: has an id`);
  check(typeof m.label === 'string' && m.label.length > 0, `${id}: has a label`);
  check(Array.isArray(m.variants) && m.variants.length >= 1, `${id}: has at least one variant`);

  const seen = new Set();
  for (const v of m.variants || []) {
    check(typeof v === 'string' && v.length > 0, `${id}: variant names are non-empty strings`);
    check(!seen.has(v), `${id}: variant "${v}" is duplicated`);
    seen.add(v);
  }

  const styles = (m.variants || []).length;
  if (m.indexed) {
    // A "pick one" module always shows a NAME picker; it MUST also offer a STYLE
    // picker so the visualisation is choosable (the Interface pattern). That means
    // it needs >= 2 variants — the exact rule this test exists to protect.
    check(styles >= 2,
      `${id}: indexed "pick one" module must have >= 2 variants so the editor shows a style picker (has ${styles})`);
    check(typeof m.itemsrc === 'string' && m.itemsrc.length > 0,
      `${id}: indexed module must name an itemsrc list`);
    check(!m.itemsrc || ids.has(m.itemsrc),
      `${id}: itemsrc "${m.itemsrc}" must be another module id (the all-in-one list)`);
  }
}

if (fails.length) {
  console.error(`FAIL: ${fails.length} catalog invariant(s) violated:`);
  for (const f of fails) console.error('  - ' + f);
  process.exit(1);
}
console.log(`OK: ${cat.length} modules — every indexed card exposes name + style pickers, all variants well-formed`);
