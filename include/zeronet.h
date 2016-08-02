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

#ifndef ZERONET_H
#define ZERONET_H

#include <functional>

namespace autom {

class LoopContainer;
class NetworkBuffer;

using Handle = size_t;

class TcpZeroSocket {
  public:
    enum EventId { ID_ERROR = 1, ID_CONNECT, ID_DATA, ID_DRAIN, ID_CLOSED };
    Handle h;

    void read() const;
    void write( const void* buff, size_t sz ) const;
    void close() const;

    void on( int eventId, std::function< void( const NetworkBuffer* ) > fn ) const;
    void on( int eventId, std::function< void( void ) > fn ) const;
};

class TcpZeroServer {
  public:
    enum EventId { ID_ERROR = 1, ID_CONNECT = 2, };
    Handle h;

    TcpZeroServer( LoopContainer* );

    bool listen( int port );

    void on( int eventId, std::function< void( TcpZeroSocket ) > fn );
    void on( int eventId, std::function< void( void ) > fn );
};

namespace net {

TcpZeroServer createServer( LoopContainer* loop );
TcpZeroSocket connect( LoopContainer* loop, const char* addr, int port );

}

}

#endif
