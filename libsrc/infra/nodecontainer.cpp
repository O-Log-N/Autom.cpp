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

#include "nodecontainer.h"
#include "loopcontainer.h"
#include "../../include/anode.h"

using namespace autom;

void InfraNodeContainer::addNode( Node* node ) {
    nodes.insert( node );
    node->parentLoop = zero;
    node->run();
}

void InfraNodeContainer::removeNode( Node* node ) {
    node->futureCleanup();
    AASSERT4( node->isEmpty() );
    nodes.erase( node );
}

void InfraNodeContainer::run() {
    zero->run();
}

void InfraNodeContainer::debugDump( int line ) const {
    INFRATRACE4( "line {} Nodes: {} ----------", line, nodes.size() );
    for( auto it : nodes ) {
        it->debugDump();
    }
}
