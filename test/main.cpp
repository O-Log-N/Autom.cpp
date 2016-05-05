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


class CStep {
  public:
    enum { NONE = 0, WAIT, EXEC, COND };
    unsigned int opCode;
    unsigned int id;
    std::function< void( const std::exception* ) > fn;
    CStep* next;

    CStep() {
        opCode = NONE;
        next = nullptr;
    }
    explicit CStep( std::function< void( const std::exception* ) > fn_ ) {
        opCode = EXEC;
        id = NONE;
        fn = fn_;
        next = nullptr;
    }
    CStep( CStep&& other ) : opCode( other.opCode ), id( other.id ), next( other.next ) {
        std::swap( fn, other.fn );
        std::swap( next, other.next );
    }
    CStep( const CStep& other ) = default;
    ~CStep() {}

    CStep* ccatch( std::function< void( const std::exception& ) > fn ) {
        return this;
    }

    static void afterEvent( const CStep* s );
};

class CCode {
  public:
    static void exec( const CStep* s ) {
        if( !s )
            return;
        if( CStep::WAIT == s->opCode ) {
            ATRACE0( "Waiting {}", s->id );
            return;
        } else if( CStep::EXEC == s->opCode ) {
            s->fn( nullptr );
        } else if( CStep::COND == s->opCode ) {
            s->fn( nullptr );
        } else {
            AASSERT4( 0 );
        }
        exec( s->next );
    }
    static void debugPrint( const CStep* s ) {
        if( !s )
            return;
        ATRACE0( "OpCode {} id {}", s->opCode, s->id );
        debugPrint( s->next );
    }
    CCode( Node*, const CStep* s ) {
        debugPrint( s );
        exec( s );
    }
    static CStep ttry( CStep* s ) {
        return *s;
    }
    static CStep* waitFor( const Future<Timer>& future, std::function< void( const std::exception* ) > fn ) {
        CStep* s = new CStep;
        s->opCode = CStep::WAIT;
        s->id = future.infraGetId();
        future.then( [ = ]( const std::exception * ex ) {
            fn( ex );
            CStep::afterEvent( s );
        } );
        return s;
    }
    static CStep* group( CStep* s1, CStep* s2 ) {
        AASSERT4( ! s1->next );
        s1->next = s2;
        return s1;
    }
    static CStep* group( std::function< void( const std::exception* ) > fn, CStep* s2 ) {
        return group( new CStep( fn ), s2 );
    }
	static CStep* group( CStep* s1, std::function< void( const std::exception* ) > fn ) {
		return group( s1, new CStep( fn ) );
	}
	static CStep* iif( const Future<bool>& b, std::function< void( const std::exception* ) > fn1, std::function< void( const std::exception* ) > fn2 ) 		{
        CStep* s = new CStep;
        s->opCode = CStep::COND;
        s->id = 0;
        s->fn = [ = ]( const std::exception * ex ) {
            if( b.value() )
                fn1( ex );
            else
                fn2( ex );
            CStep::afterEvent( s );
        };
        return s;
    }
};

void CStep::afterEvent( const CStep* s ) {
    ATRACE0( "afterEvent id {}", s->id );
    CCode::exec( s->next );
}

class NodeServer3 : public Node	{
  public:
    void run() override {
        std::string fname( "path1" );
        Future<Timer> data( this ), data2( this ), data3( this );
        CCode code( this, CCode::ttry(
        CCode::group( [ = ]( const std::exception* ) {
            startTimeout( data, this, 10 );
        },
        CCode::group( CCode::waitFor( data, [ = ]( const std::exception* ) {
            infraConsole.log( "READ1: file {}---{}", fname.c_str(), "data" );
            startTimeout( data2, this, 11 );
        } ),
        CCode::group( CCode::waitFor( data2, [ = ]( const std::exception* ) {
            infraConsole.log( "READ2: {} : {}", "data", "data2" );
            startTimeout( data3, this, 12 );
        } ),
        CCode::waitFor( data3, [ = ]( const std::exception* ) {
            infraConsole.log( "READ3: {} : {}", "data2", "data3" );
        } ) ) ) )
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
        CCode code( this, CCode::ttry(
        CCode::group( [ = ]( const std::exception* ) {
            startTimeout( data, this, 10 );
        },
        CCode::group( CCode::waitFor( data, [ = ]( const std::exception* ) {
            infraConsole.log( "READ1: file {}---{}", fname.c_str(), "data" );
            *( ( bool* )&cond.value() ) = false;
        } ),
        CCode::group( CCode::iif( cond, [ = ]( const std::exception* ) {
            startTimeout( data2, this, 12 );
            infraConsole.log( "Positive branch" );
            CCode::waitFor( data2, [ = ]( const std::exception* ) {
                infraConsole.log( "READ2: {} : {}", "data", "data2" );
            } );
        },
        // eelse
        [ = ]( const std::exception* ) {
            startTimeout( data3, this, 13 );
            infraConsole.log( "Negative branch" );
            CCode::waitFor( data3, [ = ]( const std::exception* ) {
                infraConsole.log( "READ3: {} : {}", "data", "data3" );
            } );
        } ),
        [ = ]( const std::exception* ) {
            infraConsole.log( "Invariant after iif" );
        } ) ) )
        ).ccatch( [ = ]( const std::exception & x ) {
            infraConsole.log( "oopsies: {}", x.what() );
        } ) );//ccatch+code
    }
};

void testServer() {
    InfraNodeContainer fs;
    Node* p = new NodeServer4;
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
