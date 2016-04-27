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

#include "../libsrc/infra/infraconsole.h"
#include "aassert.h"

namespace autom {

using FutureFunction = std::function< void( const std::exception* ) >;
using FutureId = unsigned int;

class Node;
class InfraFutureBase;
class InfraNodeContainer;

struct NodeQTimer;
struct NodeQAccept;
struct NodeQBuffer;
struct NodeQConnect;

struct NodeQItem {
    FutureId id;
    Node* node;
};

class Node {
    using FutureMap = std::unordered_map< FutureId, std::unique_ptr< InfraFutureBase > >;
    FutureMap futureMap;
    FutureId nextFutureIdCount = 0;

  public:
    InfraNodeContainer* parentFS;

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
