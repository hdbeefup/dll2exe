// ARM32/ARMNT implementation of runtime functions registry.

#ifndef _PELOADER_FUNCTIONS_REGISTRY_ARM32_
#define _PELOADER_FUNCTIONS_REGISTRS_ARM32_

#include "peloader.h"

namespace PEFileDetails
{

struct PERuntimeFunctionARM32
{
    PEFile::PESectionDataReference BeginAddress;
    union
    {
        struct
        {
            std::uint32_t Flag : 2;
            std::uint32_t FunctionLength : 11;
            std::uint32_t Ret : 2;
            std::uint32_t H : 1;
            std::uint32_t Reg : 3;
            std::uint32_t R : 1;
            std::uint32_t L : 1;
            std::uint32_t C : 1;
            std::uint32_t StackAdjust : 10;
        };
        std::uint32_t UnwindData;
    };
};

struct PEFunctionRegistryARM32 : PEFile::PEDataDirectoryGeneric
{
    inline PEFunctionRegistryARM32( void ) = default;
    inline PEFunctionRegistryARM32( const PEFunctionRegistryARM32& ) = delete;
    inline PEFunctionRegistryARM32( PEFunctionRegistryARM32&& ) = default;

    void SerializeDataDirectory( PEFile::PESection *targetSect, std::uint64_t peImageBase ) override;

    peVector <PERuntimeFunctionARM32> entries;
};

};

#endif //_PELOADER_FUNCTIONS_REGISTRY_ARM32_