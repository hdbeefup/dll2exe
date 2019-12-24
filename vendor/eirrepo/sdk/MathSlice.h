/*****************************************************************************
*
*  PROJECT:     Eir SDK
*  LICENSE:     See LICENSE in the top level directory
*  FILE:        eirrepo/sdk/MathSlice.h
*  PURPOSE:     Implementation of mathematical intervals
*
*  Find the Eir SDK at: https://osdn.net/projects/eirrepo/
*  Multi Theft Auto is available from http://www.multitheftauto.com/
*
*****************************************************************************/

#ifndef _EIR_SDK_MATH_SLICE_HEADER_
#define _EIR_SDK_MATH_SLICE_HEADER_

#include <type_traits>
#include <limits>
#include <algorithm>

#include "MacroUtils.h"
#include "eirutils.h"

namespace eir
{

// Forward declarations for inter-changeability.
template <typename numberType> struct lowerBound;
template <typename numberType> struct upperBound;

// Definition of an upper bound.
template <typename numberType>
struct upperBound
{
    // If we are an unsigned numberType we want to have the specialty that a maximum value is remapped to the value "-1"
    // meaning a value below all unsigned values. This is made to properly model empty intervals and is actually an
    // optimization trade-off.

    AINLINE upperBound( void ) noexcept
    {
        this->data.value = 0;

        if constexpr ( std::is_floating_point <numberType>::value )
        {
            this->data.isIncluded = true;
        }
    }
    AINLINE upperBound( numberType value ) noexcept
    {
        this->data.value = std::move( value );

        if constexpr ( std::is_floating_point <numberType>::value )
        {
            this->data.isIncluded = true;
        }
    }
    // If isIncluded == true then this constructor will never throw an exception.
    AINLINE upperBound( numberType value, bool isIncluded ) noexcept(std::is_floating_point <numberType>::value)
    {
        if constexpr ( std::is_integral <numberType>::value )
        {
            if ( isIncluded == false )
            {
                if constexpr ( std::is_unsigned <numberType>::value )
                {
                    // Special optimization: treat max as -1.
                    if ( value == std::numeric_limits <numberType>::max() )
                    {
                        throw eir_exception();
                    }

                    if ( value == std::numeric_limits <numberType>::min() )
                    {
                        value = std::numeric_limits <numberType>::max();
                    }
                    else
                    {
                        value--;
                    }
                }
                else
                {
                    // Regular range.
                    if ( value == std::numeric_limits <numberType>::min() )
                    {
                        throw eir_exception();
                    }

                    value--;
                }
            }

            this->data.value = std::move( value );
        }
        else if constexpr ( std::is_floating_point <numberType>::value )
        {
            this->data.value = std::move( value );
            this->data.isIncluded = isIncluded;
        }
        else
        {
#ifdef _MSC_VER
            static_assert( false, "invalid numberType in advanced upperBound constructor" );
#endif //_MSC_VER
        }
    }
    AINLINE upperBound( const lowerBound <numberType>& value, bool isIncluded ) noexcept(std::is_floating_point <numberType>::value);

    AINLINE upperBound( const upperBound& ) = default;
    AINLINE upperBound( upperBound&& ) = default;

    AINLINE ~upperBound( void ) = default;

    AINLINE upperBound& operator = ( const upperBound& ) = default;
    AINLINE upperBound& operator = ( upperBound&& ) = default;

    AINLINE static upperBound minus_one( void )
    {
        if constexpr ( std::is_integral <numberType>::value && std::is_unsigned <numberType>::value )
        {
            return upperBound( std::numeric_limits <numberType>::max() );
        }
        else
        {
            return upperBound( -1 );
        }
    }

    AINLINE void lowbound( numberType bound, bool isIncluded = true ) noexcept(std::is_floating_point <numberType>::value)
    {
        // Imagine how painful this stuff were if we did not have "constexpr-if".
        // You'd have to write method/class specializations for each case!

        if constexpr ( std::is_integral <numberType>::value )
        {
            if constexpr ( std::is_unsigned <numberType>::value )
            {
                numberType curBound = this->data.value;

                // Nothing can be smaller than -1.
                if ( curBound != std::numeric_limits <numberType>::max() )
                {
                    if ( isIncluded == false )
                    {
                        if ( bound == std::numeric_limits <numberType>::max() )
                        {
                            // We do not support -2.
                            throw eir_exception();
                        }

                        if ( bound == std::numeric_limits <numberType>::min() )
                        {
                            bound = std::numeric_limits <numberType>::max();
                        }
                        else
                        {
                            bound--;
                        }
                    }

                    // Special case: treat max as -1.
                    if ( bound == std::numeric_limits <numberType>::max() )
                    {
                        this->data.value = std::move( bound );
                    }
                    else
                    {
                        if ( bound < curBound )
                        {
                            this->data.value = bound;
                        }
                    }
                }
            }
            else
            {
                if ( isIncluded == false )
                {
                    if ( bound == std::numeric_limits <numberType>::min() )
                    {
                        // I decided against storing the "alwaysFalse" condition inside the bounds for
                        // integral values because it is such a rare condition that should be prevented
                        // by the slice user. Otherwise we would pollute many memory and other logic
                        // classes!
                        throw eir_exception();
                    }

                    bound--;
                }

                if ( bound < this->data.value )
                {
                    this->data.value = std::move( bound );
                }
            }
        }
        else if constexpr ( std::is_floating_point <numberType>::value )
        {
            numberType curBound = this->data.value;

            if ( bound == curBound )
            {
                if ( isIncluded == false )
                {
                    this->data.isIncluded = false;
                }
            }
            else if ( bound < curBound )
            {
                this->data.value = bound;
                this->data.isIncluded = isIncluded;
            }
        }
        else
        {
#ifdef _MSC_VER
            static_assert( false, "invalid numberType for upperBound::lowbound" );
#endif //_MSC_VER
        }
    }

    // If we are a floating-point then we are a number that converges very close to our value from negative infinity.

    // Operators between upperBounds.
    AINLINE friend bool operator <= ( const upperBound& left, const upperBound& right ) noexcept
    {
        if constexpr ( std::is_integral <numberType>::value )
        {
            if constexpr ( std::is_unsigned <numberType>::value )
            {
                // Remember that we are always inclusive in integral mode.
                numberType leftBound = left.data.value;
                numberType rightBound = right.data.value;

                if ( leftBound == rightBound )
                {
                    return true;
                }

                // Special case: -1 is smaller equal than any value of bound.
                if ( leftBound == std::numeric_limits <numberType>::max() )
                {
                    return true;
                }

                // Special case: treat max as -1.
                if ( rightBound == std::numeric_limits <numberType>::max() )
                {
                    return false;
                }

                return ( leftBound <= rightBound );
            }
            else
            {
                return ( left.get_value() <= right.get_value() );
            }
        }
        else if constexpr ( std::is_floating_point <numberType>::value )
        {
            bool isSameIncluded = ( right.data.isIncluded || !left.data.isIncluded );

            if ( isSameIncluded )
            {
                return ( left.data.value <= right.data.value );
            }
            else
            {
                return ( left.data.value < right.data.value );
            }
        }
        else
        {
#ifdef _MSC_VER
            static_assert( false, "not implemented operator <= for numberType" );
#endif //_MSC_VER
        }
    }
    AINLINE friend bool operator > ( const upperBound& left, const upperBound& right ) noexcept
    {
        return !( operator <= ( left, right ) );
    }
    AINLINE friend bool operator >= ( const upperBound& left, const upperBound& right ) noexcept
    {
        return operator <= ( right, left );
    }
    AINLINE friend bool operator < ( const upperBound& left, const upperBound& right ) noexcept
    {
        return !( operator >= ( left, right ) );
    }

    // Do not forget to implement equality properly.

    AINLINE friend bool operator == ( const upperBound& left, const upperBound& right ) noexcept
    {
        if ( left.data.value != right.data.value )
        {
            return false;
        }

        if constexpr ( std::is_floating_point <numberType>::value )
        {
            if ( left.data.isIncluded != right.data.isIncluded )
            {
                return false;
            }
        }

        return true;
    }
    AINLINE friend bool operator != ( const upperBound& left, const upperBound& right ) noexcept
    {
        return !( operator == ( left, right ) );
    }

    // Standard comparison with number.
    AINLINE bool is_bounded( const upperBound& right ) const noexcept
    {
        return ( right <= *this );
    }

    AINLINE const numberType& get_value( void ) const noexcept
    {
        return this->data.value;
    }

    AINLINE bool is_included( void ) const noexcept
    {
        if constexpr ( std::is_integral <numberType>::value )
        {
            return true;
        }
        else if constexpr ( std::is_floating_point <numberType>::value )
        {
            return this->data.isIncluded;
        }
        else
        {
            return false;
        }
    }

private:
    struct fields_integral
    {
        numberType value;
    };

    struct fields_floating_point
    {
        numberType value;
        bool isIncluded;
    };

    typename std::conditional <std::is_floating_point <numberType>::value, fields_floating_point, fields_integral>::type data;
};

// Definition of a lower bound.
template <typename numberType>
struct lowerBound
{
    AINLINE lowerBound( void ) noexcept
    {
        this->data.value = 0;

        if constexpr ( std::is_floating_point <numberType>::value )
        {
            this->data.isIncluded = true;
        }
    }
    AINLINE lowerBound( numberType value ) noexcept
    {
        this->data.value = std::move( value );

        if constexpr ( std::is_floating_point <numberType>::value )
        {
            this->data.isIncluded = true;
        }
    }
    // If isIncluded is true then this constructor will never throw an exception.
    AINLINE lowerBound( numberType value, bool isIncluded ) noexcept(std::is_floating_point <numberType>::value)
    {
        if constexpr ( std::is_integral <numberType>::value )
        {
            if ( isIncluded == false )
            {
                if ( value == std::numeric_limits <numberType>::max() )
                {
                    throw eir_exception();
                }

                value++;
            }

            this->data.value = std::move( value );
        }
        else if constexpr ( std::is_floating_point <numberType>::value )
        {
            this->data.value = std::move( value );
            this->data.isIncluded = isIncluded;
        }
        else
        {
#ifdef _MSC_VER
            static_assert( false, "invalid numberType in advanced lowerBound constructor" );
#endif //_MSC_VER
        }
    }
    AINLINE lowerBound( const upperBound <numberType>& value, bool isIncluded ) noexcept(std::is_floating_point <numberType>::value);

    AINLINE lowerBound( const lowerBound& ) = default;
    AINLINE lowerBound( lowerBound&& ) = default;

    AINLINE ~lowerBound( void ) = default;

    AINLINE lowerBound& operator = ( const lowerBound& ) = default;
    AINLINE lowerBound& operator = ( lowerBound&& ) = default;

    AINLINE void highbound( numberType bound, bool isIncluded = true ) noexcept(std::is_floating_point <numberType>::value)
    {
        if constexpr ( std::is_integral <numberType>::value )
        {
            if ( isIncluded == false )
            {
                if ( bound == std::numeric_limits <numberType>::max() )
                {
                    // I decided against storing the "alwaysFalse" condition inside the bounds for
                    // integral values because it is such a rare condition that should be prevented
                    // by the slice user. Otherwise we would pollute many memory and other logic
                    // classes!
                    throw eir_exception();
                }

                bound++;
            }

            if ( bound > this->data.value )
            {
                this->data.value = bound;
            }
        }
        else if constexpr ( std::is_floating_point <numberType>::value )
        {
            numberType curBound = this->data.value;

            if ( bound == curBound )
            {
                if ( isIncluded == false )
                {
                    this->data.isIncluded = false;
                }
            }
            else if ( bound > curBound )
            {
                this->data.value = bound;
                this->data.isIncluded = isIncluded;
            }
        }
        else
        {
#ifdef _MSC_VER
            static_assert( false, "invalid numberType for lowerBound::highbound" );
#endif //_MSC_VER
        }
    }

    // If we are a floating-point then our value is a number that converges very close to the actual value from infinity.

    AINLINE friend bool operator <= ( const lowerBound& left, const lowerBound& right ) noexcept
    {
        if constexpr ( std::is_integral <numberType>::value )
        {
            return ( left.data.value <= right.data.value );
        }
        else if constexpr ( std::is_floating_point <numberType>::value )
        {
            bool isSameIncluded = ( left.data.isIncluded || !right.data.isIncluded );

            if ( isSameIncluded )
            {
                return ( left.data.value <= right.data.value );
            }
            else
            {
                return ( left.data.value < right.data.value );
            }
        }
        else
        {
            return false;
        }
    }
    AINLINE friend bool operator > ( const lowerBound& left, const lowerBound& right ) noexcept
    {
        return !( operator <= ( left, right ) );
    }
    AINLINE friend bool operator >= ( const lowerBound& left, const lowerBound& right ) noexcept
    {
        return operator <= ( right, left );
    }
    AINLINE friend bool operator < ( const lowerBound& left, const lowerBound& right ) noexcept
    {
        return !( operator >= ( left, right ) );
    }

    // Equality implementation.
    AINLINE friend bool operator == ( const lowerBound& left, const lowerBound& right ) noexcept
    {
        if ( left.data.value != right.data.value )
        {
            return false;
        }

        if constexpr ( std::is_floating_point <numberType>::value )
        {
            if ( left.data.isIncluded != right.data.isIncluded )
            {
                return false;
            }
        }

        return true;
    }
    AINLINE friend bool operator != ( const lowerBound& left, const lowerBound& right ) noexcept
    {
        return !( operator == ( left, right ) );
    }

    // Standard comparison with number.
    AINLINE bool is_bounded( const lowerBound& value ) const noexcept
    {
        return ( *this <= value );
    }

    AINLINE const numberType& get_value( void ) const noexcept
    {
        return this->data.value;
    }

    AINLINE bool is_included( void ) const noexcept
    {
        if constexpr ( std::is_integral <numberType>::value )
        {
            return true;
        }
        else if constexpr ( std::is_floating_point <numberType>::value )
        {
            return this->data.isIncluded;
        }
        else
        {
            return false;
        }
    }

private:
    struct fields_integral
    {
        numberType value;
    };

    struct fields_floating_point
    {
        numberType value;
        bool isIncluded;
    };

    typename std::conditional <std::is_floating_point <numberType>::value, fields_floating_point, fields_integral>::type data;
};

// *** Methods that work in combination of both lowerBound and upperBound.

// Cross-domain constructor for upperBound.
template <typename numberType>
AINLINE upperBound <numberType>::upperBound( const lowerBound <numberType>& value, bool isIncluded ) noexcept(std::is_floating_point <numberType>::value)
{
    if constexpr ( std::is_integral <numberType>::value )
    {
        numberType low_value = value.get_value();

        if constexpr ( std::is_unsigned <numberType>::value )
        {
            if ( isIncluded == false )
            {
                // Special case: treat max() as -1.
                if ( low_value == 0 )
                {
                    low_value = std::numeric_limits <numberType>::max();
                }
                else
                {
                    low_value--;
                }
            }
            else
            {
                // This value is not displayable in this domain.
                if ( low_value == std::numeric_limits <numberType>::max() )
                {
                    throw eir_exception();
                }
            }

            this->data.value = low_value;
        }
        else
        {
            if ( isIncluded == false )
            {
                if ( low_value == std::numeric_limits <numberType>::min() )
                {
                    throw eir_exception();
                }

                low_value--;
            }

            this->data.value = low_value;
        }
    }
    else if constexpr ( std::is_floating_point <numberType>::value )
    {
        this->data.value = value.get_value();
        this->data.isIncluded = isIncluded;
    }
    else
    {
#ifdef _MSC_VER
        static_assert( false, "no cross-domain constructor for upperBound at numberType" );
#endif //_MSC_VER
    }
}

// Cross-domain constructor for lowerBound.
template <typename numberType>
AINLINE lowerBound <numberType>::lowerBound( const upperBound <numberType>& value, bool isIncluded ) noexcept(std::is_floating_point <numberType>::value)
{
    if constexpr ( std::is_integral <numberType>::value )
    {
        numberType highValue = value.get_value();

        if constexpr ( std::is_unsigned <numberType>::value )
        {
            if ( isIncluded == false )
            {
                // Special case: treat max as -1.
                if ( highValue == std::numeric_limits <numberType>::max() )
                {
                    highValue = 0;
                }
                else
                {
                    highValue++;
                }
            }
            else
            {
                // Special case: since max is actually -1 and -1 is not in the domain of lowerBound,
                //  we must bail here.
                if ( highValue == std::numeric_limits <numberType>::max() )
                {
                    throw eir_exception();
                }
            }

            this->data.value = highValue;
        }
        else
        {
            if ( isIncluded == false )
            {
                if ( highValue == std::numeric_limits <numberType>::max() )
                {
                    throw eir_exception();
                }

                highValue++;
            }

            this->data.value = highValue;
        }
    }
    else if constexpr ( std::is_floating_point <numberType>::value )
    {
        this->data.value = value.get_value();
        this->data.isIncluded = isIncluded;
    }
    else
    {
#ifdef _MSC_VER
        static_assert( false, "no cross-domain constructor for lowerBound at numberType" );
#endif //_MSC_VER
    }
}

// Returns the difference between upper border and lower border of an interval.
template <typename numberType>
AINLINE numberType get_bound_span( const lowerBound <numberType>& low, const upperBound <numberType>& high ) noexcept
{
    if constexpr ( std::is_integral <numberType>::value )
    {
        if constexpr ( std::is_unsigned <numberType>::value )
        {
            numberType lowValue = low.get_value();
            numberType highValue = high.get_value();

            // Remember that only highValue has a special case for max.

            // Special case: treat max as -1.
            if ( highValue == std::numeric_limits <numberType>::max() )
            {
                // Really does not work.
                return 0;
            }

            // Actually really cool that we can handle this case so gracefully.

            if ( lowValue > highValue )
            {
                // Meow.
                return 0;
            }

            return ( highValue - lowValue + 1 );
        }
        else
        {
            return ( high.get_value() - low.get_value() + 1 );
        }
    }
    else if constexpr ( std::is_floating_point <numberType>::value )
    {
        return ( high.get_value() - low.get_value() );
    }
    else
    {
#ifdef _MSC_VER
        static_assert( false, "invalid numberType in get_bound_span function" );
#endif //_MSC_VER
    }
}

// Compares two bound spans and returns the status.
template <typename numberType>
AINLINE eCompResult compare_bounds( const lowerBound <numberType>& low, const upperBound <numberType>& high )
{
    if constexpr ( std::is_integral <numberType>::value )
    {
        if constexpr ( std::is_unsigned <numberType>::value )
        {
            numberType lowBound = low.get_value();
            numberType highBound = high.get_value();

            // Special case: treat max as -1.
            if ( highBound == std::numeric_limits <numberType>::max() )
            {
                return eCompResult::LEFT_GREATER;
            }

            return eir::DefaultValueCompare( lowBound, highBound );
        }
        else
        {
            return eir::DefaultValueCompare( low.get_value(), high.get_value() );
        }
    }
    else if constexpr ( std::is_floating_point <numberType>::value )
    {
        numberType lowBound = low.get_value();
        numberType highBound = high.get_value();

        if ( lowBound < highBound )
        {
            return eCompResult::LEFT_LESS;
        }
        if ( lowBound > highBound )
        {
            return eCompResult::LEFT_GREATER;
        }

        // Equality if the hard case.
        if ( low.is_included() == false || high.is_included() == false )
        {
            return eCompResult::LEFT_GREATER;
        }

        return eCompResult::EQUAL;
    }
    else
    {
#ifdef _MSC_VER
        static_assert( false, "invalid numberType in compare_bounds function" );
#endif //_MSC_VER
    }
}

// *** Operators between lowerBound and upperBound.

template <typename numberType>
AINLINE bool operator <= ( const lowerBound <numberType>& left, const upperBound <numberType>& right ) noexcept
{
    eCompResult cmpRes = compare_bounds( left, right );

    return ( cmpRes == eCompResult::LEFT_LESS || cmpRes == eCompResult::EQUAL );
}
template <typename numberType>
AINLINE bool operator > ( const lowerBound <numberType>& left, const upperBound <numberType>& right ) noexcept
{
    return !( operator <= ( left, right ) );
}
template <typename numberType>
AINLINE bool operator >= ( const upperBound <numberType>& left, const lowerBound <numberType>& right ) noexcept
{
    return operator <= ( right, left );
}
template <typename numberType>
AINLINE bool operator < ( const upperBound <numberType>& left, const lowerBound <numberType>& right ) noexcept
{
    return !( operator >= ( left, right ) );
}

// *** Now the other way round, operators.

template <typename numberType>
AINLINE bool operator <= ( const upperBound <numberType>& left, const lowerBound <numberType>& right ) noexcept
{
    eCompResult cmpRes = compare_bounds( right, left );

    return ( cmpRes == eCompResult::LEFT_GREATER || cmpRes == eCompResult::EQUAL );
}
template <typename numberType>
AINLINE bool operator > ( const upperBound <numberType>& left, const lowerBound <numberType>& right ) noexcept
{
    return !( operator <= ( left, right ) );
}
template <typename numberType>
AINLINE bool operator < ( const lowerBound <numberType>& left, const upperBound <numberType>& right ) noexcept
{
    return operator > ( right, left );
}
template <typename numberType>
AINLINE bool operator >= ( const lowerBound <numberType>& left, const upperBound <numberType>& right ) noexcept
{
    return !( operator < ( left, right ) );
}

// *** Equality operators between upperBound and lowerBound.

template <typename numberType>
AINLINE bool operator == ( const lowerBound <numberType>& left, const upperBound <numberType>& right ) noexcept
{
    if ( left.get_value() != right.get_value() )
    {
        return false;
    }

    if ( left.is_included() == false || right.is_included() == false )
    {
        return false;
    }

    return true;
}
template <typename numberType>
AINLINE bool operator != ( const lowerBound <numberType>& left, const upperBound <numberType>& right ) noexcept
{
    return !( operator == ( left, right ) );
}
template <typename numberType>
AINLINE bool operator == ( const upperBound <numberType>& left, const lowerBound <numberType>& right ) noexcept
{
    return operator == ( right, left );
}
template <typename numberType>
AINLINE bool operator != ( const upperBound <numberType>& left, const lowerBound <numberType>& right ) noexcept
{
    return operator != ( right, left );
}

// Returns the border that tightly aligns on another border.
template <typename numberType>
AINLINE upperBound <numberType> GetTightBound( const lowerBound <numberType>& border )
{
    return upperBound( border, !border.is_included() );
}

template <typename numberType>
AINLINE lowerBound <numberType> GetTightBound( const upperBound <numberType>& border )
{
    return lowerBound( border, !border.is_included() );
}

// Sometimes there is no tight bound.
template <typename numberType>
AINLINE bool HasTightBound( const lowerBound <numberType>& border )
{
    if constexpr ( std::is_integral <numberType>::value )
    {
        if constexpr ( std::is_unsigned <numberType>::value )
        {
            return true;
        }
        else
        {
            // Due to the inclusive-optimization we have to make sure.
            return ( border.get_value() > std::numeric_limits <numberType>::min() );
        }
    }
    else if constexpr ( std::is_floating_point <numberType>::value )
    {
        return true;
    }
    else
    {
        return false;
    }
}

template <typename numberType>
AINLINE bool HasTightBound( const upperBound <numberType>& border )
{
    if constexpr ( std::is_integral <numberType>::value )
    {
        if constexpr ( std::is_unsigned <numberType>::value )
        {
            return true;
        }
        else
        {
            // Same as for above.
            return ( border.get_value() < std::numeric_limits <numberType>::max() );
        }
    }
    else if constexpr ( std::is_floating_point <numberType>::value )
    {
        return true;
    }
    else
    {
        return false;
    }
}

enum eIntersectionResult
{
    INTERSECT_EQUAL,
    INTERSECT_INSIDE,
    INTERSECT_BORDER_START,
    INTERSECT_BORDER_END,
    INTERSECT_ENCLOSING,
    INTERSECT_FLOATING_START,
    INTERSECT_FLOATING_END,
    INTERSECT_UNKNOWN   // if something went horribly wrong (like NaNs).
};

// Static methods for easier result management.
AINLINE bool isBorderIntersect( eIntersectionResult result ) noexcept
{
    return ( result == INTERSECT_BORDER_START || result == INTERSECT_BORDER_END );
}

AINLINE bool isFloatingIntersect( eIntersectionResult result ) noexcept
{
    return ( result == INTERSECT_FLOATING_START || result == INTERSECT_FLOATING_END );
}

AINLINE bool isCoveredIntersect( eIntersectionResult result ) noexcept
{
    return ( result == INTERSECT_EQUAL || result == INTERSECT_INSIDE );
}

AINLINE bool isCoveringIntersect( eIntersectionResult result ) noexcept
{
    return ( result == INTERSECT_EQUAL || result == INTERSECT_ENCLOSING );
}

// Representation of a mathematical interval.
template <typename numberType>
class mathSlice
{
    AINLINE mathSlice( numberType startOffset, numberType endOffset, bool startIncluded, bool endIncluded )
        : startBound( startOffset, startIncluded ), endBound( endOffset, endIncluded )
    {
        return;
    }

    AINLINE mathSlice( const lowerBound <numberType>& low, const upperBound <numberType>& high ) noexcept
        : startBound( low ), endBound( high )
    {
        return;
    }

public:
    AINLINE mathSlice( void ) noexcept : startBound( 0 ), endBound( 0, false )
    {
        return;
    }

    AINLINE mathSlice( numberType startOffset, numberType dataSize ) noexcept : startBound( startOffset ), endBound( startOffset + dataSize, false )
    {
        return;
    }

    // Note that both offsets are inclusive.
    static AINLINE mathSlice fromOffsets( numberType startOffset, numberType endOffset, bool startIncluded = true, bool endIncluded = true )
    {
        return mathSlice( startOffset, endOffset, startIncluded, endIncluded );
    }

    static AINLINE mathSlice fromBounds( const lowerBound <numberType>& low, const upperBound <numberType>& high ) noexcept
    {
        return mathSlice( low, high );
    }

    AINLINE bool IsEmpty( void ) const noexcept
    {
        return ( this->startBound > this->endBound );
    }

    AINLINE numberType GetSliceSize( void ) const noexcept
    {
        return get_bound_span( this->startBound, this->endBound );
    }

    AINLINE void SetSlicePosition( numberType val ) noexcept(noexcept(lowerBound(val)) && noexcept(upperBound(val, false)))
    {
        const numberType sliceSize = GetSliceSize();

        this->startBound = lowerBound( val );
        this->endBound = upperBound( val + sliceSize, false );
    }


    AINLINE void OffsetSliceBy( numberType val ) noexcept(noexcept(mathSlice().SetSlicePosition( val )))
    {
        SetSlicePosition( this->startBound.get_value() + val );
    }

    AINLINE void SetSliceStartBound( lowerBound <numberType> bnd ) noexcept
    {
        this->startBound = std::move( bnd );
    }

    AINLINE void SetSliceStartPoint( numberType val ) noexcept
    {
        this->SetSliceStartBound( lowerBound( val ) );
    }

    AINLINE void SetSliceEndBound( upperBound <numberType> bnd ) noexcept
    {
        this->endBound = std::move( bnd );
    }

    AINLINE void SetSliceEndPoint( numberType val ) noexcept
    {
        this->SetSliceEndBound( upperBound( val ) );
    }

    AINLINE numberType GetSliceStartPoint( void ) const noexcept
    {
        return startBound.get_value();
    }

    AINLINE const lowerBound <numberType>& GetSliceStartBound( void ) const noexcept
    {
        return startBound;
    }

    AINLINE bool IsStartIncluded( void ) const noexcept
    {
        return startBound.is_included();
    }

    AINLINE numberType GetSliceEndPoint( void ) const noexcept
    {
        return endBound.get_value();
    }

    AINLINE const upperBound <numberType>& GetSliceEndBound( void ) const noexcept
    {
        return endBound;
    }

    AINLINE bool IsEndIncluded( void ) const noexcept
    {
        return endBound.is_included();
    }

    // Quick helper. You should consider using the intersectWith method instead.
    AINLINE friend bool operator == ( const mathSlice& left, const mathSlice& right )
    {
        return ( left.startBound == right.startBound && left.endBound == right.endBound );
    }
    AINLINE friend bool operator != ( const mathSlice& left, const mathSlice& right )
    {
        return !( operator == ( left, right ) );
    }

    AINLINE void collapse( void ) noexcept
    {
        this->endBound = upperBound( this->startBound.get_value(), false );
    }

    AINLINE eIntersectionResult intersectWith( const mathSlice& right ) const noexcept
    {
        // Make sure the slice has a valid size.
        if ( this->IsEmpty() == false && right.IsEmpty() == false )
        {
            // Get generic stuff.
            lowerBound <numberType> sliceStartA = this->startBound;
            upperBound <numberType> sliceEndA = this->endBound;

            lowerBound <numberType> sliceStartB = right.startBound;
            upperBound <numberType> sliceEndB = right.endBound;

            // slice A -> this
            // slice B -> right

            // Handle all cases.
            // We implement the logic with comparisons only, as it is the most transparent for all number types.
            if ( sliceStartA == sliceStartB && sliceEndA == sliceEndB )
            {
                // Slice A is equal to Slice B
                return INTERSECT_EQUAL;
            }

            if ( sliceStartB >= sliceStartA && sliceEndB <= sliceEndA )
            {
                // Slice A is enclosing Slice B
                return INTERSECT_ENCLOSING;
            }

            if ( sliceStartB <= sliceStartA && sliceEndB >= sliceEndA )
            {
                // Slice A is inside Slice B
                return INTERSECT_INSIDE;
            }

            if ( sliceStartB < sliceStartA && ( sliceEndB >= sliceStartA && sliceEndB <= sliceEndA ) )
            {
                // Slice A is being intersected at the starting point.
                return INTERSECT_BORDER_START;
            }

            if ( sliceEndB > sliceEndA && ( sliceStartB >= sliceStartA && sliceStartB <= sliceEndA ) )
            {
                // Slice A is being intersected at the ending point.
                return INTERSECT_BORDER_END;
            }

            if ( sliceStartB < sliceStartA && sliceEndB < sliceStartA )
            {
                // Slice A is after Slice B
                return INTERSECT_FLOATING_END;
            }

            if ( sliceStartB > sliceEndA && sliceEndB > sliceEndA )
            {
                // Slice A is before Slice B
                return INTERSECT_FLOATING_START;
            }
        }

        return INTERSECT_UNKNOWN;
    }

    AINLINE bool getSharedRegion( const mathSlice& right, mathSlice& sharedOut ) const noexcept
    {
        eIntersectionResult intResult = this->intersectWith( right );

        lowerBound <numberType> start;
        upperBound <numberType> end;
        bool hasPosition = false;

        if ( intResult == INTERSECT_EQUAL || intResult == INTERSECT_ENCLOSING )
        {
            start = right.GetSliceStartBound();
            end = right.GetSliceEndBound();

            hasPosition = true;
        }
        else if ( intResult == INTERSECT_INSIDE )
        {
            start = this->GetSliceStartBound();
            end = this->GetSliceEndBound();

            hasPosition = true;
        }
        else if ( intResult == INTERSECT_BORDER_START )
        {
            start = this->GetSliceStartBound();
            end = right.GetSliceEndBound();

            hasPosition = true;
        }
        else if ( intResult == INTERSECT_BORDER_END )
        {
            start = right.GetSliceStartBound();
            end = this->GetSliceEndBound();

            hasPosition = true;
        }
        else if ( intResult == INTERSECT_FLOATING_START || intResult == INTERSECT_FLOATING_END )
        {
            // Nothing to do.
        }
        // we could also intersect unknown, in which case we do not care.

        if ( hasPosition )
        {
            sharedOut = fromBounds( start, end );
        }

        return hasPosition;
    }

    AINLINE mathSlice getSharedEnclosingRegion( const mathSlice& unifyWith ) const
    {
        return mathSlice::fromBounds(
            std::min( this->startBound, unifyWith.startBound ),
            std::max( this->endBound, unifyWith.endBound )
        );
    }

    template <typename callbackType>
    AINLINE void subtractRegion( const mathSlice& subtractBy, const callbackType& cb ) const noexcept
    {
        // This method does only work for integers, for now.

        // We return all memory data that is in our region but outside of subtractBy.
        lowerBound <numberType> ourStartBound = this->startBound;
        upperBound <numberType> ourEndBound = this->endBound;

        if ( ourStartBound > ourEndBound )
            return;

        lowerBound <numberType> subStartBound = subtractBy.startBound;
        upperBound <numberType> subEndBound = subtractBy.endBound;

        // If the subtract region has nothing, then we just return our entire region.
        if ( subStartBound > subEndBound )
        {
            cb( *this, true );
            return;
        }

        eIntersectionResult intResult = this->intersectWith( subtractBy );

        if ( intResult == INTERSECT_FLOATING_START || intResult == INTERSECT_FLOATING_END )
        {
            // Since there is nothing subtracted, we can return the entire thing.
            cb( *this, true );
        }
        else if ( intResult == INTERSECT_BORDER_START )
        {
            // Just return the remainder.
            const mathSlice remainder = fromBounds( GetTightBound( subEndBound ), ourEndBound );

            cb( remainder, true );
        }
        else if ( intResult == INTERSECT_BORDER_END )
        {
            const mathSlice remainder = fromBounds( ourStartBound, GetTightBound( subStartBound ) );

            cb( remainder, true );
        }
        else if ( intResult == INTERSECT_ENCLOSING )
        {
            // Since we are enclosing the subtractBy, we could have both region before and region after subtractBy.
            if ( ourStartBound < subStartBound )
            {
                mathSlice leftRegion = fromBounds( ourStartBound, GetTightBound( subStartBound ) );

                cb( leftRegion, true );
            }

            if ( ourEndBound > subEndBound )
            {
                mathSlice rightRegion = fromBounds( GetTightBound( subEndBound ), ourEndBound );

                cb( rightRegion, false );
            }
        }
        else if ( intResult == INTERSECT_INSIDE ||
                  intResult == INTERSECT_EQUAL )
        {
            // We have been entirely overshadowed/removed by subtractBy.
        }
        else if ( intResult == INTERSECT_UNKNOWN )
        {
            // Could be caused by some hardware math fault, we just return the entire thing.
            cb( *this, true );
        }
    }

private:
    lowerBound <numberType> startBound;
    upperBound <numberType> endBound;
};

// Helper for safely comparing signed with unsigned to equal.
template <typename leftNumberType, typename rightNumberType>
AINLINE bool are_integers_equal( const leftNumberType& left, const rightNumberType& right )
{
    if constexpr ( std::is_unsigned <leftNumberType>::value && !std::is_unsigned <rightNumberType>::value )
    {
        if ( right < 0 )
        {
            return false;
        }

        return ( left == (typename std::make_unsigned <rightNumberType>::type)right );
    }
    else if constexpr ( !std::is_unsigned <leftNumberType>::value && std::is_unsigned <rightNumberType>::value )
    {
        if ( left < 0 )
        {
            return false;
        }

        return ( (typename std::make_unsigned <leftNumberType>::type)left == right );
    }
    else
    {
        return ( left == right );
    }
}

// Neighborhood function.
template <typename leftNumberType, typename rightNumberType>
AINLINE bool IsNumberInNeighborhood( const leftNumberType& left, const rightNumberType& right ) noexcept
{
    if constexpr ( std::is_floating_point <leftNumberType>::value || std::is_floating_point <rightNumberType>::value )
    {
        return ( left == right );
    }

    if constexpr ( std::is_integral <leftNumberType>::value && std::is_integral <rightNumberType>::value )
    {
        if ( are_integers_equal( left, right ) )
        {
            return true;
        }

        if ( left > std::numeric_limits <leftNumberType>::min() )
        {
            if ( are_integers_equal( left - 1, right ) )
            {
                return true;
            }
        }

        if ( left < std::numeric_limits <leftNumberType>::max() )
        {
            if ( are_integers_equal( left + 1, right ) )
            {
                return true;
            }
        }
    }

    return false;
}

template <typename numberType>
AINLINE bool AreBoundsTight( const upperBound <numberType>& left, const lowerBound <numberType>& right ) noexcept
{
    // If we are at the maximum already then do not bother.
    if ( HasTightBound( left ) == false )
    {
        return false;
    }

    // There always is a tight bound so meow.
    return ( right == GetTightBound( left ) );
}

} //namespace eir

#endif //_EIR_SDK_MATH_SLICE_HEADER_
