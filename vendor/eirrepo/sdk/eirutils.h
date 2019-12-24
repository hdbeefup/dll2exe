/*****************************************************************************
*
*  PROJECT:     Eir SDK
*  LICENSE:     See LICENSE in the top level directory
*  FILE:        eirrepo/sdk/eirutils.h
*  PURPOSE:     Things that are commonly used but do not warrant for own header
*
*  Find the Eir SDK at: https://osdn.net/projects/eirrepo/
*  Multi Theft Auto is available from http://www.multitheftauto.com/
*
*****************************************************************************/

#ifndef _EIR_COMMON_SDK_UTILITIES_
#define _EIR_COMMON_SDK_UTILITIES_

#include <algorithm>
#include <type_traits>

#include <cstddef>
#include <malloc.h>

#include "DataUtil.h"
#include "MacroUtils.h"

namespace eir
{

// A very special pass-in just for creation with the allocator initialization.
enum class constr_with_alloc
{
    DEFAULT
};

// Construction with allocator-copy (sometimes required).
enum class constr_with_alloc_copy
{
    DEFAULT
};

// Comparison results.
enum class eCompResult
{
    LEFT_LESS,
    EQUAL,
    LEFT_GREATER
};

// Basic function.
template <typename leftType, typename rightType>
AINLINE eCompResult DefaultValueCompare( const leftType& left, const rightType& right )
{
    if ( left < right )
    {
        return eCompResult::LEFT_LESS;
    }
    if ( left > right )
    {
        return eCompResult::LEFT_GREATER;
    }

    return eCompResult::EQUAL;
}

} // eir

// TODO: flesh out this exception stuff.
struct eir_exception
{
};

// Calculates the length of a string to the zero-terminator.
template <typename charType>
inline size_t cplen_tozero( const charType *chars )
{
    size_t len = 0;

    while ( *chars != (charType)0 )
    {
        len++;
        chars++;
    }

    return len;
}

namespace eir
{

// Constructs an object with the help of a static memory allocator.
template <typename structType, typename allocatorType, typename... Args>
inline structType* static_new_struct( void *refMem, Args&&... theArgs )
{
    // Attempt to allocate a block of memory for bootstrapping.
    void *mem = allocatorType::Allocate( refMem, sizeof( structType ), alignof( structType ) );

    if ( !mem )
    {
        throw eir_exception();
    }

    try
    {
        return new (mem) structType( std::forward <Args> ( theArgs )... );
    }
    catch( ... )
    {
        allocatorType::Free( refMem, mem );

        throw;
    }
}

// Destroys an object that was previously allocated using a specific static memory allocator.
template <typename structType, typename allocatorType>
inline void static_del_struct( void *refMem, structType *theStruct ) noexcept
{
    theStruct->~structType();

    void *mem = theStruct;

    allocatorType::Free( refMem, mem );
}

// Constructs an object using an object memory allocator.
template <typename structType, typename allocatorType, typename... Args>
inline structType* dyn_new_struct( allocatorType& allocMan, void *refMem, Args&&... theArgs )
{
    // Attempt to allocate a block of memory for bootstrapping.
    void *mem = allocMan.Allocate( refMem, sizeof( structType ), alignof( structType ) );

    if ( !mem )
    {
        throw eir_exception();
    }

    try
    {
        return new (mem) structType( std::forward <Args> ( theArgs )... );
    }
    catch( ... )
    {
        allocMan.Free( refMem, mem );

        throw;
    }
}

// Destroys an object that was allocated by an object memory allocator.
template <typename structType, typename allocatorType>
inline void dyn_del_struct( allocatorType& memAlloc, void *refMem, structType *theStruct ) noexcept
{
    theStruct->~structType();

    void *mem = theStruct;

    memAlloc.Free( refMem, mem );
}

} // eir

// Optimization helper.
enum class size_opt_constr_with_default
{
    DEFAULT
};

// Conditionally make an object static inline or a real member.
// This is there to solve the problem of zero-size structs, because static methods can be invoked same as
// object methods.
template <bool trueRealMemberFalseStaticInline, typename structType, typename fieldsStruct>
struct size_opt : public fieldsStruct
{
    template <typename... allocArgs>
    AINLINE size_opt( int, fieldsStruct&& fields, allocArgs&&... theArgs ) : fieldsStruct( std::forward <fieldsStruct> ( fields ) )
    {
        return;
    }
    // meow.
    template <typename... allocArgs>
    AINLINE size_opt( size_opt_constr_with_default, allocArgs&&... theArgs )
    {
        return;
    }
    AINLINE size_opt( void )
    {
        return;
    }
    AINLINE size_opt( const size_opt& right )
    {
        return;
    }
    AINLINE size_opt( size_opt&& right )
    {
        return;
    }
    AINLINE size_opt( structType&& right )
    {
        return;
    }

    AINLINE size_opt& operator = ( const size_opt& right )
    {
        return *this;
    }
    AINLINE size_opt& operator = ( size_opt&& right )
    {
        return *this;
    }

    inline static structType allocData;
};

template <typename structType, typename fieldsStruct>
struct size_opt <true, structType, fieldsStruct> : public fieldsStruct
{
    template <typename... Args>
    AINLINE size_opt( int _, fieldsStruct&& fields, Args&&... allocArgs )
        : fieldsStruct( std::forward <fieldsStruct> ( fields ) ), allocData( std::forward <Args> ( allocArgs )... )
    {
        return;
    }
    template <typename... allocArgs>
    AINLINE size_opt( size_opt_constr_with_default, allocArgs&&... theArgs )
        : allocData( std::forward <allocArgs> ( theArgs )... )
    {
        return;
    }
    AINLINE size_opt( void ) : allocData()
    {
        return;
    }
    AINLINE size_opt( const size_opt& right ) : allocData( right.allocData )
    {
        return;
    }
    AINLINE size_opt( size_opt&& right ) : allocData( std::move( right.allocData ) )
    {
        return;
    }
    AINLINE size_opt( structType&& right ) : allocData( std::move( right ) )
    {
        return;
    }

    AINLINE size_opt& operator = ( const size_opt& right )
    {
        this->allocData = right.allocData;

        return *this;
    }
    AINLINE size_opt& operator = ( size_opt&& right )
    {
        this->allocData = std::move( right.allocData );

        return *this;
    }

    structType allocData;
};

// A lot of objects used internally by NativeExecutive can only be constructed once the runtime has initialized for the first time.
// Thus we want to provide the space for them, but not initialize with program load.
template <typename structType>
struct alignas(alignof(structType)) optional_struct_space
{
    inline optional_struct_space( void ) = default;
    inline optional_struct_space( const optional_struct_space& ) = delete;
    inline optional_struct_space( optional_struct_space&& ) = delete;

    inline optional_struct_space& operator = ( const optional_struct_space& ) = delete;
    inline optional_struct_space& operator = ( optional_struct_space&& ) = delete;

    template <typename... Args>
    AINLINE void Construct( Args&&... constrArgs )
    {
        new (struct_space) structType( std::forward <Args> ( constrArgs )... );
    }

    AINLINE void Destroy( void )
    {
        ( *( (structType*)struct_space ) ).~structType();
    }

    AINLINE structType& get( void ) const
    {
        return *(structType*)struct_space;
    }

    AINLINE operator structType& ( void ) const
    {
        return get();
    }

private:
    alignas(alignof(structType)) char struct_space[ sizeof(structType) ];

    // We removed alive-checking because this object is used in
    // pre-CRT-init situations and the construction of classes has
    // not happened yet.
};

// It is often required to construct an optional_struct_space as part of a running stack.
// Thus we need a wrapper that simplifies the cleanup of such activity.
template <typename structType>
struct optional_struct_space_init
{
    template <typename... constrArgs>
    AINLINE optional_struct_space_init( optional_struct_space <structType>& varloc, constrArgs&&... args ) noexcept : varloc( &varloc )
    {
        varloc.Construct( std::forward <constrArgs> ( args )... );
    }
    AINLINE optional_struct_space_init( const optional_struct_space_init& ) = delete;
    AINLINE optional_struct_space_init( optional_struct_space_init&& right ) noexcept
    {
        this->varloc = right.varloc;

        right.varloc = nullptr;
    }
    AINLINE ~optional_struct_space_init( void )
    {
        if ( optional_struct_space <structType> *varloc = this->varloc )
        {
            varloc->Destroy();
        }
    }

    AINLINE optional_struct_space_init& operator = ( const optional_struct_space_init& ) = delete;
    AINLINE optional_struct_space_init& operator = ( optional_struct_space_init&& right ) noexcept
    {
        if ( optional_struct_space <structType> *varloc = this->varloc )
        {
            varloc->Destroy();
        }

        this->varloc = right.varloc;

        return *this;
    }

    AINLINE structType& get( void ) const
    {
        optional_struct_space <structType> *varloc = this->varloc;

        if ( varloc == nullptr )
        {
            throw eir_exception();
        }

        return varloc->get();
    }

private:
    optional_struct_space <structType> *varloc;
};

// The basic allocator that links to the CRT.
// I was heavy against exposing this but I cannot overcome the static-initialization order in current C++17.
struct CRTHeapAllocator
{
    static AINLINE void* Allocate( void *refPtr, size_t memSize, size_t alignment )
    {
#ifdef _MSC_VER
        return _aligned_malloc( memSize, alignment );
#else
        // The use of aligned_alloc is braindead because it actually requires the size to be
        // an integral multiple of alignment. So you can understand why Microsoft does not want
        // it in their CRT.
        return memalign( alignment, memSize );
#endif //_MSC_VER
    }

    static AINLINE bool Resize( void *refPtr, void *memPtr, size_t newSize )
    {
        // I cannot believe that Microsoft has not yet added "_aligned_expand".
        return false;
    }

    static AINLINE void Free( void *refPtr, void *memPtr )
    {
#ifdef _MSC_VER
        _aligned_free( memPtr );
#else
        free( memPtr );
#endif //_MSC_VER
    }
};

// Not really realloc() but very similar.
template <typename allocatorType>
AINLINE void* TemplateMemTransalloc( allocatorType& memAlloc, void *refPtr, void *memPtr, size_t oldSize, size_t newSize, size_t alignment = sizeof(std::max_align_t) )
{
    if ( newSize == 0 )
    {
        if ( memPtr )
        {
            memAlloc.Free( refPtr, memPtr );
        }

        return nullptr;
    }

    if ( memPtr == nullptr )
    {
        return memAlloc.Allocate( refPtr, newSize, alignment );
    }

    bool couldResize = memAlloc.Resize( refPtr, memPtr, newSize );

    if ( couldResize )
    {
        return memPtr;
    }

    void *newMemPtr = memAlloc.Allocate( refPtr, newSize, alignment );

    if ( newMemPtr == nullptr )
    {
        return nullptr;
    }

    FSDataUtil::copy_impl( (const char*)memPtr, (const char*)memPtr + oldSize, (char*)newMemPtr );

    memAlloc.Free( refPtr, memPtr );

    return newMemPtr;
}

// Safe assert that crashes the program on failure.
inline static void check_debug_condition( bool condition, const char *expression, const char *filename, size_t linenum )
{
    if ( !condition )
    {
        // Crash the program. A debugger should know what this is about.
        *(char*)nullptr = 0;
    }
}

#define FATAL_ASSERT( expr )  check_debug_condition( expr, #expr, __FILE__, __LINE__ )

#endif //_EIR_COMMON_SDK_UTILITIES_
