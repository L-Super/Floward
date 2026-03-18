//
// Created by LMR on 25-7-22.
//

#include "Config.h"
#include <fstream>

#include "Logger.hpp"

namespace fs = std::filesystem;

Config::~Config() { save(); }

expected<bool, std::string> Config::load(const fs::path& file) {
  if (!fs::exists(file)) {
    auto parentPath = file.parent_path();
    if (!fs::exists(parentPath)) {
      fs::create_directories(parentPath);
    }

    // create an empty file
    std::ofstream out(file);
  }
  filepath_ = file;
  std::ifstream in(file);
  if (!in.is_open())
    return unexpected("File not open");

  // Check if file is empty
  in.seekg(0, std::ios::end);
  if (in.tellg() == 0) {
    // File is empty, do not parse
    return unexpected("File is empty");
  }
  in.seekg(0, std::ios::beg);

  try {
    data_ = nlohmann::json::parse(in);
  }
  catch (const std::exception& e) {
    spdlog::error("Config json parse failed. {}", e.what());
    return unexpected(e.what());
  }

  return true;
}

bool Config::save() const {
  std::ofstream out(filepath_);
  if (!out)
    return false;
  if (!data_.empty())
    out << data_.dump(4);
  return true;
}

void Config::setServerConfig(const ServerConfig& server) { data_["server"] = server; }

std::optional<ServerConfig> Config::getServerConfig() const {
  if (!data_.contains("server"))
    return std::nullopt;

  try {
    ServerConfig server = data_["server"];
    return std::make_optional(server);
  }
  catch (const std::exception& e) {
    spdlog::error("Server config not found. {}", e.what());
  }

  return std::nullopt;
}

void Config::setUserInfo(const UserInfo& userInfo) { data_["user_info"] = userInfo; }

std::optional<UserInfo> Config::getUserInfo() const {
  if (!data_.contains("user_info"))
    return std::nullopt;

  try {
    UserInfo userInfo = data_["user_info"];
    return std::make_optional(userInfo);
  }
  catch (const std::exception& e) {
    spdlog::error("User info not found. {}", e.what());
  }
  return std::nullopt;
}