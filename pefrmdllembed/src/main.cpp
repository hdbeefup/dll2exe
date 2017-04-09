#include <peframework.h>
#define ASMJIT_STATIC
#include <asmjit/asmjit.h>

#undef ABSOLUTE

#include <unordered_map>

#include <fstream>

#include <asmjitshared.h>

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

int main( int argc, char *argv[] )
{
    std::cout <<
        "pefrmdllembed - Inject DLL file into EXE file" << std::endl
     << "visit http://pefrm-units.osdn.jp/pefrmdllembed.html" << std::endl << std::endl;

    // Fetch possible input executable and input module from arguments.
    const char *inputExecImageName = "input.exe";

    if ( argc >= 2 )
    {
        inputExecImageName = argv[1];
    }

    const char *inputModImageName = "input.dll";

    if ( argc >= 3 )
    {
        inputModImageName = argv[2];
    }

    const char *outputModImageName = "output.exe";

    if ( argc >= 4 )
    {
        outputModImageName = argv[3];
    }

    std::cout << "loading: \"" << inputExecImageName << "\", \"" << inputModImageName << "\"" << std::endl << std::endl;

    // We leave out all kinds of exception management as an exercise to the reader.

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

    // Check that both images are of same machine type.
    std::uint16_t exeMachineType = exeImage.pe_finfo.machine_id;
    std::uint16_t modMachineType = moduleImage.pe_finfo.machine_id;

    if ( exeMachineType != modMachineType )
    {
        std::cout << "machine types of images do not match" << std::endl;

        return -3;
    }

    // We only support x86 and x64 for now.
    if ( exeMachineType != PEL_IMAGE_FILE_MACHINE_I386 && exeMachineType != PEL_IMAGE_FILE_MACHINE_AMD64 )
    {
        std::cout << "unsupported machine type (x86 or x64 only)" << std::endl;

        return -4;
    }

    // Check that the module is a DLL and the executable is an EXE.
    if ( exeImage.pe_finfo.isDLL != false )
    {
        std::cout << "provided executable image is not an executable image" << std::endl;

        return -5;
    }

    if ( moduleImage.pe_finfo.isDLL != true )
    {
        std::cout << "provided DLL image is not a DLL image" << std::endl;

        return -6;
    }

    // Get required meta-data.
    std::uint64_t exeModuleBase = exeImage.GetImageBase();

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
        std::cout << "invalid machine type for asmjit code generation" << std::endl;

        return -7;
    }

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

    // Check if we have to embed any new relocations.
    bool requiresRelocations = ( exeImage.HasRelocationInfo() == true );

    // We need the module image base for rebase pointer fixing.
    std::uint64_t modImageBase = moduleImage.GetImageBase();

    // Special allocations of data inside the executable image that cannot be stored in
    // the executable itself.
    std::list <PEFile::PESectionAllocation> exeModuleAllocs;

    // We want to generate specialized code as executable entry point.
    // This allows us to do specialized patching according to rules of PE merging.
    asmjit::CodeInfo asmCodeInfo( genCodeArch );

    asmjit::CodeHolder asmCodeHolder;
    asmCodeHolder.init( asmCodeInfo );

    // We need to remember a label of the entry point.
    asmjit::Label entryPointLabel;

    // Generate code along with binding.
    {
        asmjit::X86Assembler x86_asm( &asmCodeHolder );

        // Perform binding of PE references.
        // We keep a list of all sections that we put into the executable image.
        // This is important to transfer all the remaining data that is tied to sections.
        std::unordered_map
            <PEFile::PESection *, // we assume the original module stays immutable.
                PEFile::PESectionReference> sectLinkMap;

        sectLinkMap.reserve( moduleImage.GetSectionCount() );

        auto calcRedirRef = [&]( const PEFile::PESectionDataReference& srcRef ) -> PEFile::PESectionDataReference
        {
            PEFile::PESection *srcSect = srcRef.GetSection();

            assert( srcSect != NULL );

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

            assert( srcSect != NULL );

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
                std::cout << "fatal: failed to allocate module section in executable image" << std::endl;

                exit( -14 );
            }

            PEFile::PESectionReference sectInsideRef( refInside );

            // Remember this link.
            sectLinkMap[ theSect ] = std::move( sectInsideRef );
        });

        // Maybe we need to create a new meta-section.
        PEFile::PESection metaSection;
        metaSection.shortName = ".meta";
        metaSection.chars.sect_containsInitData = true;
        metaSection.chars.sect_mem_execute = false;
        metaSection.chars.sect_mem_read = true;
        metaSection.chars.sect_mem_write = true;    // because we inject rogue IATs.

        // Decide about the thunk entry size.
        std::uint32_t thunkEntrySize = archPointerSize;

        // Embed all import directories.
        size_t oldImpDirCount = exeImage.imports.size();
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
                size_t modImpCount = impDesc.funcs.size();

                for ( size_t n = 0; n < modImpCount; n++ )
                {
                    const PEFile::PEImportDesc::importFunc& impFunc = impDesc.funcs[ n ];

                    PEFile::PEImportDesc::importFunc carbonCopy;
                    carbonCopy.isOrdinalImport = impFunc.isOrdinalImport;
                    carbonCopy.name = impFunc.name;
                    carbonCopy.ordinal_hint = impFunc.ordinal_hint;

                    newImports.funcs.push_back( std::move( carbonCopy ) );
                }

                // Since we are spreading thunk IATs across the executable image we cannot
                // use the Win32 PE loader feature to store them in read-only sections.
                // We have to bundle all IATs in one place to do that.
                // Solution: make the section of the IAT writable (hack!)

                auto findIter = sectLinkMap.find( impDesc.firstThunkRef.GetSection() );

                assert( findIter != sectLinkMap.end() );

                PEFile::PESection *modRedirSect = findIter->second.GetSection();

                modRedirSect->chars.sect_mem_write = true;

                PEFile::PESectionDataReference newThunkRef( modRedirSect, impDesc.firstThunkRef.GetSectionOffset() );

                // Remember this.
                newImports.firstThunkRef = std::move( newThunkRef );

                exeImage.imports.push_back( std::move( newImports ) );
            }

            // Make sure we rewrite the imports directory.
            exeImage.importsAllocEntry = PEFile::PESectionAllocation();
        }

        // Just for the heck of it we could embed exports aswell.

        // If the module image has TLS indices we have to initialize
        // utility functions in a special thunk array.
        PEFile::PESectionDataReference utilityThunkRef;

        bool hasStaticTLS = ( moduleImage.tlsInfo.addressOfIndexRef.GetSection() != NULL );
        
        if ( hasStaticTLS )
        {
            std::cout << "WARNING: module image has static TLS; might not work as expected" << std::endl;

            PEFile::PEImportDesc utilityDesc;
            utilityDesc.DLLName = "Kernel32.dll";
            
            // All functions we need.
            {
                PEFile::PEImportDesc::importFunc tlsAllocInfo;
                tlsAllocInfo.isOrdinalImport = false;
                tlsAllocInfo.name = "TlsAlloc";
                tlsAllocInfo.ordinal_hint = 0;
                utilityDesc.funcs.push_back( std::move( tlsAllocInfo ) );

                PEFile::PEImportDesc::importFunc tlsSetValueInfo;
                tlsSetValueInfo.isOrdinalImport = false;
                tlsSetValueInfo.name = "TlsSetValue";
                tlsSetValueInfo.ordinal_hint = 0;
                utilityDesc.funcs.push_back( std::move( tlsSetValueInfo ) );

                PEFile::PEImportDesc::importFunc virtualAllocInfo;
                virtualAllocInfo.isOrdinalImport = false;
                virtualAllocInfo.name = "VirtualAlloc";
                virtualAllocInfo.ordinal_hint = 0;
                utilityDesc.funcs.push_back( std::move( virtualAllocInfo ) );
            }

            // Allocate a thunk array as destination.
            const std::uint32_t thunkArraySize = (std::uint32_t)( archPointerSize * utilityDesc.funcs.size() );

            PEFile::PESectionAllocation thunkAllocEntry;
            metaSection.Allocate( thunkAllocEntry, thunkArraySize );

            // Put into the import descriptor.
            utilityDesc.firstThunkRef = PEFile::PESectionDataReference( thunkAllocEntry.GetSection(), thunkAllocEntry.ResolveInternalOffset( 0 ) );

            // Remember for the code.
            utilityThunkRef = PEFile::PESectionDataReference( thunkAllocEntry.GetSection(), thunkAllocEntry.ResolveInternalOffset( 0 ) );

            exeModuleAllocs.push_back( std::move( thunkAllocEntry ) );

            exeImage.imports.push_back( std::move( utilityDesc ) );

            // Invalidate the native array.
            exeImage.importsAllocEntry = PEFile::PESectionAllocation();
        }

        // Embed the section, if required.
        if ( metaSection.IsEmpty() == false )
        {
            metaSection.Finalize();

            exeImage.AddSection( std::move( metaSection ) );
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

                    PEFile::PEBaseReloc::eRelocType relocType = modRelocItem.type;

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

                    BufferPatternFind( dataBuf, (size_t)exeSect->stream.Size(), _countof(patterns), patterns,
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
            assert( utilityThunkRef.GetSection() != NULL );

            x86_asm.xor_( x86_asm.zax(), x86_asm.zax() ),
            x86_asm.mov( asmjit::X86Mem( exeModuleBase + embedImageBaseOffset + moduleImage.tlsInfo.addressOfIndexRef.GetRVA() ), x86_asm.zax() );
        }

        // Need this param for initializers.
        std::uint32_t dllInstanceHandle = 0;    // todo(?)

        // Call all initializers if we have some.
        if ( PEFile::PESection *tlsSect = moduleImage.tlsInfo.addressOfCallbacksRef.GetSection() )
        {
            std::cout << "linking TLS callbacks" << std::endl;

            std::uint32_t indexOfCallback = 0;

            while ( true )
            {
                std::uint64_t callbackPtr;

                tlsSect->stream.Seek( (std::int32_t)( indexOfCallback * archPointerSize ) );

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

                // Call this function.
                std::uint32_t rvaToCallback = (std::uint32_t)( callbackPtr - exeModuleBase );
                
                std::uint32_t paramReserved = 0;
                std::uint32_t paramReason = DLL_PROCESS_ATTACH;

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

        // Call into the DLL entry point with the default parameters.
        std::uint32_t rvaToDLLEntryPoint = calcRedirRVA( moduleImage.peOptHeader.addressOfEntryPointRef );
        {
            std::uint32_t paramReserved = 0;
            std::uint32_t paramReason = DLL_PROCESS_ATTACH;

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

        // We jump to the original executable entry point.
        x86_asm.jmp( exeImage.peOptHeader.addressOfEntryPointRef.GetRVA() );

        // Finished generating code.
    }

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

        PEStreamSTL peOutStream( &stlStreamOut );

        exeImage.WriteToStream( &peOutStream );
    }

    // Success!

    return 0;
}