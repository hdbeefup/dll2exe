/*****************************************************************************
*
*  PROJECT:     Eir SDK
*  LICENSE:     See LICENSE in the top level directory
*  FILE:        eirrepo/sdk/MetaHelpers.h
*  PURPOSE:     Memory management templates
*
*  Find the Eir SDK at: https://osdn.net/projects/eirrepo/
*  Multi Theft Auto is available from http://www.multitheftauto.com/
*
*****************************************************************************/

#ifndef _COMMON_META_PROGRAMMING_HELPERS_
#define _COMMON_META_PROGRAMMING_HELPERS_

// Thanks to http://stackoverflow.com/questions/87372/check-if-a-class-has-a-member-function-of-a-given-signature
#include <type_traits>

#include "rwlist.hpp"

#define INSTANCE_METHCHECKEX( checkName, methName ) \
    template<typename, typename T> \
    struct has_##checkName { \
        static_assert( \
            std::integral_constant<T, false>::value, \
            "Second template parameter needs to be of function type."); \
    }; \
    template<typename C, typename Ret, typename... Args> \
    struct has_##checkName##<C, Ret(Args...)> { \
    private: \
        template<typename T> \
        static constexpr auto check(T*) \
        -> typename \
            std::is_same< \
                decltype( std::declval<T>().##methName##( std::declval<Args>()... ) ), \
                Ret \
            >::type; \
        template<typename> \
        static constexpr std::false_type check(...); \
        typedef decltype(check<C>(0)) type; \
    public: \
        static constexpr bool value = type::value; \
    };

#define INSTANCE_METHCHECK( methName ) INSTANCE_METHCHECKEX( methName, methName )

// Check if a class has a specific field.
#define INSTANCE_FIELDCHECK( fieldName ) \
    template <typename T, typename = int> \
    struct hasField_##fieldName : std::false_type { }; \
    template <typename T> \
    struct hasField_##fieldName <T, decltype((void) T::fieldName, 0)> : std::true_type { };

#define PERFORM_FIELDCHECK( className, fieldName ) ( hasField_##fieldName <className>::value )

#define INSTANCE_SUBSTRUCTCHECK( subStructName ) \
    template <typename T, typename = int> \
    struct hasSubStruct_##subStructName : std::false_type { }; \
    template <typename T> \
    struct hasSubStruct_##subStructName <T, decltype(typename T::subStructName(), 0)> : std::true_type { };

#define PERFORM_SUBSTRUCTCHECK( className, subStructName ) ( hasSubStruct_##subStructName <className>::value )

// Providing the everything-purpose standard allocator pattern in the Eir SDK!
// We want a common setup where the link to the DynamicTypeSystem (DTS) is fixed to the position of the DTS.
#define DEFINE_HEAP_REDIR_ALLOC( allocTypeName ) \
    struct allocTypeName \
    { \
        static inline void* Allocate( void *refMem, size_t memSize, size_t alignment ); \
        static inline bool Resize( void *refMem, void *objMem, size_t reqNewSize ); \
        static inline void Free( void *refMem, void *memPtr ); \
    };

// Non-inline version of the heap allocator template.
#define DEFINE_HEAP_ALLOC( allocTypeName ) \
    struct allocTypeName \
    { \
        static void* Allocate( void *refMem, size_t memSize, size_t alignment ); \
        static bool Resize( void *refMem, void *objMem, size_t reqNewSize ); \
        static void Free( void *refMem, void *memPtr ); \
    };

// This thing assumes that the object pointed at by allocNode is of type "NativeHeapAllocator",
// but you may of course implement your own thing that has the same semantics.
#define IMPL_HEAP_REDIR_ALLOC( allocTypeName, hostStructTypeName, redirNode, allocNode ) \
    void* allocTypeName::Allocate( void *refMem, size_t memSize, size_t alignment ) \
    { \
        hostStructTypeName *hostStruct = LIST_GETITEM( hostStructTypeName, refMem, redirNode ); \
        return hostStruct->allocNode.Allocate( memSize, alignment ); \
    } \
    bool allocTypeName::Resize( void *refMem, void *objMem, size_t reqNewSize ) \
    { \
        hostStructTypeName *hostStruct = LIST_GETITEM( hostStructTypeName, refMem, redirNode ); \
        return hostStruct->allocNode.SetAllocationSize( objMem, reqNewSize ); \
    } \
    void allocTypeName::Free( void *refMem, void *memPtr ) \
    { \
        hostStructTypeName *hostStruct = LIST_GETITEM( hostStructTypeName, refMem, redirNode ); \
        hostStruct->allocNode.Free( memPtr ); \
    }

// Default macros fot the allocator templates.
#define IMPL_HEAP_REDIR_METH_ALLOCATE_ARGS ( void *refMem, size_t memSize, size_t alignment )
#define IMPL_HEAP_REDIR_METH_ALLOCATE_RETURN void*

#define IMPL_HEAP_REDIR_METH_RESIZE_ARGS ( void *refMem, void *objMem, size_t reqNewSize )
#define IMPL_HEAP_REDIR_METH_RESIZE_RETURN bool

#define IMPL_HEAP_REDIR_METH_FREE_ARGS ( void *refMem, void *memPtr )
#define IMPL_HEAP_REDIR_METH_FREE_RETURN void

// Direct allocation helpers that redirect calls to another static allocator that depends on a parent struct.
#define IMPL_HEAP_REDIR_DIRECT_ALLOC_METH_ALLOCATE_BODY( allocTypeName, hostStructTypeName, redirNode, directAllocTypeName ) \
    { \
        hostStructTypeName *hostStruct = LIST_GETITEM( hostStructTypeName, refMem, redirNode ); \
        return directAllocTypeName::Allocate( hostStruct, memSize, alignment ); \
    }

#define IMPL_HEAP_REDIR_DIRECT_ALLOC_METH_RESIZE_BODY( allocTypeName, hostStructTypeName, redirNode, directAllocTypeName ) \
    { \
        hostStructTypeName *hostStruct = LIST_GETITEM( hostStructTypeName, refMem, redirNode ); \
        return directAllocTypeName::Resize( hostStruct, objMem, reqNewSize ); \
    }

#define IMPL_HEAP_REDIR_DIRECT_ALLOC_METH_FREE_BODY( allocTypeName, hostStructTypeName, redirNode, directAllocTypeName ) \
    { \
        hostStructTypeName *hostStruct = LIST_GETITEM( hostStructTypeName, refMem, redirNode ); \
        directAllocTypeName::Free( hostStruct, memPtr ); \
    }

// A simple redirector for allocators.
#define IMPL_HEAP_REDIR_DIRECT_ALLOC( allocTypeName, hostStructTypeName, redirNode, directAllocTypeName ) \
    IMPL_HEAP_REDIR_METH_ALLOCATE_RETURN allocTypeName::Allocate IMPL_HEAP_REDIR_METH_ALLOCATE_ARGS \
    IMPL_HEAP_REDIR_DIRECT_ALLOC_METH_ALLOCATE_BODY( allocTypeName, hostStructTypeName, redirNode, directAllocTypeName ) \
    IMPL_HEAP_REDIR_METH_RESIZE_RETURN allocTypeName::Resize IMPL_HEAP_REDIR_METH_RESIZE_ARGS \
    IMPL_HEAP_REDIR_DIRECT_ALLOC_METH_RESIZE_BODY( allocTypeName, hostStructTypeName, redirNode, directAllocTypeName ) \
    IMPL_HEAP_REDIR_METH_FREE_RETURN allocTypeName::Free IMPL_HEAP_REDIR_METH_FREE_ARGS \
    IMPL_HEAP_REDIR_DIRECT_ALLOC_METH_FREE_BODY( allocTypeName, hostStructTypeName, redirNode, directAllocTypeName )

// Similar to direct allocation but redirect calls to member allocator template instead.
#define IMPL_HEAP_REDIR_DYN_ALLOC_METH_ALLOCATE_BODY( hostStructTypeName, redirNode, dynAllocNode ) \
    { \
        hostStructTypeName *hostStruct = LIST_GETITEM( hostStructTypeName, refMem, redirNode ); \
        return hostStruct->dynAllocNode.Allocate( hostStruct, memSize, alignment ); \
    }

#define IMPL_HEAP_REDIR_DYN_ALLOC_METH_RESIZE_BODY( hostStructTypeName, redirNode, dynAllocNode ) \
    { \
        hostStructTypeName *hostStruct = LIST_GETITEM( hostStructTypeName, refMem, redirNode ); \
        return hostStruct->dynAllocNode.Resize( hostStruct, objMem, reqNewSize ); \
    }

#define IMPL_HEAP_REDIR_DYN_ALLOC_METH_FREE_BODY( hostStructTypeName, redirNode, dynAllocNode ) \
    { \
        hostStructTypeName *hostStruct = LIST_GETITEM( hostStructTypeName, refMem, redirNode ); \
        hostStruct->dynAllocNode.Free( hostStruct, memPtr ); \
    }

#endif //_COMMON_META_PROGRAMMING_HELPERS_
