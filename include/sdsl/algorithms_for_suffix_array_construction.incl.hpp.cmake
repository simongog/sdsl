#ifndef INCLUDED_SDSL_ALGORITHMS_FOR_SUFFIX_ARRAY_CONSTRUCTION_INCL
#define INCLUDED_SDSL_ALGORITHMS_FOR_SUFFIX_ARRAY_CONSTRUCTION_INCL

#cmakedefine divsufsort_FOUND
#cmakedefine divsufsort64_FOUND

#ifdef divsufsort_FOUND
	#include "divsufsort.h"
#endif

#ifdef divsufsort64_FOUND
	#include "divsufsort64.h"
#endif

#endif
