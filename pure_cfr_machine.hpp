#ifndef __PURE_CFR_PURE_CFR_MACHINE_HPP__
#define __PURE_CFR_PURE_CFR_MACHINE_HPP__

/* pure_cfr_machine.hpp
 * Richard Gibson, Jun 28, 2013
 * Email: richard.g.gibson@gmail.com
 *
 * Stores regrets + average strategy for all players, stores the betting tree
 * and constains Pure CFR iterations routine.
 *
 * Copyright (C) 2013 by Richard Gibson
 */

/* C / C++ / STL indluces */

/* project_acpc_server includes */
extern "C" {
#include "acpc_server_code/rng.h"
}

/* Pure CFR includes */
#include "parameters.hpp"
#include "entries.hpp"
#include "constants.hpp"
#include "hand.hpp"
#include "abstract_game.hpp"
#include "parallel_hashmap/phmap.h"

class PureCfrMachine {
public:
  
  PureCfrMachine( const Parameters &params );
  ~PureCfrMachine( );

  void do_iteration( rng_state_t &rng, intmax_t num_iterations );
  
  /* Returns 0 on success, 1 on failure, -1 on warning */
  int write_dump( const char *dump_prefix, intmax_t iterations_complete, const bool do_regrets = true ) const;
  int load_dump( const char *dump_prefix );
  void load_phmap();

protected:  
  int generate_hand( hand_t &hand, rng_state_t &rng );
  int walk_pure_cfr( const int position,
		     const BettingNode *cur_node,
		     const hand_t &hand,
		     rng_state_t &rng,
         std::unordered_map<int8_t, std::vector<int8_t>> history,
         int8_t prev_round,
         int64_t num_iterations,
         char *history_str,
         int last_char,
	       uint64_t dart);

  AbstractGame ag;
  const bool do_average;
  Entries *regrets[ MAX_ROUNDS ];
  Entries *avg_strategy[ MAX_PURE_CFR_PLAYERS ][ MAX_ROUNDS ];
  uint deck[52];
};

#endif
