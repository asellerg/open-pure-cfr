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

std::vector<float> bins{0., 0.01111111, 0.01212121, 0.03636364, 0.04040404,
     0.04444444, 0.06767677, 0.07878788, 0.08484848, 0.10505051,
     0.12121212, 0.13434343, 0.14141414, 0.14545455, 0.14949495,
     0.16868687, 0.18181818, 0.1979798 , 0.20707071, 0.21818182,
     0.22222222, 0.22525253, 0.24040404, 0.25454545, 0.27070707,
     0.28282828, 0.29393939, 0.3010101 , 0.31515152, 0.32323232,
     0.33636364, 0.35151515, 0.36565657, 0.37878788, 0.39090909,
     0.3959596 , 0.40808081, 0.41515152, 0.42424242, 0.42929293,
     0.44141414, 0.45656566, 0.47070707, 0.48383838, 0.49494949,
     0.50505051, 0.51313131, 0.52323232, 0.53333333, 0.54242424,
     0.54949495, 0.55858586, 0.56868687, 0.58181818, 0.5959596 ,
     0.60909091, 0.62020202, 0.63232323, 0.64141414, 0.65151515,
     0.66060606, 0.67171717, 0.68080808, 0.68888889, 0.6989899 ,
     0.71010101, 0.72020202, 0.73434343, 0.74646465, 0.75757576,
     0.76969697, 0.78080808, 0.79191919, 0.8040404 , 0.81212121,
     0.82121212, 0.82929293, 0.83838384, 0.84747475, 0.85858586,
     0.86868687, 0.86969697, 0.88282828, 0.8969697 , 0.91010101,
     0.92222222, 0.93434343, 0.94646465, 0.95252525, 0.96262626,
     0.97272727, 0.98484848, 0.99191919, 1.};


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
    // auto lower = std::lower_bound(bins.begin(), bins.end(), hs);
    // int8_t bucket = -1;
    // if (lower != bins.end()) {
    //   bucket = std::distance(bins.begin(), lower);
    // }
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
