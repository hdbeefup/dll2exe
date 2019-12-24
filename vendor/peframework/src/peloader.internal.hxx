#ifndef _PELOADER_INTERNAL_
#define _PELOADER_INTERNAL_

// Forward to the global header, because it is sometimes necessary.
#include "peloader.serialize.h"

// Helper function for pointer size.
inline std::uint32_t GetPEPointerSize( bool isExtendedFormat )
{
    if ( isExtendedFormat )
    {
        return sizeof(std::uint64_t);
    }

    return sizeof(std::uint32_t);
}

static AINLINE std::uint32_t VA2RVA( std::uint64_t va, std::uint64_t imageBase )
{
    if ( va == 0 )
        return 0;

    return (std::uint32_t)( va - imageBase );
}

#endif //_PELOADER_INTERNAL_