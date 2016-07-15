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

#include "../include/zerotimer.h"
#include "../include/timer.h"
#include "../include/anode.h"
#include "infra/nodecontainer.h"

using namespace autom;

void autom::startTimeout( const Future< Timer >& future, Node* node, unsigned secDelay ) {
    FutureId id = future.infraGetId();
    startTimeout( node->parentLoop, [node, id]() {
        NodeQTimer item;
        item.id = id;
        node->infraProcessTimer( item );
    }, secDelay );
}

autom::Future< Timer > autom::startTimeout( Node* node, unsigned secDelay ) {
    Future< Timer > future( node );
    FutureId id = future.infraGetId();
    startTimeout( node->parentLoop, [node, id]() {
        NodeQTimer item;
        item.id = id;
        node->infraProcessTimer( item );
    }, secDelay );
    return future;
}

MultiFuture< Timer > autom::setInterval( Node* node, unsigned secRepeat ) {
    MultiFuture< Timer > future( node );
    FutureId id = future.infraGetId();
    setInterval( node->parentLoop, [node, id]() {
        NodeQTimer item;
        item.id = id;
        node->infraProcessTimer( item );
    }, secRepeat );

    return future;
}
