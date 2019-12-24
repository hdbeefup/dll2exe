#define ASMJIT_STATIC
#include <asmjit/asmjit.h>

#undef ABSOLUTE

#include <peframework.h>

#include <assert.h>

namespace asmjitshared
{

bool EmbedASMJITCodeIntoModule(
    PEFile& exeImage, bool requiresRelocations, const asmjit::CodeHolder& asmCodeHolder, const asmjit::Label& entryPointLabel,
    PEFile::PESectionDataReference& entryPointRefOut
)
{
    std::uint64_t exeModuleBase = exeImage.GetImageBase();

    const auto& asmSections = asmCodeHolder.getSections();

    size_t asmSectCount = asmSections.getLength();

    // We have to link asmjit sections with our PE executable sections.
    peMap <std::uint32_t, PEFile::PESectionReference> asmSectLink;

    for ( size_t n = 0; n < asmSectCount; n++ )
    {
        const asmjit::SectionEntry *theSect = asmSections.getAt( n );

        const asmjit::CodeBuffer& srcBuf = theSect->getBuffer();

        // Make sure we do not take-over empty sections.
        size_t bufSize = srcBuf.getLength();

        if ( bufSize == 0 )
            continue;

        std::uint32_t sectFlags = theSect->getFlags();

        // Do not take over implicit sections.
        if ( ( sectFlags & asmjit::SectionEntry::kFlagImplicit ) != 0 )
        {
            continue;
        }

        PEFile::PESection asmSection;
        asmSection.shortName = theSect->getName();

        // Set special properties.
        {
            bool isExecutable = ( sectFlags & asmjit::SectionEntry::kFlagExec ) != 0;
            //bool isZero = ( sectFlags & asmjit::SectionEntry::kFlagZero ) != 0;
            bool isConst = ( sectFlags & asmjit::SectionEntry::kFlagConst ) != 0;

            asmSection.chars.sect_hasNoPadding = false;
            asmSection.chars.sect_containsCode = isExecutable;
            asmSection.chars.sect_containsInitData = true;
            asmSection.chars.sect_containsUninitData = false;
            asmSection.chars.sect_link_other = false;
            asmSection.chars.sect_link_info = false;
            asmSection.chars.sect_link_remove = false;
            asmSection.chars.sect_link_comdat = false;
            asmSection.chars.sect_noDeferSpecExcepts = false;
            asmSection.chars.sect_gprel = false;
            asmSection.chars.sect_mem_farData = false;
            asmSection.chars.sect_mem_purgeable = false;
            asmSection.chars.sect_mem_16bit = false;
            asmSection.chars.sect_mem_locked = false;
            asmSection.chars.sect_mem_preload = false;

            // Process alignment.
            PEFile::PESection::eAlignment sectAlign = PEFile::PESection::eAlignment::BYTES_512;

            std::uint32_t reqAlign = theSect->getAlignment();

            switch( reqAlign )
            {
            case 0:         sectAlign = PEFile::PESection::eAlignment::BYTES_UNSPECIFIED; break;
            case 1:         sectAlign = PEFile::PESection::eAlignment::BYTES_1; break;
            case 2:         sectAlign = PEFile::PESection::eAlignment::BYTES_2; break;
            case 4:         sectAlign = PEFile::PESection::eAlignment::BYTES_4; break;
            case 8:         sectAlign = PEFile::PESection::eAlignment::BYTES_8; break;
            case 16:        sectAlign = PEFile::PESection::eAlignment::BYTES_16; break;
            case 32:        sectAlign = PEFile::PESection::eAlignment::BYTES_32; break;
            case 64:        sectAlign = PEFile::PESection::eAlignment::BYTES_64; break;
            case 128:       sectAlign = PEFile::PESection::eAlignment::BYTES_128; break;
            case 256:       sectAlign = PEFile::PESection::eAlignment::BYTES_256; break;
            case 512:       sectAlign = PEFile::PESection::eAlignment::BYTES_512; break;
            case 1024:      sectAlign = PEFile::PESection::eAlignment::BYTES_1024; break;
            case 2048:      sectAlign = PEFile::PESection::eAlignment::BYTES_2048; break;
            case 4096:      sectAlign = PEFile::PESection::eAlignment::BYTES_4096; break;
            case 8192:      sectAlign = PEFile::PESection::eAlignment::BYTES_8192; break;
            default:
                return false;
            }

            asmSection.chars.sect_alignment = sectAlign;
            asmSection.chars.sect_link_nreloc_ovfl = false;
            asmSection.chars.sect_mem_discardable = false;
            asmSection.chars.sect_mem_not_cached = false;
            asmSection.chars.sect_mem_not_paged = false;
            asmSection.chars.sect_mem_shared = false;
            asmSection.chars.sect_mem_execute = isExecutable;
            asmSection.chars.sect_mem_read = true;
            asmSection.chars.sect_mem_write = ( isConst == false );
        }

        // Copy in all the data.
        asmSection.stream.Seek( 0 );
        asmSection.stream.Write( srcBuf.getData(), bufSize );

        // Allocate enough virtual size.
        asmSection.Finalize();

        // Store it inside of our executable.
        PEFile::PESection *insideLink = exeImage.AddSection( std::move( asmSection ) );

        // Link things together.
        asmSectLink[ theSect->getId() ] = insideLink;
    }

    // Process relocations of the code.
    // This will cast new relocation entries into the executable.
    {
        const auto& asmRelocs = asmCodeHolder.getRelocEntries();

        size_t numRelocs = asmRelocs.getLength();

        for ( size_t n = 0; n < numRelocs; n++ )
        {
            const asmjit::RelocEntry *asmRelocEntry = asmRelocs.getAt( n );

            // We assume that we get a section for each relocation entry (that asmjit does not screw up).
            std::uint32_t srcSectIndex = asmRelocEntry->getSourceSectionId();
            std::uint32_t dstSectIndex = asmRelocEntry->getTargetSectionId();

            if ( dstSectIndex == asmjit::SectionEntry::kInvalidId )
            {
                dstSectIndex = 0;
            }

            // Fetch the sections.
            auto iterSrcSect = asmSectLink.Find( srcSectIndex );

            assert( iterSrcSect != nullptr );

            PEFile::PESection *srcSect = iterSrcSect->GetValue().GetSection();

            auto iterDstSect = asmSectLink.Find( dstSectIndex );

            assert( iterDstSect != nullptr );

            // TODO: fix this and use the destination section.
            //PEFile::PESection *dstSect = iterDstSect->second.GetSection();

            // Perform the relocation adjustment.
            // We first add the image base to the data that is pointed-at.
            std::uint32_t asmRelocType = asmRelocEntry->getType();

            if ( asmRelocType == asmjit::RelocEntry::kTypeNone )
            {
                // Ignore this entry.
                continue;
            }

            // I assume that asmjit does just use HIGHLOW and DIR64 relocation things.
            // This is pretty easy to implement.
            std::uint32_t relocSize = asmRelocEntry->getSize();

            PEFile::PEBaseReloc::eRelocType peRelocType = PEFile::PEBaseReloc::eRelocType::ABSOLUTE;

            if ( relocSize == 4 )
            {
                peRelocType = PEFile::PEBaseReloc::eRelocType::HIGHLOW;
            }
            else if ( relocSize == 8 )
            {
                peRelocType = PEFile::PEBaseReloc::eRelocType::DIR64;
            }
            else
            {
                return false;
            }

            // Get ready to access the section.
            std::uint32_t relSectOffset = (std::uint32_t)asmRelocEntry->getSourceOffset();

            srcSect->stream.Seek( relSectOffset );

            // I had to look at the implementation of asmjit's relocate function to get the gist of it.
            // This is because the structures of asmjit gave me no clue; they lack in detail sometimes.

            std::uint64_t writePtr;

            if ( asmRelocType == asmjit::RelocEntry::kTypeAbsToAbs )
            {
                // The absolute offset is a RVA, so we should transform it into a real memory pointer.
                writePtr = exeModuleBase + asmRelocEntry->getData();
            }
            else if ( asmRelocType == asmjit::RelocEntry::kTypeRelToAbs )
            {
                // A relative pointer is based on the current relocation location.
                std::uint32_t rvaToReloc = srcSect->ResolveRVA( relSectOffset + relocSize );

                writePtr = rvaToReloc + asmRelocEntry->getData() + exeModuleBase;
            }
            else if ( asmRelocType == asmjit::RelocEntry::kTypeAbsToRel )
            {
                // Since we are an absolute offset we are a RVA into the executable module.
                // In that case we have to subtract by the RVA to the relocation.
                std::uint32_t rvaToReloc = srcSect->ResolveRVA( relSectOffset + relocSize );

                std::uint32_t rvaToTarget = (std::uint32_t)asmRelocEntry->getData();

                writePtr = rvaToTarget - rvaToReloc;
            }
            else if ( asmRelocType == asmjit::RelocEntry::kTypeTrampoline )
            {
                // A trampoline is unknown. We need to find out more about it.
                // We assume it is same as abs2rel.
                std::uint32_t rvaToReloc = srcSect->ResolveRVA( relSectOffset + relocSize );

                std::uint32_t rvaToTarget = (std::uint32_t)asmRelocEntry->getData();

                writePtr = rvaToTarget - rvaToReloc;
            }
            else
            {
                // Unexpected relocation type, so bail.
                return false;
            }

            if ( peRelocType == PEFile::PEBaseReloc::eRelocType::HIGHLOW )
            {
                srcSect->stream.WriteUInt32( (std::uint32_t)writePtr );
            }
            else if ( peRelocType == PEFile::PEBaseReloc::eRelocType::DIR64 )
            {
                srcSect->stream.WriteUInt64( writePtr );
            }
            else
            {
                assert( 0 );
            }

            // TODO: think about cross-section relocations.
            //  those would require adjustment of the pointer between reading and writing.

            if ( requiresRelocations )
            {
                // Register this relocation into the PE image.
                exeImage.AddRelocation( srcSect->ResolveRVA( relSectOffset ), peRelocType );
            }
        }
    }

    // We should return the entry point to the code.
    const asmjit::LabelEntry *entryPointLabelEntry = asmCodeHolder.getLabelEntry( entryPointLabel );

    if ( entryPointLabelEntry == nullptr )
        return false;

    if ( !entryPointLabelEntry->isBound() == false )
        return false;

    // Fetch the generated offset.
    std::uint32_t asmEntryPointSectID = entryPointLabelEntry->getSectionId();

    // FIXME:
    if ( asmEntryPointSectID == asmjit::SectionEntry::kInvalidId )
    {
        asmEntryPointSectID = 0;
    }

    PEFile::PESection *asmEntryPointSect = asmSectLink[asmEntryPointSectID].GetSection();

    assert( asmEntryPointSect != nullptr );

    std::uint32_t asmEntryPointOffset = (std::uint32_t)entryPointLabelEntry->getOffset();

    entryPointRefOut = PEFile::PESectionDataReference( asmEntryPointSect, asmEntryPointOffset );

    // Done.
    return true;
}

};
