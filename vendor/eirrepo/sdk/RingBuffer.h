/*****************************************************************************
*
*  PROJECT:     Eir SDK
*  LICENSE:     See LICENSE in the top level directory
*  FILE:        eirrepo/sdk/RingBuffer.h
*  PURPOSE:     Ring-buffer handling helpers
*
*  Find the Eir SDK at: https://osdn.net/projects/eirrepo/
*  Multi Theft Auto is available from http://www.multitheftauto.com/
*
*****************************************************************************/

#ifndef _RING_BUFFER_HANDLERS_
#define _RING_BUFFER_HANDLERS_

#include "eirutils.h"

#include <algorithm>

namespace eir
{

template <typename numberType>
struct RingBufferProcessor
{
    AINLINE RingBufferProcessor( numberType iter, numberType end_iter, numberType size )
        : iter( std::move( iter ) ), end_iter( std::move( end_iter ) ), size( std::move( size ) )
    {
        return;
    }
    AINLINE RingBufferProcessor( const RingBufferProcessor& ) = default;
    AINLINE RingBufferProcessor( RingBufferProcessor&& ) = default;

    AINLINE ~RingBufferProcessor( void )
    {
        return;
    }

    AINLINE RingBufferProcessor& operator = ( const RingBufferProcessor& ) = default;
    AINLINE RingBufferProcessor& operator = ( RingBufferProcessor&& ) = default;

    AINLINE numberType GetAvailableBytes( void ) const
    {
        numberType iter = this->iter;
        numberType end_iter = this->end_iter;

        if ( iter <= end_iter )
        {
            return ( end_iter - iter );
        }
        else
        {
            numberType first_left_write_count = ( size - iter );
            numberType second_left_write_count = ( end_iter );

            return ( first_left_write_count + second_left_write_count );
        }
    }

    template <typename callbackType>
    AINLINE void PerformUpdate( numberType cnt, const callbackType& cb )
    {
        numberType iter = this->iter;
        numberType end_iter = this->end_iter;
        numberType size = this->size;

        if ( GetAvailableBytes() < cnt )
        {
            throw eir_exception();
        }

        if ( iter <= end_iter )
        {
            cb( iter, 0, cnt );

            iter += cnt;

            if ( iter == size )
            {
                iter = 0;
            }
        }
        else
        {
            numberType first_left_write_count = ( size - iter );
            numberType second_left_write_count = ( end_iter );

            numberType left_to_write = cnt;

            numberType first_write_count = std::min( cnt, first_left_write_count );

            if ( first_write_count > 0 )
            {
                cb( iter, 0, first_write_count );
                
                iter += first_write_count;

                if ( iter == size )
                {
                    iter = 0;
                }
            }

            if ( left_to_write > first_write_count )
            {
                left_to_write -= first_write_count;

                std::uint32_t second_write_count = std::min( left_to_write, second_left_write_count );

                if ( second_write_count > 0 )
                {
                    cb( iter, first_write_count, second_write_count );

                    iter += second_write_count;
                }
            }
        }

        this->iter = iter;
    }

    AINLINE void Increment( numberType cnt )
    {
        this->iter = ( ( this->iter + cnt ) % this->size );
    }

private:
    template <typename numberType>
    AINLINE static numberType minus_modulus( numberType value, numberType subtractBy, numberType modulus )
    {
        numberType real_subtractBy = ( subtractBy % modulus );

        if ( real_subtractBy > value )
        {
            numberType neg_value = ( real_subtractBy - value );

            value = ( modulus - neg_value );
        }
        else
        {
            value -= real_subtractBy;
        }

        return value;
    }

public:
    AINLINE void Decrement( numberType cnt )
    {
        this->iter = minus_modulus( this->iter, cnt, this->size );
    }

    AINLINE void IncrementEnd( numberType cnt )
    {
        this->end_iter = ( ( this->end_iter + cnt ) % this->size );
    }

    AINLINE void DecrementEnd( numberType cnt )
    {
        this->end_iter = minus_modulus( this->end_iter, cnt, this->size );
    }

    AINLINE numberType GetOffset( numberType get_iter ) const
    {
        numberType iter = this->iter;
        numberType end_iter = this->end_iter;
        numberType size = this->size;

        if ( iter >= end_iter )
        {
            if ( get_iter >= iter && get_iter < size )
            {
                return ( get_iter - iter );
            }

            if ( get_iter < end_iter )
            {
                return ( ( size - iter ) + get_iter );
            }

            throw eir_exception();
        }
        else
        {
            if ( get_iter < iter || get_iter >= end_iter )
            {
                throw eir_exception();
            }

            return ( get_iter - iter );
        }
    }

    numberType iter;
    numberType end_iter;
    numberType size;
};

} //namespace eir

#endif //_RING_BUFFER_HANDLERS_