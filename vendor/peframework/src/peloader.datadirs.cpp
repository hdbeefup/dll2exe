// Generic data directory parsing manager.

#include "peloader.h"

#include "peloader.internal.hxx"

#include "peloader.datadirs.hxx"

// Include modules.
extern void registerRuntimeFunctionParser( void );

extern void unregisterRuntimeFunctionParser( void );

// Now the manager implementation/initializer.
struct PEDataDirectoryManager
{
    inline PEDataDirectoryManager( void )
    {
        // Initialize modules.
        registerRuntimeFunctionParser();
    }

    inline ~PEDataDirectoryManager( void )
    {
        // Shutdown modules.
        unregisterRuntimeFunctionParser();
    }

    peMap <std::uint32_t, PEFile::PEDataDirectoryParser*> parserMap;
};

static PEDataDirectoryManager dataDirMan;

void registerDataDirectoryParser( std::uint32_t idx, PEFile::PEDataDirectoryParser *parser )
{
    dataDirMan.parserMap[ idx ] = parser;
}

void unregisterDataDirectoryParser( std::uint32_t idx )
{
    dataDirMan.parserMap.RemoveByKey( idx );
}

PEFile::PEDataDirectoryParser* findDataDirectoryParser( std::uint32_t idx )
{
    return ( dataDirMan.parserMap.FindOrDefault( idx ) );
}