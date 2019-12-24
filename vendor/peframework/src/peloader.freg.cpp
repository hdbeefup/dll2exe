#include "peframework.h"

#include "peloader.internal.hxx"

#include "peloader.datadirs.hxx"

// Include all possible implementations.
#include "peloader.freg.x64.h"
#include "peloader.freg.arm.legacy.h"
#include "peloader.freg.mips32.h"
#include "peloader.freg.arm32.h"
#include "peloader.freg.arm64.h"

// Implementations of serialization.
void PEFileDetails::PEFunctionRegistryX64::SerializeDataDirectory( PEFile::PESection *targetSect, std::uint64_t peImageBase )
{
    const auto& exceptRFs = this->entries;

    std::uint32_t numExceptEntries = (std::uint32_t)exceptRFs.GetCount();

    const std::uint32_t exceptTableSize = ( sizeof(PEStructures::IMAGE_RUNTIME_FUNCTION_ENTRY_X64) * numExceptEntries );

    if ( numExceptEntries != 0 )
    {
        PEFile::PESectionAllocation exceptTableAlloc;
        targetSect->Allocate( exceptTableAlloc, exceptTableSize, sizeof(std::uint32_t) );

        // Now write all entries.
        // TODO: documentation says that these entries should be address sorted.
        for ( std::uint32_t n = 0; n < numExceptEntries; n++ )
        {
            const PEFileDetails::PERuntimeFunctionX64& rfEntry = exceptRFs[ n ];

            PEStructures::IMAGE_RUNTIME_FUNCTION_ENTRY_X64 funcInfo;
            funcInfo.BeginAddress = rfEntry.beginAddrRef.GetRVA();
            funcInfo.EndAddress = rfEntry.endAddrRef.GetRVA();
            funcInfo.UnwindInfoAddress = rfEntry.unwindInfoRef.GetRVA();

            const std::uint32_t rfEntryOff = ( n * sizeof(PEStructures::IMAGE_RUNTIME_FUNCTION_ENTRY_X64) );

            exceptTableAlloc.WriteToSection( &funcInfo, sizeof(funcInfo), rfEntryOff );
        }

        // Remember this valid exception table.
        this->allocEntry = std::move( exceptTableAlloc );
    }
}

void PEFileDetails::PEFunctionRegistryARMLegacy::SerializeDataDirectory( PEFile::PESection *targetSect, std::uint64_t peImageBase )
{
    const auto& exceptRFs = this->entries;

    std::uint32_t numExceptEntries = (std::uint32_t)exceptRFs.GetCount();

    const std::uint32_t exceptTableSize = ( sizeof(PEStructures::IMAGE_RUNTIME_FUNCTION_ENTRY_ARM_LEGACY) * numExceptEntries );

    if ( numExceptEntries != 0 )
    {
        PEFile::PESectionAllocation exceptTableAlloc;
        targetSect->Allocate( exceptTableAlloc, exceptTableSize, sizeof(std::uint32_t) );

        // Now write all entries.
        // TODO: documentation says that these entries should be address sorted.
        for ( std::uint32_t n = 0; n < numExceptEntries; n++ )
        {
            const PEFileDetails::PERuntimeFunctionARMLegacy& rfEntry = exceptRFs[ n ];

            PEStructures::IMAGE_RUNTIME_FUNCTION_ENTRY_ARM_LEGACY funcInfo;
            funcInfo.BeginAddress = (std::uint32_t)( rfEntry.beginAddr.GetRVA() + peImageBase );
            funcInfo.PrologLength = rfEntry.prologLength;
            funcInfo.FunctionLength = rfEntry.functionLength;
            funcInfo.is32Bit = rfEntry.is32Bit;
            funcInfo.hasExceptHandler = rfEntry.hasExceptionHandler;

            const std::uint32_t rfEntryOff = ( n * sizeof(PEStructures::IMAGE_RUNTIME_FUNCTION_ENTRY_ARM_LEGACY) );

            exceptTableAlloc.WriteToSection( &funcInfo, sizeof(funcInfo), rfEntryOff );
        }

        // Remember the new data.
        this->allocEntry = std::move( exceptTableAlloc );
    }
}

void PEFileDetails::PEFunctionRegistryARM32::SerializeDataDirectory( PEFile::PESection *targetSect, std::uint64_t peImageBase )
{
    const auto& exceptRFs = this->entries;

    std::uint32_t numExceptEntries = (std::uint32_t)exceptRFs.GetCount();

    const std::uint32_t exceptTableSize = ( sizeof(PEStructures::IMAGE_RUNTIME_FUNCTION_ENTRY_ARM32) * numExceptEntries );

    if ( numExceptEntries != 0 )
    {
        PEFile::PESectionAllocation exceptTableAlloc;
        targetSect->Allocate( exceptTableAlloc, exceptTableSize, sizeof(std::uint32_t) );

        for ( std::uint32_t n = 0; n < numExceptEntries; n++ )
        {
            const PEFileDetails::PERuntimeFunctionARM32& rfEntry = entries[ n ];

            PEStructures::IMAGE_RUNTIME_FUNCTION_ENTRY_ARM32 info;
            info.BeginAddress = rfEntry.BeginAddress.GetRVA();
            info.UnwindData = rfEntry.UnwindData;

            const std::uint32_t rfEntryOff = ( n * sizeof(PEStructures::IMAGE_RUNTIME_FUNCTION_ENTRY_ARM32) );

            exceptTableAlloc.WriteToSection( &info, sizeof(info), rfEntryOff );
        }

        this->allocEntry = std::move( exceptTableAlloc );
    }
}

void PEFileDetails::PEFunctionRegistryARM64::SerializeDataDirectory( PEFile::PESection *targetSect, std::uint64_t peImageBase )
{
    const auto& exceptRFs = this->entries;

    std::uint32_t numExceptEntries = (std::uint32_t)exceptRFs.GetCount();

    const std::uint32_t exceptTableSize = ( sizeof(PEStructures::IMAGE_RUNTIME_FUNCTION_ENTRY_ARM64) * numExceptEntries );

    if ( numExceptEntries != 0 )
    {
        PEFile::PESectionAllocation exceptTableAlloc;
        targetSect->Allocate( exceptTableAlloc, exceptTableSize, sizeof(std::uint32_t) );

        for ( std::uint32_t n = 0; n < numExceptEntries; n++ )
        {
            const PEFileDetails::PERuntimeFunctionARM64& rfEntry = entries[ n ];

            PEStructures::IMAGE_RUNTIME_FUNCTION_ENTRY_ARM64 info;
            info.BeginAddress = rfEntry.BeginAddress.GetRVA();
            info.UnwindData = rfEntry.UnwindData;

            const std::uint32_t rfEntryOff = ( n * sizeof(PEStructures::IMAGE_RUNTIME_FUNCTION_ENTRY_ARM64) );

            exceptTableAlloc.WriteToSection( &info, sizeof(info), rfEntryOff );
        }

        this->allocEntry = std::move( exceptTableAlloc );
    }
}

// The centralized deserializer implementation.
struct PEFunctionRegistryDataDirectoryParser : public PEFile::PEDataDirectoryParser
{
    PEFile::PEDataDirectoryGeneric* DeserializeData( std::uint16_t machine_id, PEFile::PESectionMan& sections, std::uint64_t peImageBase, PEFile::PEDataStream stream, std::uint32_t va, std::uint32_t vsize ) const override
    {
        if ( machine_id == PEL_IMAGE_FILE_MACHINE_ARM ||
             machine_id == PEL_IMAGE_FILE_MACHINE_POWERPC ||
             machine_id == PEL_IMAGE_FILE_MACHINE_SH3 || machine_id == PEL_IMAGE_FILE_MACHINE_SH3DSP || machine_id == PEL_IMAGE_FILE_MACHINE_SH3E ||
             machine_id == PEL_IMAGE_FILE_MACHINE_SH4 )
        {
            PEFileDetails::PEFunctionRegistryARMLegacy exceptRFs;

            const std::uint32_t numFuncs = ( vsize / sizeof( PEStructures::IMAGE_RUNTIME_FUNCTION_ENTRY_ARM_LEGACY ) );

            for ( size_t n = 0; n < numFuncs; n++ )
            {
                PEStructures::IMAGE_RUNTIME_FUNCTION_ENTRY_ARM_LEGACY func;
                stream.Read( &func, sizeof(func) );

                PEFile::PESection *beginAddrSect = nullptr;
                std::uint32_t beginAddrSectOff = 0;

                if ( std::uint32_t BeginAddress = func.BeginAddress )
                {
                    std::uint32_t rvaBeginAddress = VA2RVA( BeginAddress, peImageBase );

                    bool gotLocation = sections.GetPEDataLocation( rvaBeginAddress, &beginAddrSectOff, &beginAddrSect );

                    if ( gotLocation == false )
                    {
                        throw peframework_exception(
                            ePEExceptCode::CORRUPT_PE_STRUCTURE,
                            "invalid PE runtime function ARM-legacy/PPC/WCE begin address"
                        );
                    }
                }

                PEFileDetails::PERuntimeFunctionARMLegacy info;
                info.beginAddr = PEFile::PESectionDataReference( beginAddrSect, beginAddrSectOff );
                info.prologLength = func.PrologLength;
                info.functionLength = func.FunctionLength;
                info.is32Bit = func.is32Bit;
                info.hasExceptionHandler = func.hasExceptHandler;

                exceptRFs.entries.AddToBack( std::move( info ) );
            }

            return eir::static_new_struct <PEFileDetails::PEFunctionRegistryARMLegacy, PEGlobalStaticAllocator> ( nullptr, std::move( exceptRFs ) );
        }
        else if ( machine_id == PEL_IMAGE_FILE_MACHINE_ARMNT )
        {
            PEFileDetails::PEFunctionRegistryARM32 exceptRFs;

            const std::uint32_t numFuncs = ( vsize / sizeof( PEStructures::IMAGE_RUNTIME_FUNCTION_ENTRY_ARM32 ) );

            for ( size_t n = 0; n < numFuncs; n++ )
            {
                PEStructures::IMAGE_RUNTIME_FUNCTION_ENTRY_ARM32 func;
                stream.Read( &func, sizeof(func) );

                PEFile::PESection *beginAddrSect = nullptr;
                std::uint32_t beginAddrSectOff = 0;

                if ( std::uint32_t BeginAddress = func.BeginAddress )
                {
                    bool gotLocation = sections.GetPEDataLocation( BeginAddress, &beginAddrSectOff, &beginAddrSect );

                    if ( gotLocation == false )
                    {
                        throw peframework_exception(
                            ePEExceptCode::CORRUPT_PE_STRUCTURE,
                            "invalid PE runtime function ARM32 begin address"
                        );
                    }
                }

                PEFileDetails::PERuntimeFunctionARM32 info;
                info.BeginAddress = PEFile::PESectionDataReference( beginAddrSect, beginAddrSectOff );
                info.UnwindData = func.UnwindData;

                exceptRFs.entries.AddToBack( std::move( info ) );
            }

            return eir::static_new_struct <PEFileDetails::PEFunctionRegistryARM32, PEGlobalStaticAllocator> ( nullptr, std::move( exceptRFs ) );
        }
        else if ( machine_id == PEL_IMAGE_FILE_MACHINE_ARM64 )
        {
            PEFileDetails::PEFunctionRegistryARM64 exceptRFs;

            const std::uint32_t numFuncs = ( vsize / sizeof( PEStructures::IMAGE_RUNTIME_FUNCTION_ENTRY_ARM64 ) );

            for ( size_t n = 0; n < numFuncs; n++ )
            {
                PEStructures::IMAGE_RUNTIME_FUNCTION_ENTRY_ARM64 func;
                stream.Read( &func, sizeof(func) );

                PEFile::PESection *beginAddrSect = nullptr;
                std::uint32_t beginAddrSectOff = 0;

                if ( std::uint32_t BeginAddress = func.BeginAddress )
                {
                    bool gotLocation = sections.GetPEDataLocation( BeginAddress, &beginAddrSectOff, &beginAddrSect );

                    if ( gotLocation == false )
                    {
                        throw peframework_exception(
                            ePEExceptCode::CORRUPT_PE_STRUCTURE,
                            "invalid PE runtime function ARM64 begin address"
                        );
                    }
                }

                PEFileDetails::PERuntimeFunctionARM64 info;
                info.BeginAddress = PEFile::PESectionDataReference( beginAddrSect, beginAddrSectOff );
                info.UnwindData = func.UnwindData;

                exceptRFs.entries.AddToBack( std::move( info ) );
            }

            return eir::static_new_struct <PEFileDetails::PEFunctionRegistryARM64, PEGlobalStaticAllocator> ( nullptr, std::move( exceptRFs ) );
        }
        else if ( machine_id == PEL_IMAGE_FILE_MACHINE_AMD64 || machine_id == PEL_IMAGE_FILE_MACHINE_IA64 )
        {
            PEFileDetails::PEFunctionRegistryX64 exceptRFs;

            const std::uint32_t numFuncs = ( vsize / sizeof( PEStructures::IMAGE_RUNTIME_FUNCTION_ENTRY_X64 ) );

            for ( size_t n = 0; n < numFuncs; n++ )
            {
                PEStructures::IMAGE_RUNTIME_FUNCTION_ENTRY_X64 func;
                stream.Read( &func, sizeof(func) );

                // Since the runtime function entry stores RVAs, we want to remember them
                // relocation independent.
                PEFile::PESection *beginAddrSect = nullptr;
                std::uint32_t beginAddrSectOff = 0;

                if ( std::uint32_t BeginAddress = func.BeginAddress )
                {
                    bool gotLocation = sections.GetPEDataLocation( BeginAddress, &beginAddrSectOff, &beginAddrSect );

                    if ( !gotLocation )
                    {
                        throw peframework_exception(
                            ePEExceptCode::CORRUPT_PE_STRUCTURE,
                            "invalid PE runtime function X64 begin address"
                        );
                    }
                }
                PEFile::PESection *endAddrSect = nullptr;
                std::uint32_t endAddrSectOff = 0;

                if ( std::uint32_t EndAddress = func.EndAddress )
                {
                    bool gotLocation = sections.GetPEDataLocation( EndAddress, &endAddrSectOff, &endAddrSect );

                    if ( !gotLocation )
                    {
                        throw peframework_exception(
                            ePEExceptCode::CORRUPT_PE_STRUCTURE,
                            "invalid PE runtime function end address"
                        );
                    }
                }
                PEFile::PESection *unwindInfoSect = nullptr;
                std::uint32_t unwindInfoSectOff = 0;

                if ( std::uint32_t UnwindInfoAddress = func.UnwindInfoAddress )
                {
                    bool gotLocation = sections.GetPEDataLocation( UnwindInfoAddress, &unwindInfoSectOff, &unwindInfoSect );

                    if ( !gotLocation )
                    {
                        throw peframework_exception(
                            ePEExceptCode::CORRUPT_PE_STRUCTURE,
                            "invalid PE runtime function unwind info address"
                        );
                    }
                }

                PEFileDetails::PERuntimeFunctionX64 funcInfo;
                funcInfo.beginAddrRef = PEFile::PESectionDataReference( beginAddrSect, beginAddrSectOff );
                funcInfo.endAddrRef = PEFile::PESectionDataReference( endAddrSect, endAddrSectOff );
                funcInfo.unwindInfoRef = PEFile::PESectionDataReference( unwindInfoSect, unwindInfoSectOff );

                exceptRFs.entries.AddToBack( std::move( funcInfo ) );
            }

            return eir::static_new_struct <PEFileDetails::PEFunctionRegistryX64, PEGlobalStaticAllocator> ( nullptr, std::move( exceptRFs ) );
        }
        else if ( machine_id == PEL_IMAGE_FILE_MACHINE_MIPSFPU )
        {
            // TODO.
        }

        // Unknown machine type.
        return nullptr;
    }
};

static optional_struct_space <PEFunctionRegistryDataDirectoryParser> parser;

// We need one static component called the data directory extension registry that manages all such extensions as this and
// stores their handlers inside.

void registerRuntimeFunctionParser( void )
{
    parser.Construct();

    registerDataDirectoryParser( PEL_IMAGE_DIRECTORY_ENTRY_EXCEPTION, &parser.get() );
}

void unregisterRuntimeFunctionParser( void )
{
    unregisterDataDirectoryParser( PEL_IMAGE_DIRECTORY_ENTRY_EXCEPTION );

    parser.Destroy();
}