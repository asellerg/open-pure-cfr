#ifndef __PURE_CFR_CONSTANTS_HPP__
#define __PURE_CFR_CONSTANTS_HPP__

/* constants.hpp
 * Richard Gibson, Jun 28, 2013
 * Email: richard.g.gibson@gmail.com
 *
 * Defines a number of contant values for array sizes and stuff.
 *
 * Copyright (C) 2013 by Richard Gibson
 */

/* C / C++ / STL includes */

/* C project-acpc-server includes */
extern "C" {
#include "acpc_server_code/game.h"
}

#include "parallel_hashmap/phmap.h"

/* pure cfr includes */

/* Maximum number of players this program can handle right now */
const int MAX_PURE_CFR_PLAYERS = 6;

/* Maximum number of abstract actions a player can choose from */
const int MAX_ABSTRACT_ACTIONS = 8;

/* Length of strings used for filenames */
const int PATH_LENGTH = 1024;

/* Number of iterations to run per thread before checking for pause or quit */
const int ITERATION_BLOCK_SIZE = 1000;

/* Enum of card abstraction types */
typedef enum {
  CARD_ABS_NULL = 0,
  CARD_ABS_POTENTIAL = 1,
  CARD_ABS_BLIND = 2,
  NUM_CARD_ABS_TYPES = 3
} card_abs_type_t;
extern const char card_abs_type_to_str[ NUM_CARD_ABS_TYPES ][ PATH_LENGTH ];

/* Enum of action abstraction types */
typedef enum {
  ACTION_ABS_NULL = 0,
  ACTION_ABS_FCPA = 1,
  NUM_ACTION_ABS_TYPES = 2
} action_abs_type_t;
extern const char action_abs_type_to_str[ NUM_ACTION_ABS_TYPES ][ PATH_LENGTH ];

/* Enum of all possible combinations of players that have not folded at a leaf */
typedef enum {
  LEAF_UNUSED = 0,
  LEAF_P0 = 1,
  LEAF_P1 = 2,
  LEAF_P0_P1 = 3,
  LEAF_P2 = 4,
  LEAF_P0_P2 = 5,
  LEAF_P1_P2 = 6,
  LEAF_P0_P1_P2 = 7,
  LEAF_P3 = 8,
  LEAF_P4 = 16,
  LEAF_P5 = 32,
  LEAF_P0_P3 = 9,
  LEAF_P0_P4 = 17,
  LEAF_P0_P5 = 33,
  LEAF_P1_P3 = 10,
  LEAF_P1_P4 = 18,
  LEAF_P1_P5 = 34,
  LEAF_P2_P3 = 12,
  LEAF_P2_P4 = 20,
  LEAF_P2_P5 = 36,
  LEAF_P3_P4 = 24,
  LEAF_P3_P5 = 40,
  LEAF_P4_P5 = 48,
  LEAF_P0_P1_P3 = 11,
  LEAF_P0_P1_P4 = 19,
  LEAF_P0_P1_P5 = 35,
  LEAF_P0_P2_P3 = 13,
  LEAF_P0_P2_P4 = 21,
  LEAF_P0_P2_P5 = 37,
  LEAF_P0_P3_P4 = 25,
  LEAF_P0_P3_P5 = 41,
  LEAF_P0_P4_P5 = 49,
  LEAF_P1_P2_P3 = 14,
  LEAF_P1_P2_P4 = 22,
  LEAF_P1_P2_P5 = 38,
  LEAF_P1_P3_P4 = 26,
  LEAF_P1_P3_P5 = 42,
  LEAF_P1_P4_P5 = 50,
  LEAF_P2_P3_P4 = 28,
  LEAF_P2_P3_P5 = 44,
  LEAF_P2_P4_P5 = 52,
  LEAF_P3_P4_P5 = 56,
  LEAF_P0_P1_P2_P3 = 15,
  LEAF_P0_P1_P2_P4 = 23,
  LEAF_P0_P1_P2_P5 = 39,
  LEAF_P0_P1_P3_P4 = 27,
  LEAF_P0_P1_P3_P5 = 43,
  LEAF_P0_P1_P4_P5 = 51,
  LEAF_P0_P2_P3_P4 = 29,
  LEAF_P0_P2_P3_P5 = 45,
  LEAF_P0_P2_P4_P5 = 53,
  LEAF_P0_P3_P4_P5 = 57,
  LEAF_P1_P2_P3_P4 = 30,
  LEAF_P1_P2_P3_P5 = 46,
  LEAF_P1_P2_P4_P5 = 54,
  LEAF_P1_P3_P4_P5 = 58,
  LEAF_P2_P3_P4_P5 = 60,
  LEAF_P0_P1_P2_P3_P4 = 31,
  LEAF_P0_P2_P3_P4_P5 = 61,
  LEAF_P0_P1_P3_P4_P5 = 59,
  LEAF_P0_P1_P2_P4_P5 = 55,
  LEAF_P0_P1_P2_P3_P5 = 47,
  LEAF_P1_P2_P3_P4_P5 = 62,
  LEAF_P0_P1_P2_P3_P4_P5 = 63,
  LEAF_NUM_TYPES = 64
} leaf_type_t;

/* Possible regret and average strategy storage types */
typedef enum {
  TYPE_UINT8_T = 0,
  TYPE_INT = 1,
  TYPE_UINT32_T = 2,
  TYPE_UINT64_T = 3,
  TYPE_UINT16_T = 4,
  TYPE_INT16_T = 5,
  TYPE_NUM_TYPES = 6
} pure_cfr_entry_type_t;

extern const pure_cfr_entry_type_t
REGRET_TYPES[ MAX_ROUNDS ];

extern const pure_cfr_entry_type_t
AVG_STRATEGY_TYPES[ MAX_ROUNDS ];

#define NMSP phmap
#define MTX std::mutex
#define MAPNAME phmap::parallel_flat_hash_map
#define EXTRAARGS , NMSP::priv::hash_default_hash<K>, \
                            NMSP::priv::hash_default_eq<K>, \
                            std::allocator<std::pair<const K, V>>, 4, MTX
template <class K, class V>
using HashT      = MAPNAME<K, V EXTRAARGS>;

// Enumerations to represent the rank and suit of a card.
enum class Rank {
    TWO, THREE, FOUR, FIVE, SIX, SEVEN, EIGHT,
    NINE, TEN, JACK, QUEEN, KING, ACE
};

enum class Suit {
    CLUBS, DIAMONDS, HEARTS, SPADES
};

struct Card {
    unsigned int value : 6;  // 6 bits can represent values from 0 to 63.

    void set(Rank rank, Suit suit) {
        value = static_cast<int>(suit) * 13 + static_cast<int>(rank);
    }

    Rank getRank() const {
        return static_cast<Rank>(value % 13);
    }

    Suit getSuit() const {
        return static_cast<Suit>(value / 13);
    }
};

struct Hand {
    Card cards[7];
};

using hash_t     = HashT<uint64_t, uint16_t>;

#endif
