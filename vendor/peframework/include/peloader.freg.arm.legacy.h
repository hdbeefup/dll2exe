// ARM/SH3/SH4/PowerPC (legacy stuff) implementation of function registry entries.

#ifndef _PELOADER_FUNCTIONS_REGISTRY_ARM_LEGACY_
#define _PELOADER_FUNCTIONS_REGISTRY_ARM_LEGACY_

#include "peloader.h"

namespace PEFileDetails
{

struct PERuntimeFunctionARMLegacy
{
    PEFile::PESectionDataReference beginAddr;
    std::uint32_t prologLength : 8;
    std::uint32_t functionLength : 22;
    std::uint32_t is32Bit : 1;
    std::uint32_t hasExceptionHandler : 1;
};

struct PEFunctionRegistryARMLegacy : public PEFile::PEDataDirectoryGeneric
{
    inline PEFunctionRegistryARMLegacy( void ) = default;
    inline PEFunctionRegistryARMLegacy( const PEFunctionRegistryARMLegacy& ) = delete;
    inline PEFunctionRegistryARMLegacy( PEFunctionRegistryARMLegacy&& ) = default;

    void SerializeDataDirectory( PEFile::PESection *targetSect, std::uint64_t peImageBase );

    peVector <PEFileDetails::PERuntimeFunctionARMLegacy> entries;
};

}

#endif //_PELOADER_FUNCTIONS_REGISTRY_ARM_LEGACY_