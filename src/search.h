#ifndef SEARCH_H
#define SEARCH_H

#include <string>
#include "pieces_placement.h"

// Forward declarations of search-related functions
void calculateMovesForPosition(const std::string& fen);
void calculateMovesForStartingPosition();

#endif // SEARCH_H 