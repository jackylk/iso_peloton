/*-------------------------------------------------------------------------
 *
 * pretty_printer.h
 * file description
 *
 * Copyright(c) 2015, CMU
 *
 * /n-store/src/common/pretty_printer.h
 *
 *-------------------------------------------------------------------------
 */

#pragma once

#include "storage/tuple.h"
#include "storage/tile.h"

namespace nstore {

//===--------------------------------------------------------------------===//
// Pretty printer
//===--------------------------------------------------------------------===//

class PrettyPrinter {
	PrettyPrinter(const PrettyPrinter&) = delete;
	PrettyPrinter& operator=(const PrettyPrinter&) = delete;

public:

	// pretty print tuple pointers
	static void PrintTuple(storage::Tuple *source) {
		if (source != nullptr)
			std::cout << (*source);
		else
			std::cout << "[ nullptr tuple ]\n";
	}

	// pretty print tile pointers
	static void PrintTile(storage::Tile *source) {
		if (source != nullptr)
			std::cout << (*source);
		else
			std::cout << "[ nullptr tile ]\n";
	}

};

}  // End nstore namespace