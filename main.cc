#include <atomic>
#include <fstream>
#include <future>
#include <iostream>
#include <nlohmann/json.hpp>
#include <string>
#include <vector>

#define CPPHTTPLIB_OPENSSL_SUPPORT
#include "cpp-httplib/httplib.h"

namespace {

void WorkerThread(const std::vector<std::string>& urls,
                  const std::string& access_token, std::vector<size_t>* sizes,
                  std::atomic<size_t>* shared_counter) {
  const httplib::Headers headers = {
      {"Authorization", std::string("Bearer ") + access_token}};

  httplib::Client client("https://storage.googleapis.com");

  while (true) {
    const size_t index = (*shared_counter)++;
    if (index >= urls.size()) {
      return;  // All done.
    }
    const std::string& url = urls[index];
    const auto res = client.Get(url.c_str(), headers);
    if (!res) {
      std::cerr << "failed to fetch " << url << ": " << res.error()
                << std::endl;
      continue;
    }

    if (res->status != 200) {
      std::cerr << "status for " << url << ": " << res->status << std::endl;
      continue;
    }

    (*sizes)[index] = res->body.size();
  }
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

  httplib::Server server;

  server.Get("/", [&urls](const httplib::Request&, httplib::Response& res) {
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

    std::cout << "starting workers for request processing" << std::endl;
    std::vector<size_t> sizes(urls.size());
    constexpr size_t kNumWorkers = 50;
    std::vector<std::thread> threads;
    threads.reserve(kNumWorkers);
    std::atomic<size_t> shared_counter(0);
    for (size_t i = 0; i < kNumWorkers; ++i) {
      threads.push_back(std::thread(
          [&] { WorkerThread(urls, access_token, &sizes, &shared_counter); }));
    }

    for (auto& thread : threads) {
      thread.join();
    }

    size_t sum = 0;
    for (size_t size : sizes) {
      sum += size;
    }

    res.set_content(std::string("total bytes: ") + std::to_string(sum),
                    "text/plain");
  });

  server.listen("0.0.0.0", 8080);

  return 0;
}