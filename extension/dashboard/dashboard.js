// MatrixDSL Lab - dashboard logic.
// Owner: Shreyas Mandem (24BCE0381)
//
// Talks to tools/mtxserve. Served by mtxserve itself the page uses its own
// origin; opened from the Chrome extension it calls http://127.0.0.1:8765.
"use strict";

const API = location.protocol.startsWith("http") ? "" : "http://127.0.0.1:8765";
const $ = (s) => document.querySelector(s);
const fmt = (n) => Number(n).toLocaleString("en-US");
const bytes = (n) => n >= 1048576 ? (n / 1048576).toFixed(2) + " MiB" : n >= 1024 ? (n / 1024).toFixed(1) + " KiB" : n + " B";
const x2 = (v) => (Number.isFinite(v) ? v.toFixed(2) : "-") + "×";
const esc = (s) => String(s).replace(/&/g, "&amp;").replace(/</g, "&lt;").replace(/>/g, "&gt;");
const reduceMotion = window.matchMedia && matchMedia("(prefers-reduced-motion: reduce)").matches;

let programs = [];
let last = null;

async function api(path, init) {
  const r = await fetch(API + path, init);
  if (!r.ok) throw new Error(path + ": HTTP " + r.status);
  return r.json();
}

/* ---------------- connection ---------------- */
function setStatus(on, text) {
  const s = $("#status");
  s.className = "status " + (on ? "on" : "off");
  s.querySelector("span").textContent = text;
  $("#offline").hidden = on;
  $("#app").hidden = !on;
}

async function connect() {
  try {
    const h = await api("/api/health");
    setStatus(true, "backend " + (API || location.host).replace("http://", "") + " · v" + h.version);
    if (!programs.length) await loadPrograms();
  } catch (e) {
    setStatus(false, "backend offline");
  }
}

async function loadPrograms() {
  programs = await api("/api/programs");
  const sel = $("#program");
  sel.innerHTML = "";
  const groups = {};
  programs.forEach((p, i) => {
    if (!groups[p.group]) {
      groups[p.group] = document.createElement("optgroup");
      groups[p.group].label = p.group;
      sel.appendChild(groups[p.group]);
    }
    const o = document.createElement("option");
    o.value = i;
    o.textContent = p.name;
    groups[p.group].appendChild(o);
  });
  const start = programs.findIndex((p) => p.name === "batched_layer");
  sel.value = start >= 0 ? start : 0;
  $("#source").value = programs[sel.value].source;
  run();
}

/* ---------------- run ---------------- */
async function run() {
  const btn = $("#run");
  btn.disabled = true;
  btn.textContent = "Compiling 4 configurations…";
  $("#diag").hidden = true;
  try {
    const res = await api("/api/run", { method: "POST", headers: { "Content-Type": "text/plain" }, body: $("#source").value });
    last = res;
    render(res);
  } catch (e) {
    $("#verdict").textContent = "Could not reach the backend: " + e.message;
    connect();
  } finally {
    btn.disabled = false;
    btn.textContent = "Compile & simulate ▸";
  }
}

function render(res) {
  const diag = $("#diag");
  if (res.diagnostics) {
    diag.textContent = res.diagnostics;
    diag.hidden = false;
  }
  if (!res.ok) {
    $("#verdict").innerHTML = "The program did not compile. See the diagnostics on the left.";
    $("#race").disabled = true;
    return;
  }
  const base = res.configs[0], ours = res.configs[3];
  const speed = base.overlap / ours.overlap, dram = base.dram / Math.max(1, ours.dram);
  const vsScalar = res.macs / ours.overlap;
  $("#verdict").innerHTML =
    "MatrixDSL runs this program <span class='x'>" + x2(speed) + "</span> faster than a traditional compile and moves <span class='x'>" +
    x2(dram) + "</span> less DRAM traffic" + (res.macs ? ". It needs <span class='x'>" + x2(vsScalar) + "</span> fewer cycles than a scalar core's lower bound." : ".") +
    (res.configs.every((c) => c.verified) ? " <b>All four versions verified.</b>" : " <span class='bad'>Verification failed - see the table.</span>");

  $("#c-scalar").textContent = fmt(res.macs);
  $("#c-macs").textContent = fmt(res.macs);
  $("#c-base").textContent = fmt(base.overlap);
  $("#c-base-dram").textContent = bytes(base.dram);
  $("#c-base-instr").textContent = fmt(base.instr);
  $("#c-base-ok").innerHTML = base.verified ? "<span class='ok'>verified</span>" : "<span class='bad'>mismatch</span>";
  $("#c-ours").textContent = fmt(ours.overlap);
  $("#c-ours-dram").textContent = bytes(ours.dram);
  $("#c-ours-instr").textContent = fmt(ours.instr);
  $("#c-ours-ok").innerHTML = ours.verified ? "<span class='ok'>verified</span>" : "<span class='bad'>mismatch</span>";

  renderTable(res);
  renderTimelines(res);
  renderOutputs(res);
  renderInside(res);
  buildRace(res);
  $("#race").disabled = false;
  playRace();
}

function renderTable(res) {
  const base = res.configs[0];
  const label = { baseline: "Traditional (baseline)", fusion: "Fusion only", tiling: "Scratchpad tiling only", "fusion+tiling": "MatrixDSL (fusion + tiling)" };
  let h = "<tr><th>Configuration</th><th>Instructions</th><th>DRAM</th><th>Scratchpad</th><th>Cycles (seq)</th><th>Cycles (overlap)</th><th>Speed-up</th><th>Fused</th><th>Staged</th><th>Output</th></tr>";
  res.configs.forEach((c, i) => {
    h += "<tr class='" + (i === 3 ? "best" : "") + "'><td>" + label[c.name] + "</td><td>" + fmt(c.instr) + "</td><td>" + bytes(c.dram) +
      "</td><td>" + bytes(c.scratch) + "</td><td>" + fmt(c.seq) + "</td><td>" + fmt(c.overlap) + "</td><td>" + x2(base.overlap / c.overlap) +
      "</td><td>" + c.fused + "</td><td>" + c.staged + "</td><td>" + (c.verified ? "<span class='ok'>✓ verified</span>" : "<span class='bad'>✗ " + esc(c.mismatch || c.error) + "</span>") + "</td></tr>";
  });
  $("#cfgTable").innerHTML = h;
}

/* ---------------- race ---------------- */
let raceRows = [];
function buildRace(res) {
  const rows = [
    { cls: "", name: "Scalar core (bound)", v: res.macs },
    { cls: "trad", name: "Traditional", v: res.configs[0].overlap },
    { cls: "ours", name: "MatrixDSL", v: res.configs[3].overlap },
  ].filter((r) => r.v > 0);
  const max = Math.max(...rows.map((r) => r.v));
  $("#raceRows").innerHTML = rows.map((r) =>
    "<div class='rrow " + r.cls + "'><span class='nm'>" + r.name + "</span><div class='rtrack'><div class='rfill'></div></div><span class='rval'>0</span></div>").join("");
  raceRows = rows.map((r, i) => ({ ...r, el: $("#raceRows").children[i], frac: r.v / max }));
}
let raceFrame = 0;
function playRace() {
  cancelAnimationFrame(raceFrame);
  const total = reduceMotion ? 1 : 6000;
  const t0 = performance.now();
  raceRows.forEach((r) => r.el.classList.remove("done"));
  const tick = (now) => {
    const t = Math.min(1, (now - t0) / total);
    let running = false;
    raceRows.forEach((r) => {
      const p = Math.min(1, t / r.frac);
      r.el.querySelector(".rfill").style.width = (p * r.frac * 100) + "%";
      r.el.querySelector(".rval").textContent = fmt(Math.round(p * r.v)) + " cyc";
      if (p >= 1) r.el.classList.add("done"); else running = true;
    });
    if (running) raceFrame = requestAnimationFrame(tick);
  };
  raceFrame = requestAnimationFrame(tick);
}

/* ---------------- timelines ---------------- */
function css(name) { return getComputedStyle(document.documentElement).getPropertyValue(name).trim(); }
function drawTimeline(canvas, cfg, axisCycles) {
  const dpr = window.devicePixelRatio || 1;
  const w = canvas.clientWidth, h = canvas.clientHeight;
  canvas.width = w * dpr; canvas.height = h * dpr;
  const g = canvas.getContext("2d");
  g.scale(dpr, dpr);
  g.clearRect(0, 0, w, h);
  const colors = [css("--amber"), css("--green2"), css("--slate")];
  let x = 0;
  const total = cfg.timeline.reduce((a, r) => a + r[1], 0);
  const scale = (w * (total / axisCycles)) / Math.max(1, total);
  cfg.timeline.forEach(([k, c]) => {
    const wd = c * scale;
    g.fillStyle = colors[k] || colors[2];
    g.fillRect(x, 4, Math.max(wd, 0.6), h - 8);
    x += wd;
  });
  g.font = "600 11px " + css("--mono");
  const label = fmt(total) + " cycles (sequential)";
  const tw = g.measureText(label).width;
  const lx = x + 8 + tw <= w ? x + 8 : w - tw - 8;
  if (lx < x) { // over the bar: give the label its own backing
    g.fillStyle = css("--surface");
    g.fillRect(lx - 4, h / 2 - 9, tw + 8, 18);
  }
  g.fillStyle = css("--ink2");
  g.fillText(label, lx, h / 2 + 4);
}
function renderTimelines(res) {
  const axis = res.configs[0].seq;
  drawTimeline($("#tl-base"), res.configs[0], axis);
  drawTimeline($("#tl-ours"), res.configs[3], axis);
}

/* ---------------- outputs ---------------- */
function renderOutputs(res) {
  const box = $("#outputs");
  box.innerHTML = "";
  $("#outNote").textContent = res.prints.length ? "computed on the MDT simulator · red < 0 < green" : "";
  res.prints.forEach((p) => {
    const rows = Math.min(p.rows, 32), cols = Math.min(p.cols, 32);
    let max = 1e-9;
    for (let i = 0; i < rows; i++) for (let j = 0; j < cols; j++) max = Math.max(max, Math.abs(p.values[i * p.cols + j] || 0));
    const div = document.createElement("div");
    div.className = "mat";
    const small = cols > 12;
    let cells = "";
    for (let i = 0; i < rows; i++)
      for (let j = 0; j < cols; j++) {
        const v = p.values[i * p.cols + j] || 0, a = Math.min(1, Math.abs(v) / max);
        const col = v >= 0 ? "rgba(46,107,80," + (0.08 + 0.92 * a) + ")" : "rgba(163,58,42," + (0.08 + 0.92 * a) + ")";
        cells += "<span title='[" + i + "][" + j + "] = " + v + "' style='background:" + col + "'></span>";
      }
    div.innerHTML = "<span class='nm'>" + esc(p.name) + " <span class='muted small'>" + (p.batch > 1 ? p.batch + "×" : "") + p.rows + "×" + p.cols +
      (p.batch > 1 ? " · batch 0 shown" : "") + "</span></span><div class='grid " + (small ? "small" : "") + "' style='grid-template-columns:repeat(" + cols + ",auto)'>" + cells + "</div>";
    box.appendChild(div);
  });
}

/* ---------------- inside ---------------- */
function hl(text, kind) {
  return text.split("\n").map((line) => {
    let c = "";
    const semi = line.indexOf(";");
    if (semi >= 0) { c = "<span class='c'>" + esc(line.slice(semi)) + "</span>"; line = line.slice(0, semi); }
    let s = esc(line);
    if (kind === "asm") s = s.replace(/\b([A-Z][A-Z.]{1,})\b/g, "<span class='k'>$1</span>").replace(/\b([RMV]\d+)\b/g, "<span class='r'>$1</span>");
    else s = s.replace(/(%\d+)/g, "<span class='r'>$1</span>").replace(/= ([A-Za-z]+)/g, "= <span class='k'>$1</span>");
    return s + c;
  }).join("\n");
}
function renderInside(res) {
  $("#irBefore").innerHTML = hl(res.irBefore, "ir");
  $("#irAfter").innerHTML = hl(res.configs[3].ir || "", "ir");
  $("#asmBase").innerHTML = hl(res.configs[0].asm || "", "asm");
  $("#asmOurs").innerHTML = hl(res.configs[3].asm || "", "asm");
  $("#asmBaseN").textContent = "(" + fmt(res.configs[0].static) + " instructions" + (res.configs[0].asmLines > 400 ? ", first 400 lines" : "") + ")";
  $("#asmOursN").textContent = "(" + fmt(res.configs[3].static) + " instructions" + (res.configs[3].asmLines > 400 ? ", first 400 lines" : "") + ")";
}

/* ---------------- suite ---------------- */
async function runSuite() {
  const btn = $("#suiteRun");
  btn.disabled = true;
  btn.textContent = "Compiling 32 programs…";
  try {
    const suite = await api("/api/suite");
    renderSuite(suite);
  } catch (e) {
    $("#geo").textContent = "Backend error: " + e.message;
  } finally {
    btn.disabled = false;
    btn.textContent = "Run again ▸";
  }
}
function renderSuite(suite) {
  const names = ["fusion", "tiling", "fusion+tiling"];
  const colors = ["#b9b4a6", css("--amber"), css("--green2")];
  const geo = names.map((_, k) => Math.exp(suite.reduce((a, w) => a + Math.log(w.configs[0].overlap / w.configs[k + 1].overlap), 0) / suite.length));
  const geoD = names.map((_, k) => Math.exp(suite.reduce((a, w) => a + Math.log(w.configs[0].dram / Math.max(1, w.configs[k + 1].dram)), 0) / suite.length));
  const allOk = suite.every((w) => w.configs.every((c) => c.verified));
  $("#geo").innerHTML = names.map((n, k) => "<div><span>" + n + " · geo-mean</span><b>" + x2(geo[k]) + "</b><span>faster · " + x2(geoD[k]) + " less DRAM</span></div>").join("");

  const W = 900, H = 360, L = 50, B = 300, T = 30, maxV = Math.max(3.2, ...suite.flatMap((w) => w.configs.map((c) => w.configs[0].overlap / c.overlap)));
  const sy = (v) => B - (v / maxV) * (B - T);
  const gw = (W - L - 10) / suite.length;
  let svg = "<svg viewBox='0 0 " + W + " " + H + "' role='img' aria-label='Speed-up per workload'>";
  for (let v = 0; v <= maxV; v += 0.5) svg += "<line x1='" + L + "' x2='" + (W - 10) + "' y1='" + sy(v) + "' y2='" + sy(v) + "' stroke='" + css("--line") + "'/><text x='" + (L - 6) + "' y='" + (sy(v) + 4) + "' text-anchor='end'>" + v.toFixed(1) + "×</text>";
  suite.forEach((w, i) => {
    const x0 = L + i * gw + gw * 0.15, bw = (gw * 0.7) / 3;
    names.forEach((_, k) => {
      const v = w.configs[0].overlap / w.configs[k + 1].overlap;
      svg += "<rect x='" + (x0 + k * bw) + "' y='" + sy(v) + "' width='" + (bw - 2) + "' height='" + (B - sy(v)) + "' fill='" + colors[k] + "'><title>" + w.name + " · " + names[k] + ": " + v.toFixed(2) + "×</title></rect>";
    });
    const best = w.configs[0].overlap / w.configs[3].overlap;
    svg += "<text x='" + (x0 + 2.5 * bw) + "' y='" + (sy(best) - 5) + "' text-anchor='middle'>" + best.toFixed(2) + "×</text>";
    svg += "<text x='" + (L + i * gw + gw / 2) + "' y='" + (B + 18) + "' text-anchor='middle'>" + w.name + "</text>";
  });
  names.forEach((n, k) => { svg += "<rect x='" + (L + 10 + k * 170) + "' y='" + (H - 24) + "' width='12' height='12' fill='" + colors[k] + "'/><text x='" + (L + 28 + k * 170) + "' y='" + (H - 14) + "'>" + n + "</text>"; });
  svg += "<text x='" + (W - 10) + "' y='" + (H - 14) + "' text-anchor='end'>" + (allOk ? "all 32 runs verified ✓" : "verification FAILED") + "</text></svg>";
  $("#suiteChart").innerHTML = svg;

  let h = "<tr><th>Workload</th><th>Traditional cycles</th><th>MatrixDSL cycles</th><th>Speed-up</th><th>Traditional DRAM</th><th>MatrixDSL DRAM</th><th>DRAM reduction</th><th>Verified</th></tr>";
  suite.forEach((w) => {
    const b = w.configs[0], o = w.configs[3];
    h += "<tr><td>" + w.name + "</td><td>" + fmt(b.overlap) + "</td><td>" + fmt(o.overlap) + "</td><td>" + x2(b.overlap / o.overlap) + "</td><td>" + bytes(b.dram) + "</td><td>" + bytes(o.dram) +
      "</td><td>" + x2(b.dram / Math.max(1, o.dram)) + "</td><td>" + (w.configs.every((c) => c.verified) ? "<span class='ok'>✓</span>" : "<span class='bad'>✗</span>") + "</td></tr>";
  });
  $("#suiteTable").innerHTML = h;
}

/* ---------------- wiring ---------------- */
document.querySelectorAll(".tab").forEach((t) => t.addEventListener("click", () => {
  document.querySelectorAll(".tab").forEach((x) => x.setAttribute("aria-selected", x === t ? "true" : "false"));
  document.querySelectorAll(".view").forEach((v) => { v.hidden = v.id !== "view-" + t.dataset.view; });
  if (t.dataset.view === "compare" && last) renderTimelines(last);
}));
$("#program").addEventListener("change", (e) => { $("#source").value = programs[e.target.value].source; run(); });
$("#run").addEventListener("click", run);
$("#race").addEventListener("click", playRace);
$("#suiteRun").addEventListener("click", runSuite);
$("#retry").addEventListener("click", connect);
$("#source").addEventListener("keydown", (e) => { if ((e.ctrlKey || e.metaKey) && e.key === "Enter") { e.preventDefault(); run(); } });
window.addEventListener("resize", () => { if (last && !$("#view-compare").hidden) renderTimelines(last); });
connect();
