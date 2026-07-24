#include "updater.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <sys/wait.h>
#include <unistd.h>

#include <gio/gio.h>
#include <json-glib/json-glib.h>
#include <libsoup/soup.h>

namespace fs = std::filesystem;

namespace {

std::string shell_quote(const std::string &value) {
  std::string out = "'";
  for (char c : value) {
    if (c == '\'')
      out += "'\\''";
    else
      out.push_back(c);
  }
  out.push_back('\'');
  return out;
}

int run_status(const std::string &command, GabUpdater::LogFn log) {
  std::array<char, 512> buffer{};
  FILE *pipe = popen((command + " 2>&1").c_str(), "r");
  if (!pipe)
    return 127;
  while (fgets(buffer.data(), static_cast<int>(buffer.size()), pipe) != nullptr) {
    if (log)
      log(buffer.data());
  }
  int status = pclose(pipe);
  if (WIFEXITED(status))
    return WEXITSTATUS(status);
  return 1;
}

bool path_exists(const char *path) { return access(path, F_OK) == 0; }

std::string normalize_version(std::string version) {
  while (!version.empty() && (version.front() == 'v' || version.front() == 'V'))
    version.erase(version.begin());
  return version;
}

std::vector<int> parse_version_parts(const std::string &v) {
  std::vector<int> parts;
  std::string cur;
  for (char c : v) {
    if (std::isdigit(static_cast<unsigned char>(c))) {
      cur.push_back(c);
    } else if (!cur.empty()) {
      parts.push_back(std::stoi(cur));
      cur.clear();
    }
  }
  if (!cur.empty())
    parts.push_back(std::stoi(cur));
  return parts;
}

} // namespace

int GabUpdater::compare_versions(const std::string &a_in, const std::string &b_in) {
  const auto a = parse_version_parts(normalize_version(a_in));
  const auto b = parse_version_parts(normalize_version(b_in));
  const std::size_t n = std::max(a.size(), b.size());
  for (std::size_t i = 0; i < n; ++i) {
    const int x = i < a.size() ? a[i] : 0;
    const int y = i < b.size() ? b[i] : 0;
    if (x < y)
      return -1;
    if (x > y)
      return 1;
  }
  return 0;
}

std::string GabUpdater::detect_pkg_kind() {
  if (path_exists("/usr/bin/apt-get") || path_exists("/usr/bin/dpkg"))
    return "deb";
  if (path_exists("/usr/bin/dnf") || path_exists("/usr/bin/rpm"))
    return "rpm";
  return "tar";
}

bool GabUpdater::fetch_latest(GabReleaseInfo &info, std::string &error) {
  info = {};
  g_autoptr(SoupSession) session = soup_session_new();
  g_autoptr(SoupMessage) msg = soup_message_new("GET", kGithubApiLatest);
  if (!msg) {
    error = "Failed to create HTTP request";
    return false;
  }
  soup_message_headers_append(soup_message_get_request_headers(msg), "User-Agent",
                              "LumaBuilder-Updater/0.2");
  soup_message_headers_append(soup_message_get_request_headers(msg), "Accept",
                              "application/vnd.github+json");

  g_autoptr(GError) gerror = nullptr;
  g_autoptr(GBytes) bytes =
      soup_session_send_and_read(session, msg, nullptr, &gerror);
  if (!bytes) {
    error = gerror ? gerror->message : "Network request failed";
    return false;
  }

  const SoupStatus status = soup_message_get_status(msg);
  if (status != SOUP_STATUS_OK) {
    error = "GitHub API returned HTTP " + std::to_string(static_cast<int>(status));
    return false;
  }

  gsize size = 0;
  const char *data = static_cast<const char *>(g_bytes_get_data(bytes, &size));
  g_autoptr(JsonParser) parser = json_parser_new();
  if (!json_parser_load_from_data(parser, data, static_cast<gssize>(size), &gerror)) {
    error = gerror ? gerror->message : "Failed to parse GitHub JSON";
    return false;
  }

  JsonNode *root = json_parser_get_root(parser);
  if (!JSON_NODE_HOLDS_OBJECT(root)) {
    error = "Unexpected GitHub API response";
    return false;
  }
  JsonObject *obj = json_node_get_object(root);

  const char *tag = json_object_get_string_member_with_default(obj, "tag_name", "");
  const char *name = json_object_get_string_member_with_default(obj, "name", "");
  const char *body = json_object_get_string_member_with_default(obj, "body", "");
  const char *html = json_object_get_string_member_with_default(obj, "html_url", "");

  info.tag = tag ? tag : "";
  info.version = normalize_version(info.tag);
  info.name = name ? name : "";
  info.body = body ? body : "";
  info.html_url = html ? html : "";

  if (json_object_has_member(obj, "assets")) {
    JsonArray *assets = json_object_get_array_member(obj, "assets");
    const guint n = json_array_get_length(assets);
    for (guint i = 0; i < n; ++i) {
      JsonObject *asset = json_array_get_object_element(assets, i);
      GabReleaseAsset item;
      item.name = json_object_get_string_member_with_default(asset, "name", "");
      item.url =
          json_object_get_string_member_with_default(asset, "browser_download_url", "");
      item.content_type =
          json_object_get_string_member_with_default(asset, "content_type", "");
      item.size = json_object_get_int_member_with_default(asset, "size", 0);
      if (!item.name.empty() && !item.url.empty())
        info.assets.push_back(std::move(item));
    }
  }

  if (info.version.empty()) {
    error = "Latest release has no version tag";
    return false;
  }
  return true;
}

bool GabUpdater::is_newer(const GabReleaseInfo &info, const std::string &current_version) {
  return compare_versions(current_version, info.version) < 0;
}

GabReleaseAsset GabUpdater::pick_asset(const GabReleaseInfo &info) {
  const std::string kind = detect_pkg_kind();
  auto match = [&](const std::string &suffix) -> GabReleaseAsset {
    for (const auto &a : info.assets) {
      if (a.name.size() >= suffix.size() &&
          a.name.compare(a.name.size() - suffix.size(), suffix.size(), suffix) == 0)
        return a;
    }
    return {};
  };

  GabReleaseAsset chosen;
  if (kind == "deb")
    chosen = match(".deb");
  else if (kind == "rpm")
    chosen = match(".rpm");
  if (chosen.name.empty())
    chosen = match(".tar.gz");
  if (chosen.name.empty() && !info.assets.empty())
    chosen = info.assets.front();
  return chosen;
}

bool GabUpdater::download_file(const std::string &url, const std::string &dest,
                               std::string &error, LogFn log) {
  if (log)
    log("Downloading " + url + "\n");

  g_autoptr(SoupSession) session = soup_session_new();
  g_autoptr(SoupMessage) msg = soup_message_new("GET", url.c_str());
  if (!msg) {
    error = "Failed to create download request";
    return false;
  }
  soup_message_headers_append(soup_message_get_request_headers(msg), "User-Agent",
                              "LumaBuilder-Updater/0.2");

  g_autoptr(GError) gerror = nullptr;
  g_autoptr(GBytes) bytes =
      soup_session_send_and_read(session, msg, nullptr, &gerror);
  if (!bytes) {
    error = gerror ? gerror->message : "Download failed";
    return false;
  }
  if (soup_message_get_status(msg) != SOUP_STATUS_OK) {
    error = "Download HTTP " +
            std::to_string(static_cast<int>(soup_message_get_status(msg)));
    return false;
  }

  fs::create_directories(fs::path(dest).parent_path());
  gsize size = 0;
  gconstpointer data = g_bytes_get_data(bytes, &size);
  g_autoptr(GError) write_error = nullptr;
  if (!g_file_set_contents(dest.c_str(), static_cast<const char *>(data),
                           static_cast<gssize>(size), &write_error)) {
    error = write_error ? write_error->message : "Failed to write package";
    return false;
  }
  if (log)
    log("Saved " + dest + " (" + std::to_string(size) + " bytes)\n");
  return true;
}

bool GabUpdater::install_package(const std::string &path, std::string &error, LogFn log) {
  const std::string lower = path;
  int status = 1;

  if (lower.size() >= 4 && lower.substr(lower.size() - 4) == ".deb") {
    if (log)
      log("Installing .deb with apt-get (polkit prompt may appear)…\n");
    status = run_status("pkexec env DEBIAN_FRONTEND=noninteractive apt-get install -y " +
                            shell_quote(path),
                        log);
  } else if (lower.size() >= 4 && lower.substr(lower.size() - 4) == ".rpm") {
    if (log)
      log("Installing .rpm with dnf/rpm (polkit prompt may appear)…\n");
    status = run_status("pkexec dnf install -y " + shell_quote(path), log);
    if (status != 0)
      status = run_status("pkexec rpm -Uvh " + shell_quote(path), log);
  } else if (lower.find(".tar.gz") != std::string::npos) {
    const char *home = getenv("HOME");
    if (!home) {
      error = "HOME is not set";
      return false;
    }
    const fs::path dest = fs::path(home) / ".local" / "opt" / "luma-builder";
    fs::create_directories(dest.parent_path());
    if (log)
      log("Extracting portable package to " + dest.string() + "…\n");
    status = run_status("tar -xzf " + shell_quote(path) + " -C " +
                            shell_quote(dest.parent_path().string()),
                        log);
    if (status == 0) {
      fs::path extracted;
      for (const auto &entry : fs::directory_iterator(dest.parent_path())) {
        if (!entry.is_directory())
          continue;
        const auto name = entry.path().filename().string();
        if (name.rfind("luma-builder", 0) == 0 && entry.path() != dest) {
          extracted = entry.path();
          break;
        }
      }
      if (!extracted.empty()) {
        std::error_code ec;
        fs::remove_all(dest, ec);
        fs::rename(extracted, dest, ec);
        if (ec) {
          error = "Failed to place portable install: " + ec.message();
          return false;
        }
      }
      const fs::path bin_link = fs::path(home) / ".local" / "bin" / "luma-builder";
      fs::create_directories(bin_link.parent_path());
      fs::remove(bin_link);
      fs::create_symlink(dest / "bin" / "luma-builder", bin_link);
      if (log) {
        log("Installed to " + dest.string() + "\n");
        log("Symlink: " + bin_link.string() + "\n");
        log("Restart Luma Builder to use the new version.\n");
      }
    }
  } else {
    error = "Unsupported package type: " + path;
    return false;
  }

  if (status != 0) {
    error = "Install failed with exit code " + std::to_string(status);
    return false;
  }
  return true;
}

bool GabUpdater::download_and_install(const GabReleaseInfo &info, std::string &error,
                                      LogFn log) {
  GabReleaseAsset asset = pick_asset(info);
  if (asset.url.empty()) {
    error = "No downloadable package found in the latest release";
    return false;
  }

  const char *home = getenv("HOME");
  const fs::path cache =
      fs::path(home ? home : "/tmp") / ".cache" / "luma-builder" / "updates";
  const fs::path dest = cache / asset.name;

  if (!download_file(asset.url, dest.string(), error, log))
    return false;
  return install_package(dest.string(), error, log);
}
