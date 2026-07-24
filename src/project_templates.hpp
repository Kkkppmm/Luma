#pragma once

#include <string>
#include <vector>

struct GabTemplateMeta {
  std::string id;
  std::string title;
  std::string description;
  std::string category;
};

struct GabProjectInfo;

class GabProjectTemplates {
public:
  static std::vector<GabTemplateMeta> catalog();
  static const GabTemplateMeta *find(const std::string &id);
  static bool generate(const GabProjectInfo &info, std::string &error);
  static std::string sanitize_app_id(const std::string &name);
  static std::string display_title(const GabTemplateMeta &meta);
};
