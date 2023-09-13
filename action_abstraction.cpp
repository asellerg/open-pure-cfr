/* action_abstraction.hpp
 * Richard Gibson, Jun 28, 2013
 *
 * Home of the action_abstraction abstract class and all implementing classes
 *
 * Copyright (C) 2013 by Richard Gibson
 */

/* C / C++ / STL indluces */
#include <assert.h>

/* project_acpc_server includes */
extern "C" {
}

/* Pure CFR includes */
#include "action_abstraction.hpp"

ActionAbstraction::ActionAbstraction( )
{
}

ActionAbstraction::~ActionAbstraction( )
{
}

NullActionAbstraction::NullActionAbstraction( )
{
}

NullActionAbstraction::~NullActionAbstraction( )
{
}

int NullActionAbstraction::get_actions( const Game *game,
					const State &state,
					Action actions
					[ MAX_ABSTRACT_ACTIONS ],
					uint16_t* action_mask ) const
{
  int num_actions = 0;
  bool error = false;
  for( int a = 0; a < NUM_ACTION_TYPES; ++a ) {
    Action action;
    action.type = ( ActionType ) a;
    action.size = 0;
    if( action.type == a_raise ) {
      int32_t min_raise_size;
      int32_t max_raise_size;
      if( raiseIsValid( game, &state, &min_raise_size, &max_raise_size ) ) {
	if( num_actions + ( max_raise_size - min_raise_size + 1 )
	    > MAX_ABSTRACT_ACTIONS ) {
	  error = true;
	  break;
	}
	for( int s = min_raise_size; s <= max_raise_size; ++s ) {
	  actions[ num_actions ] = action;
	  actions[ num_actions ].size = s;
	  ++num_actions;
	}
      }
    } else if( isValidAction( game, &state, 0, &action ) ) {
      /* If you hit this assert, there are too many abstract actions allowed.
       * Either coarsen the betting abstraction or increase MAX_ABSTRACT_ACTIONS
       * in constants.hpp
       */
      if( num_actions >= MAX_ABSTRACT_ACTIONS ) {
	error = true;
	break;
      }
      actions[ num_actions ] = action;
      ++num_actions;
    }
  }

  /* If you hit this assert, there are too many abstract actions allowed.
   * Either coarsen the betting abstraction or increase MAX_ABSTRACT_ACTIONS
   * in constants.hpp
   */
  assert( !error );

  return num_actions;
}

FcpaActionAbstraction::FcpaActionAbstraction( )
{
}

FcpaActionAbstraction::~FcpaActionAbstraction( )
{
}

int FcpaActionAbstraction::get_actions( const Game *game,
					const State &state,
					Action actions
					[ MAX_ABSTRACT_ACTIONS ],
					uint16_t* action_mask ) const
{
  assert( MAX_ABSTRACT_ACTIONS >= 8 );
  
  int num_actions = 0;
  for( int a = 0; a < NUM_ACTION_TYPES; ++a ) {
    Action action;
    action.type = ( ActionType ) a;
    action.size = 0;
    if( action.type == a_raise ) {
      int32_t min_raise_size;
      int32_t max_raise_size;
      if( raiseIsValid( game, &state, &min_raise_size, &max_raise_size ) ) {
	/* Check for pot-size raise being valid.  First, get the pot size. */
	int32_t pot = 0;
	for( int p = 0; p < game->numPlayers; ++p ) {
	  pot += state.spent[ p ];
	}
	/* Add amount needed to call.  This gives the size of a pot-sized raise */
	uint8_t player = currentPlayer( game, &state );
	int amount_to_call = state.maxSpent - state.spent[ player ];
	pot += amount_to_call;
	/* Raise size is total amount of chips committed over all rounds
	 * after making the raise.
	 */
	int pot_raise_size = pot + ( state.spent[ player ] + amount_to_call );
	// int quarter_pot_raise_size = int(0.25 * pot) + ( state.spent[ player ] + amount_to_call );
	int one_third_pot_raise_size = int(0.33 * pot) + ( state.spent[ player ] + amount_to_call );
	int half_pot_raise_size = int(0.5 * pot) + ( state.spent[ player ] + amount_to_call );
	int two_thirds_pot_raise_size = int(0.66 * pot) + ( state.spent[ player ] + amount_to_call );
	// int three_quarters_pot_raise_size = int(0.75 * pot) + ( state.spent[ player ] + amount_to_call );
	if( one_third_pot_raise_size >= min_raise_size && one_third_pot_raise_size < max_raise_size && state.round != 0 && !anyRaises(&state)) {
	  actions[ num_actions ] = action;
	  actions[ num_actions ].size = one_third_pot_raise_size;
	  ++num_actions;
	  *action_mask |= (1 << 2);
	}
	if( half_pot_raise_size < max_raise_size && ((state.round == 0 && !anyRaises(&state)) || (state.round != 0 && anyRaises(&state)))) {
	  actions[ num_actions ] = action;
	  actions[ num_actions ].size = half_pot_raise_size;
	  ++num_actions;
	  *action_mask |= (1 << 3);
	}
	if( two_thirds_pot_raise_size < max_raise_size && state.round != 0 && !anyRaises(&state)) {
	  actions[ num_actions ] = action;
	  actions[ num_actions ].size = two_thirds_pot_raise_size;
	  ++num_actions;
	  *action_mask |= (1 << 4);
	}
	if( state.round == 0 && pot_raise_size < max_raise_size ) {
	  actions[ num_actions ] = action;
	  actions[ num_actions ].size = pot_raise_size;
	  ++num_actions;
	  *action_mask |= (1 << 5);
	}
	if (state.round != 0 && (anyRaises(&state) || ((10000 - state.spent[player]) < (3 * pot)))) {
		/* Now add all-in */
		actions[ num_actions ] = action;
		actions[ num_actions ].size = max_raise_size;
		++num_actions;
		*action_mask |= (1 << 6);
	}
      }
	
    } else if( isValidAction( game, &state, 0, &action ) ) {
      /* Fold and call */
      if (action.type == 0) {
	if (anyRaises(&state)) {
      	  *action_mask |= 1;
	  actions[ num_actions ] = action;
          ++num_actions;
	}
      } else {
      	*action_mask |= (1 << 1);
        actions[ num_actions ] = action;
        ++num_actions;
      }
    }
  }

  return num_actions;
}
