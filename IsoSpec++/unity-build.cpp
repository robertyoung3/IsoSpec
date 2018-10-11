#include "platform.h"

#if !ISOSPEC_GOT_SYSTEM_MMAN && ISOSPEC_GOT__MMAN
	#include "mman.c"
#endif

#include "allocator.cpp"
#include "dirtyAllocator.cpp"
#include "isoSpec++.cpp"
#include "isoMath.cpp"
#include "marginalTrek++.cpp"
#include "operators.cpp"
#include "element_tables.cpp"
//#include "cwrapper.cpp"
#include "tabulator.cpp"
