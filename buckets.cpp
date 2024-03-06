#include <atomic>
#include <chrono>
#include <cstdio>
#include <ctype.h>
#include <ctime>
#include <sw/redis++/redis++.h>
#include <iostream>
#include <fstream>
#include <regex>
#include <thread>
#include "poker.h"
#include "pokerlib.h"
#include "constants.hpp"
#include "parallel_hashmap/phmap_dump.h"
using namespace std::chrono;

using namespace sw::redis;

hash_t buckets;

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
  {'c', 2},
  {'d', 1},
  {'h', 0},
  {'s', 3}
};

#define MAX_SUITS 4

std::atomic<uint64_t> num_keys;

void _read_keys(std::string pattern) {

  std::ifstream infile("/home/asellerg/data/rdb/" + pattern + ".data");
  std::string line;
  std::string comma = ",";
  const std::regex base_regex("[0-9AKQJTcdhs]+,([0-9]+),");
  while (std::getline(infile, line)) {
    auto delimiter = line.find(comma);
    std::string key_str = line.substr(0, delimiter);
    std::smatch base_match;
    std::regex_match(line, base_match, base_regex);
    auto match = base_match[1].str().c_str();
    auto bucket = std::atoi(match);

    const char *key = key_str.c_str();
    uint64_t board[7] = {0};
    for (int i = 0; i < strlen(key); i+=2) {
      int rank = ranks[key[i]];
      int suit = suits[key[i+1]];
      int card = ((13*suit)+rank);
      // 0 means unset otherwise.
      board[i/2] = card + 1;
    }
    uint64_t idx = (board[0]) | (board[1] << 8) | (board[2] << 16) | (board[3] << 24) | (board[4] << 32) | (board[5] << 40) | (board[6] << 48);
    assert(bucket <= 1024);
    buckets[idx] = bucket;
    num_keys.store(buckets.size());
    if (num_keys.load() % 1000 == 0) {
      printf("%ld keys written.\n", num_keys.load());
    }
  }
}


int main( const int argc, const char *argv[] )
{
  num_keys.store(0);
  std::vector<std::thread> threads(13);
  auto start = high_resolution_clock::now();
  _read_keys("preflop");
  char ranks_as_str[14] = "23456789TJQKA";
  for (int i = 0; i < 13; i++) {
    char pattern[2];
    pattern[0] = ranks_as_str[i];
    pattern[1] = '\0';
    threads[i] = std::thread(_read_keys, std::string(pattern));
  }
  for (auto& thread : threads) {
     thread.join();
  }
  auto stop = high_resolution_clock::now();
  auto duration = duration_cast<seconds>(stop - start);
  printf("Total time: %d seconds.\n", duration.count());
  phmap::BinaryOutputArchive out("/home/asellerg/data/buckets.bin");
  buckets.phmap_dump(out);
  printf("Saved phmap: %ld.\n", buckets.size());

  return 0;
}
