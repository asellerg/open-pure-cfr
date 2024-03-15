#include <atomic>
#include <chrono>
#include <ctime>
#include <glob.h>
#include <iostream>
#include <fstream>
#include <queue>
#include <regex>
#include <sw/redis++/redis++.h>
#include <thread>
#include "constants.hpp"
#include <cassert>

using namespace sw::redis;

using namespace std::chrono;

std::atomic<uint64_t> num_keys;


void _process_file(std::string boards_filename) {
  auto redis = Redis("unix:///run/redis.sock/0");
  std::ifstream infile(boards_filename);
  std::string board;
  const std::regex base_regex(".*_([0-9]+).*");
  std::smatch base_match;
  std::regex_match(boards_filename, base_match, base_regex);
  std::queue<std::string> boards;
  while (std::getline(infile, board)) {
    boards.push(board);
  }

  auto phs_file = "/sda/data/phs_ec2/river_distros_" + base_match[1].str() + ".txt";
  std::ifstream fp(phs_file);
  std::string line;
  auto pipeline = redis.pipeline();
  while (std::getline(fp, line)) {
    auto hs = std::atof(line.c_str());
    auto key = boards.front().c_str();
    int16_t bucket = -1;
    bucket = (int16_t) (hs * 200.);
    assert(bucket <= 200);
    assert(bucket >= 0);
    pipeline.set(key, std::to_string(bucket));
    num_keys++;
    if (boards.empty()) {
      break;
    }
    boards.pop();
    if (num_keys.load() % 100000 == 0) {
      printf("%ld keys loaded.\n", num_keys.load());
      pipeline.exec();
      pipeline = redis.pipeline();
    }
  }
  pipeline.exec();
}


int main( const int argc, const char *argv[] )
{
  num_keys.store(0);
  std::vector<std::thread> threads(128);
  auto start = high_resolution_clock::now();
  glob_t buf;
  char **cur;
  std::string pattern = "/sda/data/phs_ec2/river_boards_*";
  glob(pattern.c_str(), 0, NULL, &buf);
  int i = 0;
  for (cur = buf.gl_pathv; *cur; cur++) {
    threads[i] = std::thread(_process_file, std::string(*cur));
    i++;
  }
  for (auto& thread : threads) {
     thread.join();
  }
  auto stop = high_resolution_clock::now();
  auto duration = duration_cast<seconds>(stop - start);
  printf("Total time: %d seconds.\n", duration.count());
  return 0;
}
