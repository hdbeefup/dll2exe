// ARM64 implementation of function registry entries.

#ifndef _PELOADER_FUNCTIONS_REGISTRY_ARM64_
#define _PELOADER_FUNCTIONS_REGISTRY_ARM64_

#include "peloader.h"

namespace PEFileDetails
{

struct PERuntimeFunctionARM64
{
    PEFile::PESectionDataReference BeginAddress;
    union
    {
        struct
        {
            std::uint32_t flag : 2;
            std::uint32_t funcLen : 11;
            std::uint32_t regF : 3;
            std::uint32_t regI : 4;
            std::uint32_t H : 1;
            std::uint32_t CR : 2;
            std::uint32_t frameSize : 9;
        };
        std::uint32_t UnwindData;
    };
};

struct PEFunctionRegistryARM64 : public PEFile::PEDataDirectoryGeneric
{
    inline PEFunctionRegistryARM64( void ) = default;
    inline PEFunctionRegistryARM64( const PEFunctionRegistryARM64& ) = delete;
    inline PEFunctionRegistryARM64( PEFunctionRegistryARM64&& ) = default;

    void SerializeDataDirectory( PEFile::PESection *targetSect, std::uint64_t peImageBase ) override;

    peVector <PEFileDetails::PERuntimeFunctionARM64> entries;
};

};

#endif //_PELOADER_FUNCTIONS_REGISTRY_ARM64_