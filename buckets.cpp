#include <atomic>
#include <chrono>
#include <cstdio>
#include <ctime>
#include <sw/redis++/redis++.h>
#include <iostream>
#include <thread>
#include "poker.h"
#include "pokerlib.h"
#include "constants.hpp"
#include "parallel_hashmap/phmap_dump.h"
using namespace std::chrono;

using namespace sw::redis;

hash_t buckets;
HashT<uint64_t, const char *> debug;

uint deck[52];


std::unordered_map<char, int> ranks = {
  {'2', 0},
  {'3', 1},
  {'4', 2},
  {'5', 3},
  {'6', 4},
  {'7', 5},
  {'8', 6},
  {'9', 7},
  {'T', 8},
  {'J', 9},
  {'Q', 10},
  {'K', 11},
  {'A', 12}
};

std::unordered_map<char, int> suits = {
  {'c', 3},
  {'d', 2},
  {'h', 1},
  {'s', 0}
};

#define MAX_SUITS 4

std::atomic<uint64_t> num_keys;

void _read_keys(std::string pattern) {
  auto cursor = 0LL;
  auto count = 500000;
  auto redis = Redis("unix:///run/redis.sock/0");
  std::unordered_set<std::string> keys;
  uint64_t num_local_keys = 0;
  while (true) {
    cursor = redis.scan(cursor, pattern, count, std::inserter(keys, keys.begin()));
    if (cursor == 0) {
      break;
    }
    num_keys.store(num_keys.load() + keys.size() - num_local_keys);
    num_local_keys = keys.size();
    printf("%ld keys scanned.\n", num_keys.load());
  }

  auto pipeline = redis.pipeline();
  for (auto iter = keys.begin(); iter != keys.end(); ++iter) {
    const char *key = (*iter).c_str();
    pipeline.get(key);
  }

  int k = 0;
  sw::redis::QueuedReplies resp = pipeline.exec();
  for (auto iter = keys.begin(); iter != keys.end(); ++iter) {
    const char *key = (*iter).c_str();
    uint64_t board[7] = {0};
    for (int i = 0; i < strlen(key); i+=2) {
      int rank = ranks[key[i]];
      int suit = suits[key[i+1]];
      int card = ((rank)*MAX_SUITS+(suit));
      board[i/2] = card + 1;
    }
    uint64_t idx = (board[0]) | (board[1] << 8) | (board[2] << 16) | (board[3] << 24) | (board[4] << 32) | (board[5] << 40) | (board[6] << 48);
    uint16_t bucket = stoi(resp.get<std::string>(k));
    if(buckets.count(idx)) {
      assert(buckets[idx] == bucket);
    }
    buckets[idx] = bucket;
    k++;
  }
  printf("%ld keys written.\n", buckets.size());
}


int main( const int argc, const char *argv[] )
{
  num_keys.store(0);
  init_deck(deck);
  std::vector<std::thread> threads(5);
  auto start = high_resolution_clock::now();
  char ranks_as_str[14] = "23456789TJQKA";
  for (int i = 0; i < 5; i++) {
    char pattern[3];
    pattern[0] = ranks_as_str[i];
    pattern[1] = '*';
    pattern[2] = '\0';
    threads[i] = std::thread(_read_keys, std::string(pattern));
  }

  for (auto& thread : threads) {
    thread.join();
  }
  auto stop = high_resolution_clock::now();
  auto duration = duration_cast<seconds>(stop - start);
  printf("Total time: %d seconds.\n", duration.count());
  phmap::BinaryOutputArchive out("./buckets23456.bin");
  buckets.phmap_dump(out);

  return 0;
}
