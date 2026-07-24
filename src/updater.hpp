#pragma once

#include <functional>
#include <string>
#include <vector>

struct GabReleaseAsset {
  std::string name;
  std::string url;
  std::string content_type;
  long size = 0;
};

struct GabReleaseInfo {
  std::string tag;       /* e.g. v0.2.0 */
  std::string version;   /* e.g. 0.2.0 */
  std::string name;
  std::string body;
  std::string html_url;
  std::vector<GabReleaseAsset> assets;
};

class GabUpdater {
public:
  using LogFn = std::function<void(const std::string &line)>;

  static constexpr const char *kGithubApiLatest =
      "https://api.github.com/repos/Kkkppmm/Luma/releases/latest";

  static int compare_versions(const std::string &a, const std::string &b);
  static bool fetch_latest(GabReleaseInfo &info, std::string &error);
  static bool is_newer(const GabReleaseInfo &info, const std::string &current_version);
  static GabReleaseAsset pick_asset(const GabReleaseInfo &info);
  static bool download_file(const std::string &url, const std::string &dest,
                            std::string &error, LogFn log = nullptr);
  static bool install_package(const std::string &path, std::string &error,
                              LogFn log = nullptr);
  static bool download_and_install(const GabReleaseInfo &info, std::string &error,
                                   LogFn log = nullptr);
  static std::string detect_pkg_kind(); /* deb | rpm | tar */
};
