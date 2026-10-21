//===----------------------------------------------------------------------===//
//
// mtxbench - the fusion x scratchpad-tiling ablation study (Phase 3)
//
// Owner: Shreyas Mandem (24BCE0381)
//
// Compiles every workload under the four configurations of
// docs/review2/Review2_Report.md section 8, runs each on the MDT simulator,
// checks the output against the reference interpreter, and reports what the
// performance model measured. Nothing here is estimated by hand: every
// number in the tables comes from an executed, verified program.
//
// Usage: mtxbench [workload-dir] [--md results.md] [--csv results.csv]
//
//===----------------------------------------------------------------------===//

#include "driver/Pipeline.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

using namespace matrixdsl;
namespace fs = std::filesystem;

namespace {

struct Row {
  std::string workload, config;
  long long staticInstr, dynInstr, dram, scratch, seq, overlap;
  int fused, staged;
  bool correct;
};

std::string thousands(long long v) {
  std::string s = std::to_string(v), out;
  int n = 0;
  for (auto it = s.rbegin(); it != s.rend(); ++it) {
    if (n && n % 3 == 0 && *it != '-')
      out.insert(out.begin(), ',');
    out.insert(out.begin(), *it);
    ++n;
  }
  return out;
}

std::string ratio(long long base, long long v) {
  char buf[32];
  std::snprintf(buf, sizeof(buf), "%.2fx", v ? static_cast<double>(base) / v : 0.0);
  return buf;
}

} // namespace

int main(int argc, char **argv) {
  std::string dir = "benchmarks/workloads", mdPath, csvPath;
  for (int i = 1; i < argc; ++i) {
    const std::string a = argv[i];
    if (a == "--md" && i + 1 < argc) mdPath = argv[++i];
    else if (a == "--csv" && i + 1 < argc) csvPath = argv[++i];
    else dir = a;
  }

  std::vector<std::string> files;
  for (const auto &e : fs::directory_iterator(dir))
    if (e.path().extension() == ".mtx")
      files.push_back(e.path().generic_string());
  std::sort(files.begin(), files.end());
  if (files.empty()) {
    std::fprintf(stderr, "mtxbench: no .mtx workloads in '%s'\n", dir.c_str());
    return 1;
  }

  std::vector<Row> rows;
  bool allCorrect = true;
  for (const std::string &file : files) {
    std::string source;
    readFile(file, source);
    const std::string name = fs::path(file).stem().string();
    std::vector<tensorir::PrintValue> expected;
    std::string error;
    if (!referenceOutput(source, expected, error)) {
      std::fprintf(stderr, "mtxbench: %s: %s\n", name.c_str(), error.c_str());
      return 1;
    }
    for (const Config &cfg : kConfigs) {
      PipelineOptions opts;
      opts.filename = file;
      opts.fusion = cfg.fusion;
      opts.tiling = cfg.tiling;
      Compilation c = compile(source, opts);
      if (!c.ok) {
        std::fprintf(stderr, "mtxbench: %s\n", c.diagnostics.c_str());
        return 1;
      }
      Execution x = simulate(c.assembly, false);
      if (!x.ok) {
        std::fprintf(stderr, "mtxbench: %s [%s]: %s\n", name.c_str(), cfg.name,
                     x.error.c_str());
        return 1;
      }
      std::string report;
      const bool ok = tensorir::samePrints(expected, x.prints, 1e-5, report);
      allCorrect &= ok;
      rows.push_back({name, cfg.name, c.codegen.instructions, x.perf.instructionCount,
                      x.perf.bytesMovedDRAM, x.perf.bytesMovedScratchpad,
                      x.perf.sequentialCycles, x.perf.overlapAwareCycles, c.fused,
                      static_cast<int>(c.tiling.staged.size()), ok});
      std::fprintf(stderr, "  %-18s %-14s %s\n", name.c_str(), cfg.name,
                   ok ? "verified" : report.c_str());
    }
  }

  // ---- Markdown report ----------------------------------------------------
  std::ostringstream md;
  md << "| Workload | Config | Instr (dyn) | DRAM bytes | Scratchpad bytes | "
        "Cycles (seq) | Cycles (overlap) | Speed-up | DRAM vs base | Correct |\n"
     << "|---|---|---:|---:|---:|---:|---:|---:|---:|:---:|\n";
  std::vector<double> logSpeed[4], logDram[4];
  for (size_t i = 0; i < rows.size(); i += 4) {
    const Row &base = rows[i];
    for (int k = 0; k < 4; ++k) {
      const Row &r = rows[i + k];
      md << "| " << (k == 0 ? "`" + r.workload + "`" : "") << " | " << r.config
         << " | " << thousands(r.dynInstr) << " | " << thousands(r.dram) << " | "
         << thousands(r.scratch) << " | " << thousands(r.seq) << " | "
         << thousands(r.overlap) << " | " << ratio(base.overlap, r.overlap)
         << " | " << ratio(base.dram, r.dram) << " | " << (r.correct ? "yes" : "NO")
         << " |\n";
      logSpeed[k].push_back(std::log(static_cast<double>(base.overlap) / r.overlap));
      logDram[k].push_back(std::log(static_cast<double>(base.dram) / std::max(1LL, r.dram)));
    }
  }
  md << "\n| Config | Geo-mean speed-up (overlap-aware cycles) | Geo-mean DRAM reduction |\n"
     << "|---|---:|---:|\n";
  for (int k = 0; k < 4; ++k) {
    double s = 0, d = 0;
    for (double v : logSpeed[k]) s += v;
    for (double v : logDram[k]) d += v;
    char buf[96];
    std::snprintf(buf, sizeof(buf), "| %s | %.2fx | %.2fx |\n", kConfigs[k].name,
                  std::exp(s / logSpeed[k].size()), std::exp(d / logDram[k].size()));
    md << buf;
  }

  std::printf("%s", md.str().c_str());
  std::printf("\nAll %zu runs verified against the reference interpreter: %s\n",
              rows.size(), allCorrect ? "yes" : "NO");

  if (!mdPath.empty())
    std::ofstream(mdPath) << md.str();
  if (!csvPath.empty()) {
    std::ofstream csv(csvPath);
    csv << "workload,config,static_instr,dynamic_instr,dram_bytes,scratchpad_bytes,"
           "seq_cycles,overlap_cycles,ops_fused,operands_staged,correct\n";
    for (const Row &r : rows)
      csv << r.workload << "," << r.config << "," << r.staticInstr << "," << r.dynInstr
          << "," << r.dram << "," << r.scratch << "," << r.seq << "," << r.overlap
          << "," << r.fused << "," << r.staged << "," << (r.correct ? 1 : 0) << "\n";
  }
  return allCorrect ? 0 : 1;
}
