// Genera data/shaders.json a partir de shaders.js del dock HTML (obs-shader-dock), para que
// los dos docks reconozcan los mismos shaders.
// Uso: node tools/manifest.mjs <ruta a shaders.js> [salida]
import { readFileSync, writeFileSync } from 'node:fs';
import vm from 'node:vm';

const [src, out = new URL('../data/shaders.json', import.meta.url)] = process.argv.slice(2);
if (!src) { console.error('Uso: node tools/manifest.mjs <shaders.js> [salida]'); process.exit(1); }
const ctx = { window: {} };
vm.runInNewContext(readFileSync(src, 'utf8'), ctx);
const list = ctx.window.SHADER_MANIFEST;
if (!Array.isArray(list)) { console.error('shaders.js no define window.SHADER_MANIFEST'); process.exit(1); }
const clean = list.map((e) => {
  const o = { repo: e.repo, label: e.label, label_en: e.label_en || e.label, type: e.type };
  if (e.duration) o.duration = e.duration;
  if (e.files) o.files = e.files;
  if (e.kinds) o.kinds = e.kinds;
  return o;
});
writeFileSync(out, JSON.stringify(clean, null, 2) + '\n');
console.log(`${clean.length} entradas`);
