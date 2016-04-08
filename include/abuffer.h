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

#ifndef ABUFFER_H
#define ABUFFER_H

#include <utility>
#include <string>

namespace autom {

using NetworkBuffer = std::string;

class Buffer {
    std::string s;

  public:
    const char* toString() const {
        return s.c_str();
    }

    Buffer() {}
    Buffer( const char* s_ ) : s( s_ ) {}
    Buffer( const Buffer& other ) = default;
    Buffer( Buffer&& ) = default;
    Buffer& operator=( const Buffer& ) = default;
    Buffer& operator=( Buffer&& ) = default;

    void fromNetwork( const NetworkBuffer& b ) {
        s = b;
    }
};
}
#endif
