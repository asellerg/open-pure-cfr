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
std::atomic<uint64_t> num_spanner_mutations;

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
  auto client = spanner::Client(spanner::MakeConnection(spanner::Database("epicac", "prod", "trial_122")));

  int k = 0;
  std::ifstream infile("/sda/open_pure_cfr/avg_strategy/split/" + std::to_string(filename));
  std::string line;
  std::string colon = ":";
  std::vector<spanner::Mutation> mutations;
  while (std::getline(infile, line)) {
    auto delimiter = line.find(colon);
    std::string key_str = line.substr(0, delimiter);
    std::string value_str = line.substr(delimiter + 1);
    auto vals = split(value_str, ',');
    std::vector<double> strategy;
    auto total = 0.;
    for (int r = 0; r < vals.size() - 1; r++) {
      auto val = std::stof(vals[r]);
      total += val;
      strategy.push_back(val);
    }
    if (total <= 100.) {
      continue;
    }
    std::swap(strategy[0], strategy[1]);
    mutations.push_back(spanner::InsertOrUpdateMutationBuilder(
        "strategy", {"info_set", "strategy"})
        .EmplaceRow(key_str, strategy)
        .Build());
    k++;
    if (k > 10000) {
      auto commit_result = client.Commit(spanner::Mutations(std::move(mutations)));

      if (!commit_result or !commit_result.ok()) {
        std::cerr << "Error writing data to Spanner: " << commit_result.status() << "\n";
        return;
      } else {
        num_spanner_mutations.store(num_spanner_mutations.load() + k);
      }
      k = 0;
      printf("%ld Spanner mutations.\n", num_spanner_mutations.load());
    }

  }
  // Write the leftovers.
  client.Commit(spanner::Mutations(std::move(mutations)));
}


int main( const int argc, const char *argv[] )
{
  num_spanner_mutations.store(0);
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
