// X64 and Itanium implementation of runtime functions registry.

#ifndef _PELOADER_FUNCTIONS_REGISTRY_X64_
#define _PELOADER_FUNCTIONS_REGISTRY_X64_

#include "peloader.h"

namespace PEFileDetails
{

struct PERuntimeFunctionX64
{
    PEFile::PESectionDataReference beginAddrRef;
    PEFile::PESectionDataReference endAddrRef;
    PEFile::PESectionDataReference unwindInfoRef;
};

struct PEFunctionRegistryX64 : public PEFile::PEDataDirectoryGeneric
{
    inline PEFunctionRegistryX64( void ) = default;
    inline PEFunctionRegistryX64( const PEFunctionRegistryX64& ) = delete;
    inline PEFunctionRegistryX64( PEFunctionRegistryX64&& ) = default;

    void SerializeDataDirectory( PEFile::PESection *targetSect, std::uint64_t peImageBase ) override;

    peVector <PEFileDetails::PERuntimeFunctionX64> entries;
};

}

#endif //_PELOADER_FUNCTIONS_REGISTRY_X64_