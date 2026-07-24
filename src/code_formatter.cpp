#include "code_formatter.hpp"

#include <algorithm>
#include <cctype>
#include <sstream>

std::vector<std::string> GabCodeFormatter::split_lines(const std::string &text) {
  std::vector<std::string> lines;
  std::string cur;
  for (size_t i = 0; i < text.size(); ++i) {
    char c = text[i];
    if (c == '\n') {
      lines.push_back(cur);
      cur.clear();
    } else if (c != '\r') {
      cur.push_back(c);
    }
  }
  lines.push_back(cur);
  return lines;
}

std::string GabCodeFormatter::join_lines(const std::vector<std::string> &lines,
                                         bool keep_final_newline) {
  std::ostringstream o;
  for (size_t i = 0; i < lines.size(); ++i) {
    o << lines[i];
    if (i + 1 < lines.size())
      o << '\n';
    else if (keep_final_newline && !lines.back().empty())
      o << '\n';
  }
  return o.str();
}

static std::string rtrim_copy(const std::string &s) {
  size_t end = s.size();
  while (end > 0 && std::isspace(static_cast<unsigned char>(s[end - 1])))
    --end;
  return s.substr(0, end);
}

static std::string make_indent(const GabFormatOptions &opts) {
  if (opts.insert_spaces)
    return std::string(std::max(1, opts.tab_width), ' ');
  return "\t";
}

static std::string normalize_leading_indent(const std::string &line,
                                            const GabFormatOptions &opts) {
  size_t i = 0;
  int spaces = 0;
  while (i < line.size()) {
    if (line[i] == ' ') {
      ++spaces;
      ++i;
    } else if (line[i] == '\t') {
      spaces += std::max(1, opts.tab_width);
      ++i;
    } else {
      break;
    }
  }
  int level = spaces / std::max(1, opts.tab_width);
  std::string out;
  for (int n = 0; n < level; ++n)
    out += make_indent(opts);
  out += line.substr(i);
  return out;
}

std::string GabCodeFormatter::trim_trailing_whitespace(const std::string &text) {
  auto lines = split_lines(text);
  for (auto &l : lines)
    l = rtrim_copy(l);
  bool final_nl = !text.empty() && text.back() == '\n';
  return join_lines(lines, final_nl);
}

std::string GabCodeFormatter::ensure_final_newline(const std::string &text) {
  if (text.empty() || text.back() == '\n')
    return text;
  return text + "\n";
}

std::string GabCodeFormatter::format_document(const std::string &text,
                                              const GabFormatOptions &opts) {
  auto lines = split_lines(text);
  for (auto &l : lines) {
    l = normalize_leading_indent(l, opts);
    if (opts.trim_trailing)
      l = rtrim_copy(l);
  }
  std::string out = join_lines(lines, false);
  if (opts.ensure_final_newline)
    out = ensure_final_newline(out);
  return out;
}

std::string GabCodeFormatter::format_lines(const std::string &text, int start_line,
                                           int end_line, const GabFormatOptions &opts) {
  auto lines = split_lines(text);
  int n = static_cast<int>(lines.size());
  start_line = std::max(0, std::min(start_line, n - 1));
  end_line = std::max(start_line, std::min(end_line, n - 1));
  for (int i = start_line; i <= end_line; ++i) {
    lines[i] = normalize_leading_indent(lines[i], opts);
    if (opts.trim_trailing)
      lines[i] = rtrim_copy(lines[i]);
  }
  bool final_nl = !text.empty() && text.back() == '\n';
  return join_lines(lines, final_nl);
}

std::string GabCodeFormatter::indent_lines(const std::string &text, int start_line,
                                           int end_line, const GabFormatOptions &opts,
                                           bool unindent) {
  auto lines = split_lines(text);
  int n = static_cast<int>(lines.size());
  start_line = std::max(0, std::min(start_line, n - 1));
  end_line = std::max(start_line, std::min(end_line, n - 1));
  std::string unit = make_indent(opts);
  for (int i = start_line; i <= end_line; ++i) {
    if (unindent) {
      if (lines[i].rfind(unit, 0) == 0)
        lines[i] = lines[i].substr(unit.size());
      else if (!lines[i].empty() && lines[i][0] == '\t')
        lines[i] = lines[i].substr(1);
      else {
        int remove = 0;
        while (remove < opts.tab_width &&
               remove < static_cast<int>(lines[i].size()) && lines[i][remove] == ' ')
          ++remove;
        lines[i] = lines[i].substr(remove);
      }
    } else {
      lines[i] = unit + lines[i];
    }
  }
  bool final_nl = !text.empty() && text.back() == '\n';
  return join_lines(lines, final_nl);
}

std::string GabCodeFormatter::sort_lines(const std::string &text, int start_line,
                                         int end_line) {
  auto lines = split_lines(text);
  int n = static_cast<int>(lines.size());
  start_line = std::max(0, std::min(start_line, n - 1));
  end_line = std::max(start_line, std::min(end_line, n - 1));
  std::sort(lines.begin() + start_line, lines.begin() + end_line + 1);
  bool final_nl = !text.empty() && text.back() == '\n';
  return join_lines(lines, final_nl);
}

std::string GabCodeFormatter::toggle_line_comments(const std::string &text,
                                                   int start_line, int end_line) {
  auto lines = split_lines(text);
  int n = static_cast<int>(lines.size());
  start_line = std::max(0, std::min(start_line, n - 1));
  end_line = std::max(start_line, std::min(end_line, n - 1));

  bool all_commented = true;
  for (int i = start_line; i <= end_line; ++i) {
    size_t pos = lines[i].find_first_not_of(" \t");
    if (pos == std::string::npos)
      continue;
    if (lines[i].compare(pos, 2, "//") != 0) {
      all_commented = false;
      break;
    }
  }

  for (int i = start_line; i <= end_line; ++i) {
    if (lines[i].find_first_not_of(" \t") == std::string::npos)
      continue;
    size_t pos = lines[i].find_first_not_of(" \t");
    if (all_commented) {
      if (lines[i].compare(pos, 2, "//") == 0) {
        size_t rem = 2;
        if (pos + rem < lines[i].size() && lines[i][pos + rem] == ' ')
          ++rem;
        lines[i].erase(pos, rem);
      }
    } else {
      lines[i].insert(pos, "// ");
    }
  }
  bool final_nl = !text.empty() && text.back() == '\n';
  return join_lines(lines, final_nl);
}
