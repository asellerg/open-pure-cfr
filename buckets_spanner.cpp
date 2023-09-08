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

void _write_keys(std::vector<std::string>& keys) {
  auto redis = Redis("unix:///run/redis.sock/2");
  auto client = spanner::Client(spanner::MakeConnection(spanner::Database("epicac", "prod", "blueprint")));
  auto pipeline = redis.pipeline();
  for (auto iter = keys.begin(); iter != keys.end(); ++iter) {
    const char *key = (*iter).c_str();
    pipeline.get(key);
  }

  int k = 0;
  int j = 0;
  sw::redis::QueuedReplies resp = pipeline.exec();
  std::vector<spanner::Mutation> mutations;
  for (auto iter = keys.begin(); iter != keys.end(); ++iter) {
    std::string key = *iter;
    std::vector<sw::redis::OptionalString> vals;
    redis.hmget(key, {"fold", "call", "raise 0.33", "raise 0.5", "raise 0.66", "raise 1", "raise all"}, std::back_inserter(vals));
    std::vector<double> strategy;
    for (int r = 0; r < vals.size(); r++) {
      auto val = vals[r];
      if (val.has_value()) {
        strategy.push_back(std::stof(*val));
      }
    }
    mutations.push_back(spanner::InsertOrUpdateMutationBuilder(
        "strategy", {"info_set", "strategy"})
        .EmplaceRow(key, strategy)
        .Build());
    k++;
    if (k > 500) {
      j++;
      auto commit_result = client.Commit(spanner::Mutations(std::move(mutations)));

      if (!commit_result or !commit_result.ok()) {
        std::cerr << "Error writing data to Spanner: " << commit_result.status() << "\n";
        return;
      } else {
        num_spanner_mutations.store(num_spanner_mutations.load() + k);
      }
      k = 0;
      if (j == 10) {
        printf("%ld Spanner mutations.\n", num_spanner_mutations.load());
        j = 0;
      }
    }

  }

}


int main( const int argc, const char *argv[] )
{
  num_keys.store(0);
  num_spanner_mutations.store(0);
  auto num_threads = 128;
  std::vector<std::thread> threads(num_threads);
  auto start = high_resolution_clock::now();
  auto cursor = 0LL;
  auto count = 1000000;
  auto redis = Redis("unix:///run/redis.sock/2");

  std::vector<std::string> chunks[num_threads];

  uint64_t num_local_keys = 0;
  int i = 0;
  while (true) {
    num_local_keys = chunks[i].size();
    cursor = redis.scan(cursor, "*", count, std::inserter(chunks[i], chunks[i].begin()));
    if (cursor == 0) {
      break;
    }
    num_keys.store(num_keys.load() + chunks[i].size() - num_local_keys);
    printf("%ld keys scanned.\n", num_keys.load());
    i++;
    if (i == (num_threads - 1)) {
      i = 0;
    }
  }

  for (int i = 0; i < num_threads; i++) {
    threads[i] = std::thread(_write_keys, std::ref(chunks[i]));
  }


  for (auto& thread : threads) {
    thread.join();
  }
  auto stop = high_resolution_clock::now();
  auto duration = duration_cast<seconds>(stop - start);
  printf("Total time: %d seconds.\n", duration.count());

  return 0;
}
