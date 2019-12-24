/*****************************************************************************
*
*  PROJECT:     Eir SDK
*  LICENSE:     See LICENSE in the top level directory
*  FILE:        eirrepo/sdk/MemoryRaw.h
*  PURPOSE:     Base memory management definitions for to-the-metal things
*
*  Find the Eir SDK at: https://osdn.net/projects/eirrepo/
*  Multi Theft Auto is available from http://www.multitheftauto.com/
*
*****************************************************************************/

#ifndef _MEMORY_RAW_DEFS_
#define _MEMORY_RAW_DEFS_

#include "MathSlice.h"

// Mathematically correct data slice logic.
// It is one of the most important theorems in computing abstraction.
// TODO: limit this type to integral things only.
template <typename numberType>
using sliceOfData = eir::mathSlice <numberType>;

#include <bitset>

// Macro that defines how alignment works.
//  num: base of the number to be aligned
//  sector: aligned-offset that should be added to num
//  align: number of bytes to align to
// EXAMPLE: ALIGN( 0x1001, 4, 4 ) -> 0x1000 (equivalent of compiler structure padding alignment)
//          ALIGN( 0x1003, 1, 4 ) -> 0x1000
//          ALIGN( 0x1003, 2, 4 ) -> 0x1004
template <typename numberType>
AINLINE numberType _ALIGN_GP( numberType num, numberType sector, numberType align )
{
	// General purpose alignment routine.
    // Not as fast as the bitfield version.
    numberType sectorOffset = ((num) + (sector) - 1);

    return sectorOffset - ( sectorOffset % align );
}

template <typename numberType>
AINLINE numberType _ALIGN_NATIVE( numberType num, numberType sector, numberType align )
{
	const size_t bitCount = sizeof( align ) * 8;

    // assume math based on x86 bits.
    if ( std::bitset <bitCount> ( align ).count() == 1 )
    {
        //bitfield version. not compatible with non-bitfield alignments.
        return (((num) + (sector) - 1) & (~((align) - 1)));
    }
    else
    {
		return _ALIGN_GP( num, sector, align );
    }
}

template <typename numberType>
AINLINE numberType ALIGN( numberType num, numberType sector, numberType align )
{
	return _ALIGN_GP( num, sector, align );
}

// Optimized primitives.
template <> AINLINE char			ALIGN( char num, char sector, char align )								{ return _ALIGN_NATIVE( num, sector, align ); }
template <> AINLINE unsigned char	ALIGN( unsigned char num, unsigned char sector, unsigned char align )	{ return _ALIGN_NATIVE( num, sector, align ); }
template <> AINLINE short			ALIGN( short num, short sector, short align )							{ return _ALIGN_NATIVE( num, sector, align ); }
template <> AINLINE unsigned short	ALIGN( unsigned short num, unsigned short sector, unsigned short align ){ return _ALIGN_NATIVE( num, sector, align ); }
template <> AINLINE int				ALIGN( int num, int sector, int align )									{ return _ALIGN_NATIVE( num, sector, align ); }
template <> AINLINE unsigned int	ALIGN( unsigned int num, unsigned int sector, unsigned int align )
{
	return (unsigned int)_ALIGN_NATIVE( (int)num, (int)sector, (int)align );
}

// Helper macro (equivalent of EXAMPLE 1)
template <typename numberType>
inline numberType ALIGN_SIZE( numberType num, numberType sector )
{
    return ( ALIGN( (num), (sector), (sector) ) );
}

// Aligning thigns to the boundary below.
template <typename numberType>
AINLINE numberType SCALE_DOWN( numberType value, numberType modval )
{
    // This is faster than divide-and-multiply, plus it does exactly the same.
    numberType rem = ( value % modval );

    return ( value - rem );
}

#endif //_MEMORY_RAW_DEFS_