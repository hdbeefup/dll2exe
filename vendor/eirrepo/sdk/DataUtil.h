/*****************************************************************************
*
*  PROJECT:     Eir SDK
*  LICENSE:     See LICENSE in the top level directory
*  FILE:        eirrepo/sdk/DataUtil.h
*  PURPOSE:     Simple helpers for memory operations
*
*  Find the Eir SDK at: https://osdn.net/projects/eirrepo/
*  Multi Theft Auto is available from http://www.multitheftauto.com/
*
*****************************************************************************/

#ifndef _EIRREPO_DATA_UTILITIES_
#define _EIRREPO_DATA_UTILITIES_

#include <type_traits>

#include "MacroUtils.h"

// Some helpers.
namespace FSDataUtil
{
    template <typename dataType>
    static inline void copy_impl( const dataType *srcPtr, const dataType *srcPtrEnd, dataType *dstPtr ) noexcept
    {
        static_assert( std::is_trivial <dataType>::value == true );

        while ( srcPtr != srcPtrEnd )
        {
            *dstPtr++ = *srcPtr++;
        }
    }

    template <typename dataType>
    static inline void copy_backward_impl( const dataType *srcPtr, const dataType *srcPtrEnd, dataType *dstPtr ) noexcept
    {
        static_assert( std::is_trivial <dataType>::value == true );

        while ( srcPtr != srcPtrEnd )
        {
            *(--dstPtr) = *(--srcPtrEnd);
        }
    }
};

namespace eir
{

template <typename unsignedNumberType>
AINLINE unsignedNumberType ROTL( unsignedNumberType value, unsigned int rotBy )
{
    static constexpr unsigned int bitCount = sizeof(value) * 8;

    return ( value << rotBy ) | ( value >> ( bitCount - rotBy ) );
}
template <typename unsignedNumberType>
AINLINE unsignedNumberType ROTR( unsignedNumberType value, unsigned int rotBy )
{
    static constexpr unsigned int bitCount = sizeof(value) * 8;

    return ( value >> rotBy ) | ( value << ( bitCount - rotBy ) );
}

};

#endif //_EIRREPO_DATA_UTILITIES_