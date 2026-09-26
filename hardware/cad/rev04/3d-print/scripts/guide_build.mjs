/**
 * Build the editable 22-slide manual with @oai/artifact-tool.
 * Final media/evidence is mandatory. This script never controls Fusion.
 *
 * Run with the bundled Node executable. RUNTIME_NODE_MODULES and SKILL_DIR
 * can override the documented local defaults for another contributor.
 */
import fs from "node:fs/promises";
import path from "node:path";
import crypto from "node:crypto";
import { fileURLToPath, pathToFileURL } from "node:url";
import { createRequire } from "node:module";

const RELEASE = path.resolve(path.dirname(fileURLToPath(import.meta.url)), "..");
const DOCS = path.join(RELEASE, "docs");
const SOURCE = path.join(DOCS, "source");
// The presentation finalizer requires private receipts outside the final-output
// directory. Keep all temporary guide work under this owned script prefix.
const BUILD = path.join(RELEASE, "scripts/guide-build");
const REPOSITORY = path.resolve(RELEASE, "../../../..");
const RUNTIME = process.env.GUIDE_RUNTIME ?? "/Users/magnustrandokken/.cache/codex-runtimes/codex-primary-runtime/dependencies";
const MODULES = process.env.RUNTIME_NODE_MODULES ?? path.join(RUNTIME, "node/node_modules");
// The finalizer's isolated first-party import uses this explicit runtime path.
process.env.RUNTIME_NODE_MODULES = MODULES;
const SKILL = process.env.SKILL_DIR ?? "/Users/magnustrandokken/.codex/plugins/cache/openai-primary-runtime/presentations/26.904.11930/skills/presentations";
const localRequire = createRequire(path.join(MODULES, "guide-loader.cjs"));
const { Presentation, PresentationFile } = await import(pathToFileURL(localRequire.resolve("@oai/artifact-tool")).href);
const { resolvePresentationFont, finalizePresentation } = await import(pathToFileURL(path.join(SKILL, "container_tools/artifact_tool_utils.mjs")).href);
const FAMILY = resolvePresentationFont({ fontFamily: "Arial" });
const W = 1280, H = 720;
const C = { ink: "#193342", body: "#344A58", accent: "#14756C", muted: "#62737C", rule: "#CCD7DB", white: "#FFFFFF", pale: "#EDF3F3" };

const source = JSON.parse(await fs.readFile(path.join(SOURCE, "guide.json"), "utf8"));
const release = JSON.parse(await fs.readFile(path.join(SOURCE, "release.json"), "utf8"));
if (source.slides.length !== 22 || source.slide_count !== 22) throw new Error("Exactly 22 slides required.");
if (release.status !== "ready_for_documentation" || !release.final_native_bom_reconciled || !release.print_manifest_reconciled) {
  throw new Error("Final revised CAD, BOM and print manifest have not been reconciled.");
}
if (!Object.keys(release.evidence ?? {}).length) throw new Error("Final release evidence is required.");
for (const [name, relPath] of Object.entries(release.evidence)) {
  if (typeof relPath !== "string") throw new Error("Evidence path required: " + name);
  await fs.access(path.resolve(RELEASE, relPath));
}
function substitute(value) {
  if (typeof value === "string") return value.replace(/\{\{([a-z0-9_]+)\}\}/g, (_, key) => {
    if (!(key in release.facts)) throw new Error("Missing release fact: " + key);
    return String(release.facts[key]);
  });
  if (Array.isArray(value)) return value.map(substitute);
  if (value && typeof value === "object") return Object.fromEntries(Object.entries(value).map(([k, v]) => [k, substitute(v)]));
  return value;
}
const slides = source.slides.map(substitute);
const requiredAssets = [...new Set(slides.map(s => s.image).filter(Boolean))];
const assetBytes = new Map();
const assetHashes = {};
for (const name of requiredAssets) {
  const asset = release.assets[name];
  if (!asset?.reviewed || asset.provenance !== "actual Fusion export") throw new Error("Final reviewed Fusion view required: " + name);
  const components = asset.children ?? [asset];
  for (const [index, child] of components.entries()) {
    if (!child.path) throw new Error("Actual Fusion image path required: " + name);
    const key = asset.children ? name + ":" + index : name;
    const bytes = await fs.readFile(path.resolve(RELEASE, child.path));
    assetBytes.set(key, bytes);
    assetHashes[key] = { path: child.path, sha256: crypto.createHash("sha256").update(bytes).digest("hex") };
  }
}
if (new Set(Object.values(assetHashes).map(a => a.sha256)).size !== Object.keys(assetHashes).length) {
  throw new Error("Distinct guide image subjects must use distinct final views.");
}
await fs.mkdir(BUILD, { recursive: true });
await fs.mkdir(DOCS, { recursive: true });
const presentation = Presentation.create({ slideSize: { width: W, height: H } });
const tableSlides = [];

function textbox(slide, value, x, y, w, h, size = 23, color = C.body, bold = false) {
  const box = slide.shapes.add({ geometry: "textbox", position: { left: x, top: y, width: w, height: h },
    fill: "none", line: { fill: "none", width: 0 } });
  box.text = value;
  box.text.style = { typeface: FAMILY, fontSize: size, bold, color, autoFit: "none",
    insets: { left: 0, right: 0, top: 0, bottom: 0 }, verticalAlignment: "top", wrap: "square" };
  return box;
}
function paragraphs(slide, items, x, y, w, h, size = 22, afterPoints = 11) {
  const value = items.map(text => ({ runs: [{ run: text }], spaceAfter: afterPoints * 100 }));
  return textbox(slide, value, x, y, w, h, size);
}
function image(slide, key, x, y, w, h) {
  const spec = release.assets[key];
  const components = spec.children ?? [spec];
  return components.map((child, index) => {
    const id = spec.children ? key + ":" + index : key;
    const frame = child.frame ?? [0, 0, 1, 1];
    const left = x + frame[0] * w, top = y + frame[1] * h;
    const width = frame[2] * w, height = frame[3] * h;
    if (child.label) textbox(slide, child.label, left, top, width, 27, 19, C.ink, true);
    return slide.images.add({ blob: new Uint8Array(assetBytes.get(id)), contentType: "image/png",
      alt: (child.label ?? key.replaceAll("_", " ")) + ", actual Fusion view of the revised A3 enclosure",
      fit: "contain", ...(child.crop ? { crop: child.crop } : {}),
      position: { left, top: top + (child.label ? 30 : 0), width, height: height - (child.label ? 30 : 0) } });
  });
}
function table(slide, spec, frame, fontSize = 20) {
  const values = [spec.headers, ...spec.rows];
  let proportions;
  if (values[0].length === 2) proportions = [0.36, 0.64];
  else if (values[0].length === 3) proportions = spec.headers[0] === "ID" ? [0.14, 0.75, 0.11] : [0.22, 0.49, 0.29];
  else proportions = [0.09, 0.37, 0.08, 0.46];
  const t = slide.tables.add({ rows: values.length, columns: values[0].length,
    left: frame.x, top: frame.y, width: frame.w, height: frame.h,
    columnWidths: proportions.map(p => p * frame.w), values });
  t.styleOptions = { headerRow: true, bandedRows: false };
  t.borders.assign({ style: "solid", fill: C.rule, width: 0.7 });
  for (let r = 0; r < values.length; r++) {
    t.rows[r].height = frame.h / values.length;
    for (let c = 0; c < values[0].length; c++) {
      const cell = t.getCell(r, c);
      cell.fill = r === 0 ? C.pale : C.white;
      cell.text.style = { typeface: FAMILY, fontSize, bold: r === 0,
        color: r === 0 ? C.ink : C.body, verticalAlignment: "middle",
        insets: { left: 10, right: 10, top: 5, bottom: 5 }, autoFit: "none" };
    }
  }
  return t;
}
function sourceUri(value) {
  if (/^https?:\/\//.test(value)) return value;
  return path.relative(DOCS, path.resolve(REPOSITORY, value)).split(path.sep).join("/");
}
const sourceLabels = {
  repo: "Project repository", upstream: "Original Trimix Analyzer", gct: "GCT USB4720 drawing",
  ao2: "Honeywell AO2", md62: "Winsen MD62", co: "Winsen ZE07-CO",
  bme: "Bosch BME280", electrical: "Electrical design", usb: "USB charging notes",
  mechanical: "Verification record", measurements: "Measurement checklist", provenance: "Sensor provenance",
  printing: "Print preparation", fit_record: "Physical fit record",
};
function footer(slide, item, number) {
  const noteBox = textbox(slide, item.note, 64, 627, 1120, 51, 16, C.muted);
  if (item.note.includes("EXPORT_NOTES.md")) noteBox.text.get("EXPORT_NOTES.md").link = { uri: "EXPORT_NOTES.md", isExternal: true };
  const runs = [{ run: "Sources: " }];
  item.refs.forEach((key, index) => {
    if (index) runs.push({ run: ", " });
    runs.push({ run: sourceLabels[key] ?? key,
      textStyle: { color: C.accent, underline: "sng" },
      link: { uri: sourceUri(source.sources[key] ?? key), isExternal: true } });
  });
  textbox(slide, [{ runs }], 64, 689, 1050, 24, 13, C.muted);
  textbox(slide, String(number).padStart(2, "0") + " / 22", 1152, 690, 80, 20, 14, C.muted);
}

for (const [index, item] of slides.entries()) {
  const slide = presentation.slides.add();
  slide.background.fill = C.white;
  if (item.layout === "cover") {
    textbox(slide, item.title, 64, 104, 625, 145, 68, C.ink, true);
    textbox(slide, item.lead, 64, 250, 540, 120, 32, C.accent);
    paragraphs(slide, item.body, 64, 416, 540, 170, 22);
    image(slide, item.image, 675, 54, 547, 550);
  } else {
    textbox(slide, item.title, 64, 43, 1152, 58, 42, C.ink, true);
    textbox(slide, item.lead, 64, 109, 1152, 48, 24, C.accent);
    if (item.layout === "gallery") {
      image(slide, item.image, 64, 172, 1152, 348);
      paragraphs(slide, item.body, 64, 546, 1152, 75, 20);
    } else if (item.layout === "table") {
      tableSlides.push(index + 1);
      const withBody = item.body.length > 0;
      const height = withBody ? 300 : 436;
      table(slide, item.table, { x: 64, y: 174, w: 1152, h: height },
        item.table.rows.length > 8 ? 19 : 20);
      if (withBody) paragraphs(slide, item.body, 64, 490, 1152, 126, 20, 5);
    } else if (item.layout === "overview") {
      paragraphs(slide, item.body, 64, 186, 534, 425, 23);
      tableSlides.push(index + 1);
      table(slide, item.table, { x: 652, y: 190, w: 564, h: 380 }, 22);
    } else if (item.layout === "text") {
      const textBody = paragraphs(slide, item.body, 64, 188, 1128, 407, 27);
      if (index === 21) {
        const targets = [["github.com/magnus188/aegir", source.sources.repo],
          ["License details", source.sources.license]];
        for (const [label, uri] of targets) textBody.text.get(label).link = { uri, isExternal: true };
      }
    } else {
      paragraphs(slide, item.body, 64, 183, 512, 427, item.body.length > 4 ? 21 : 22);
      image(slide, item.image, 620, 175, 596, 432);
    }
  }
  footer(slide, item, index + 1);
  const references = item.refs.map(key => source.sources[key] ?? key);
  const imageSource = item.image ? (release.assets[item.image].children ?? [release.assets[item.image]])
    .map(a => "Actual Fusion image: " + a.path) : [];
  slide.speakerNotes.textFrame.setText([item.note, ...references, ...imageSource].join("\n\n"));
}

const version = process.env.GUIDE_OUTPUT_SUFFIX ?? "A3";
if (!/^[A-Za-z0-9_-]+$/.test(version)) throw new Error("Invalid output suffix.");
const filename = "Trimix_Enclosure_" + version + "_Community_Guide.pptx";
const candidate = path.join(BUILD, "candidate-" + version + ".pptx");
const final = path.join(DOCS, filename);
await (await PresentationFile.exportPptx(presentation)).save(candidate);
await fs.writeFile(path.join(BUILD, "asset-manifest.json"), JSON.stringify(assetHashes, null, 2));
await fs.writeFile(path.join(BUILD, "resolved-content.json"), JSON.stringify(slides, null, 2));
await finalizePresentation({
  explicitTotalSlideCount: 22, requiredNativeTableOwnerSlides: tableSlides,
  requiredNativeChartOwnerSlides: [], workspaceDir: RELEASE, candidatePath: candidate, finalPath: final,
  pythonExecutable: path.join(RUNTIME, "python/bin/python3"),
  integrityValidatorPath: path.join(SKILL, "container_tools/inspect_presentation_package_integrity.py"),
  layoutValidatorPath: path.join(SKILL, "container_tools/inspect_presentation_layout_geometry.py"),
  layoutArgs: ["--expected-slide-size-emu", "12192000,6858000", "--validate-bullet-geometry",
    "--validate-heading-fit", ...tableSlides.flatMap(n => ["--require-native-table-slide", String(n)])],
  fontPolicy: { basis: "design", families: [FAMILY] },
  verifyArtifactToolImport: true, receiptPath: path.join(BUILD, filename + ".validation.json"),
});
console.log(JSON.stringify({ final, slides: 22, native_table_slides: tableSlides, font: FAMILY }, null, 2));
