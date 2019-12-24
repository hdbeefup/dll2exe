// API specific to ease management of relocation entries in PE files.

#include "peframework.h"

#include "peloader.internal.hxx"

void PEFile::AddRelocation( std::uint32_t rva, PEBaseReloc::eRelocType relocType )
{
    // We only support particular types of items here.
    if ( relocType != PEBaseReloc::eRelocType::ABSOLUTE &&
         relocType != PEBaseReloc::eRelocType::HIGH &&
         relocType != PEBaseReloc::eRelocType::LOW &&
         relocType != PEBaseReloc::eRelocType::HIGHLOW &&
         relocType != PEBaseReloc::eRelocType::DIR64 )
    {
        throw peframework_exception(
            ePEExceptCode::RUNTIME_ERROR,
            "invalid relocation type registration attempt"
        );
    }

    // Since we divided base relocation RVA by chunk size (default 4K) we can
    // simple divide to get the index by RVA aswell. Pretty nice!

    std::uint32_t dictIndex = ( rva / baserelocChunkSize );

    PEBaseReloc& relocDict = this->baseRelocs[ dictIndex ];

    // Items inside of a base relocation chunk are not structured particularily.
    // At least this is my assumption, based on eRelocType::HIGHADJ.

    std::uint32_t insideChunkOff = ( rva % baserelocChunkSize );

    PEBaseReloc::item newItem;
    newItem.type = (std::uint16_t)relocType;
    newItem.offset = insideChunkOff;

    relocDict.items.AddToBack( std::move( newItem ) );

    // We need a new base relocations array.
    this->baseRelocAllocEntry = PESectionAllocation();
}

void PEFile::RemoveRelocations( std::uint32_t rva, std::uint32_t regionSize )
{
    if ( regionSize == 0 )
        return;

    // We remove all relocations inside of the given region.

    std::uint32_t baserelocRemoveIndex = ( rva / baserelocChunkSize );

    auto *foundMinimumNode = this->baseRelocs.FindMinimumByCriteria(
        [&]( const decltype(PEFile::baseRelocs)::Node *leftNode )
    {
        std::uint32_t relocIndex = leftNode->GetKey();

        if ( relocIndex < baserelocRemoveIndex )
        {
            return eir::eCompResult::LEFT_LESS;
        }

        return eir::eCompResult::EQUAL;
    });

    typedef sliceOfData <std::uint32_t> rvaSlice_t;

    rvaSlice_t requestSlice( rva, regionSize );

    decltype(this->baseRelocs)::iterator iter( foundMinimumNode );

    while ( !iter.IsEnd() )
    {
        decltype(this->baseRelocs)::Node *curNode = iter.Resolve();

        bool doRemove = false;
        {
            PEBaseReloc& relocDict = curNode->GetValue();

            // Check the relationship to this item.
            rvaSlice_t dictSlice( relocDict.offsetOfReloc, baserelocChunkSize );

            eir::eIntersectionResult intResult = requestSlice.intersectWith( dictSlice );

            if ( eir::isFloatingIntersect( intResult ) )
            {
                // We are finished.
                break;
            }
            else if ( intResult == eir::INTERSECT_ENCLOSING ||
                      intResult == eir::INTERSECT_EQUAL )
            {
                // The request is enclosing the base relocation block.
                // We must get rid of it entirely.
                doRemove = true;
            }
            else if ( intResult == eir::INTERSECT_INSIDE ||
                      intResult == eir::INTERSECT_BORDER_END ||
                      intResult == eir::INTERSECT_BORDER_START )
            {
                // We remove single items from this base relocation entry.
                size_t numItems = relocDict.items.GetCount();
                size_t n = 0;

                while ( n < numItems )
                {
                    PEBaseReloc::item& dictItem = relocDict.items[ n ];

                    // TODO: maybe make dict items size after what memory they actually take.

                    rvaSlice_t itemSlice( dictSlice.GetSliceStartPoint() + dictItem.offset, 1 );

                    eir::eIntersectionResult itemIntResult = requestSlice.intersectWith( itemSlice );

                    bool shouldRemove = ( eir::isFloatingIntersect( itemIntResult ) == false );

                    if ( shouldRemove )
                    {
                        relocDict.items.RemoveByIndex( n );

                        numItems--;
                    }
                    else
                    {
                        n++;
                    }
                }

                // Now see if we can remove an empty dict.
                if ( numItems == 0 )
                {
                    doRemove = true;
                }
            }
            else
            {
                assert( 0 );
            }
        }

        if ( doRemove )
        {
            iter.Increment();

            this->baseRelocs.RemoveNode( curNode );
        }
        else
        {
            iter.Increment();
        }
    }
    
    // Finished.
}

void PEFile::OnWriteAbsoluteVA( PESection *writeSect, std::uint32_t sectOff, bool is64Bit )
{
    // Check if we need to write a relocation entry.
    bool needsRelocation = false;

    if ( this->peOptHeader.dll_hasDynamicBase )
    {
        needsRelocation = true;
    }

    if ( !needsRelocation )
    {
        if ( this->baseRelocs.IsEmpty() == false )
        {
            needsRelocation = true;
        }
    }

    if ( needsRelocation )
    {
        // We either write a 32bit or 64bit relocation entry.
        PEBaseReloc::eRelocType relocType;

        if ( is64Bit )
        {
            relocType = PEBaseReloc::eRelocType::DIR64;
        }
        else
        {
            relocType = PEBaseReloc::eRelocType::HIGHLOW;
        }

        // Calculate the RVA.
        std::uint32_t rva = writeSect->ResolveRVA( sectOff );

        this->AddRelocation( rva, relocType );
    }
}

bool PEFile::WriteModulePointer( PESection *writeSect, std::uint32_t sectOff, std::uint32_t targetRVA )
{
    std::uint64_t vaPointer = ( this->sections.GetImageBase() + targetRVA );

    writeSect->stream.Seek( sectOff );

    bool success;
    bool is64bit = this->isExtendedFormat;
    
    if ( is64bit )
    {
        success = writeSect->stream.WriteUInt64( vaPointer );
    }
    else
    {
        success = writeSect->stream.WriteUInt32( (std::uint32_t)( vaPointer ) );
    }

    if ( success )
    {
        this->OnWriteAbsoluteVA( writeSect, sectOff, is64bit );
    }

    return success;
}