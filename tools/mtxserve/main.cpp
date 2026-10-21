//===----------------------------------------------------------------------===//
//
// mtxserve - local backend for the MatrixDSL Lab dashboard (Phase 3)
//
// Owner: Shreyas Mandem (24BCE0381)
//
// A dependency-free HTTP/1.1 server on 127.0.0.1 (never a public interface)
// that runs the real compiler pipeline - the same Pipeline object behind
// matrixdslc - and returns every stage as JSON. The Chrome extension in
// extension/ and the same dashboard served at http://127.0.0.1:8765/ both
// talk to it.
//
//   GET  /api/health            {"ok":true,...}
//   GET  /api/programs          bundled programs (workloads, examples, e2e)
//   POST /api/run    <source>   compile + run under all four configurations
//   GET  /api/suite             the full ablation study, every run verified
//   GET  /<file>                the dashboard (extension/dashboard/)
//
// "Traditional" in the dashboard is the baseline configuration: the same
// front end and the same MDT hardware, compiled without tensor-level
// optimisation (no fusion, every tile fetched from DRAM) - so the comparison
// isolates exactly what the compiler contributes.
//
// Usage: mtxserve [--port N] [--root <repo dir>]
//
//===----------------------------------------------------------------------===//

#include "driver/Pipeline.h"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <sstream>
#include <string>
#include <vector>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
using socket_t = SOCKET;
#define CLOSESOCK closesocket
#else
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
using socket_t = int;
#define CLOSESOCK close
#define INVALID_SOCKET (-1)
#endif

#ifndef MATRIXDSL_SOURCE_DIR
#define MATRIXDSL_SOURCE_DIR "."
#endif

using namespace matrixdsl;
namespace fs = std::filesystem;

namespace {

std::string gRoot = MATRIXDSL_SOURCE_DIR;

// ------------------------------------------------------------------ JSON --

std::string esc(const std::string &s) {
  std::string out;
  out.reserve(s.size() + 8);
  for (unsigned char c : s) {
    switch (c) {
    case '"': out += "\\\""; break;
    case '\\': out += "\\\\"; break;
    case '\n': out += "\\n"; break;
    case '\r': break;
    case '\t': out += "\\t"; break;
    default:
      if (c < 0x20) {
        char buf[8];
        std::snprintf(buf, sizeof(buf), "\\u%04x", c);
        out += buf;
      } else {
        out += static_cast<char>(c);
      }
    }
  }
  return out;
}
std::string str(const std::string &s) { return "\"" + esc(s) + "\""; }
std::string num(double v) {
  char buf[32];
  std::snprintf(buf, sizeof(buf), "%.6g", v);
  return buf;
}

std::string firstLines(const std::string &text, int n, int &total) {
  total = 0;
  size_t cut = std::string::npos;
  for (size_t i = 0; i < text.size(); ++i)
    if (text[i] == '\n' && ++total == n)
      cut = i + 1;
  return cut == std::string::npos ? text : text.substr(0, cut);
}

/// Multiply-accumulates the program performs: the work a scalar core with
/// one MAC unit would need at least one cycle each for.
long long countMACs(const tensorir::TensorFunction &fn) {
  long long macs = 0;
  for (const auto &op : fn.ops()) {
    const auto &s = op->shape;
    const long long elems = 1LL * s.batch * s.rows * s.cols;
    if (tensorir::isMatMulFamily(op->kind))
      macs += elems * op->operands[0]->shape.cols;
    else if (op->kind != tensorir::TensorOpKind::Load &&
             op->kind != tensorir::TensorOpKind::Const &&
             op->kind != tensorir::TensorOpKind::Print &&
             op->kind != tensorir::TensorOpKind::ScratchLoad &&
             op->kind != tensorir::TensorOpKind::ScratchStore)
      macs += elems;
  }
  return macs;
}

/// At most `limit` timeline buckets of [transfer, compute, other] cycles.
std::string timeline(const std::vector<std::pair<int, long long>> &runs, size_t limit) {
  long long total = 0;
  for (const auto &r : runs)
    total += r.second;
  std::ostringstream os;
  os << "[";
  if (runs.size() <= limit) {
    for (size_t i = 0; i < runs.size(); ++i)
      os << (i ? "," : "") << "[" << runs[i].first << "," << runs[i].second << "]";
  } else {
    // Bucket by cycle position, keeping per-class totals inside each bucket.
    const double width = static_cast<double>(total) / limit;
    std::vector<long long> b(3, 0);
    double edge = width;
    long long pos = 0;
    bool first = true;
    auto flush = [&]() {
      for (int k = 0; k < 3; ++k)
        if (b[k]) {
          os << (first ? "" : ",") << "[" << k << "," << b[k] << "]";
          first = false;
          b[k] = 0;
        }
    };
    for (const auto &r : runs) {
      b[r.first] += r.second;
      pos += r.second;
      if (pos >= edge) {
        flush();
        edge += width;
      }
    }
    flush();
  }
  os << "]";
  return os.str();
}

std::string printsJson(const std::vector<tensorir::PrintValue> &prints) {
  std::ostringstream os;
  os << "[";
  for (size_t p = 0; p < prints.size(); ++p) {
    const auto &v = prints[p];
    os << (p ? "," : "") << "{\"name\":" << str(v.name) << ",\"batch\":" << v.batch
       << ",\"rows\":" << v.rows << ",\"cols\":" << v.cols << ",\"values\":[";
    const size_t shown = std::min<size_t>(v.values.size(), 4096);
    for (size_t k = 0; k < shown; ++k)
      os << (k ? "," : "") << num(v.values[k]);
    os << "]}";
  }
  os << "]";
  return os.str();
}

// ------------------------------------------------------------------- API --

std::string runProgram(const std::string &source, const std::string &name) {
  std::vector<tensorir::PrintValue> expected;
  std::string refError;
  const bool refOk = referenceOutput(source, expected, refError);

  std::ostringstream os;
  os << "{\"name\":" << str(name) << ",\"configs\":[";
  bool anyOk = false;
  std::string diagnostics, irBefore;
  std::vector<tensorir::PrintValue> shownPrints;
  long long macs = 0;

  for (int k = 0; k < 4; ++k) {
    const Config &cfg = kConfigs[k];
    PipelineOptions opts;
    opts.filename = name;
    opts.fusion = cfg.fusion;
    opts.tiling = cfg.tiling;
    Compilation c = compile(source, opts);
    if (k == 0) {
      diagnostics = c.diagnostics;
      irBefore = c.irBefore;
    }
    os << (k ? "," : "") << "{\"name\":" << str(cfg.name)
       << ",\"fusion\":" << (cfg.fusion ? "true" : "false")
       << ",\"tiling\":" << (cfg.tiling ? "true" : "false")
       << ",\"compiled\":" << (c.ok ? "true" : "false");
    if (c.ok) {
      if (k == 0)
        macs = countMACs(c.ir);
      Execution x = simulate(c.assembly, false);
      std::string report;
      const bool verified = x.ok && refOk &&
                            tensorir::samePrints(expected, x.prints, 1e-5, report);
      int asmLines = 0;
      const std::string asmHead = firstLines(c.assembly, 400, asmLines);
      os << ",\"ran\":" << (x.ok ? "true" : "false") << ",\"error\":" << str(x.error)
         << ",\"verified\":" << (verified ? "true" : "false")
         << ",\"mismatch\":" << str(report)
         << ",\"instr\":" << x.perf.instructionCount
         << ",\"static\":" << c.codegen.instructions
         << ",\"dram\":" << x.perf.bytesMovedDRAM
         << ",\"scratch\":" << x.perf.bytesMovedScratchpad
         << ",\"seq\":" << x.perf.sequentialCycles
         << ",\"overlap\":" << x.perf.overlapAwareCycles
         << ",\"fused\":" << c.fused << ",\"dead\":" << c.deadOps
         << ",\"staged\":" << c.tiling.staged.size()
         << ",\"ir\":" << str(c.irAfter) << ",\"asm\":" << str(asmHead)
         << ",\"asmLines\":" << asmLines << ",\"timeline\":" << timeline(x.runs, 600);
      if (x.ok) {
        anyOk = true;
        shownPrints = x.prints;
      }
    }
    os << "}";
  }
  os << "],\"ok\":" << (anyOk ? "true" : "false")
     << ",\"diagnostics\":" << str(diagnostics) << ",\"irBefore\":" << str(irBefore)
     << ",\"reference\":" << (refOk ? "true" : "false")
     << ",\"macs\":" << macs << ",\"prints\":" << printsJson(shownPrints) << "}";
  return os.str();
}

std::vector<fs::path> programFiles(const std::string &sub) {
  std::vector<fs::path> files;
  std::error_code ec;
  for (const auto &e : fs::directory_iterator(fs::path(gRoot) / sub, ec))
    if (e.path().extension() == ".mtx")
      files.push_back(e.path());
  std::sort(files.begin(), files.end());
  return files;
}

std::string listPrograms() {
  std::ostringstream os;
  os << "[";
  bool first = true;
  const char *groups[][2] = {{"benchmarks/workloads", "Workloads"},
                             {"examples", "Examples"},
                             {"tests/e2e/valid", "Test corpus"}};
  for (const auto &g : groups)
    for (const fs::path &p : programFiles(g[0])) {
      std::string src;
      readFile(p.string(), src);
      os << (first ? "" : ",") << "{\"group\":" << str(g[1])
         << ",\"name\":" << str(p.stem().string()) << ",\"source\":" << str(src) << "}";
      first = false;
    }
  os << "]";
  return os.str();
}

std::string runSuite() {
  std::ostringstream os;
  os << "[";
  bool first = true;
  for (const fs::path &p : programFiles("benchmarks/workloads")) {
    std::string src;
    readFile(p.string(), src);
    std::vector<tensorir::PrintValue> expected;
    std::string err;
    const bool refOk = referenceOutput(src, expected, err);
    os << (first ? "" : ",") << "{\"name\":" << str(p.stem().string()) << ",\"configs\":[";
    first = false;
    for (int k = 0; k < 4; ++k) {
      PipelineOptions opts;
      opts.filename = p.filename().string();
      opts.fusion = kConfigs[k].fusion;
      opts.tiling = kConfigs[k].tiling;
      Compilation c = compile(src, opts);
      Execution x = c.ok ? simulate(c.assembly, false) : Execution{};
      std::string report;
      const bool ok = x.ok && refOk && tensorir::samePrints(expected, x.prints, 1e-5, report);
      os << (k ? "," : "") << "{\"name\":" << str(kConfigs[k].name)
         << ",\"overlap\":" << x.perf.overlapAwareCycles << ",\"dram\":"
         << x.perf.bytesMovedDRAM << ",\"instr\":" << x.perf.instructionCount
         << ",\"verified\":" << (ok ? "true" : "false") << "}";
    }
    os << "]}";
  }
  os << "]";
  return os.str();
}

// ------------------------------------------------------------------ HTTP --

const char *mimeFor(const std::string &path) {
  const std::string ext = fs::path(path).extension().string();
  if (ext == ".html") return "text/html; charset=utf-8";
  if (ext == ".js") return "text/javascript; charset=utf-8";
  if (ext == ".css") return "text/css; charset=utf-8";
  if (ext == ".svg") return "image/svg+xml";
  if (ext == ".png") return "image/png";
  if (ext == ".json") return "application/json";
  return "application/octet-stream";
}

void sendAll(socket_t s, const std::string &data) {
  size_t sent = 0;
  while (sent < data.size()) {
    const int n = send(s, data.data() + sent, static_cast<int>(data.size() - sent), 0);
    if (n <= 0)
      return;
    sent += static_cast<size_t>(n);
  }
}

void respond(socket_t s, int code, const char *type, const std::string &body) {
  std::ostringstream os;
  os << "HTTP/1.1 " << code << (code == 200 ? " OK" : code == 204 ? " No Content" : " Error")
     << "\r\nContent-Type: " << type << "\r\nContent-Length: " << body.size()
     << "\r\nAccess-Control-Allow-Origin: *"
     << "\r\nAccess-Control-Allow-Methods: GET, POST, OPTIONS"
     << "\r\nAccess-Control-Allow-Headers: Content-Type"
     << "\r\nCache-Control: no-store\r\nConnection: close\r\n\r\n"
     << body;
  sendAll(s, os.str());
}

void handle(socket_t s) {
  std::string req;
  char buf[8192];
  size_t headerEnd = std::string::npos;
  while (headerEnd == std::string::npos) {
    const int n = recv(s, buf, sizeof(buf), 0);
    if (n <= 0)
      return;
    req.append(buf, n);
    headerEnd = req.find("\r\n\r\n");
    if (req.size() > (1u << 20))
      return;
  }
  size_t contentLength = 0;
  {
    std::string lower = req.substr(0, headerEnd);
    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
    const size_t p = lower.find("content-length:");
    if (p != std::string::npos)
      contentLength = std::strtoul(lower.c_str() + p + 15, nullptr, 10);
  }
  if (contentLength > (1u << 20)) {
    respond(s, 413, "text/plain", "program too large");
    return;
  }
  while (req.size() < headerEnd + 4 + contentLength) {
    const int n = recv(s, buf, sizeof(buf), 0);
    if (n <= 0)
      break;
    req.append(buf, n);
  }
  const std::string body = req.substr(headerEnd + 4, contentLength);
  std::istringstream line(req.substr(0, req.find("\r\n")));
  std::string method, target;
  line >> method >> target;
  const std::string path = target.substr(0, target.find('?'));
  std::printf("  %s %s\n", method.c_str(), path.c_str());

  if (method == "OPTIONS")
    return respond(s, 204, "text/plain", "");
  if (path == "/api/health")
    return respond(s, 200, "application/json",
                   "{\"ok\":true,\"name\":\"MatrixDSL\",\"version\":\"3.0\","
                   "\"configs\":[\"baseline\",\"fusion\",\"tiling\",\"fusion+tiling\"]}");
  if (path == "/api/programs")
    return respond(s, 200, "application/json", listPrograms());
  if (path == "/api/suite")
    return respond(s, 200, "application/json", runSuite());
  if (path == "/api/run" && method == "POST")
    return respond(s, 200, "application/json", runProgram(body, "editor.mtx"));

  // Static files: the dashboard, served from the extension's own folder so
  // the browser tab and the Chrome extension show the identical page.
  std::string file = path == "/" ? "/index.html" : path;
  if (file.find("..") != std::string::npos)
    return respond(s, 404, "text/plain", "not found");
  std::string content;
  if (readFile(gRoot + "/extension/dashboard" + file, content))
    return respond(s, 200, mimeFor(file), content);
  respond(s, 404, "text/plain", "not found");
}

} // namespace

int main(int argc, char **argv) {
  int port = 8765;
  for (int i = 1; i < argc; ++i) {
    const std::string a = argv[i];
    if (a == "--port" && i + 1 < argc) port = std::atoi(argv[++i]);
    else if (a == "--root" && i + 1 < argc) gRoot = argv[++i];
  }

#ifdef _WIN32
  WSADATA wsa;
  if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
    std::fprintf(stderr, "mtxserve: WSAStartup failed\n");
    return 1;
  }
#endif
  socket_t server = socket(AF_INET, SOCK_STREAM, 0);
  if (server == INVALID_SOCKET) {
    std::fprintf(stderr, "mtxserve: cannot create socket\n");
    return 1;
  }
  int yes = 1;
  setsockopt(server, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char *>(&yes), sizeof(yes));
  sockaddr_in addr{};
  addr.sin_family = AF_INET;
  addr.sin_port = htons(static_cast<unsigned short>(port));
  addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK); // 127.0.0.1 only
  if (bind(server, reinterpret_cast<sockaddr *>(&addr), sizeof(addr)) != 0 ||
      listen(server, 16) != 0) {
    std::fprintf(stderr, "mtxserve: cannot listen on 127.0.0.1:%d (in use?)\n", port);
    return 1;
  }
  std::printf("MatrixDSL Lab backend on http://127.0.0.1:%d/  (root: %s)\n"
              "Open that URL, or click the MatrixDSL Lab extension. Ctrl+C to stop.\n",
              port, gRoot.c_str());
  std::fflush(stdout);

  for (;;) {
    socket_t client = accept(server, nullptr, nullptr);
    if (client == INVALID_SOCKET)
      continue;
    handle(client);
    std::fflush(stdout);
    CLOSESOCK(client);
  }
}
