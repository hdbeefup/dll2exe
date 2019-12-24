/*****************************************************************************
*
*  PROJECT:     Eir SDK
*  LICENSE:     See LICENSE in the top level directory
*  FILE:        eirrepo/sdk/avlsetmaputil.h
*  PURPOSE:     Shared code between Set and Map objects
*
*  Find the Eir SDK at: https://osdn.net/projects/eirrepo/
*  Multi Theft Auto is available from http://www.multitheftauto.com/
*
*****************************************************************************/

#ifndef _AVL_SET_AND_MAP_SHARED_HEADER_
#define _AVL_SET_AND_MAP_SHARED_HEADER_

#include "MacroUtils.h"
#include "AVLTree.h"

#define MAKE_SETMAP_ITERATOR( iteratorName, hostType, nodeType, nodeRedirNode, treeMembPath, avlTreeType ) \
    struct iteratorName \
    { \
        AINLINE iteratorName( void ) = default; \
        AINLINE iteratorName( hostType& host ) : real_iter( host.treeMembPath ) \
        { \
            return; \
        } \
        AINLINE iteratorName( hostType *host ) : real_iter( host->treeMembPath ) \
        { \
            return; \
        } \
        AINLINE iteratorName( nodeType *iter ) : real_iter( &iter->nodeRedirNode ) \
        { \
            return; \
        } \
        AINLINE iteratorName( iteratorName&& ) = default; \
        AINLINE iteratorName( const iteratorName& ) = default; \
        AINLINE ~iteratorName( void ) = default; \
        AINLINE bool IsEnd( void ) const \
        { \
            return real_iter.IsEnd(); \
        } \
        AINLINE void Increment( void ) \
        { \
            real_iter.Increment(); \
        } \
        AINLINE nodeType* Resolve( void ) \
        { \
            return AVL_GETITEM( nodeType, real_iter.Resolve(), nodeRedirNode ); \
        } \
    private: \
        typename avlTreeType::diff_node_iterator real_iter; \
    }

namespace eir
{

// Default comparator for objects inside the Map/Set.
struct GenericDefaultComparator
{
    template <typename firstKeyType, typename secondKeyType>
    static AINLINE bool is_less_than( const firstKeyType& left, const secondKeyType& right )
    {
        // We want to hide signed-ness problems, kinda.
        if constexpr ( std::is_integral <firstKeyType>::value == true && std::is_integral <secondKeyType>::value == true )
        {
            if constexpr ( std::is_signed <firstKeyType>::value == true && std::is_signed <secondKeyType>::value == false )
            {
                if ( left < 0 )
                {
                    return true;
                }

                return ( (typename std::make_unsigned <firstKeyType>::type)left < right );
            }
            else if constexpr ( std::is_signed <firstKeyType>::value == false && std::is_signed <secondKeyType>::value == true )
            {
                if ( right < 0 )
                {
                    return false;
                }

                return ( left < (typename std::make_unsigned <secondKeyType>::type)right );
            }
            else
            {
                return ( left < right );
            }
        }
        else
        {
            // This is important because not all key types come with a usable "operator <" overload,
            // for example it is not really a good idea for eir::String because you ought to take
            // case-sensitivity into account!
            return ( left < right );
        }
    }
};

}

#endif //_AVL_SET_AND_MAP_SHARED_HEADER_