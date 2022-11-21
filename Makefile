#
# Makefile for Open Pure CFR
# Richard Gibson, June 26, 2013
#

OPT = -Wall -O3 -ffast-math -funroll-all-loops -ftree-vectorize -DHAVE_MMAP -I/home/asellerg/XPokerEval/XPokerEval.CactusKev/ -L/home/asellerg/XPokerEval/XPokerEval.CactusKev/ -L/usr/local/redis-plus-plus/ -I/usr/local/redis-plus-plus/src/
# OPT = -std=c++11 -O0 -Wall -g -fno-inline -lstdc++

PURE_CFR_FILES = pure_cfr.o acpc_server_code/game.o acpc_server_code/rng.o constants.o parameters.o utility.o card_abstraction.o action_abstraction.o betting_node.o entries.o abstract_game.o player_module.o pure_cfr_machine.o /usr/local/redis-plus-plus/lib/libredis++.a /usr/local/lib/libhiredis.a /home/asellerg/XPokerEval/XPokerEval.CactusKev/mtrand.o /home/asellerg/XPokerEval/XPokerEval.CactusKev/pokerlib.o

PRINT_PLAYER_STRATEGY_FILES = print_player_strategy.o player_module.o acpc_server_code/game.o acpc_server_code/rng.o constants.o parameters.o utility.o card_abstraction.o action_abstraction.o betting_node.o entries.o abstract_game.o /usr/local/redis-plus-plus/lib/libredis++.a /usr/local/lib/libhiredis.a

PURE_CFR_PLAYER_FILES = pure_cfr_player.o player_module.o acpc_server_code/game.o acpc_server_code/rng.o acpc_server_code/net.o constants.o parameters.o utility.o card_abstraction.o action_abstraction.o betting_node.o entries.o abstract_game.o /usr/local/redis-plus-plus/lib/libredis++.a /usr/local/lib/libhiredis.a

BUCKETS_FILES = buckets.cpp /usr/local/redis-plus-plus/lib/libredis++.a /usr/local/lib/libhiredis.a /home/asellerg/XPokerEval/XPokerEval.CactusKev/mtrand.o /home/asellerg/XPokerEval/XPokerEval.CactusKev/pokerlib.o

all: pure_cfr print_player_strategy pure_cfr_player buckets

%.o: %.cpp
	$(CXX) $(OPT) -c $^

pure_cfr: $(PURE_CFR_FILES)
	$(CXX) $(OPT) -pthread -o $@ $(PURE_CFR_FILES)

print_player_strategy: $(PRINT_PLAYER_STRATEGY_FILES)
	$(CXX) $(OPT) -ltbb -std=c++11 -o $@ $(PRINT_PLAYER_STRATEGY_FILES)

pure_cfr_player: $(PURE_CFR_PLAYER_FILES)
	$(CXX) $(OPT) -ltbb -std=c++11 -o $@ $(PURE_CFR_PLAYER_FILES)

buckets: $(BUCKETS_FILES)
	$(CXX) $(OPT) -pthread -lhiredis -lredis++ -mcmodel=medium -o $@ $(BUCKETS_FILES)

clean: 
	-rm *.o acpc_server_code/*.o
	-rm pure_cfr print_player_strategy pure_cfr_player hands
