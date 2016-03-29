/*******************************************************************************
Copyright (C) 2016 OLogN Technologies AG
This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License version 2 as
published by the Free Software Foundation.
This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.
You should have received a copy of the GNU General Public License along
with this program; if not, write to the Free Software Foundation, Inc.,
51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
*******************************************************************************/

#ifndef AASSERT_H
#define AASSERT_H

#include "../3rdparty/cppformat/cppformat/format.h"

#include <exception>
#include <string>

namespace autom
{
class AssertionError : public std::exception
{
    std::string longMessage;

public:
    template< typename... ARGS >
    AssertionError( const char* cond_, const char* file_, int line_, const char* msg_, const ARGS& ... args ) {
        longMessage = fmt::format( "Assertion {} failed, file {}, line {} ", cond_, file_, line_ );
        longMessage.append( fmt::format( msg_, args... ) );
    }

    template< typename... ARGS >
    AssertionError( const char* cond_, const char* file_, int line_ )
    {
        longMessage = fmt::format( "Assertion {} failed, file {}, line {} ", cond_, file_, line_ );
    }

    const char* what() const noexcept override
    {
        return longMessage.c_str();
    }
};
}

#ifdef __GNUC__
#define AASSERT(cond,...)\
if( !(cond) )\
	throw autom::AssertionError( #cond, __FILE__, __LINE__, ## __VA_ARGS__ );
#else
#define AASSERT(cond,...)\
if( !(cond) )\
	throw autom::AssertionError( #cond, __FILE__, __LINE__, __VA_ARGS__ );
#endif

#endif
