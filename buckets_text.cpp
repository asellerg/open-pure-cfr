#include <atomic>
#include <chrono>
#include <cstdio>
#include <ctime>
#include <sw/redis++/redis++.h>
#include <iostream>
#include <thread>
#include <vector>
#include "poker.h"
#include "pokerlib.h"
#include "constants.hpp"
#include "parallel_hashmap/phmap_dump.h"
#include "google/cloud/spanner/admin/database_admin_client.h"
#include "google/cloud/spanner/admin/instance_admin_client.h"
#include "google/cloud/spanner/client.h"

using namespace std::chrono;

using namespace sw::redis;

namespace spanner = ::google::cloud::spanner;

std::atomic<uint64_t> num_keys;
std::atomic<uint64_t> num_writes;

std::vector<std::string> split(const std::string& str, char delimiter) {
  std::vector<std::string> tokens;
  size_t start = 0;
  size_t end = str.find(delimiter);

  while (end != std::string::npos) {
    tokens.push_back(str.substr(start, end - start));
    start = end + 1;
    end = str.find(delimiter, start);
  }

  tokens.push_back(str.substr(start));
  return tokens;
}

void _write_keys(int filename) {
  auto redis = Redis("unix:///run/redis.sock/2");
  int k = 0;
  std::ifstream infile("/sda/open_pure_cfr/avg_strategy/split/" + std::to_string(filename));
  std::string line;
  std::string colon = ":";
  std::vector<spanner::Mutation> mutations;
  auto pipeline = redis.pipeline();
  while (std::getline(infile, line)) {
    auto delimiter = line.find(colon);
    std::string key_str = line.substr(0, delimiter);
    std::string value_str = line.substr(delimiter + 1);
    auto vals = split(value_str, ',');
    std::vector<double> strategy;
    for (int r = 0; r < vals.size() - 1; r++) {
      auto val = vals[r];
      strategy.push_back(std::stof(val));
    }
    std::map<std::string, double> local_strategy_dict = {{"fold", 0.}, {"call", 0.}, {"raise 0.33", 0.}, {"raise 0.5", 0.}, {"raise 0.66", 0.}, {"raise 1", 0.}, {"raise all", 0.}};
    int i = 0;
    for (auto& entry : local_strategy_dict) {
      entry.second = strategy[i];
      i++;
    }

    pipeline.hmset(key_str, local_strategy_dict.begin(), local_strategy_dict.end());
    k++;
    if (k > 5000) {
      pipeline.exec();
      num_writes.store(num_writes.load() + k);
      pipeline = redis.pipeline();
      k = 0;
      printf("%ld redis writes.\n", num_writes.load());
    }

  }

}


int main( const int argc, const char *argv[] )
{
  num_writes.store(0);
  auto num_threads = 128;
  std::vector<std::thread> threads(num_threads);
  auto start = high_resolution_clock::now();

  for (int i = 0; i < num_threads; i++) {
    threads[i] = std::thread(_write_keys, i);
  }


  for (auto& thread : threads) {
    thread.join();
  }
  auto stop = high_resolution_clock::now();
  auto duration = duration_cast<seconds>(stop - start);
  printf("Total time: %d seconds.\n", duration.count());

  return 0;
}
