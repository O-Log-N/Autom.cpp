#include "../include/anode.h"
#include "../include/aassert.h"

using namespace autom;

Future::Future( Node* node_ ) : node( node_ ) {
    futureId = node->nextFutureId();
}

void Future::then( FutureFunction fn ) {
    node->registerFuture( getId(), fn );
}

const Buffer& Future::value() const {
    return node->findResult( futureId );
}

void Node::infraProcessEvent( const NodeQItem& item ) {
    auto it = futureMap.find( item.id );
    if( it != futureMap.end() ) {
        it->second.result = item.b;
        it->second.fn();
//		futureMap.erase( it );
    }
}

const Buffer& Node::findResult( FutureId id ) const {
    auto it = futureMap.find( id );
    if( it != futureMap.end() )
        return it->second.result;
    AASSERT0( 0 );
}

void FS::run() {
    NodeQItem item;
    while( true ) {
        while( inputQueue.pop( item ) ) {
            if( nodes.end() != nodes.find( item.node ) )
                if( item.node->parentFS == this )
                    item.node->infraProcessEvent( item );
        }
        inputQueue.wait();
    }
}

void FS::sampleAsyncEvent( Node* node, FutureId id ) {
    _sleep( 10000 );
    std::string s( fmt::format( "Hello Future {}!", id ) );
    NodeQItem item;
    item.id = id;
    item.b = Buffer( s.c_str() );
    item.node = node;
    node->parentFS->pushEvent( item );
}

Future FS::readFile( Node* node, const char* path ) {
    Future future( node );
    std::thread th( sampleAsyncEvent, node, future.getId() );
    th.detach();
    return future;
}


