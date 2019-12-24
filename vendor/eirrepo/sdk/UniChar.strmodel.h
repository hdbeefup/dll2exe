/*****************************************************************************
*
*  PROJECT:     Eir SDK
*  LICENSE:     See LICENSE in the top level directory
*  FILE:        eirrepo/sdk/UniChar.strmodel.h
*  PURPOSE:     String-model based functions like comparison based on UniChar
*
*  Find the Eir SDK at: https://osdn.net/projects/eirrepo/
*  Multi Theft Auto is available from http://www.multitheftauto.com/
*
*****************************************************************************/

#ifndef _EIRREPO_CHARACTER_CHAIN_FUNCTIONS_
#define _EIRREPO_CHARACTER_CHAIN_FUNCTIONS_

#include "MacroUtils.h"

//TODO: find ways to share as much code as possible between the different string model functions!

// IMPORTANT: this function is designed to terminate on '\0' inside any string aswell.
template <typename srcCharType, typename dstCharType, typename srcItemProcessor, typename dstItemProcessor>
inline eir::eCompResult StringComparatorGeneric(
    const srcCharType *srcStr, const dstCharType *dstStr,
    bool caseSensitive,
    const srcItemProcessor& srcProc, const dstItemProcessor& dstProc
)
{
    try
    {
        typedef character_env <srcCharType> src_char_env;
        typedef character_env <dstCharType> dst_char_env;

        typedef typename src_char_env::ucp_t src_ucp_t;
        typedef typename dst_char_env::ucp_t dst_ucp_t;

        const std::locale& classic_loc = std::locale::classic();

        toupper_lookup <src_ucp_t> srcFacet( classic_loc );
        toupper_lookup <dst_ucp_t> dstFacet( classic_loc );

        // Do a real unicode comparison.
		charenv_charprov_tozero <srcCharType> srcIterProv( srcStr );
		charenv_charprov_tozero <dstCharType> dstIterProv( dstStr );

        character_env_iterator <srcCharType, decltype(srcIterProv)> srcIter( srcStr, std::move( srcIterProv ) );
        character_env_iterator <dstCharType, decltype(dstIterProv)> dstIter( dstStr, std::move( dstIterProv ) );

        size_t src_n = 0;
        size_t dst_n = 0;

        while ( true )
        {
            bool isSrcEnd = srcIter.IsEnd();
            bool isDstEnd = dstIter.IsEnd();

            if ( !isSrcEnd )
            {
                if ( srcProc.IsFinished( srcStr, src_n ) )
                {
                    isSrcEnd = true;
                }
            }

            if ( !isDstEnd )
            {
                if ( dstProc.IsFinished( dstStr, dst_n ) )
                {
                    isDstEnd = true;
                }
            }

            if ( isSrcEnd && isDstEnd )
            {
                break;
            }
            
            if ( isSrcEnd )
            {
                return eir::eCompResult::LEFT_LESS;
            }
            
            if ( isDstEnd )
            {
                return eir::eCompResult::LEFT_GREATER;
            }

            src_n += srcIter.GetIterateCount();
            dst_n += dstIter.GetIterateCount();

            src_ucp_t srcChar = srcIter.Resolve();
            dst_ucp_t dstChar = dstIter.Resolve();

            srcIter.Increment();
            dstIter.Increment();

            eir::eCompResult cmpVal = CompareCharacterEx( srcChar, dstChar, srcFacet, dstFacet, caseSensitive );

            if ( cmpVal != eir::eCompResult::EQUAL )
            {
                return cmpVal;
            }
        }

        return eir::eCompResult::EQUAL;
    }
    catch( std::bad_cast& )
    {
        // Something horrible happened.
        return eir::eCompResult::LEFT_LESS;
    }
}

template <typename charType>
struct boundedCharItemProc
{
    size_t cpLen;

    AINLINE boundedCharItemProc( size_t cpLen )
    {
        this->cpLen = cpLen;
    }

    AINLINE bool IsFinished( const charType *strPtr, size_t cp_iter ) const
    {
        return ( cp_iter >= cpLen );
    }
};

template <typename charType>
struct unboundedCharItemProc
{
    AINLINE bool IsFinished( const charType *strPtr, size_t cp_iter ) const
    {
        return false;
    }
};

// Comparison of encoded strings.
// We expect the amount of code points as length, NOT the actual rendered string length (UCP length).
template <typename srcCharType, typename dstCharType>
inline bool UniversalCompareStrings(
    const srcCharType *srcStr, size_t srcCPLen,
    const dstCharType *dstStr, size_t dstCPLen,
    bool caseSensitive
)
{
    boundedCharItemProc <srcCharType> srcProc( srcCPLen );
    boundedCharItemProc <dstCharType> dstProc( dstCPLen );

    return ( StringComparatorGeneric( srcStr, dstStr, caseSensitive, srcProc, dstProc ) == eir::eCompResult::EQUAL );
}

template <typename srcCharType, typename dstCharType, typename srcAllocatorType, typename dstAllocatorType>
inline bool UniversalCompareStrings(
	const eir::String <srcCharType, srcAllocatorType>& src,
	const eir::String <dstCharType, dstAllocatorType>& dst,
	bool caseSensitive
)
{
	boundedCharItemProc <srcCharType> srcProc( src.GetLength() );
	boundedCharItemProc <dstCharType> dstProc( dst.GetLength() );

    return ( StringComparatorGeneric( src.GetConstString(), dst.GetConstString(), caseSensitive, srcProc, dstProc ) == eir::eCompResult::EQUAL );
}

template <typename srcCharType, typename dstCharType>
inline bool BoundedStringEqual(
    const srcCharType *srcStr, size_t srcCPLen,
    const dstCharType *dstStr,
    bool caseSensitive
)
{
    boundedCharItemProc <srcCharType> srcProc( srcCPLen );
    unboundedCharItemProc <dstCharType> dstProc;

    return ( StringComparatorGeneric( srcStr, dstStr, caseSensitive, srcProc, dstProc ) == eir::eCompResult::EQUAL );
}

template <typename srcCharType, typename dstCharType>
inline bool StringEqualToZero(
    const srcCharType *srcStr,
    const dstCharType *dstStr,
    bool caseSensitive
)
{
    unboundedCharItemProc <srcCharType> srcProc;
    unboundedCharItemProc <dstCharType> dstProc;

    return ( StringComparatorGeneric( srcStr, dstStr, caseSensitive, srcProc, dstProc ) == eir::eCompResult::EQUAL );
}

template <typename srcCharType, typename dstCharType>
inline eir::eCompResult FixedStringCompare(
    const srcCharType *srcStr, size_t srcCPLen,
    const dstCharType *dstStr, size_t dstCPLen,
    bool caseSensitive
)
{
    boundedCharItemProc <srcCharType> srcProc( srcCPLen );
    boundedCharItemProc <dstCharType> dstProc( dstCPLen );

    return StringComparatorGeneric( srcStr, dstStr, caseSensitive, srcProc, dstProc );
}

template <typename srcCharType, typename dstCharType>
inline eir::eCompResult BoundedStringCompare(
    const srcCharType *srcStr, size_t srcCPLen,
    const dstCharType *dstStr,
    bool caseSensitive
)
{
    boundedCharItemProc <srcCharType> srcProc( srcCPLen );
    unboundedCharItemProc <dstCharType> dstProc;

    return StringComparatorGeneric( srcStr, dstStr, caseSensitive, srcProc, dstProc );
}

template <typename srcCharType, typename dstCharType>
inline eir::eCompResult StringCompareToZero(
    const srcCharType *srcStr,
    const dstCharType *dstStr,
    bool caseSensitive
)
{
    unboundedCharItemProc <srcCharType> srcProc;
    unboundedCharItemProc <dstCharType> dstProc;

    return StringComparatorGeneric( srcStr, dstStr, caseSensitive, srcProc, dstProc );
}

#endif //_EIRREPO_CHARACTER_CHAIN_FUNCTIONS_
