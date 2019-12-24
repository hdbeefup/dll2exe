// Helper API for managing exports.
#include "peloader.h"

#include "peexcept.h"

std::uint32_t PEFile::PEExportDir::AddExport( func&& entry )
{
    size_t currentIndex = this->functions.GetCount();

    this->functions.AddToBack( std::move( entry ) );

    // We need to rewrite stuff.
    this->allocEntry = PESectionAllocation();
    this->funcAddressAllocEntry = PESectionAllocation();

    return (std::uint32_t)currentIndex;
}

void PEFile::PEExportDir::MapName( std::uint32_t ordinal, const char *name )
{
    mappedName newNameMap;
    newNameMap.name = name;

    this->funcNameMap.Set( std::move( newNameMap ), std::move( ordinal ) );

    // Need to recommit memory.
    this->allocEntry = PESectionAllocation();
    this->funcNamesAllocEntry = PESectionAllocation();
}

void PEFile::PEExportDir::RemoveExport( std::uint32_t ordinal )
{
    size_t curNumFunctions = this->functions.GetCount();

    if ( ordinal >= curNumFunctions )
    {
        throw peframework_exception( ePEExceptCode::RUNTIME_ERROR, "ordinal out of bounds for removing export" );
    }

    // Simply reset the function field.
    {
        func& expEntry = this->functions[ ordinal ];
        expEntry.isForwarder = false;
        expEntry.expRef = PESectionDataReference();
        expEntry.forwarder.Clear();
    }

    // Remove all name mappings of this ordinal.
    {
        auto iter = this->funcNameMap.begin();

        while ( iter != this->funcNameMap.end() )
        {
            auto *curNode = *iter;

            ++iter;

            if ( curNode->GetValue() == ordinal )
            {
                this->funcNameMap.RemoveNode( curNode );
            }
        }
    }
}

static inline std::uint32_t ResolveExportOrdinal( const PEFile::PEExportDir& expDir, bool isOrdinal, std::uint32_t ordinal, const peString <char>& name, bool& hasOrdinal )
{
    if ( isOrdinal )
    {
        hasOrdinal = true;
        // Need to subtract the ordinal base.
        return ( ordinal - expDir.ordinalBase );
    }

    auto findIter = expDir.funcNameMap.Find( name );

    if ( findIter != nullptr )
    {
        hasOrdinal = true;
        // Internally we do not store with ordinal base offset.
        return (std::uint32_t)( findIter->GetValue() );
    }

    return false;
}

PEFile::PEExportDir::func* PEFile::PEExportDir::ResolveExport( bool isOrdinal, std::uint32_t ordinal, const peString <char>& name )
{
    bool hasImportOrdinal = false;
    size_t impOrdinal = ResolveExportOrdinal( *this, isOrdinal, ordinal, name, hasImportOrdinal );

    if ( hasImportOrdinal && impOrdinal < this->functions.GetCount() )
    {
        PEFile::PEExportDir::func& expFunc = this->functions[ impOrdinal ];

        if ( expFunc.isForwarder == false )
        {
            return &expFunc;
        }
    }

    return nullptr;
}