/*****************************************************************************
*
*  PROJECT:     Eir SDK
*  LICENSE:     See LICENSE in the top level directory
*  FILE:        eirrepo/sdk/StringUtils.h
*  PURPOSE:     Common string helpers
*
*  Find the Eir SDK at: https://osdn.net/projects/eirrepo/
*  Multi Theft Auto is available from http://www.multitheftauto.com/
*
*****************************************************************************/

#ifndef _STRING_COMMON_SDK_UTILITIES_
#define _STRING_COMMON_SDK_UTILITIES_

#include <sdk/String.h>

template <bool case_sensitive>
struct lexical_string_comparator
{
    template <typename charType, typename allocatorType>
    AINLINE static bool is_less_than( const charType *left, const eir::String <charType, allocatorType>& right )
    {
        return ( BoundedStringCompare( right.GetConstString(), right.GetLength(), left, case_sensitive ) == eir::eCompResult::LEFT_GREATER );
    }

    template <typename charType, typename allocatorType>
    AINLINE static bool is_less_than( const eir::String <charType, allocatorType>& left, const charType *right )
    {
        return ( BoundedStringCompare( left.GetConstString(), left.GetLength(), right, case_sensitive ) == eir::eCompResult::LEFT_LESS );
    }

    template <typename charType, typename fat, typename sat>
    AINLINE static bool is_less_than( const eir::String <charType, fat>& left, const eir::String <charType, sat>& right )
    {
        return ( FixedStringCompare( left.GetConstString(), left.GetLength(), right.GetConstString(), right.GetLength(), case_sensitive ) == eir::eCompResult::LEFT_LESS );
    }
};

#endif //_STRING_COMMON_SDK_UTILITIES_