/*****************************************************************************
*
*  PROJECT:     Eir SDK
*  LICENSE:     See LICENSE in the top level directory
*  FILE:        eirrepo/sdk/Function.h
*  PURPOSE:     Dynamic function storage struct helper (i.e. lambdas)
*
*  Find the Eir SDK at: https://osdn.net/projects/eirrepo/
*  Multi Theft Auto is available from http://www.multitheftauto.com/
*
*****************************************************************************/

// Motivation for adding this type was that the STL does not support providing
// a custom allocator for std::function.

#ifndef _DYNAMIC_FUNCTION_STORAGE_
#define _DYNAMIC_FUNCTION_STORAGE_

#include <type_traits>

#include "MacroUtils.h"
#include "eirutils.h"

namespace eir
{

// TODO: add object-allocator support to eir::Function, refptr support, etc.

template <typename allocatorType, typename returnType, typename... cbArgs>
struct Function
{
    AINLINE Function( void )
    {
        this->func_mem = nullptr;
    }

    template <typename callbackType>
    AINLINE Function( callbackType&& cb )
    {
        static_assert( std::is_invocable <callbackType, cbArgs...>::value, "lambda has to be invocable with the given cbArgs" );
        static_assert( std::is_same <typename std::invoke_result <callbackType, cbArgs...>::type, returnType>::value, "lambda has to be invocable with given returnType" );

        struct lambda_function_storage : public virtual_function_storage
        {
            AINLINE lambda_function_storage( callbackType&& cb ) : cb( std::move( cb ) )
            {
                return;
            }

            AINLINE returnType invoke( cbArgs&&... args )
            {
                return cb( std::forward <cbArgs> ( args )... );
            }

            callbackType cb;
        };

        this->func_mem = static_new_struct <lambda_function_storage, allocatorType> ( nullptr, std::move( cb ) );
    }
    AINLINE Function( const Function& right ) = delete;
    AINLINE Function( Function&& right ) noexcept
    {
        this->func_mem = right.func_mem;

        right.func_mem = nullptr;
    }

private:
    AINLINE void _clear_mem( void )
    {
        if ( virtual_function_storage *func_mem = this->func_mem )
        {
            static_del_struct <virtual_function_storage, allocatorType> ( nullptr, func_mem );

            this->func_mem = nullptr;
        }
    }

public:
    AINLINE ~Function( void )
    {
        _clear_mem();
    }

    AINLINE Function& operator = ( const Function& ) = delete;
    AINLINE Function& operator = ( Function&& right ) noexcept
    {
        _clear_mem();

        this->func_mem = right.func_mem;

        right.func_mem = nullptr;

        return *this;
    }

    AINLINE bool is_good( void ) const
    {
        return ( this->func_mem != nullptr );
    }

    AINLINE returnType operator () ( cbArgs&&... args )
    {
        virtual_function_storage *func_mem = this->func_mem;

        if ( func_mem == nullptr )
        {
            throw eir_exception();
        }

        return func_mem->invoke( std::forward <cbArgs> ( args )... );
    }

private:
    struct virtual_function_storage
    {
        virtual ~virtual_function_storage( void )
        {
            return;
        }

        virtual returnType invoke( cbArgs&&... args ) = 0;
    };

    virtual_function_storage *func_mem;
};

}

#endif //_DYNAMIC_FUNCTION_STORAGE_
