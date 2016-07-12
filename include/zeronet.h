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
#include "../3rdparty/libuv/include/uv.h"
#include "aassert.h"
#include "abuffer.h"
#include "../libsrc/infra/loopcontainer.h"

namespace autom {

class TcpZeroSocket {
  public:
    enum EventId { ID_ERROR = 1, ID_CONNECT, ID_DATA, ID_DRAIN, ID_CLOSED };
    uv_stream_t* stream;

    TcpZeroSocket() {
        onConnected = []() {};
        onRead = []( const NetworkBuffer* ) {};
        onClosed = []() {};
        onError = []() {};
    }

    std::function< void( void ) > onConnected;
    std::function< void( const NetworkBuffer* ) > onRead;
    std::function< void( void ) > onClosed;
    std::function< void( void ) > onError;

    void read() const;
    void write( const void* buff, size_t sz ) const;
    void close() const;

    void on( int eventId, std::function< void( const NetworkBuffer* ) > fn ) {
        if( ID_DATA == eventId )
            onRead = fn;
        else
            AASSERT4( false );
    }
    void on( int eventId, std::function< void( void ) > fn ) {
        if( ID_CLOSED == eventId )
            onClosed = fn;
        else if( ID_ERROR == eventId )
            onError = fn;
        else if( ID_CONNECT == eventId )
            onConnected = fn;
        else
            AASSERT4( false );
    }
};

class TcpZeroServer {
    uv_tcp_t* handle;
    LoopContainer* loop;

  public:
    enum EventId { ID_ERROR = 1, ID_CONNECT = 2, };

    std::function< void( TcpZeroSocket* ) > onConnect;
    std::function< void( void ) > onError;

    explicit TcpZeroServer( LoopContainer* loop_ ) : loop( loop_ ) {
        onConnect = []( TcpZeroSocket* ) {};
        onError = []() {};
    }
    void on( int eventId, std::function< void( TcpZeroSocket* ) > fn ) {
        if( TcpZeroServer::ID_CONNECT == eventId )
            onConnect = fn;
        else
            AASSERT4( 0 );
    }
    void on( int eventId, std::function< void( void ) > fn ) {
        if( TcpZeroServer::ID_ERROR == eventId )
            onError = fn;
        else
            AASSERT4( 0 );
    }
    bool listen( int port );
};

namespace net {

TcpZeroServer* createServer( LoopContainer* loop );
TcpZeroSocket* connect( LoopContainer* loop, const char* addr, int port );

}

}

#endif


