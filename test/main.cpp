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
    int debugOpCode;
    InfraFutureBase* infraPtr;
    std::function< void( const std::exception* ) > fn;
    CStep* next;

    CStep() {
        debugOpCode = NONE;
        infraPtr = nullptr;
        next = nullptr;
    }
    explicit CStep( std::function< void( const std::exception* ) > fn_ ) {
        debugOpCode = EXEC;
        infraPtr = nullptr;
        fn = fn_;
        next = nullptr;
    }
    CStep( CStep&& other );
    CStep( const CStep& other ) = default;
    ~CStep() = default;

    CStep* ccatch( std::function< void( const std::exception& ) > fn ) {
        return this;
    }
    CStep* endOfChain() {
        auto p = this;
        while( p->next )
            p = p->next;
        return p;
    }
};

class CCode {
  public:
    static void exec( const CStep* s ) {
        while( s ) {
            if( s->infraPtr ) {
                AASSERT4( CStep::WAIT == s->debugOpCode );
                ATRACE0( "Waiting {} ...", ( void* )s->infraPtr );
                return;
            } else {
                AASSERT4( ( CStep::EXEC == s->debugOpCode ) || ( CStep::COND == s->debugOpCode ) );
                s->fn( nullptr );
            }

            auto tmp = s;
            s = s->next;
            delete tmp;
        }
    }
    static void debugPrint( const CStep* s ) {
        if( !s )
            return;
        ATRACE0( "{} OpCode {} id {}", ( void* )s, s->debugOpCode, ( void* )s->infraPtr );
        debugPrint( s->next );
    }
    CCode( Node*, const CStep* s ) {
        debugPrint( s );
        exec( s );
    }
    static CStep* ttry( CStep* s ) {
        return s;
    }
    template< typename T, typename... Ts >
    static T ttry( T s, Ts... Vals ) {
        s->next = ttry( Vals... );
        return s;
    }
    static CStep* waitFor( const Future<Timer>& future, std::function< void( const std::exception* ) > fn ) {
        CStep* s = new CStep;
        s->debugOpCode = CStep::WAIT;
        s->infraPtr = future.infraGetPtr();
        future.then( [fn, s]( const std::exception * ex ) {
            fn( ex );
            exec( s->next );
        } );
        return s;
    }
    static CStep* iif( const Future<bool>& b, CStep* s1, CStep* s2 ) {
        AASSERT4( s1 );
        AASSERT4( s2 );
        CStep* s = new CStep;
        s->debugOpCode = CStep::COND;
        auto e1 = s1->endOfChain();
        auto e2 = s2->endOfChain();
        s->fn = [ s, s1, s2, e1, e2, b ]( const std::exception * ex ) {
            // insert active branch into exec list
            CStep* active, *passive, *end;
            if( b.value() ) {
                active = s1;
                passive = s2;
                end = e1;
            } else {
                active = s2;
                passive = s1;
                end = e2;
            }
            // insert active branch in execution chain
            auto tmp = s->next;
            s->next = active;
            end->next = tmp;
            // delete passive branch
            while( passive ) {
                tmp = passive;
                passive = passive->next;
                if( tmp->infraPtr )
                    tmp->infraPtr->cleanup();
                delete tmp;
            }
        };
        return s;
    }
    static CStep* group( CStep* s1, CStep* s2 ) {
        AASSERT4( !s1->next );
        s1->next = s2;
        return s1;
    }
    static CStep* group( std::function< void( const std::exception* ) > fn, CStep* s2 ) {
        return group( new CStep( fn ), s2 );
    }
    static CStep* group( CStep* s1, std::function< void( const std::exception* ) > fn ) {
        return group( s1, new CStep( fn ) );
    }
};

class NodeServer3 : public Node	{
  public:
    void run() override {
        std::string fname( "path1" );
        Future<Timer> data( this ), data2( this ), data3( this );
        CCode code( this, CCode::ttry(
        new CStep( [ = ]( const std::exception* ) {
            startTimeout( data, this, 5 );
            startTimeout( data3, this, 4 );
        } ),
        CCode::waitFor( data, [ = ]( const std::exception* ) {
            infraConsole.log( "READ1: file {}---{}", fname.c_str(), "data" );
            startTimeout( data2, this, 6 );
        } ),
        CCode::waitFor( data2, [ = ]( const std::exception* ) {
            infraConsole.log( "READ2: {} : {}", "data", "data2" );
        } ),
        CCode::waitFor( data3, [ = ]( const std::exception* ) {
            infraConsole.log( "READ3: {} : {}", "data2", "data3" );
        } )
        )->ccatch( [ = ]( const std::exception & x ) {
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
        new CStep( [ = ]( const std::exception* ) {
            startTimeout( data, this, 5 );
        } ),
        CCode::waitFor( data, [ = ]( const std::exception* ) {
            infraConsole.log( "READ1: file {}---{}", fname.c_str(), "data" );
            *( ( bool* )&cond.value() ) = false;
        } ),

        CCode::iif( cond, CCode::group( [ = ]( const std::exception* ) {
            startTimeout( data2, this, 6 );
            infraConsole.log( "Positive branch" );
        },
        CCode::waitFor( data2, [ = ]( const std::exception* ) {
            infraConsole.log( "READ2: {} : {}", "data", "data2" );
        } )
                                      ),
        // eelse
        CCode::group( [ = ]( const std::exception* ) {
            startTimeout( data3, this, 7 );
            infraConsole.log( "Negative branch" );
        },
        CCode::waitFor( data3, [ = ]( const std::exception* ) {
            infraConsole.log( "READ3: {} : {}", "data", "data3" );
        } )
                    ) ),
        new CStep( [ = ]( const std::exception* ) {
            infraConsole.log( "Invariant after iif" );
        } )
        )->ccatch( [ = ]( const std::exception & x ) {
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
        CCode code( this, CCode::ttry(
        new CStep( [ = ]( const std::exception* ) {
            startTimeout( data, this, 5 );
        } ),
        CCode::waitFor( data, [ = ]( const std::exception* ) {
            infraConsole.log( "READ1: file {}---{}", fname.c_str(), "data" );
            *( ( bool* )&cond.value() ) = false;
        } ),

        CCode::iif( cond, CCode::group( [ = ]( const std::exception* ) {
            startTimeout( data2, this, 6 );
            infraConsole.log( "Positive branch" );
        },
        CCode::waitFor( data2, [ = ]( const std::exception* ) {
            infraConsole.log( "READ2: {} : {}", "data", "data2" );
        } )
                                      ),
        // eelse
        CCode::group( [ = ]( const std::exception* ) {
            startTimeout( data3, this, 7 );
            infraConsole.log( "Negative branch" );
            *( ( bool* )&cond.value() ) = true;
        },
        CCode::waitFor( data3, [ = ]( const std::exception* ) {
            infraConsole.log( "READ3" );
        } )
                    ) ),
        CCode::iif( cond, CCode::group( [ = ]( const std::exception* ) {
            startTimeout( data4, this, 6 );
            infraConsole.log( "Positive branch" );
        },
        CCode::waitFor( data4, [ = ]( const std::exception* ) {
            infraConsole.log( "READ4" );
        } )
                                      ),
        // eelse
        CCode::group( [ = ]( const std::exception* ) {
            startTimeout( data5, this, 7 );
            infraConsole.log( "Negative branch" );
        },
        CCode::waitFor( data5, [ = ]( const std::exception* ) {
            infraConsole.log( "READ5" );
        } )
                    ) )
        )->ccatch( [ = ]( const std::exception & x ) {
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
