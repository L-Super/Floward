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

Config::CallbackId Config::addObserver(const std::string& key, PrefCallback callback) {
  auto id = nextId_++;
  observers_[key].push_back({id, std::move(callback)});
  return id;
}

void Config::removeObserver(const std::string& key, CallbackId id) {
  auto it = observers_.find(key);
  if (it == observers_.end())
    return;
  auto& vec = it->second;
  vec.erase(std::remove_if(vec.begin(), vec.end(), [id](const ObserverEntry& e) { return e.id == id; }), vec.end());
  if (vec.empty())
    observers_.erase(it);
}

void Config::notify(const std::string& key) {
  auto it = observers_.find(key);
  if (it == observers_.end())
    return;
  for (const auto& entry : it->second) {
    entry.callback();
  }
}

void Config::setServerConfig(const ServerConfig& server) {
  data_["server"] = server;
  notify("server");
}

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

void Config::setUserInfo(const UserInfo& userInfo) {
  data_["user_info"] = userInfo;
  notify("user_info");
}

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