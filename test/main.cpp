#include <exception>
#include <iostream>

#include "../include/aassert.h"
#include "../include/aconsole.h"
#include "../include/abuffer.h"
#include "../include/anode.h"
#include "../include/net.h"
#include "../include/timer.h"
#include "../libsrc/infra/infraconsole.h"
#include "../libsrc/infra/nodecontainer.h"

using namespace std;
using namespace autom;

NodeConsoleWrapper console;

static void test1() {
    console.trace( "Starting..." );
    console.assignNewConsole( unique_ptr<NodeConsole>( new NodeConsole ) );
    auto idt1 = console.timeWithLabel();
    auto idt2 = console.timeWithLabel();
    console.info( "Hello, {0}{1}", "World", '!' );
    console.timeEnd( idt1, "1" );
    INFRATRACE3( "TRACE LEVEL IS {0}, line {1}", console.traceLevel(), __LINE__ );
    console.timeEnd( idt2, "2" );
    int a = 2, b = 3;
    AASSERT3( b > a );
    AASSERT3( a > b, "---{}---{}---{}---", 1, 2, 3 );
}

class NodeOne : public Node {
  public:
    void run() override {
        auto data = startTimeout( this, 2 );
        data.then( [ = ]() {
            infraConsole.log( "TIMER1" );
            auto data2 = startTimeout( this, 5 );
            data2.then( [ = ]() {
                infraConsole.log( "TIMER2" );
                auto data3 = startTimeout( this, 10 );
                data3.then( [ = ]() {
                    infraConsole.log( "TIMER3" );
                } );
            } );
        } );
    }
};

static void test2() {
    for( ;; ) {
        InfraNodeContainer fs;
        Node* p = new NodeOne;
        fs.addNode( p );
        fs.run();
        fs.removeNode( p );
        delete p;
    }
}

static void test3_timer_fn( uv_timer_t* handle ) {
    ( *( size_t* ) & ( handle->data ) )++;
    if( ( int )( handle->data ) > 10 )
        uv_timer_stop( handle );
    INFRATRACE0( "N={}", handle->data );
}
/*
static void test3() {
	uv_loop_t* loop = uv_default_loop();
	uv_timer_t timer;
	uv_timer_init( loop, &timer );
	timer.data = 0;
	uv_timer_start( &timer, test3_timer_fn, 0, 1000 );

	uv_run( loop, UV_RUN_DEFAULT );
	uv_loop_close( loop );
}
*/

class NodeServer : public Node {
  public:
    void run() override {
        int connectToPort = 7001;
        auto server = net::createServer( this );
        auto s = server.listen( 7000 );
        if( !s.isOk() ) {
            s = server.listen( 7001 );
            connectToPort = 7000;
        }
        s.onEach( [ = ]() {
            infraConsole.log( "Connection accepted" );
            auto fromNet = s.value().read( this );
            fromNet.onEach( [ = ]() {
                INFRATRACE0( "Received '{}'", fromNet.value().toString() );
            } );
            auto end = s.value().end( this );
            end.then( [ = ]() {
                INFRATRACE0( "Disconnected" );
            } );
        } );

        auto data = setInterval( this, 5 );
        data.onEach( [ = ]() {
            INFRATRACE0( "connecting {}", connectToPort );
            auto c = net::connect( this, connectToPort );
            c.then( [ = ]() {
                INFRATRACE0( "Writing..." );
                c.value().write( this, "bom bom", 8 );
                c.value().close( this );
            } );
        } );
    }
};

class NodeServer2 : public Node {
  public:
    void run() override {
        int connectToPort = 7001;
        auto server = net::createServer( this );
        auto s = server.listen( 7000 );
        if( !s.isOk() ) {
            s = server.listen( 7001 );
            connectToPort = 7000;
        }
        s.onEach( [ = ]() {
            infraConsole.log( "Connection accepted" );
            auto fromNet = s.value().read( this );
            fromNet.onEach( [ = ]() {
                INFRATRACE0( "Received '{}'", fromNet.value().toString() );
            } );
            auto end = s.value().end( this );
            end.then( [ = ]() {
                INFRATRACE0( "Disconnected" );
            } );
        } );

        auto t = autom::startTimeout( this, 10 );
        t.then( [ = ]() {
            auto con = net::connect( this, connectToPort );
            con.then( [ = ]() {
                INFRATRACE0( "connected {}", connectToPort );
                auto t2 = setInterval( this, 5 );
                t2.onEach( [ = ]() {
                    INFRATRACE0( "Writing..." );
                    con.value().write( this, "bom bom", 8 );
                } );
            } );
        } );
    }
};

void testServer() {
    InfraNodeContainer fs;
    Node* p = new NodeServer;
    fs.addNode( p );
    fs.run();
    fs.removeNode( p );
    delete p;
}

int main() {
    try {
        testServer();
        //      test3();
        //      test2();
        //		test1();
    } catch( const std::exception& e ) {
        console.error( "std::exception '{}'", e.what() );
        return 1;
    } catch( ... ) {
        console.error( "Unknown exception!" );
        return 2;
    }

    return 0;
}
