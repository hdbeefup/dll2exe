// PEloader helpers for dealing with the debug directory.

#include "peloader.h"

#include "peloader.internal.hxx"

#include <time.h>

PEFile::PEDebugDesc& PEFile::AddDebugData( std::uint32_t debugType )
{
    // Create a new descriptor.
    {
        PEDebugDesc newDesc;
        newDesc.characteristics = 0;
        newDesc.timeDateStamp = (std::uint32_t)time( nullptr );
        newDesc.majorVer = 0;
        newDesc.minorVer = 0;
        newDesc.type = debugType;
    
        this->debugDescs.AddToBack( std::move( newDesc ) );

        // Invalidate the PE native array.
        this->debugDescsAlloc = PESectionAllocation();
    }

    // Return it.
    return this->debugDescs.GetBack();
}

bool PEFile::ClearDebugDataOfType( std::uint32_t debugType )
{
    bool hasChanged = false;

    size_t numDebugDescs = this->debugDescs.GetCount();
    size_t n = 0;

    while ( n < numDebugDescs )
    {
        PEDebugDesc& debugInfo = this->debugDescs[ n ];

        if ( debugInfo.type == debugType )
        {
            this->debugDescs.RemoveByIndex( n );

            numDebugDescs--;

            hasChanged = true;
        }
        else
        {
            n++;
        }
    }

    if ( hasChanged )
    {
        this->debugDescsAlloc = PESectionAllocation();
    }

    return hasChanged;
}