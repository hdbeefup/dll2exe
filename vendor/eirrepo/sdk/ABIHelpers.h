/******************************************************************************
*
*  PROJECT:     Eir SDK
*  LICENSE:     See LICENSE in the top level directory
*  FILE:        eirrepo/sdk/ABIHelpers.h
*  PURPOSE:     Classes and functions that help interact across ABI boundaries.
*
*  Find the Eir SDK at: https://osdn.net/projects/eirrepo/
*  Multi Theft Auto is available from http://www.multitheftauto.com/
*
*****************************************************************************/

#ifndef _EIRREPO_ABI_HELPERS_
#define _EIRREPO_ABI_HELPERS_

#include <cstdint>
#include <type_traits>
#include <algorithm>
#include "String.h"

#include "eirutils.h"
#include "DataUtil.h"
#include "MetaHelpers.h"
#include "MacroUtils.h"
#include "MemoryRaw.h"

template <typename charType>
struct abiVirtualString
{
private:
    // IMPORTANT: the allocatorType must be moveable.
    //  You cannot use an allocatorType that relies on the "refPtr" argument; it is always nullptr so do not bother.

    // Characters should be a trivial type, meaning that it does not throw exceptions and stuff when
    // copying or assigning. It also does use the default constructor.
    static_assert( std::is_trivial <charType>::value == true, "abiVirtualString charType has to be of trivial type" );

    struct abstractVirtualContainer
    {
        virtual void Destroy( void ) noexcept = 0;

        size_t strLen;
        size_t strOff;
    };

    template <typename allocatorType>
    struct virtualContainer : public abstractVirtualContainer
    {
        INSTANCE_SUBSTRUCTCHECK( is_object );

        // Need to allow placement of allocators inside this struct because of its virtual nature.
        static constexpr bool hasObjectAllocator = PERFORM_SUBSTRUCTCHECK( allocatorType, is_object );

        AINLINE virtualContainer( allocatorType allocData ) : allocData( std::move( allocData ) )
        {
            return;
        }

        virtual void Destroy( void ) noexcept final
        {
            if constexpr ( hasObjectAllocator )
            {
                // Must move out the allocatorType, if it is an object.
                allocatorType local_alloc = std::move( this->allocData );

                this->~virtualContainer();

                local_alloc.Free( nullptr, this );

                // The allocatorType will be destroyed now.
            }
            else
            {
                // Just free data.
                this->allocData.Free( nullptr, this );
            }
        }

        allocatorType allocData;
    };

    abstractVirtualContainer *container;

    template <typename allocatorType, typename... Args>
    AINLINE void allocateContainer( const charType *str, size_t strLen, Args&&... theArgs )
    {
        using charVirtualContainer = virtualContainer <allocatorType>;

        size_t startOfCharData = ALIGN( sizeof(charVirtualContainer), alignof(charType), alignof(charType) );
        size_t memSize = ( startOfCharData + sizeof(charType) * ( strLen + 1 ) );

        charVirtualContainer *container;

        if constexpr ( charVirtualContainer::hasObjectAllocator )
        {
            allocatorType allocData( std::forward <Args> ( theArgs )... );

            void *memPtr = allocData.Allocate( nullptr, memSize, alignof(charVirtualContainer) );

            if ( !memPtr )
            {
                throw eir_exception();
            }

            try
            {
                container = new (memPtr) charVirtualContainer( std::move( allocData ) );
            }
            catch( ... )
            {
                allocData.Free( nullptr, memPtr );
                throw;
            }
        }
        else
        {
            void *memPtr = allocatorType::Allocate( nullptr, memSize, alignof(charVirtualContainer) );

            if ( !memPtr )
            {
                throw eir_exception();
            }

            try
            {
                container = new (memPtr) charVirtualContainer();
            }
            catch( ... )
            {
                allocatorType::Free( nullptr, memPtr );
                throw;
            }
        }

        charType *strBuf = (charType*)( (char*)container + startOfCharData );

        if ( strLen > 0 )
        {
            FSDataUtil::copy_impl( str, str + strLen, strBuf );
        }

        strBuf[ strLen ] = (charType)0;
        container->strLen = strLen;
        container->strOff = startOfCharData;

        this->container = container;
    }

    inline abiVirtualString( void )
    {
        return;
    }

public:
    template <typename allocatorType, typename... Args>
    static AINLINE abiVirtualString Make( const charType *str, size_t strLen, Args&&... allocArgs )
    {
        abiVirtualString string;
        string.template allocateContainer <allocatorType, Args...> ( str, strLen, std::forward <Args> ( allocArgs )... );
        return string;
    }

    inline abiVirtualString( abiVirtualString&& right ) noexcept
    {
        this->container = right.container;

        right.container = nullptr;
    }

    inline ~abiVirtualString( void )
    {
        if ( abstractVirtualContainer *container = this->container )
        {
            container->Destroy();
        }
    }

    inline abiVirtualString& operator = ( const abiVirtualString& right )
    {
        if ( this->container )
        {
            this->container->Destroy();

            this->container = nullptr;
        }

        if ( abstractVirtualContainer *rightCont = right.container )
        {
            const charType *str = rightCont->stringBuffer;

            allocateContainer( str, rightCont->strLen );
        }

        return *this;
    }

    inline abiVirtualString& operator = ( abiVirtualString&& right ) noexcept
    {
        if ( abstractVirtualContainer *container = this->container )
        {
            container->Destroy();
        }

        this->container = right.container;

        right.container = nullptr;

        return *this;
    }

    inline const charType* c_str( void ) const noexcept
    {
        abstractVirtualContainer *container = this->container;

        if ( !container )
        {
            return eir::GetEmptyString <charType> ();
        }

        size_t charbuf_off = container->strOff;

        return (charType*)( (char*)container + charbuf_off );
    }

    inline size_t size( void ) const noexcept
    {
        size_t len = 0;

        if ( abstractVirtualContainer *container = this->container )
        {
            len = container->strLen;
        }

        return len;
    }

    inline size_t length( void ) const noexcept
    { return size(); }
};

typedef abiVirtualString <char> abiString;
typedef abiVirtualString <wchar_t> abiWideString;

#endif //_EIRREPO_ABI_HELPERS_