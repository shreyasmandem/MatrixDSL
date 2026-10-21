//===----------------------------------------------------------------------===//
// MatrixDSL - source-annotated diagnostics implementation
// Owner: Vinay A (24BCB0131)
//===----------------------------------------------------------------------===//

#include "Diagnostics.h"

#include <sstream>

namespace matrixdsl {

namespace {

std::string sourceLine(const std::string &source, int line) {
  std::istringstream in(source);
  std::string text;
  for (int n = 1; std::getline(in, text); ++n)
    if (n == line) {
      if (!text.empty() && text.back() == '\r')
        text.pop_back();
      return text;
    }
  return "";
}

} // namespace

std::string renderDiagnostic(const std::string &source,
                             const std::string &filename, int line, int column,
                             const std::string &severity,
                             const std::string &message,
                             const std::string &note) {
  std::ostringstream os;
  os << filename << ":" << line << ":" << column << ": " << severity << ": "
     << message << "\n";
  const std::string text = sourceLine(source, line);
  if (line > 0 && !text.empty()) {
    const std::string gutter = std::to_string(line);
    os << "  " << gutter << " | " << text << "\n";
    os << "  " << std::string(gutter.size(), ' ') << " | ";
    for (int c = 1; c < column && c <= static_cast<int>(text.size()); ++c)
      os << (text[c - 1] == '\t' ? '\t' : ' ');
    os << "^\n";
  }
  if (!note.empty())
    os << "  note: " << note << "\n";
  return os.str();
}

} // namespace matrixdsl
