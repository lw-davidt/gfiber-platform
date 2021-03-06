/*
 * Copyright 2016 Google Inc. All rights reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef HTTP_CURL_ENV_H
#define HTTP_CURL_ENV_H

#include <curl/curl.h>
#include <memory>
#include <mutex>
#include "request.h"
#include "url.h"
#include "utils.h"

namespace http {

class Request;

class CurlEnv : public std::enable_shared_from_this<CurlEnv> {
 public:
  struct Options {
    int curl_options = CURL_GLOBAL_NOTHING;
    bool disable_dns_cache = false;
    int max_connections = 0;
  };

  static std::shared_ptr<CurlEnv> NewCurlEnv(const Options &options);
  virtual ~CurlEnv();

  Request::Ptr NewRequest(const Url &url);

  void Lock(curl_lock_data lock_type);
  void Unlock(curl_lock_data lock_type);

 private:
  explicit CurlEnv(const Options &options);

  Options options_;

  // used to lock on curl global state
  std::mutex curl_mutex_;
  bool set_max_connections_;

  std::mutex dns_mutex_;
  CURLSH *share_;  // owned

  DISALLOW_COPY_AND_ASSIGN(CurlEnv);
};

}  // namespace http

#endif  // HTTP_CURL_ENV_H
