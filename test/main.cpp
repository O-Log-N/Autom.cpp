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
    enum { NONE = 0, WAIT, JOIN, EXEC, COND };
    unsigned int opCode;
    unsigned int id;
    std::function< void( const std::exception* ) > fn;
    CStep* next1;
    CStep* next2;

    CStep() {
        opCode = NONE;
        next1 = next2 = nullptr;
    }
    explicit CStep( std::function< void( const std::exception* ) > fn_ ) {
        opCode = EXEC;
        id = NONE;
        fn = fn_;
        next1 = next2 = nullptr;
    }
    CStep( CStep&& other ) : opCode( other.opCode ), id( other.id ) {
        std::swap( fn, other.fn );
        next1 = other.next1;
        next2 = other.next2;
        other.next1 = other.next2 = nullptr;
    }
    CStep( const CStep& other ) : opCode( other.opCode ), id( other.id ), fn( other.fn ) {
        next1 = next2 = nullptr;
        if( other.next1 )
            next1 = new CStep( *other.next1 );
        if( other.next2 )
            next2 = new CStep( *other.next2 );
    }
    ~CStep() {
        delete next1;
        delete next2;
        next1 = next2 = nullptr;
    }

    CStep* ccatch( std::function< void( const std::exception& ) > fn ) {
        return this;
    }
};

class CCode {
	static CStep* linear( CStep* s ) {
		if( !s )
			return nullptr;
		CStep* res = s;
		CStep* cur = s;
		if( CStep::NONE == s->opCode ) {
			AASSERT4( 0 );
		} else if( CStep::JOIN == s->opCode ) {
			linear( cur->next1 );
			linear( cur->next2 );
		} else if( CStep::WAIT == s->opCode ) {
			s->fn( nullptr );
		} else if( CStep::EXEC == s->opCode ) {
			s->fn( nullptr );
		} else if( CStep::COND == s->opCode ) {
			if( true ) // TODO: implement
				exec( s->next1 );
			else
				exec( s->next2 );
		}

		return res;
	}
    static void exec( const CStep* s ) {
		if( !s )
			return;
        if( CStep::NONE == s->opCode ) {
            AASSERT4( 0 );
        } else if( CStep::JOIN == s->opCode ) {
            exec( s->next1 );
            exec( s->next2 );
        } else if( CStep::WAIT == s->opCode ) {
            s->fn( nullptr );
        } else if( CStep::EXEC == s->opCode ) {
            s->fn( nullptr );
        } else if( CStep::COND == s->opCode ) {
            if( true ) // TODO: implement
                exec( s->next1 );
            else
                exec( s->next2 );
        }
    }

  public:
    CCode( Node*, const CStep* s ) {
        exec( s );
    }
    static CStep ttry( CStep* s ) {
        return *s;
    }
    static CStep* waitFor( const Future<Buffer>& future, std::function< void( const std::exception* ) > fn ) {
        CStep* cmd = new CStep;
        cmd->opCode = CStep::WAIT;
        cmd->id = future.infraGetId();
        cmd->fn = fn;
        return cmd;
    }
    static CStep* join( CStep* s1, CStep* s2 ) {
        CStep* cmd = new CStep;
        cmd->opCode = CStep::JOIN;
        cmd->id = 0;
        cmd->next1 = s1;
        cmd->next2 = s2;
        return cmd;
    }
    static CStep* join( std::function< void( const std::exception* ) > fn, CStep* s2 ) {
        CStep* cmd = new CStep;
        cmd->opCode = CStep::JOIN;
        cmd->id = 0;
        cmd->next1 = new CStep( fn );
        cmd->next2 = s2;
        return cmd;
    }
    static CStep* iif( const Future<bool>&, std::function< void( const std::exception* ) >, std::function< void( const std::exception* ) > );
};

static void readFile( const autom::Future< autom::Buffer >& future, const char* s ) {
    *const_cast<Buffer*>( &future.value() ) = s;
}

class NodeServer3 : public Node	{
  public:
    void run() override {
        std::string fname( "path1" );
        Future<Buffer> data( this ), data2( this ), data3( this );
        CCode code( this, CCode::ttry( CCode::join( [ = ]( const std::exception* ) {
            readFile( data, fname.c_str() );
        },
        CCode::join( CCode::waitFor( data, [ = ]( const std::exception* ) {
            infraConsole.log( "READ1: file {}---{}", fname.c_str(), data.value().toString() );
            readFile( data2, "path2" );
        } ),
        CCode::join( CCode::waitFor( data2, [ = ]( const std::exception* ) {
            infraConsole.log( "READ2: {} : {}", data.value().toString(), data2.value().toString() );
            readFile( data3, "path3" );
        } ),
        CCode::waitFor( data3, [ = ]( const std::exception* ) {
            infraConsole.log( "READ3: {} : {}", data2.value().toString(), data3.value().toString() );
        } ) ) ) )
        ).ccatch( [ = ]( const std::exception & x ) {
            infraConsole.log( "oopsies: {}", x.what() );
        } ) );//ccatch+code
    }
};

void testServer() {
    InfraNodeContainer fs;
    Node* p = new NodeServer3;
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
