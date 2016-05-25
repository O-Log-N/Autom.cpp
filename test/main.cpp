#include <exception>
#include <iostream>

#include "../include/ccode.h"
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
        data.then( [ = ]( const std::exception * ex ) {
            infraConsole.log( "TIMER1" );
            auto data2 = startTimeout( this, 5 );
            data2.then( [ = ]( const std::exception * ex ) {
                infraConsole.log( "TIMER2" );
                auto data3 = startTimeout( this, 10 );
                data3.then( [ = ]( const std::exception * ex ) {
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
    if( ( size_t )( handle->data ) > 10 )
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
        s.onEach( [ = ]( const std::exception * ex ) {
            if( ex ) {
                infraConsole.log( "Accept error '{}'", ex->what() );
                return;
            }
            infraConsole.log( "Connection accepted" );
            auto fromNet = s.value().read( this );
            fromNet.onEach( [ = ]( const std::exception * ex ) {
                if( ex ) {
                    s.value().disconnect( this );
                    infraConsole.log( "Read error '{}'", ex->what() );
                    return;
                }
                INFRATRACE0( "Received '{}'", fromNet.value().toString() );
            } );
            auto end = s.value().closed( this );
            end.then( [ = ]( const std::exception * ex ) {
                INFRATRACE0( "Disconnected" );
            } );
        } );

        auto data = setInterval( this, 5 );
        data.onEach( [ = ]( const std::exception * ex ) {
            INFRATRACE0( "connecting {}", connectToPort );
            auto c = net::connect( this, connectToPort );
            c.then( [ = ]( const std::exception * ex ) {
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
        s.onEach( [ = ]( const std::exception * ex ) {
            infraConsole.log( "Connection accepted" );
            auto fromNet = s.value().read( this );
            fromNet.onEach( [ = ]( const std::exception * ex ) {
                INFRATRACE0( "Received '{}'", fromNet.value().toString() );
            } );
            auto end = s.value().closed( this );
            end.then( [ = ]( const std::exception * ex ) {
                INFRATRACE0( "Disconnected" );
            } );
        } );

        auto t = autom::startTimeout( this, 10 );
        t.then( [ = ]( const std::exception * ex ) {
            auto con = net::connect( this, connectToPort );
            con.then( [ = ]( const std::exception * ex ) {
                INFRATRACE0( "connected {}", connectToPort );
                auto t2 = setInterval( this, 5 );
                t2.onEach( [ = ]( const std::exception * ex ) {
                    INFRATRACE0( "Writing..." );
                    con.value().write( this, "bom bom", 8 );
                } );
            } );
        } );
    }
};

class NodeServer3 : public Node {
  public:
    void run() override {
        std::string fname( "path1" );
        Future<Timer> data1( this ), data2( this ), data3( this );
        CCode code( CCode::ttry(
        [ = ]() {
            startTimeout( data1, this, 15 );
            startTimeout( data3, this, 5 );
            infraConsole.log( "timers 1 and 3 started" );
        },
        CCode::waitFor( data1 ),
        [ = ]() {
            infraConsole.log( "Timer1: file {}", fname.c_str() );
            startTimeout( data2, this, 6 );
            infraConsole.log( "timer 2 started" );
        },
        [ = ]() {
            infraConsole.log( "After Timer1" );
        },
        CCode::waitFor( data2 ),
        [ = ]() {
            infraConsole.log( "TIMER2:" );
        },
        [ = ]() {
            infraConsole.log( "After Timer2" );
        },
        CCode::waitFor( data3 ),
        [ = ]() {
            infraConsole.log( "TIMER3" );
        },
        [ = ]() {
            infraConsole.log( "After Timer3" );
        },
        [ = ]() {
            infraConsole.log( "Last step" );
        }
        ).ccatch( [ = ]( const std::exception & x ) {
            infraConsole.log( "oopsies: {}", x.what() );
        } ) );//ccatch+code
    }
};

class NodeServer4 : public Node {
  public:
    void run() override {
        std::string fname( "path1" );
        Future<Timer> data( this ), data2( this ), data3( this );
        Future<bool> cond( this );
        CCode code( CCode::ttry(
        [ = ]() {
            startTimeout( data, this, 5 );
        },
        CCode::waitFor( data ),
        [ = ]() {
            infraConsole.log( "READ1: file {}---{}", fname.c_str(), "data" );
            cond.setValue( false );
        },
        CCode::iif( cond,
        [ = ]() {
            startTimeout( data2, this, 6 );
            infraConsole.log( "Positive branch" );
        },
        CCode::waitFor( data2 ),
        [ = ]() {
            infraConsole.log( "READ2: {} : {}", "data", "data2" );
        } ).eelse(
        [ = ]() {
            startTimeout( data3, this, 7 );
            infraConsole.log( "Negative branch" );
        },
        CCode::waitFor( data3 ),
        [ = ]() {
            infraConsole.log( "READ3: {} : {}", "data", "data3" );
        } ),
        [ = ]() {
            infraConsole.log( "Invariant after iif" );
        }
        ).ccatch( [ = ]( const std::exception & x ) {
            infraConsole.log( "oopsies: {}", x.what() );
        } ) );//ccatch+code
    }
};

class NodeServer5 : public Node {
  public:
    void run() override {
        std::string fname( "path1" );
        Future<Timer> data( this ), data2( this ), data3( this ), data4( this ), data5( this );
        Future<bool> cond( this );
        CCode code( CCode::ttry(

        [ = ]() {
            startTimeout( data, this, 5 );
        },

        CCode::waitFor( data ),
        [ = ]() {
            infraConsole.log( "READ1: file {}---{}", fname.c_str(), "data" );
            cond.setValue( true );
        },

        CCode::iif( cond,
        [ = ]() {
            startTimeout( data2, this, 6 );
            infraConsole.log( "Positive branch 1" );
        },
        CCode::waitFor( data2 ),
        [ = ]() {
            infraConsole.log( "READ2: {} : {}", "data", "data2" );
            cond.setValue( false );
        },
        CCode::iif( cond, [ = ]() {
            infraConsole.log( "nested iif +" );
        } ).eelse( [ = ]() {
            infraConsole.log( "nested iif -" );
        } )
                  ).eelse(
        [ = ]() {
            startTimeout( data3, this, 7 );
            infraConsole.log( "Negative branch 2" );
            cond.setValue( true );
        },
        CCode::waitFor( data3 ),
        [ = ]() {
            infraConsole.log( "READ3" );
        } ),

        CCode::iif( cond,
        [ = ]() {
            startTimeout( data4, this, 6 );
            infraConsole.log( "Positive branch 3" );
        },
        CCode::waitFor( data4 ), [ = ]() {
            infraConsole.log( "READ4" );
        } ).eelse(
        [ = ]() {
            startTimeout( data5, this, 7 );
            infraConsole.log( "Negative branch" );
        },
        CCode::waitFor( data5 ),
        [ = ]() {
            infraConsole.log( "READ5" );
        } )

        ).ccatch( [ = ]( const std::exception & x ) {
            infraConsole.log( "oopsies: {}", x.what() );
        } ) );//ccatch+code
    }
};

void testServer() {
    InfraNodeContainer fs;
    Node* p = new NodeServer5;
    fs.addNode( p );
    fs.run();
    fs.removeNode( p );
    delete p;
}

int main() {
    try {
        testServer();
        //      test3()
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
