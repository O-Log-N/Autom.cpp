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

#ifndef NODECONTAINER_H
#define NODECONTAINER_H

#include <set>

namespace autom {

class LoopContainer;
class Node;

class InfraNodeContainer {
    std::set< Node* > nodes;
    LoopContainer* zero;

  public:
    InfraNodeContainer( LoopContainer* c ) : zero( c ) {}
    void addNode( Node* node );
    void removeNode( Node* node );
    void run();
    void debugDump( int line ) const;
};

}

#endif
