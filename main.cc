#include <chrono>
#include <fstream>
#include <future>
#include <iostream>
#include <nlohmann/json.hpp>
#include <string>
#include <vector>

#define CPPHTTPLIB_OPENSSL_SUPPORT
#include "cpp-httplib/httplib.h"
#include "threadpool/ThreadPool.h"

namespace {

size_t MillisSinceEpoch() {
  return std::chrono::duration_cast<std::chrono::milliseconds>(
             std::chrono::system_clock::now().time_since_epoch())
      .count();
}

// Downloads the given blob and returns its size in bytes.
size_t DownloadBlob(const std::string& access_token, const std::string& url) {
  const httplib::Headers headers = {
      {"Authorization", std::string("Bearer ") + access_token}};

  httplib::Client client("https://storage.googleapis.com");
  const size_t start_ms = MillisSinceEpoch();
  const auto res = client.Get(url.c_str(), headers);
  if (!res) {
    std::cerr << "failed to fetch " << url << ": " << res.error() << std::endl;
    return 0;
  }

  if (res->status != 200) {
    std::cerr << "status for " << url << ": " << res->status << std::endl;
    return 0;
  }

  const size_t stop_ms = MillisSinceEpoch();
  std::cout << url << ": " << (stop_ms - start_ms) << " ms (" << start_ms
            << ".." << stop_ms << " ms since epoch)" << std::endl;

  return res->body.size();
}

}  // namespace

int main(int argc, char** argv) {
  std::ifstream file("blobs.txt");
  std::string str;
  std::vector<std::string> urls;
  while (std::getline(file, str)) {
    urls.push_back(str);
  }

  std::cout << "read " << urls.size() << " URLs" << std::endl;

  constexpr size_t kNumWorkers = 50;
  ThreadPool thread_pool(kNumWorkers);

  httplib::Server server;

  server.Get("/", [&urls, &thread_pool](const httplib::Request&,
                                        httplib::Response& res) {
    const size_t start_ms = MillisSinceEpoch();

    std::cout << "fetching access token" << std::endl;
    httplib::Client client("http://metadata.google.internal");
    const httplib::Headers headers = {{"Metadata-Flavor", "Google"}};
    const auto access_token_res = client.Get(
        "/computeMetadata/v1/instance/service-accounts/default/token", headers);
    if (access_token_res->status != 200) {
      std::cerr << "error fetching access token: " << access_token_res->status
                << ", " << access_token_res->body << std::endl;
      res.status = 500;
      return;
    }

    const auto access_token_json =
        nlohmann::json::parse(access_token_res->body);
    const auto access_token = access_token_json["access_token"];

    std::vector<std::future<size_t>> futures;
    futures.reserve(urls.size());
    for (const std::string& url : urls) {
      futures.push_back(thread_pool.enqueue(DownloadBlob, access_token, url));
    }

    size_t sum = 0;
    for (auto& future : futures) {
      sum += future.get();
    }

    res.set_content(std::string("total bytes: ") + std::to_string(sum),
                    "text/plain");

    const size_t stop_ms = MillisSinceEpoch();
    std::cout << "finished request processing: " << (stop_ms - start_ms)
              << " ms (" << start_ms << ".." << stop_ms << " ms since epoch)"
              << std::endl;
  });

  server.listen("0.0.0.0", 8080);

  return 0;
}
