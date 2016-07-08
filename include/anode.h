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

#ifndef ANODE_H
#define ANODE_H

#include <exception>
#include <unordered_map>

#include "aassert.h"
#include "abuffer.h"
#include "../libsrc/infra/infraconsole.h"
#include "../libsrc/infra/loopcontainer.h"

namespace autom {

using FutureFunction = std::function< void( const std::exception* ) >;
using FutureId = unsigned int;

class Node;
class InfraFutureBase;
class InfraNodeContainer;
class TcpSocket;

struct NodeQItem {
    FutureId id;
    Node* node;
};

struct NodeQTimer : public NodeQItem {
};

struct NodeQAccept : public NodeQItem {
    TcpSocket* sock;
};

struct NodeQBuffer : public NodeQItem {
    NetworkBuffer b;
    FutureId closeId;
};

struct NodeQConnect : public NodeQItem {
    TcpSocket* sock;
};

class Node {
    using FutureMap = std::unordered_map< FutureId, std::unique_ptr< InfraFutureBase > >;
    FutureMap futureMap;
    FutureId nextFutureIdCount = 0;

  public:
    virtual ~Node() = default;
    LoopContainer* parentLoop;

    FutureId nextFutureId() {
        return ++nextFutureIdCount;
    }

    InfraFutureBase* insertInfraFuture( FutureId id, InfraFutureBase* inf );
    InfraFutureBase* findInfraFuture( FutureId id );
    void futureCleanup();

    void infraProcessTimer( const NodeQTimer& item );
    void infraProcessTcpAccept( const NodeQAccept& item );
    void infraProcessTcpRead( const NodeQBuffer& item );
    void infraProcessTcpClosed( const NodeQBuffer& item );
    void infraProcessTcpConnect( const NodeQConnect& item );

    virtual void run() = 0;

    bool isEmpty() const;
    void debugDump() const;
};

}
#endif
