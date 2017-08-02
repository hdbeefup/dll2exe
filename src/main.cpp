#define _CRT_SECURE_NO_WARNINGS

#include <peframework.h>
#define ASMJIT_STATIC
#include <asmjit/asmjit.h>

#undef ABSOLUTE

#include <unordered_map>

#include <fstream>

#include <asmjitshared.h>

#include <sdk/UniChar.h>

#include "option.h"

// We need PE image structures due to Win32 image loading behavior.
#include "peloader.serialize.h"

// Some macros we need.
#define _PAGE_READWRITE     0x04

struct runtime_exception
{
    inline runtime_exception( int error_code, const char *msg )
    {
        this->msg = msg;
        this->error_code = error_code;
    }

    const char *msg;
    int error_code;
};

// Creates a redirection reference between modules.
inline PEFile::PESectionDataReference RedirectionRef(
    const PEFile::PESectionDataReference& srcRef, PEFile::PESection *targetSect )
{
    return PEFile::PESectionDataReference( targetSect, srcRef.GetSectionOffset(), srcRef.GetDataSize() );
}

template <typename callbackType>
inline void BufferPatternFind(
    const void *buf, size_t bufSize, size_t numPatterns, const char *patterns[],
    callbackType cb
)
{
    for ( size_t n = 0; n < bufSize; n++ )
    {
        // Check all patterns for validity in this position.
        for ( size_t patIdx = 0; patIdx < numPatterns; patIdx++ )
        {
            const char *curPat = patterns[patIdx];

            size_t patternLen = (size_t)*curPat++;
            
            bool curPatternMatch = true;

            size_t curIter = n;

            while ( true )
            {
                // Has the pattern ended?
                // Then we accept.
                if ( patternLen == 0 )
                {
                    break;
                }

                patternLen--;

                char c = *curPat;

                // Is the current pattern check above the buffer bound?
                // Then we reject, because the pattern is longer than expected.
                if ( curIter >= bufSize )
                {
                    curPatternMatch = false;
                    break;
                }

                // Perform special operation based on pattern content.
                if ( c == '?' )
                {
                    // We simply ignore this character, accept all content.
                }
                else
                {
                    // Check for equality.
                    char bufByte = *( (const char*)buf + curIter );

                    if ( bufByte != c )
                    {
                        curPatternMatch = false;
                        break;
                    }
                }

                curPat++;
                curIter++;
            }

            if ( curPatternMatch )
            {
                size_t matchSize = ( curIter - n );

                // Signal the runtime.
                cb( patIdx, n, matchSize );

                n += matchSize;
                break;
            }
        }

        // Continue finding matches.
    }
}

// Embed a directory entry into the executable.
template <typename calcRedirRef_t>
struct resourceHelpers
{
    static std::wstring AppendPath( const std::wstring& curPath, std::wstring nameToAppend )
    {
        if ( curPath.empty() )
        {
            return nameToAppend;
        }

        return ( curPath + L"::" + nameToAppend );
    }

    static bool EmbedResourceDirectoryInto( const std::wstring& curPath, const calcRedirRef_t& calcRedirRef, PEFile::PEResourceDir& into, const PEFile::PEResourceDir& toEmbed )
    {
        bool hasChanged = false;

        toEmbed.ForAllChildren(
            [&]( const PEFile::PEResourceItem *embedItem, bool hasIdentifierName )
        {
            PEFile::PEResourceItem *resItem = into.FindItem( hasIdentifierName, embedItem->name, embedItem->identifier );

            const std::wstring newPath = AppendPath( curPath, embedItem->GetName() );

            if ( !resItem )
            {
                std::wcout << "* merging resource tree '" << newPath << "'" << std::endl;

                // Create it if not there yet.
                resItem = CloneResourceItem( calcRedirRef, embedItem );

                // Simply insert this item.
                try
                {
                    into.AddItem( resItem );
                }
                catch( ... )
                {
                    delete resItem;

                    throw;
                }

                hasChanged = true;
            }
            else
            {
                // Need to merge the two items, embedItem into resItem.
                PEFile::PEResourceItem::eType embedItemType = embedItem->itemType;

                bool wantsMerge = false;
                bool wantsReplace = false;

                // If the types do not match, then we should replace, because we cannot merge.
                if ( embedItemType != into.itemType )
                {
                    wantsReplace = true;
                }

                // If the insert-type is data, we cannot merge anyway, because data is data.
                // Then a replace is required.
                if ( embedItemType == PEFile::PEResourceItem::eType::DATA )
                {
                    wantsReplace = true;
                }
                // Otherwise we can safely merge.
                else if ( embedItemType == PEFile::PEResourceItem::eType::DIRECTORY )
                {
                    wantsMerge = true;
                }

                if ( wantsReplace )
                {
                    // Give a warning to the user that we replace a resource.
                    std::wcout << L"* replacing resource item '" << newPath << L"'" << std::endl;

                    hasChanged = true;

                    into.RemoveItem( resItem );

                    delete resItem;

                    resItem = CloneResourceItem( calcRedirRef, embedItem );

                    try
                    {
                        into.AddItem( resItem );
                    }
                    catch( ... )
                    {
                        delete resItem;

                        throw;
                    }
                }

                if ( wantsMerge )
                {
                    // A merge is safe.
                    assert( embedItemType == PEFile::PEResourceItem::eType::DIRECTORY );

                    const PEFile::PEResourceDir *embedDir = (const PEFile::PEResourceDir*)embedItem;

                    PEFile::PEResourceDir *resDir = (PEFile::PEResourceDir*)resItem;

                    bool subHasChanged = EmbedResourceDirectoryInto( newPath, calcRedirRef, *resDir, *embedDir );

                    if ( subHasChanged )
                    {
                        hasChanged = true;
                    }
                }
            }
        });

        return hasChanged;
    }

    // Clones a resource item 
    static PEFile::PEResourceItem* CloneResourceItem( const calcRedirRef_t& calcRedirRef, const PEFile::PEResourceItem *srcItem )
    {
        PEFile::PEResourceItem *itemOut = NULL;

        PEFile::PEResourceItem::eType srcItemType = srcItem->itemType;

        if ( srcItemType == PEFile::PEResourceItem::eType::DATA )
        {
            const PEFile::PEResourceInfo *srcDataItem = (const PEFile::PEResourceInfo*)srcItem;

            PEFile::PEResourceInfo dataItem(
                srcItem->hasIdentifierName, srcDataItem->name, srcDataItem->identifier,
                calcRedirRef( srcDataItem->sectRef )
            );
            dataItem.codePage = srcDataItem->codePage;
            dataItem.reserved = srcDataItem->reserved;

            itemOut = new PEFile::PEResourceInfo( std::move( dataItem ) );
        }
        else if ( srcItemType == PEFile::PEResourceItem::eType::DIRECTORY )
        {
            const PEFile::PEResourceDir *srcDirItem = (const PEFile::PEResourceDir*)srcItem;

            PEFile::PEResourceDir dirItem(
                srcItem->hasIdentifierName, srcDirItem->name, srcDirItem->identifier
            );
            dirItem.characteristics = srcDirItem->characteristics;
            dirItem.timeDateStamp = srcDirItem->timeDateStamp;
            dirItem.majorVersion = srcDirItem->majorVersion;
            dirItem.minorVersion = srcDirItem->minorVersion;

            // We have to clone all sub directories.
            srcDirItem->ForAllChildren(
                [&]( const PEFile::PEResourceItem *srcItemChild, bool hasIdentifierName )
            {
                PEFile::PEResourceItem *newItem = CloneResourceItem( calcRedirRef, srcItemChild );

                try
                {
                    dirItem.AddItem( newItem );
                }
                catch( ... )
                {
                    delete newItem;
                    
                    throw;
                }
            });

            itemOut = new PEFile::PEResourceDir( std::move( dirItem ) );
        }
        else
        {
            assert( 0 );
        }

        return itemOut;
    }
};

static void WriteVirtualAddress( PEFile& image, PEFile::PESection *targetSect, std::uint32_t sectOffset, std::uint64_t virtualAddress, std::uint32_t archPointerSize, bool requiresRelocations )
{
    std::uint32_t itemRVA = ( targetSect->GetVirtualAddress() + sectOffset );

    targetSect->stream.Seek( sectOffset );
    
    if ( archPointerSize == 4 )
    {
        targetSect->stream.WriteUInt32( (std::uint32_t)( virtualAddress ) );
    }
    else if ( archPointerSize == 8 )
    {
        targetSect->stream.WriteUInt64( virtualAddress );
    }
    else
    {
        throw peframework_exception( ePEExceptCode::RUNTIME_ERROR, "invalid arch pointer size in PE virtual address write" );
    }

    // We could also need a base relocation entry.
    if ( requiresRelocations )
    {
        PEFile::PEBaseReloc::eRelocType relocType = PEFile::PEBaseReloc::GetRelocTypeForPointerSize( archPointerSize );

        if ( relocType != PEFile::PEBaseReloc::eRelocType::ABSOLUTE )
        {   
            image.AddRelocation( itemRVA, relocType );
        }
    }
}

template <typename splitOperatorType>
static inline bool PutSplitIntoImports( PEFile& image, size_t& splitIndex, std::uint32_t thunkTableOffset, std::uint32_t archPointerSize, splitOperatorType& splitOperator )
{
    // Returns true if the import descriptor should be removed.

    size_t numImpFuncs = splitOperator.GetImportFunctions().size();

    if ( numImpFuncs <= splitIndex )
    {
        // Outside of range or imports empty.
        return false;
    }

    // We first split the import directory into two.
    size_t first_part_count = ( splitIndex );
    size_t second_part_offset = ( first_part_count + 1 );
    size_t second_part_count = ( numImpFuncs - second_part_offset );

    if ( first_part_count == 0 && second_part_count == 0 )
    {
        // Just remove this import entry.
        return true;
    }
    else if ( first_part_count > 0 && second_part_count > 0 )
    {
        // Simply take over the remainder of the first entry into a new second entry.
        splitOperator.MakeSecondDescriptor(
            image,
            second_part_offset, second_part_count,
            // Need to point to the new entry properly.
            ( thunkTableOffset + archPointerSize )
        );

        // Trim the first entry.
        splitOperator.TrimFirstDescriptor( first_part_count );

        splitIndex++;
    }
    else if ( first_part_count > 0 )
    {
        // We simply trim this import descriptor.
        splitOperator.TrimFirstDescriptor( first_part_count );

        // Actually useless but here for mind-model completeness.
        splitIndex++;
    }
    else
    {
        // Move this import descriptor a little.
        splitOperator.RemoveFirstEntry();

        splitOperator.MoveIATBy( image, archPointerSize );
    }

    // We keep the import descriptor.
    return false;
}

template <typename splitOperatorType, typename calcRedirRef_t>
static inline bool InjectImportsWithExports(
    PEFile& image,
    PEFile::PEExportDir& exportDir, splitOperatorType& splitOperator, const calcRedirRef_t& calcRedirRef,
    size_t& numOrdinalMatches, size_t& numNameMatches,
    std::uint32_t archPointerSize, bool requiresRelocations
)
{
    // Returns true if the import descriptor should be removed.

    PEFile::PEImportDesc::functions_t& impFuncs = splitOperator.GetImportFunctions();
    PEFile::PESectionDataReference& firstThunkRef = splitOperator.GetFirstThunkRef();

    // Check if any entry of this import directory is hosted by any export entry.
    size_t impFuncIter = 0;

    while ( true )
    {
        size_t numImpFuncs = impFuncs.size();

        if ( impFuncIter >= numImpFuncs )
        {
            break;
        }

        // Check for any match.
        const PEFile::PEImportDesc::importFunc& impFunc = impFuncs[ impFuncIter ];

        bool isOrdinalMatch = impFunc.isOrdinalImport;
        std::uint32_t ordinalOfImport = impFunc.ordinal_hint;
        const std::string& nameOfImport = impFunc.name;

        const PEFile::PEExportDir::func *expFuncMatch = expFuncMatch = exportDir.ResolveExport( isOrdinalMatch, ordinalOfImport, nameOfImport );

        if ( expFuncMatch != NULL )
        {
            // Keep track of change count.
            if ( isOrdinalMatch )
            {
                std::cout << "* by ordinal " << ordinalOfImport << std::endl;

                numOrdinalMatches++;
            }
            else
            {
                std::cout << "* by name " << nameOfImport << std::endl;

                numNameMatches++;
            }

            // So if we did match, then we replace this entry.
            // Write the function address from the entry point.
            std::uint32_t thunkTableOffset = (std::uint32_t)( archPointerSize * impFuncIter );
            {
                PEFile::PESectionDataReference exeImageFuncRef = calcRedirRef( expFuncMatch->expRef );
                            
                std::uint64_t exeImageFuncVA = ( exeImageFuncRef.GetRVA() + image.GetImageBase() );

                PEFile::PESection *thunkSect = firstThunkRef.GetSection();

                // There should not be a case where import entries come without thunk RVAs.
                assert( thunkSect != NULL );

                std::uint32_t thunkSectOffset = ( firstThunkRef.GetSectionOffset() + thunkTableOffset );

                WriteVirtualAddress( image, thunkSect, thunkSectOffset, exeImageFuncVA, archPointerSize, requiresRelocations );
            }

            // Perform the split operation.
            bool removeImpDesc = PutSplitIntoImports( image, impFuncIter, thunkTableOffset, archPointerSize, splitOperator );

            // We could be done with this import descriptor entirely, in which case true is returned.
            // The runtime must remove it in that case.
            if ( removeImpDesc )
            {
                return true;
            }
        }
        else
        {
            // This import found no representation, so we simply skip it.
            impFuncIter++;
        }
    }

    // Can keep the import descriptor.
    return false;
}

struct AssemblyEnvironment
{
    struct MightyAssembler : public asmjit::X86Assembler
    {
        inline MightyAssembler( asmjit::CodeHolder *codeHolder ) : X86Assembler( codeHolder )
        {
            return;
        }

        void onEmitMemoryAbsOp( std::uint32_t sectIdx, size_t sectOff, std::int64_t immVal, size_t immLen ) override
        {
            // We spawn an abs2abs relocation entry.
            asmjit::CodeHolder *code = this->getCode();

            asmjit::RelocEntry *reloc = NULL;
            code->newRelocEntry( &reloc, asmjit::RelocEntry::kTypeAbsToAbs, (std::uint32_t)immLen );

            if ( reloc )
            {
                // Fill in things.
                reloc->_data = immVal;
                reloc->_sourceSectionId = sectIdx;
                reloc->_sourceOffset = sectOff;
            }
        }
    };

    MightyAssembler x86_asm;

    PEFile& embedImage;
    PEFile::PESection metaSect;

    // List of allocations done by the runtime that should stay until the metaSect has been placed
    // into the image, finally.
    std::list <PEFile::PESectionAllocation> persistentAllocations;

    inline AssemblyEnvironment( PEFile& embedImage, asmjit::CodeHolder *codeHolder )
        : x86_asm( codeHolder ), embedImage( embedImage )
    {
        metaSect.shortName = ".meta";
        metaSect.chars.sect_mem_read = true;
        metaSect.chars.sect_mem_write = true;
    }

    inline ~AssemblyEnvironment( void )
    {
        if ( metaSect.IsEmpty() == false )
        {
            metaSect.Finalize();

            embedImage.AddSection( std::move( this->metaSect ) );
        }
    }

    inline int EmbedModuleIntoExecutable( PEFile& moduleImage, bool requiresRelocations, const char *moduleImageName, bool injectMatchingImports, std::uint32_t archPointerSize )
    {
        PEFile& exeImage = this->embedImage;

        // moduleImage cannot be const because we seek inside of its sections.

        // Check that the module is a DLL.
        if ( moduleImage.pe_finfo.isDLL != true )
        {
            std::cout << "provided DLL image is not a DLL image" << std::endl;

            return -6;
        }

        // Decide what architecture we generate code for.
        std::uint32_t genCodeArch = this->x86_asm.getArchInfo().getType();

        std::uint16_t modMachineType = moduleImage.pe_finfo.machine_id;

        // The DLL module must be relocatable, if 32bit.
        // If 64bit then the module could be compiled RIP-relative, which is ok.
        if ( modMachineType == PEL_IMAGE_FILE_MACHINE_I386 )
        {
            if ( moduleImage.HasRelocationInfo() == false )
            {
                std::cout << "DLL image is not relocatable (x86 requirement)" << std::endl;

                return -11;
            }
        }

        // We need the module image base for rebase pointer fixing.
        std::uint64_t modImageBase = moduleImage.GetImageBase();

        // Generate code along with binding.
        
        // Perform binding of PE references.
        // We keep a list of all sections that we put into the executable image.
        // This is important to transfer all the remaining data that is tied to sections.
        std::unordered_map
            <const PEFile::PESection *, // we assume the original module stays immutable.
                PEFile::PESectionReference> sectLinkMap;

        sectLinkMap.reserve( moduleImage.GetSectionCount() );

        auto calcRedirRef = [&]( const PEFile::PESectionDataReference& srcRef ) -> PEFile::PESectionDataReference
        {
            PEFile::PESection *srcSect = srcRef.GetSection();

            if ( srcSect == NULL )
            {
                return PEFile::PESectionDataReference();
            }

            // We know that we embedded ALL sections into the executable.
            // So we should be able to find for all sections a representation.
            auto findIter = sectLinkMap.find( srcSect );

            assert( findIter != sectLinkMap.end() );

            PEFile::PESectionReference& targetRef = findIter->second;

            return RedirectionRef( srcRef, targetRef.GetSection() );
        };

        auto calcRedirRVA = [&]( const PEFile::PESectionDataReference& srcRef ) -> std::uint32_t
        {
            PEFile::PESection *srcSect = srcRef.GetSection();

            if ( srcSect == NULL )
            {
                throw runtime_exception( -20, "attempt to resolve unbound RVA" );
            }

            // We know that we embedded ALL sections into the executable.
            // So we should be able to find for all sections a representation.
            auto findIter = sectLinkMap.find( srcSect );

            assert( findIter != sectLinkMap.end() );

            PEFile::PESectionReference& targetRef = findIter->second;

            return ( targetRef.GetSection()->ResolveRVA( srcRef.GetSectionOffset() ) );
        };

        std::cout << "mapping sections of module into executable" << std::endl;

        // Embed all sections of the DLL image into the executable image.
        // For that we have to find a place where we can allocate the "image arena".
        std::uint32_t embedImageBaseOffset;
        bool foundNewBase = exeImage.FindSectionSpace( moduleImage.peOptHeader.sizeOfImage, embedImageBaseOffset );

        if ( !foundNewBase )
        {
            std::cout << "failed to find virtual address space for module image in executable image region" << std::endl;

            return -13;
        }
        
        moduleImage.ForAllSections(
            [&]( PEFile::PESection *theSect )
        {
            std::cout << "* " << theSect->shortName << std::endl;

            // Create a copy of the section.
            PEFile::PESection newSect;
            newSect.shortName = theSect->shortName;
            newSect.chars = theSect->chars;
        
            size_t sectDataSize = (size_t)theSect->stream.Size();

            theSect->stream.Seek( 0 );

            newSect.stream.Seek( 0 );
            newSect.stream.Truncate( (std::int32_t)sectDataSize );
            newSect.stream.Write( theSect->stream.Data(), sectDataSize );

            // Finalize ourselves.
            newSect.Finalize();

            // Put into image.
            std::uint32_t sectMemPos = ( embedImageBaseOffset + theSect->GetVirtualAddress() );

            newSect.SetPlacementInfo( sectMemPos, theSect->GetVirtualSize() );

            PEFile::PESection *refInside = exeImage.PlaceSection( std::move( newSect ) );

            if ( refInside == NULL )
            {
                throw runtime_exception( -14, "fatal: failed to allocate module section in executable image" );
            }

            PEFile::PESectionReference sectInsideRef( refInside );

            // Remember this link.
            sectLinkMap[ theSect ] = std::move( sectInsideRef );
        });

        std::uint64_t exeModuleBase = exeImage.GetImageBase();

        // We need to create a special PESection that contains the DLL image PE headers,
        // called ".pedata".
        {
            std::cout << "embedding module image PE headers" << std::endl;

            PEFile::PESection pedataSect;
            pedataSect.shortName = ".pedata";
            pedataSect.chars.sect_mem_execute = false;
            pedataSect.chars.sect_mem_read = true;
            pedataSect.chars.sect_mem_write = false;

            pedataSect.stream.Seek( 0 );

            // Here we go. We actually do not map worthless stuff.
            PEStructures::IMAGE_DOS_HEADER dosHeader;
            dosHeader.e_magic = PEL_IMAGE_DOS_SIGNATURE;
            dosHeader.e_cblp = 0;
            dosHeader.e_cp = 0;
            dosHeader.e_crlc = 0;
            dosHeader.e_cparhdr = 0;
            dosHeader.e_minalloc = 0;
            dosHeader.e_maxalloc = 0;
            dosHeader.e_ss = 0;
            dosHeader.e_sp = 0;
            dosHeader.e_csum = 0;
            dosHeader.e_ip = 0;
            dosHeader.e_cs = 0;
            dosHeader.e_lfarlc = 0;
            dosHeader.e_ovno = 0;
            dosHeader.e_res[0] = 0;
            dosHeader.e_res[1] = 0;
            dosHeader.e_res[2] = 0;
            dosHeader.e_res[3] = 0;
            dosHeader.e_oemid = 0;
            dosHeader.e_oeminfo = 0;
            memset( dosHeader.e_res2, 0, sizeof( dosHeader.e_res2 ) );
            // this field actually matters, but is an offset from this header now.
            dosHeader.e_lfanew = sizeof(dosHeader);

            pedataSect.stream.WriteStruct( dosHeader );

            // Have to calculate the size of the optional header that we will write.
            std::uint32_t optHeaderSize = sizeof( std::uint16_t );  // magic.

            if ( moduleImage.isExtendedFormat )
            {
                optHeaderSize += sizeof(PEStructures::IMAGE_OPTIONAL_HEADER64);
            }
            else
            {
                optHeaderSize += sizeof(PEStructures::IMAGE_OPTIONAL_HEADER32);
            }

            optHeaderSize += sizeof(PEStructures::IMAGE_DATA_DIRECTORY) * PEL_IMAGE_NUMBEROF_DIRECTORY_ENTRIES;

            // Decide on what architecture we have, and embed the correct optional headers.
            PEStructures::IMAGE_PE_HEADER peHeader;
            peHeader.Signature = PEL_IMAGE_PE_HEADER_SIGNATURE;
            peHeader.FileHeader.Machine = modMachineType;
            peHeader.FileHeader.NumberOfSections = moduleImage.GetSectionCount();
            peHeader.FileHeader.TimeDateStamp = moduleImage.pe_finfo.timeDateStamp;
            peHeader.FileHeader.PointerToSymbolTable = 0;
            peHeader.FileHeader.NumberOfSymbols = 0;
            peHeader.FileHeader.SizeOfOptionalHeader = optHeaderSize;
            peHeader.FileHeader.Characteristics = moduleImage.GetPENativeFileFlags();

            pedataSect.stream.WriteStruct( peHeader );

            // Now write the machine dependent stuff.
            if ( moduleImage.isExtendedFormat )
            {
                pedataSect.stream.WriteUInt16( PEL_IMAGE_NT_OPTIONAL_HDR64_MAGIC );

                PEStructures::IMAGE_OPTIONAL_HEADER64 optHeader;
                optHeader.MajorLinkerVersion = moduleImage.peOptHeader.majorLinkerVersion;
                optHeader.MinorLinkerVersion = moduleImage.peOptHeader.minorLinkerVersion;
                optHeader.SizeOfCode = moduleImage.peOptHeader.sizeOfCode;
                optHeader.SizeOfInitializedData = moduleImage.peOptHeader.sizeOfInitializedData;
                optHeader.SizeOfUninitializedData = moduleImage.peOptHeader.sizeOfUninitializedData;
                optHeader.AddressOfEntryPoint = 0;
                optHeader.BaseOfCode = moduleImage.peOptHeader.baseOfCode;
                optHeader.ImageBase = ( exeModuleBase + embedImageBaseOffset );
                optHeader.SectionAlignment = moduleImage.GetSectionAlignment();
                optHeader.FileAlignment = 0;
                optHeader.MajorOperatingSystemVersion = moduleImage.peOptHeader.majorOSVersion;
                optHeader.MinorOperatingSystemVersion = moduleImage.peOptHeader.minorOSVersion;
                optHeader.MajorImageVersion = moduleImage.peOptHeader.majorImageVersion;
                optHeader.MinorImageVersion = moduleImage.peOptHeader.minorImageVersion;
                optHeader.MajorSubsystemVersion = moduleImage.peOptHeader.majorSubsysVersion;
                optHeader.MinorSubsystemVersion = moduleImage.peOptHeader.minorSubsysVersion;
                optHeader.Win32VersionValue = moduleImage.peOptHeader.win32VersionValue;
                optHeader.SizeOfImage = moduleImage.peOptHeader.sizeOfImage;
                optHeader.SizeOfHeaders = moduleImage.peOptHeader.sizeOfHeaders;
                optHeader.CheckSum = moduleImage.peOptHeader.checkSum;
                optHeader.Subsystem = moduleImage.peOptHeader.subsys;
                optHeader.DllCharacteristics = moduleImage.GetPENativeDLLOptFlags();
                optHeader.SizeOfStackReserve = moduleImage.peOptHeader.sizeOfStackReserve;
                optHeader.SizeOfStackCommit = moduleImage.peOptHeader.sizeOfStackCommit;
                optHeader.SizeOfHeapReserve = moduleImage.peOptHeader.sizeOfHeapReserve;
                optHeader.SizeOfHeapCommit = moduleImage.peOptHeader.sizeOfHeapCommit;
                optHeader.LoaderFlags = moduleImage.peOptHeader.loaderFlags;
                optHeader.NumberOfRvaAndSizes = PEL_IMAGE_NUMBEROF_DIRECTORY_ENTRIES;

                // Write.
                pedataSect.stream.WriteStruct( optHeader );
            }
            else
            {
                pedataSect.stream.WriteUInt16( PEL_IMAGE_NT_OPTIONAL_HDR32_MAGIC );

                PEStructures::IMAGE_OPTIONAL_HEADER32 optHeader;
                optHeader.MajorLinkerVersion = moduleImage.peOptHeader.majorLinkerVersion;
                optHeader.MinorLinkerVersion = moduleImage.peOptHeader.minorLinkerVersion;
                optHeader.SizeOfCode = moduleImage.peOptHeader.sizeOfCode;
                optHeader.SizeOfInitializedData = moduleImage.peOptHeader.sizeOfInitializedData;
                optHeader.SizeOfUninitializedData = moduleImage.peOptHeader.sizeOfUninitializedData;
                optHeader.AddressOfEntryPoint = 0;  // we do not need that.
                optHeader.BaseOfCode = moduleImage.peOptHeader.baseOfCode;
                optHeader.BaseOfData = moduleImage.peOptHeader.baseOfData;
                optHeader.ImageBase = (std::uint32_t)( exeModuleBase + embedImageBaseOffset );
                optHeader.SectionAlignment = moduleImage.GetSectionAlignment();
                optHeader.FileAlignment = 0;    // who knows?
                optHeader.MajorOperatingSystemVersion = moduleImage.peOptHeader.majorOSVersion;
                optHeader.MinorOperatingSystemVersion = moduleImage.peOptHeader.minorOSVersion;
                optHeader.MajorImageVersion = moduleImage.peOptHeader.majorImageVersion;
                optHeader.MinorImageVersion = moduleImage.peOptHeader.minorImageVersion;
                optHeader.MajorSubsystemVersion = moduleImage.peOptHeader.majorSubsysVersion;
                optHeader.MinorSubsystemVersion = moduleImage.peOptHeader.minorSubsysVersion;
                optHeader.Win32VersionValue = moduleImage.peOptHeader.win32VersionValue;
                optHeader.SizeOfImage = moduleImage.peOptHeader.sizeOfImage;
                optHeader.SizeOfHeaders = 0;    // no idea, who cares? not going to redo this mess.
                optHeader.CheckSum = moduleImage.peOptHeader.checkSum;
                optHeader.Subsystem = moduleImage.peOptHeader.subsys;
                optHeader.DllCharacteristics = moduleImage.GetPENativeDLLOptFlags();
                optHeader.SizeOfStackReserve = (std::uint32_t)moduleImage.peOptHeader.sizeOfStackReserve;
                optHeader.SizeOfStackCommit = (std::uint32_t)moduleImage.peOptHeader.sizeOfStackCommit;
                optHeader.SizeOfHeapReserve = (std::uint32_t)moduleImage.peOptHeader.sizeOfHeapReserve;
                optHeader.SizeOfHeapCommit = (std::uint32_t)moduleImage.peOptHeader.sizeOfHeapCommit;
                optHeader.LoaderFlags = moduleImage.peOptHeader.loaderFlags;
                optHeader.NumberOfRvaAndSizes = PEL_IMAGE_NUMBEROF_DIRECTORY_ENTRIES;

                // Write.
                pedataSect.stream.WriteStruct( optHeader );
            }

            // Must write data directories now, we sort of simulate them.
            {
                PEStructures::IMAGE_DATA_DIRECTORY dataDirs[ PEL_IMAGE_NUMBEROF_DIRECTORY_ENTRIES ];
                memset( dataDirs, 0, sizeof(dataDirs) );

                // TOOD: fill them out as necessary.

                // Write the data dirs.
                pedataSect.stream.WriteStruct( dataDirs );
            }

            // Finally we write the section headers.
            // Those might actually be important, who knows.
            {
                moduleImage.ForAllSections(
                    [&]( const PEFile::PESection *theSect )
                {
                    PEStructures::IMAGE_SECTION_HEADER sectHeader;
                    strncpy( (char*)sectHeader.Name, theSect->shortName.c_str(), countof(sectHeader.Name) );
                    sectHeader.Misc.VirtualSize = theSect->GetVirtualSize();
                    // we actually have to write the old information!
                    // which is very useless at this point, unless you know what you are doing.
                    sectHeader.VirtualAddress = theSect->GetVirtualAddress();
                    sectHeader.SizeOfRawData = theSect->stream.Size();
                    sectHeader.PointerToRawData = 0;    // nobody cares.
                    sectHeader.PointerToLinenumbers = 0;
                    sectHeader.NumberOfRelocations = 0;
                    sectHeader.NumberOfLinenumbers = 0;
                    sectHeader.Characteristics = theSect->GetPENativeFlags();

                    // Write.
                    pedataSect.stream.WriteStruct( sectHeader );
                });
            }

            // Embed the section, if it fits. Otherwise we have a potential disaster.
            if ( pedataSect.IsEmpty() == false )
            {
                pedataSect.Finalize();

                std::uint32_t sectMemPos = ( embedImageBaseOffset + 0 );    // must at the beginning of the PE image.

                pedataSect.SetPlacementInfo( sectMemPos, pedataSect.GetVirtualSize() );

                PEFile::PESection *refInside = exeImage.PlaceSection( std::move( pedataSect ) );

                if ( refInside == NULL )
                {
                    std::cout << "WARNING: failed to embed module image PE headers (.pedata); module might not work properly" << std::endl;
                }
            }
        }

        // Embed all import directories.
        if ( moduleImage.imports.empty() == false )
        {
            std::cout << "embedding import directories" << std::endl;

            for ( const PEFile::PEImportDesc& impDesc : moduleImage.imports )
            {
                // We must merge with existing descriptors because the win32 PE loader expects us to.
                // This means that multiple import descriptors with redundant items is an absolute no-go.
                PEFile::PEImportDesc newImports;
                newImports.DLLName = impDesc.DLLName;

                // TODO: think about resolving the allocations too so we do not have to allocate entirely new structures
                //  during commit-phase (DLLName_allocEntry for instance).

                std::cout << "* " << impDesc.DLLName << std::endl;

                // Take over all import entries from the module.
                newImports.funcs = PEFile::PEImportDesc::CreateEquivalentImportsList( impDesc.funcs );

                //TODO: optimize this by acknowledging the allocations of DLLName and funcs inside of the redirected sections.

                // Since we are spreading thunk IATs across the executable image we cannot
                // use the Win32 PE loader feature to store them in read-only sections.
                // We have to bundle all IATs in one place to do that.
                // Solution: make the section of the IAT writable (hack!)

                newImports.firstThunkRef = calcRedirRef( impDesc.firstThunkRef );

                newImports.firstThunkRef.GetSection()->chars.sect_mem_write = true;

                exeImage.imports.push_back( std::move( newImports ) );
            }

            // Make sure we rewrite the imports directory.
            exeImage.importsAllocEntry = PEFile::PESectionAllocation();
        }

        // Just for the heck of it we could embed exports aswell.
        if ( moduleImage.exportDir.functions.empty() == false )
        {
            std::cout << "embedding export functions" << std::endl;

            // First just take over exports.
            size_t ordInputBase = exeImage.exportDir.functions.size();

            for ( const PEFile::PEExportDir::func& expEntry : moduleImage.exportDir.functions )
            {
                PEFile::PEExportDir::func newExpEntry;
                newExpEntry.expRef = calcRedirRef( expEntry.expRef );
                newExpEntry.forwarder = expEntry.forwarder;
                newExpEntry.isForwarder = expEntry.isForwarder;

                // Add it to our exports.
                exeImage.exportDir.functions.push_back( std::move( newExpEntry ) );
            }

            // Next put all the name mappings, too.
            for ( const auto& nameMapIter : moduleImage.exportDir.funcNameMap )
            {
                const PEFile::PEExportDir::mappedName& nameMap = nameMapIter.first;

                size_t funcOrd = ( ordInputBase + nameMapIter.second );

                // Just take it over.
                PEFile::PEExportDir::mappedName newNameMap;
                newNameMap.name = nameMap.name;
                //TODO: take over the name allocation in the future aswell.
                exeImage.exportDir.funcNameMap.insert( std::make_pair( std::move( newNameMap ), funcOrd ) );
            }

            // Rewrite things.
            exeImage.exportDir.allocEntry = PEFile::PESectionAllocation();
            exeImage.exportDir.funcAddressAllocEntry = PEFile::PESectionAllocation();
            exeImage.exportDir.funcNamesAllocEntry = PEFile::PESectionAllocation();
        }

        // Embed delay import directories aswell.
        if ( moduleImage.delayLoads.empty() == false )
        {
            std::cout << "embedding delay-load import directories" << std::endl;

            // We do it just like for the regular imports.
            for ( const PEFile::PEDelayLoadDesc& impDesc : moduleImage.delayLoads )
            {
                PEFile::PEDelayLoadDesc newImports;
                newImports.attrib = impDesc.attrib;
                newImports.DLLName = impDesc.DLLName;
                newImports.DLLHandleRef = calcRedirRef( impDesc.DLLHandleRef );
                    
                // The IAT always needs special handling.
                newImports.IATRef = calcRedirRef( impDesc.IATRef );

                newImports.IATRef.GetSection()->chars.sect_mem_write = true;

                newImports.importNames = PEFile::PEImportDesc::CreateEquivalentImportsList( impDesc.importNames );
                newImports.boundImportAddrTableRef = calcRedirRef( impDesc.boundImportAddrTableRef );
                newImports.unloadInfoTableRef = calcRedirRef( impDesc.unloadInfoTableRef );
                newImports.timeDateStamp = impDesc.timeDateStamp;

                //TODO: optimize this by acknowledging the allocations of DLLName and importNames in the redirected sections.

                // Take it over.
                exeImage.delayLoads.push_back( std::move( newImports ) );
            }
        }

        // Copy over the resources aswell.
        if ( moduleImage.resourceRoot.IsEmpty() == false )
        {
            std::cout << "embedding module resources" << std::endl;

            // We merge things.
            bool hasChanged =
                resourceHelpers <decltype(calcRedirRef)>::EmbedResourceDirectoryInto( std::wstring(), calcRedirRef, exeImage.resourceRoot, moduleImage.resourceRoot );

            if ( hasChanged )
            {
                // Need to write new resource data directory.
                exeImage.resAllocEntry = PEFile::PESectionAllocation();
            }
        }

        bool hasStaticTLS = ( moduleImage.tlsInfo.addressOfIndexRef.GetSection() != NULL );
        
        if ( hasStaticTLS )
        {
            std::cout << "WARNING: module image has static TLS; might not work as expected" << std::endl;
        }

        std::cout << "rebasing DLL sections" << std::endl;

        // Relocate the module pointers properly. We have to solve two problems:
        // 1) rebase the offsets to the new executable.
        // 2) identify each pointer's section and redirect it into the new layout
        for ( const std::pair <const std::uint32_t, PEFile::PEBaseReloc>& modRelocPair : moduleImage.baseRelocs )
        {
            // Calculate the offset of this relocation chunk, all entries base off of it.
            std::uint32_t relocChunkOffset = ( modRelocPair.first * PEFile::baserelocChunkSize );

            const PEFile::PEBaseReloc& modRelocChunk = modRelocPair.second;

            for ( const PEFile::PEBaseReloc::item& modRelocItem : modRelocChunk.items )
            {
                std::uint32_t modRelocRVA = ( relocChunkOffset + modRelocItem.offset );

                // Find out what section this relocation points to.
                std::uint32_t modRelocSectOffset;
                PEFile::PESection *modRelocSect = moduleImage.FindSectionByRVA( modRelocRVA, NULL, &modRelocSectOffset );

                if ( modRelocSect )
                {
                    // Get the counter-part in the executable image.
                    auto findIter = sectLinkMap.find( modRelocSect );

                    assert( findIter != sectLinkMap.end() );

                    PEFile::PESection *exeRelocSect = findIter->second.GetSection();

                    PEFile::PEBaseReloc::eRelocType relocType = (PEFile::PEBaseReloc::eRelocType)modRelocItem.type;

                    // Fix the relocation to the new image base.
                    // For that we have to find out where the target points to and
                    // where this translates to in our target image.
                    {
                        if ( relocType == PEFile::PEBaseReloc::eRelocType::HIGHLOW )
                        {
                            std::uint32_t origValue = 0;

                            exeRelocSect->stream.Seek( modRelocSectOffset );
                            exeRelocSect->stream.ReadUInt32( origValue );

                            std::uint32_t rvaTarget = ( origValue - (std::uint32_t)modImageBase );
                            std::uint32_t newTargetRVA = ( embedImageBaseOffset + rvaTarget );

                            exeRelocSect->stream.Seek( modRelocSectOffset );
                            exeRelocSect->stream.WriteUInt32( newTargetRVA + (std::uint32_t)exeModuleBase );
                        }
                        else if ( relocType == PEFile::PEBaseReloc::eRelocType::DIR64 )
                        {
                            std::uint64_t origValue = 0;

                            exeRelocSect->stream.Seek( modRelocSectOffset );
                            exeRelocSect->stream.ReadUInt64( origValue );

                            std::uint32_t rvaTarget = (std::uint32_t)( origValue - modImageBase );
                            std::uint32_t newTargetRVA = ( embedImageBaseOffset + rvaTarget );

                            exeRelocSect->stream.Seek( modRelocSectOffset );
                            exeRelocSect->stream.WriteUInt64( newTargetRVA + exeModuleBase );
                        }
                        else if ( relocType == PEFile::PEBaseReloc::eRelocType::ABSOLUTE )
                        {
                            // Gotta ignore.
                        }
                        else
                        {
                            std::cout << "unknown relocation type in PE rebasing procedure" << std::endl;

                            return -15;
                        }
                    }

                    if ( requiresRelocations )
                    {
                        // Register this new rebasing.
                        exeImage.AddRelocation( embedImageBaseOffset + modRelocRVA, relocType );
                    }
                }
            }
        }

        // We might want to inject exports into the imports of the executable module.
        if ( injectMatchingImports )
        {
            std::cout << "injecting matched PE imports..." << std::endl;

            // Should keep track of how many items we matched of which type.
            size_t numOrdinalMatches = 0;
            size_t numNameMatches = 0;

            // For each export entry in our importing module we check for all import entries
            // that match it in the executable module. If we find a match we split the import
            // directories in the thunk so that we can write into the loader address during
            // entry point.
            {
                auto dstImpDescIter = exeImage.imports.begin();

                size_t numExportFuncs = moduleImage.exportDir.functions.size();

                while ( dstImpDescIter != exeImage.imports.end() )
                {
                    PEFile::PEImportDesc& impDesc = *dstImpDescIter;

                    bool removeImpDesc = false;

                    // Do things for matching import descriptors only.
                    if ( UniversalCompareStrings(
                            impDesc.DLLName.c_str(), impDesc.DLLName.size(),
                            moduleImageName, strlen(moduleImageName),
                            false ) )
                    {
                        struct basicImpDescriptorHandler
                        {
                            PEFile::PEImportDesc& impDesc;
                            size_t archPointerSize;
                            const decltype( PEFile::imports )::iterator& dstImpDescIter;

                            AINLINE basicImpDescriptorHandler( PEFile::PEImportDesc& impDesc, const decltype( PEFile::imports )::iterator& dstImpDescIter, size_t archPointerSize )
                                : impDesc( impDesc ), dstImpDescIter( dstImpDescIter )
                            {
                                this->archPointerSize = archPointerSize;
                            }

                            AINLINE void MakeSecondDescriptor( PEFile& image, size_t functake_idx, size_t functake_count, std::uint32_t thunkRefStartOffset )
                            {
                                // Simply take over the remainder of the first entry into a new second entry.
                                PEFile::PEImportDesc newSecondDesc;
                                newSecondDesc.DLLName = impDesc.DLLName;
                                newSecondDesc.funcs.reserve( functake_count );
                                {
                                    auto beginMoveIter = std::make_move_iterator( impDesc.funcs.begin() + functake_idx );
                                    auto endMoveIter = ( beginMoveIter + functake_count );

                                    newSecondDesc.funcs.insert( newSecondDesc.funcs.begin(), beginMoveIter, endMoveIter );
                                }

                                // Need to point to the new entry properly.
                                newSecondDesc.firstThunkRef = image.ResolveRVAToRef( impDesc.firstThunkRef.GetRVA() + thunkRefStartOffset );

                                // Insert the new import descriptor after our.
                                image.imports.insert( this->dstImpDescIter + 1, std::move( newSecondDesc ) );
                            }

                            AINLINE PEFile::PEImportDesc::functions_t& GetImportFunctions( void )
                            {
                                return impDesc.funcs;
                            }

                            AINLINE PEFile::PESectionDataReference& GetFirstThunkRef( void )
                            {
                                return impDesc.firstThunkRef;
                            }

                            AINLINE void TrimFirstDescriptor( size_t trimTo )
                            {
                                impDesc.funcs.resize( trimTo );
                            }

                            AINLINE void MoveIATBy( PEFile& image, std::uint32_t moveBytes )
                            {
                                impDesc.firstThunkRef = image.ResolveRVAToRef( impDesc.firstThunkRef.GetRVA() + moveBytes );
                            }

                            AINLINE void RemoveFirstEntry( void )
                            {
                                impDesc.funcs.erase( impDesc.funcs.begin() );
                            }
                        };
                        basicImpDescriptorHandler splitOp( impDesc, dstImpDescIter, archPointerSize );

                        removeImpDesc = InjectImportsWithExports(
                            exeImage,
                            moduleImage.exportDir, splitOp, calcRedirRef,
                            numOrdinalMatches, numNameMatches,
                            archPointerSize, requiresRelocations
                        );
                    }

                    if ( removeImpDesc )
                    {
                        std::cout << "* terminated import module " << impDesc.DLLName << std::endl;

                        dstImpDescIter = exeImage.imports.erase( dstImpDescIter );
                    }
                    else
                    {
                        dstImpDescIter++;
                    }
                }
            }

            // Also do the delayed import descriptors, which should be possible.
            {
                auto dstImpDescIter = exeImage.delayLoads.begin();

                while ( dstImpDescIter != exeImage.delayLoads.end() )
                {
                    bool removeImpDesc = false;

                    // Process this import descriptor.
                    PEFile::PEDelayLoadDesc& impDesc = *dstImpDescIter;

                    if ( UniversalCompareStrings( impDesc.DLLName.c_str(), impDesc.DLLName.size(), moduleImageName, strlen(moduleImageName), false ) )
                    {
                        struct delayedImpDescriptorHandler
                        {
                            AssemblyEnvironment& env;
                            PEFile::PEDelayLoadDesc& impDesc;
                            std::uint32_t archPointerSize;
                            const decltype( PEFile::delayLoads )::iterator& dstImpDescIter;

                            AINLINE delayedImpDescriptorHandler( AssemblyEnvironment& env, PEFile::PEDelayLoadDesc& impDesc, std::uint32_t archPointerSize, const decltype( PEFile::delayLoads )::iterator& dstImpDescIter )
                                : impDesc( impDesc ), env( env ), dstImpDescIter( dstImpDescIter )
                            {
                                this->archPointerSize = archPointerSize;
                            }

                            AINLINE PEFile::PEImportDesc::functions_t& GetImportFunctions( void )
                            {
                                return impDesc.importNames;
                            }

                            AINLINE PEFile::PESectionDataReference& GetFirstThunkRef( void )
                            {
                                return impDesc.IATRef;
                            }

                            AINLINE void TrimFirstDescriptor( size_t trimTo )
                            {
                                impDesc.importNames.resize( trimTo );
                            }

                            AINLINE void RemoveFirstEntry( void )
                            {
                                impDesc.importNames.erase( impDesc.importNames.begin() );
                            }

                            AINLINE void MoveIATBy( PEFile& image, std::uint32_t moveBytes )
                            {
                                impDesc.IATRef = image.ResolveRVAToRef( impDesc.IATRef.GetRVA() + moveBytes );
                                    
                                // Move the other if it is available.
                                PEFile::PESectionDataReference& unloadIAT = impDesc.unloadInfoTableRef;

                                if ( unloadIAT.GetSection() != NULL )
                                {
                                    unloadIAT = image.ResolveRVAToRef( unloadIAT.GetRVA() + moveBytes );
                                }
                            }

                            AINLINE void MakeSecondDescriptor( PEFile& image, size_t functake_idx, size_t functake_count, std::uint32_t thunkRefStartOffset )
                            {
                                PEFile::PEDelayLoadDesc newSecondImp;
                                newSecondImp.attrib = impDesc.attrib;
                                newSecondImp.DLLName = impDesc.DLLName;
                                    
                                // Allocate a new DLL handle memory position for this descriptor.
                                {
                                    PEFile::PESectionAllocation handleAlloc;
                                    env.metaSect.Allocate( handleAlloc, this->archPointerSize, this->archPointerSize );

                                    env.persistentAllocations.push_back( std::move( handleAlloc ) );

                                    newSecondImp.DLLHandleRef = env.persistentAllocations.back();
                                }

                                // Create a special IAT out of the previous.
                                newSecondImp.IATRef = image.ResolveRVAToRef( impDesc.IATRef.GetRVA() + thunkRefStartOffset );

                                // Copy over the import names aswell.
                                newSecondImp.importNames.reserve( functake_count );
                                {
                                    auto beginMoveIter = std::make_move_iterator( impDesc.importNames.begin() + functake_idx );
                                    auto endMoveIter = ( beginMoveIter + functake_count );

                                    newSecondImp.importNames.insert( newSecondImp.importNames.begin(), beginMoveIter, endMoveIter );
                                }

                                // If an unload info table existed previously, also handle that case.
                                {
                                    PEFile::PESectionDataReference& unloadIAT = impDesc.unloadInfoTableRef;

                                    if ( unloadIAT.GetSection() != NULL )
                                    {
                                        unloadIAT = image.ResolveRVAToRef( unloadIAT.GetRVA() + thunkRefStartOffset );
                                    }
                                }

                                newSecondImp.timeDateStamp = impDesc.timeDateStamp;

                                // Put in the new descriptor after the one we manage.
                                image.delayLoads.insert( this->dstImpDescIter + 1, std::move( newSecondImp ) );

                                // Done.
                            }
                        };
                        delayedImpDescriptorHandler splitOp( *this, impDesc, archPointerSize, dstImpDescIter );

                        removeImpDesc =
                            InjectImportsWithExports(
                                exeImage,
                                moduleImage.exportDir, splitOp, calcRedirRef,
                                numOrdinalMatches, numNameMatches,
                                archPointerSize, requiresRelocations
                            );
                    }

                    if ( removeImpDesc )
                    {
                        std::cout << "* terminated delay-load import module " << impDesc.DLLName << std::endl;

                        dstImpDescIter = exeImage.delayLoads.erase( dstImpDescIter );
                    }
                    else
                    {
                        dstImpDescIter++;
                    }
                }
            }

            // If any work was done then we should rewrite the imports table.
            if ( numNameMatches > 0 || numOrdinalMatches > 0 )
            {
                exeImage.importsAllocEntry = PEFile::PESectionAllocation();
            }

            // Output some helpful statistics.
            std::cout << "injected " << numNameMatches << " named and " << numOrdinalMatches << " ordinal PE imports" << std::endl;
        }

        // TODO: generate all code that depends on RVAs over here.

        // Do we need TLS data?
        if ( moduleImage.tlsInfo.startOfRawDataRef.GetSection() != NULL )
        {
            std::cout << "patching static TLS data references" << std::endl;

            // Calculate the VA to the TLS.
            std::uint64_t vaTLSData;
            {
                auto findIter = sectLinkMap.find( moduleImage.tlsInfo.allocEntry.GetSection() );

                assert( findIter != sectLinkMap.end() );

                vaTLSData = ( exeModuleBase + findIter->second.GetSection()->ResolveRVA( moduleImage.tlsInfo.allocEntry.ResolveInternalOffset( 0 ) ) );
            }

            // We do a simple patch of all TLS references to point directly inside the TLS data array.
            // This will disable all thread-local abilities but it will make the embedding work.
            moduleImage.ForAllSections(
                [&]( PEFile::PESection *modSect )
            {
                auto findIter = sectLinkMap.find( modSect );

                assert( findIter != sectLinkMap.end() );

                PEFile::PESection *exeSect = findIter->second.GetSection();

                // Only process sections that do contain executable code.
                if ( exeSect->chars.sect_mem_execute == false )
                    return;

                // Depending on architecture...
                if ( genCodeArch == asmjit::ArchInfo::kTypeX86 )
                {
                    static const char *patterns[] =
                    {
                        "\x06\x64\xa1\x2c\x00\x00\x00",         // mov eax,fs:[1Ch]
                        "\x07\x64\x8b\x1d\x2c\x00\x00\x00",     // mov ebx,fs:[1Ch]
                        "\x07\x64\x8b\x0d\x2c\x00\x00\x00",     // mov ecx,fs:[1Ch]
                        "\x07\x64\x8b\x15\x2c\x00\x00\x00",     // mov edx,fs:[1Ch]
                        "\x07\x64\x8b\x35\x2c\x00\x00\x00",     // mov esi,fs:[1Ch]
                        "\x07\x64\x8b\x3d\x2c\x00\x00\x00"      // mov edi,fs:[1Ch]
                    };

                    char *dataBuf = (char*)exeSect->stream.Data();

                    BufferPatternFind( dataBuf, (size_t)exeSect->stream.Size(), countof(patterns), patterns,
                        [&]( size_t patIdx, size_t bufOff, size_t matchSize )
                    {
                        // Just need to put a NOP.
                        // Then patch the offset with a new one.
                        char *curPtr = ( dataBuf + bufOff );

                        char methodChars[] = { 0x05, 0x1d, 0x0d, 0x15, 0x35, 0x3d };

                        *( curPtr + 0 ) = (char)0x8d;
                        *( curPtr + 1 ) = methodChars[ patIdx ];
                        // We want to give it the address of the static TLS thing.
                        *( (std::uint32_t*)( curPtr + 2 ) ) = (std::uint32_t)( vaTLSData + 0 );

                        // If the image is relocatable, add a relocation entry aswell.
                        if ( requiresRelocations )
                        {
                            exeImage.AddRelocation( exeSect->ResolveRVA( (std::uint32_t)( bufOff + 2 ) ), PEFile::PEBaseReloc::eRelocType::HIGHLOW );
                        }

                        // Pad the remainder with NOPs.
                        memset( curPtr + 6, 0x90, matchSize - 6 );
                    });
                }
                else if ( genCodeArch == asmjit::ArchInfo::kTypeX64 )
                {
                    // No idea what to do here; this is for later I guess.
                }
                else
                {
                    assert( 0 );
                }
            });
        }

        // So if we have TLS indices, we have to use the utility thunk to allocate into the array.
        if ( hasStaticTLS )
        {
            // Good read about TEB native entries:
            // http://www.geoffchappell.com/studies/windows/win32/ntdll/structs/teb/index.htm
            // But to keep things simple we ain't gonna do that.

            // TODO: since this Kobalicek dude does not provide a good way to get abs2abs relocation
            //  entries (he is incapable to understand the concept) we have a bug here. the only way to
            //  properly solve this is to patch asmjit itself, make it put an optional abs2abs entry
            //  per X86Mov (on demand, with a flag).
            // TODO: hacked around asmjit to get something automatic; might want to share with the author of
            //  asmjit.

            x86_asm.xor_( x86_asm.zax(), x86_asm.zax() ),
            x86_asm.mov( asmjit::X86Mem( embedImageBaseOffset + moduleImage.tlsInfo.addressOfIndexRef.GetRVA() ), x86_asm.zax() );
        }

        // Need this param for initializers.
        std::uint32_t dllInstanceHandle = 0;    // todo(?)

        // Call all initializers if we have some.
        if ( PEFile::PESection *tlsSect = moduleImage.tlsInfo.addressOfCallbacksRef.GetSection() )
        {
            std::cout << "linking TLS callbacks" << std::endl;

            std::uint32_t indexOfCallback = 0;

            std::uint32_t sectoffAddrOfCallbacks = moduleImage.tlsInfo.addressOfCallbacksRef.GetSectionOffset();

            while ( true )
            {
                std::uint64_t callbackPtr;

                tlsSect->stream.Seek( (std::int32_t)( sectoffAddrOfCallbacks + indexOfCallback * archPointerSize ) );

                // Advance the index to next.
                indexOfCallback++;

                if ( archPointerSize == 4 )
                {
                    std::uint32_t value;
                    bool gotValue = tlsSect->stream.ReadUInt32( value );

                    if ( !gotValue )
                    {
                        std::cout << "failed to read 32bit TLS callback value" << std::endl;

                        return -16;
                    }

                    callbackPtr = value;
                }
                else if ( archPointerSize == 8 )
                {
                    bool gotValue = tlsSect->stream.ReadUInt64( callbackPtr );

                    if ( !gotValue )
                    {
                        std::cout << "failed to read 64bit TLS callback value" << std::endl;

                        return -16;
                    }
                }
                else
                {
                    std::cout << "invalid architecture pointer size" << std::endl;

                    return -16;
                }

                if ( callbackPtr == 0 )
                {
                    break;
                }

                // Need to determine the redirected RVA.
                // For that we have to find the section this callback is targetting.
                std::uint32_t rvaToCallback = 0;
                {
                    std::uint32_t modrvaToCallback = (std::uint32_t)( callbackPtr - modImageBase );

                    std::uint32_t modTargetSectIntOff;
                    PEFile::PESection *modTargetSect = moduleImage.FindSectionByRVA( modrvaToCallback, NULL, &modTargetSectIntOff );

                    if ( modTargetSect )
                    {
                        auto findIter = sectLinkMap.find( modTargetSect );

                        assert( findIter != sectLinkMap.end() );

                        rvaToCallback = findIter->second.GetSection()->ResolveRVA( modTargetSectIntOff );
                    }
                }

                if ( rvaToCallback != 0 )
                {
                    // Call this function.
                    std::uint32_t paramReserved = 0;
                    std::uint32_t paramReason = 1;  // DLL_PROCESS_ATTACH

                    if ( genCodeArch == asmjit::ArchInfo::kTypeX86 )
                    {
                        x86_asm.push( paramReserved );
                        x86_asm.push( paramReason );
                        x86_asm.push( dllInstanceHandle );
                        x86_asm.call( rvaToCallback );
                    }
                    else if ( genCodeArch == asmjit::ArchInfo::kTypeX64 )
                    {
                        x86_asm.mov( asmjit::x86::rcx, dllInstanceHandle );
                        x86_asm.mov( asmjit::x86::rdx, paramReason );
                        x86_asm.mov( asmjit::x86::r8, paramReserved );
                        x86_asm.call( rvaToCallback );
                    }
                    else
                    {
                        std::cout << "failed to call TLS callback due to unknown architecture" << std::endl;

                        return -17;
                    }
                }
            }
        }

        // Check if we even have an entry point.
        const PEFile::PESectionDataReference& modEntryPointRef = moduleImage.peOptHeader.addressOfEntryPointRef;

        if ( modEntryPointRef.GetSection() == NULL )
        {
            std::cout << "error: no entry point in given module (no point in embedding)" << std::endl;

            return -19;
        }

        // Call into the DLL entry point with the default parameters.
        std::uint32_t rvaToDLLEntryPoint = calcRedirRVA( modEntryPointRef );
        {
            std::uint32_t paramReserved = 0;
            std::uint32_t paramReason = 1;      // DLL_PROCESS_ATTACH

            if ( genCodeArch == asmjit::ArchInfo::kTypeX86 )
            {
                x86_asm.push( paramReserved );
                x86_asm.push( paramReason );
                x86_asm.push( dllInstanceHandle );
                x86_asm.call( rvaToDLLEntryPoint );
            }
            else if ( genCodeArch == asmjit::ArchInfo::kTypeX64 )
            {
                // Call the DLL entry point.
                x86_asm.mov( asmjit::x86::rcx, dllInstanceHandle );
                x86_asm.mov( asmjit::x86::rdx, paramReason );
                x86_asm.mov( asmjit::x86::r8, paramReserved );
                x86_asm.call( rvaToDLLEntryPoint );

                // Since the next is a call that will never return, actually adjust the stack.
                x86_asm.sub( asmjit::x86::rsp, 8 );
            }
            else
            {
                std::cout << "unknown target machine architecture for entry point generation" << std::endl;

                return -12;
            }
        }

        // Success!
        return 0;
    }
};

// Fetches a filename from a file path, or at least attempts to.
static const char* FetchFileName( const char *path )
{
    // We skip till the last slash character.
    const char *pathIter = path;
    const char *last_file_name = pathIter;

    while ( true )
    {
        char c = *pathIter;

        if ( c == 0 )
        {
            break;
        }

        pathIter++;

        if ( c == '/' || c == '\\' )
        {
            last_file_name = pathIter;
        }
    }

    return last_file_name;
}

int main( int argc, char *argv[] )
{
    std::cout <<
        "pefrmdllembed - Inject DLL file into EXE file, compiled on " __DATE__ << std::endl
     << "visit http://pefrm-units.osdn.jp/pefrmdllembed.html" << std::endl << std::endl;

    // Syntax: pefrmdllembed.exe *OPTIONS* *input exe filename* *input mod1 filename* *input mod2 filename* ... *input modn filename* *output exe filename*

    size_t curArg = 1;

    bool doFixEntryPoint = false;
    bool doInjectMatchingImports = false;
    bool doPrintHelp = false;

    if ( argc >= 1 )
    {
        // Parse all options.
        OptionParser optParser( (const char**)argv + curArg, (size_t)argc - curArg );

        while ( true )
        {
            std::string opt = optParser.FetchOption();

            if ( opt.empty() )
                break;

            if ( opt == "entryfix" || opt == "efix" )
            {
                doFixEntryPoint = true;
            }
            else if ( opt == "injimp" || opt == "impinj" )
            {
                doInjectMatchingImports = true;
            }
            else if ( opt == "help" || opt == "h" || opt == "?" )
            {
                doPrintHelp = true;
            }
            else
            {
                std::cout << "unknown cmdline option: " << opt << std::endl;
            }
        }

        size_t optArgIndex = optParser.GetArgIndex();

        curArg += optArgIndex;

        argc -= (int)optArgIndex;
    }

    // If we print help, then we just do that and quit.
    if ( doPrintHelp )
    {
        std::cout << "USAGE: -[options] *input.exe* *input1.dll* *input2.dll* ... *inputn.dll* *output.exe*" << std::endl;
        std::cout << std::endl;

        std::cout << "Option Descriptions:" << std::endl;
        std::cout << "-efix: restores original executable entry point in PE header after DLL load" << std::endl;
        std::cout << "-injimp: hooks executable imports with input DLL exports" << std::endl;
        std::cout << "-help: prints this help text" << std::endl;

        return 0;
    }

    // Fetch possible input executable and input module from arguments.
    const char *inputExecImageName = "input.exe";

    if ( argc >= 2 )
    {
        inputExecImageName = argv[curArg++];
    }

    // Calculate the amount of module images to embed.
    unsigned int numberModules = 1;

    if ( argc >= 5 )
    {
        numberModules = (unsigned int)( argc - 3 );
    }

    std::vector <const char*> toEmbedList;

    if ( argc >= 3 )
    {
        toEmbedList.reserve( numberModules );

        for ( unsigned int n = 0; n < numberModules; n++ )
        {
            const char *inputModImageName = argv[curArg++];

            toEmbedList.push_back( inputModImageName );
        }
    }
    else
    {
        toEmbedList.push_back( "input.dll" );
    }

    const char *outputModImageName = "output.exe";

    if ( argc >= 4 )
    {
        outputModImageName = argv[curArg++];
    }

    // Create a nice debug string.
    {
        std::cout << "loading: \"" << inputExecImageName << "\"";
             
        for ( unsigned int n = 0; n < numberModules; n++ )
        {
            const char *inputModImageName = toEmbedList[ n ];

            std::cout << ", \"" << inputModImageName << "\"";
        }
            
        std::cout << std::endl << std::endl;
    }

    // TODO: create a code building environment and make the DLL embedding a method of it.
    //  This should optimize the code generation output, which in terms reduces the section
    //  throughput (good due to Windows NT loader limits).

    int iReturnCode;

    try
    {
        // Load both PE images.
        PEFile exeImage;
        {
            std::cout << "loading executable image (" << inputExecImageName << ")" << std::endl;

            std::fstream stlFileStream( inputExecImageName, std::ios::binary | std::ios::in );

            if ( !stlFileStream.good() )
            {
                std::cout << "failed to load executable image" << std::endl;

                return -1;
            }

            PEStreamSTL peStream( &stlFileStream );

            exeImage.LoadFromDisk( &peStream );
        }

        // Initialize the environment.
        std::uint16_t exeMachineType = exeImage.pe_finfo.machine_id;

        // We only support x86 and x64 for now.
        if ( exeMachineType != PEL_IMAGE_FILE_MACHINE_I386 && exeMachineType != PEL_IMAGE_FILE_MACHINE_AMD64 )
        {
            throw runtime_exception( -4, "unsupported machine type (x86 or x64 only)" );
        }

        // Check that the executable is an EXE.
        if ( exeImage.pe_finfo.isDLL != false )
        {
            throw runtime_exception( -5, "provided executable image is not an executable image" );
        }

        // Decide what architecture we generate code for.
        std::uint8_t genCodeArch = asmjit::ArchInfo::kTypeNone;

        std::uint32_t archPointerSize;

        if ( exeMachineType == PEL_IMAGE_FILE_MACHINE_I386 )
        {
            genCodeArch = asmjit::ArchInfo::kTypeX86;

            archPointerSize = 4;

            std::cout << "architecture: 32bit" << std::endl;
        }
        else if ( exeMachineType == PEL_IMAGE_FILE_MACHINE_AMD64 )
        {
            genCodeArch = asmjit::ArchInfo::kTypeX64;

            archPointerSize = 8;

            std::cout << "architecture: 64bit" << std::endl;
        }
        else
        {
            throw runtime_exception( -7, "invalid machine type for asmjit code generation" );
        }

        // Check if we have to embed any new relocations.
        bool requiresRelocations = ( exeImage.HasRelocationInfo() == true );

        // We want to generate specialized code as executable entry point.
        // This allows us to do specialized patching according to rules of PE merging.
        asmjit::CodeInfo asmCodeInfo( genCodeArch );

        asmjit::CodeHolder asmCodeHolder;
        asmCodeHolder.init( asmCodeInfo );

        // We need to remember a label of the entry point.
        asmjit::Label entryPointLabel;
        {
            AssemblyEnvironment asmEnv( exeImage, &asmCodeHolder );

            asmjit::X86Assembler& x86_asm = asmEnv.x86_asm;

            // Now the entry point starts.
            entryPointLabel = x86_asm.newLabel();

            if ( genCodeArch == asmjit::ArchInfo::kTypeX64 )
            {
                // Align the stack pointer.
                x86_asm.mov( asmjit::x86::rax, asmjit::x86::rsp );
                x86_asm.and_( asmjit::x86::rax, 0x0F );
                x86_asm.sub( asmjit::x86::rsp, asmjit::x86::rax );

                // Reserve register spill space.
                x86_asm.sub( asmjit::x86::rsp, 0x20 );
            }

            // User could have requested to fix the entry point in the original executable to the previous
            // one because it is used for version detection by some executable logic.
            if ( doFixEntryPoint )
            {
                std::cout << "adjusting executable entry point to old on startup ..." << std::endl;

                PEFile::PESection metaSection;
                metaSection.shortName = ".meta";
                metaSection.chars.sect_mem_write = true;

                // Determine the size of a thunk entry.
                // It depends on the image type.
                size_t thunkEntrySize;

                if ( exeImage.isExtendedFormat )
                {
                    thunkEntrySize = 8;
                }
                else
                {
                    thunkEntrySize = 4;
                }

                // We must allocate certain functions that we should use for memory management.
                PEFile::PESectionAllocation utilThunk;
                {
                    PEFile::PEImportDesc utilImports;
                    utilImports.DLLName = "KERNEL32.DLL";
                    {
                        PEFile::PEImportDesc::importFunc fVirtualProtect;
                        fVirtualProtect.name = "VirtualProtect";
                        fVirtualProtect.isOrdinalImport = false;
                        fVirtualProtect.ordinal_hint = 0;
                        utilImports.funcs.push_back( std::move( fVirtualProtect ) );
                    }

                    // Give it some memory region.
                    size_t utilThunkSize = ( thunkEntrySize * utilImports.funcs.size() );

                    metaSection.Allocate( utilThunk, (std::uint32_t)utilThunkSize, (std::uint32_t)thunkEntrySize );

                    // We want to link the thunk into the imports desc.
                    utilImports.firstThunkRef = utilThunk;

                    exeImage.imports.push_back( std::move( utilImports ) );
                }
            
                if ( metaSection.IsEmpty() == false )
                {
                    metaSection.Finalize();

                    exeImage.AddSection( std::move( metaSection ) );
                }

                // Now since we have module RVAs for the util imports, we
                // can generate code that uses them.
                // Unprotect the beginning of the image, write the original executable
                // entry point into the PE optional header and protect it again.
                // We assume that the type of image (PE32 or PE32+) will never change.
                // Unless you are a PE hacker and want to create that tool.

                // Unprotect the first 1024 bytes of the image.
                if ( genCodeArch == asmjit::ArchInfo::kTypeX86 )
                {
                    x86_asm.sub( asmjit::x86::esp, 4 );
                    x86_asm.push( asmjit::x86::esp );
                    x86_asm.push( (std::uint32_t)_PAGE_READWRITE );
                    x86_asm.push( (std::uint32_t)1024 );
                    x86_asm.push( asmjit::Imm( 0, true ) );
                }
                else if ( genCodeArch == asmjit::ArchInfo::kTypeX64 )
                {
                    x86_asm.sub( asmjit::x86::rsp, 16 );
                    x86_asm.mov( asmjit::x86::rcx, asmjit::Imm( 0, true ) );
                    x86_asm.mov( asmjit::x86::rdx, 1024u );
                    x86_asm.mov( asmjit::x86::r8, (std::uint32_t)_PAGE_READWRITE );
                    x86_asm.mov( asmjit::x86::r9, asmjit::x86::rsp );
                }
                else
                {
                    assert( 0 );

                    return -1;
                }

                // Call the thunk function directly.
                asmjit::X86Mem fVirtualProtect( utilThunk.ResolveOffset( 0 ), (std::uint32_t)thunkEntrySize );

                x86_asm.call( fVirtualProtect );

                // Fix entry point with old.
                x86_asm.mov( x86_asm.zax(), asmjit::X86Mem( offsetof(PEStructures::IMAGE_DOS_HEADER, e_lfanew), sizeof(std::int32_t) ) );

                std::uint32_t structEP_off;

                if ( exeImage.isExtendedFormat )
                {
                    structEP_off = offsetof(PEStructures::IMAGE_OPTIONAL_HEADER64, AddressOfEntryPoint);
                }
                else
                {
                    structEP_off = offsetof(PEStructures::IMAGE_OPTIONAL_HEADER32, AddressOfEntryPoint);
                }

                // We need to skip some data.
                structEP_off += sizeof(PEStructures::IMAGE_PE_HEADER) + sizeof(std::uint16_t);

                x86_asm.add( x86_asm.zax(), asmjit::Imm( 0, true ) );
                x86_asm.mov( asmjit::X86Mem( x86_asm.zax(), structEP_off, 4 ), exeImage.peOptHeader.addressOfEntryPointRef.GetRVA() );

                // Protect the memory again.
                if ( genCodeArch == asmjit::ArchInfo::kTypeX86 )
                {
                    x86_asm.push( asmjit::x86::esp );
                    x86_asm.push( asmjit::X86Mem( asmjit::x86::esp, 4, 4 ) );
                    x86_asm.push( (std::uint32_t)1024u );
                    x86_asm.push( asmjit::Imm( 0, true ) );
                }
                else if ( genCodeArch == asmjit::ArchInfo::kTypeX64 )
                {
                    x86_asm.mov( asmjit::x86::rcx, asmjit::Imm( 0, true ) );
                    x86_asm.mov( asmjit::x86::rdx, asmjit::X86Mem( asmjit::x86::rsp, 0, 4 ) );
                    x86_asm.mov( asmjit::x86::r8, 1024u );
                    x86_asm.mov( asmjit::x86::r9, asmjit::x86::rsp );
                }
                else
                {
                    assert( 0 );
                }

                // Call the thunk function directly.
                x86_asm.call( fVirtualProtect );

                // Release stack space for the oldProt variable.
                if ( genCodeArch == asmjit::ArchInfo::kTypeX86 )
                {
                    x86_asm.add( asmjit::x86::esp, 4 );
                }
                else if ( genCodeArch == asmjit::ArchInfo::kTypeX64 )
                {
                    x86_asm.add( asmjit::x86::rsp, 16 );
                }
                else
                {
                    assert( 0 );
                }
            }

            // Embed each requested image.
            for ( unsigned int n = 0; n < numberModules; n++ )
            {
                const char *inputModImageName = toEmbedList[ n ];

                PEFile moduleImage;
                {
                    std::cout << "loading module image (" << inputModImageName << ")" << std::endl;

                    std::fstream stlFileStream( inputModImageName, std::ios::binary | std::ios::in );

                    if ( !stlFileStream.good() )
                    {
                        std::cout << "failed to load module image" << std::endl;

                        return -2;
                    }

                    PEStreamSTL peStream( &stlFileStream );

                    moduleImage.LoadFromDisk( &peStream );
                }

                std::uint16_t modMachineType = moduleImage.pe_finfo.machine_id;

                // Check that both images are of same machine type.
                if ( exeMachineType != modMachineType )
                {
                    std::cout << "machine types of images do not match" << std::endl;

                    return -3;
                }

                // Fetch module name.
                const char *moduleFileName = FetchFileName( inputModImageName );

                // Perform the embedding.
                int statusEmbed = asmEnv.EmbedModuleIntoExecutable( moduleImage, requiresRelocations, moduleFileName, doInjectMatchingImports, archPointerSize );

                if ( statusEmbed != 0 )
                {
                    return statusEmbed;
                }

                // Print some seperation for easier log viewing.
                if ( n + 1 != numberModules )
                {
                    std::cout << std::endl;
                }
            }

            // We jump to the original executable entry point.
            x86_asm.jmp( exeImage.peOptHeader.addressOfEntryPointRef.GetRVA() );

            // Finished generating code.
        }

        // Notify that there is now a divide between module code generation and asmjit embedding.
        std::cout << std::endl;

        // Commit the code into the buffers.
        asmCodeHolder.sync();

        // We have to embed all asmjit sections into our executable aswell.
        {
            std::cout << "linking asmjit code into executable" << std::endl;

            PEFile::PESectionDataReference entryPointRef;
            bool couldLinkCode = asmjitshared::EmbedASMJITCodeIntoModule( exeImage, requiresRelocations, asmCodeHolder, entryPointLabel, entryPointRef );

            if ( !couldLinkCode )
            {
                std::cout << "failed to link asmjit code into executable" << std::endl;

                return -10;
            }

            // Make our executable entry point to our newly compiled routine.
            exeImage.peOptHeader.addressOfEntryPointRef = std::move( entryPointRef );

            // Finito.
        }

        // Write out the new executable image.
        {
            std::cout << "writing output image (" << outputModImageName << ")" << std::endl;

            std::fstream stlStreamOut( outputModImageName, std::ios::binary | std::ios::out );

            if ( !stlStreamOut.good() )
            {
                std::cout << "failed to create output file (" << outputModImageName << ")" << std::endl;

                return -18;
            }

            PEStreamSTL peOutStream( &stlStreamOut );

            exeImage.WriteToStream( &peOutStream );
        }

        // Success!
        iReturnCode = 0;
    }
    catch( peframework_exception& except )
    {
        std::cout << "error: " << except.desc_str() << std::endl;

        iReturnCode = -42;

        // Continue.
    }
    catch( runtime_exception& except )
    {
        std::cout << except.msg << std::endl;

        iReturnCode = except.error_code;

        // Continue.
    }

    return iReturnCode;
}