//
// Created by LMR on 25-7-22.
//

#pragma once

#if __cplusplus >= 202300L && __has_include(<expected> )
#define HAVE_STD_EXPECTED 1
#else
#define HAVE_STD_EXPECTED 0
#endif

#if HAVE_STD_EXPECTED
#include <expected>
using std::expected;
using std::unexpected;
#else
#include "Expected.hpp"
using tl::expected;
using tl::unexpected;
#endif
#include <filesystem>
#include <functional>
#include <map>
#include <optional>
#include <vector>

#include "nlohmann/json.hpp"

#include "../net/ClipboardStruct.h"

class Config {
private:
  Config() = default;
  ~Config();

public:
  using PrefCallback = std::function<void()>;
  using CallbackId = uint64_t;

  static Config& instance() {
    static Config config;
    return config;
  }

  expected<bool, std::string> load(const std::filesystem::path& file);
  bool save() const;

  template<typename T>
  void set(const std::string& key, T&& value) {
    auto old = data_.contains(key) ? data_[key] : nlohmann::json{};
    data_[key] = std::forward<T>(value);
    if (data_[key] != old) {
      notify(key);
    }
  }

  template<typename T>
  std::optional<T> get(const std::string& key) const {
    if (!data_.contains(key))
      return {};
    return data_.at(key).get<T>();
  }

  CallbackId addObserver(const std::string& key, PrefCallback callback);
  void removeObserver(const std::string& key, CallbackId id);

  void setServerConfig(const ServerConfig& server);
  std::optional<ServerConfig> getServerConfig() const;

  void setUserInfo(const UserInfo& userInfo);
  std::optional<UserInfo> getUserInfo() const;

private:
  void notify(const std::string& key);

  struct ObserverEntry {
    CallbackId id;
    PrefCallback callback;
  };

  nlohmann::json data_;
  std::filesystem::path filepath_{};
  std::map<std::string, std::vector<ObserverEntry>> observers_{};
  CallbackId nextId_ = 1;
};