// Internal header for data directory extension API.

#ifndef _PELOADER_DATA_DIRECTORY_EXT_INTERNALS_
#define _PELOADER_DATA_DIRECTORY_EXT_INTERNALS_

#include "peloader.h"

// These functions should be called within sub-module (de-)initializers of data directory parsers.
void registerDataDirectoryParser( std::uint32_t idx, PEFile::PEDataDirectoryParser *parser );
void unregisterDataDirectoryParser( std::uint32_t idx );
PEFile::PEDataDirectoryParser* findDataDirectoryParser( std::uint32_t idx );

#endif //_PELOADER_DATA_DIRECTORY_EXT_INTERNALS_