#include "peloader.h"

#include <sdk/UniChar.h>

PEFile::PEImportDesc* PEFile::FindImportDescriptor( const char *moduleName )
{
    for ( PEImportDesc& impDesc : this->imports )
    {
        if ( UniversalCompareStrings(
                moduleName, strlen(moduleName),
                impDesc.DLLName.GetConstString(), impDesc.DLLName.GetLength(),
                false
             ) )
        {
            return &impDesc;
        }
    }

    return nullptr;
}

PEFile::PEImportDesc& PEFile::EstablishImportDescriptor( const char *moduleName )
{
    if ( PEImportDesc *impDesc = this->FindImportDescriptor( moduleName ) )
    {
        return *impDesc;
    }

    this->imports.AddToBack( moduleName );

    // Need new native array.
    this->importsAllocEntry = PESectionAllocation();
    
    return this->imports.GetBack();
}

const PEFile::PEImportDesc::importFunc* PEFile::PEImportDesc::FindImportEntry( std::uint16_t ordinal_hint, const char *name, bool isOrdinalImport, std::uint32_t *indexOut ) const
{
    std::uint32_t index = 0;
    const PEImportDesc::importFunc *funcOut = nullptr;

    for ( const PEImportDesc::importFunc& impFunc : this->funcs )
    {
        if ( isOrdinalImport )
        {
            if ( impFunc.isOrdinalImport == true )
            {
                if ( impFunc.ordinal_hint == ordinal_hint )
                {
                    funcOut = &impFunc;
                    break;
                }
            }
        }
        else
        {
            if ( impFunc.isOrdinalImport == false )
            {
                if ( impFunc.name == name )
                {
                    funcOut = &impFunc;
                    break;
                }
            }
        }

        index++;
    }

    if ( funcOut )
    {
        if ( indexOut )
        {
            *indexOut = index;
        }
    }

    return funcOut;
}

PEFile::PEImportDesc::functions_t PEFile::PEImportDesc::CreateEquivalentImportsList( const functions_t& funcs )
{
    PEImportDesc::functions_t newFuncs;

    size_t modImpCount = funcs.GetCount();

    for ( size_t n = 0; n < modImpCount; n++ )
    {
        const PEFile::PEImportDesc::importFunc& impFunc = funcs[ n ];

        PEFile::PEImportDesc::importFunc carbonCopy;
        carbonCopy.isOrdinalImport = impFunc.isOrdinalImport;
        carbonCopy.name = impFunc.name;
        carbonCopy.nameAllocEntry = impFunc.nameAllocEntry.CloneOnlyFinal();
        carbonCopy.ordinal_hint = impFunc.ordinal_hint;

        newFuncs.AddToBack( std::move( carbonCopy ) );
    }

    return newFuncs;
}