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

class AStep {
  public:
    enum { NONE = 0, WAIT, EXEC, COND };
    int debugOpCode;
    InfraFutureBase* infraPtr;
    std::function< void( const std::exception* ) > fn;
    AStep* next;

    AStep() {
        debugOpCode = NONE;
        infraPtr = nullptr;
        next = nullptr;
    }
    explicit AStep( std::function< void( const std::exception* ) > fn_ ) {
        debugOpCode = EXEC;
        infraPtr = nullptr;
        fn = fn_;
        next = nullptr;
    }
    AStep( AStep&& other );
    AStep( const AStep& other ) = default;
    ~AStep() = default;

    AStep* endOfChain() {
        auto p = this;
        while( p->next )
            p = p->next;
        return p;
    }
    void debugPrint() const {
        ATRACE0( "{} opCode {} {}", ( void* )this, debugOpCode, ( void* )infraPtr );
        if( next )
            next->debugPrint();
    }
};

class CStep {
  public:
    AStep* step;
    const Future<bool>* condition;

    CStep() : step( nullptr ), condition( nullptr ) {}
    CStep( AStep* p ) : step( p ), condition( nullptr ) {}
    CStep( std::function< void( void ) > fn ) : condition( nullptr ) {
        step = new AStep( [ = ]( const std::exception* ) {
            fn();
        } );
    }
    CStep( const CStep& ) = default;
    CStep( CStep&& ) = default;
    CStep& operator=( CStep&& ) = default;

    CStep ccatch( std::function< void( const std::exception* ) > fn ) {
        return *this;
    }

  private:
    CStep infraEelse( CStep s ) {
        return s;
    }
    CStep infraEelse( std::function< void( void ) > fn ) {
        return CStep( fn );
    }
    void infraEelseImpl( CStep* first, AStep* second ) {
        AASSERT4( first );
        AASSERT4( first->step );
		AASSERT4( first->step->next );
		AASSERT4( first->step->debugOpCode == AStep::COND );
        AASSERT4( first->condition );
        AASSERT4( second );

        AStep* f = first->step->next;
        auto e1 = f->endOfChain();
        auto e2 = second->endOfChain();
        const Future<bool>& b = *first->condition;
        first->condition = nullptr;
        f->fn = [ b, f, second, e1, e2 ]( const std::exception * ex ) {
            // insert active branch into exec list
            AStep* active, *passive, *end;
            if( b.value() ) {
                ATRACE0( "====IFELSE POSITIVE====" );
                active = f;
                passive = second;
                end = e1;
            } else {
                ATRACE0( "====IFELSE NEGATIVE====" );
                active = second;
                passive = f;
                end = e2;
            }
            // insert active branch in execution chain
            auto tmp = f->next;
            f->next = active;
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
    }

  public:
    template< typename... Ts >
    CStep eelse( std::function< void( void ) > fn, Ts... Vals ) {
        AASSERT4( step->debugOpCode == AStep::COND );
        CStep s( fn );
        s.step->next = infraEelse( Vals... ).step;
        infraEelseImpl( this, s.step );
        return *this;
    }
    template< typename... Ts >
    CStep eelse( CStep s, Ts... Vals ) {
        AASSERT4( step->debugOpCode == AStep::COND );
        s.step->next = infraEelse( Vals... ).step;
        infraEelseImpl( this, s.step );
        return *this;
    }
};

class CCode {
  public:
    static void exec( const AStep* s ) {
        while( s ) {
            if( s->infraPtr ) {
                AASSERT4( AStep::WAIT == s->debugOpCode );
                ATRACE0( "Waiting {} ...", ( void* )s->infraPtr );
                return;
            } else {
                AASSERT4( ( AStep::EXEC == s->debugOpCode ) || ( AStep::COND == s->debugOpCode ) );
                s->fn( nullptr );
            }

            auto tmp = s;
            s = s->next;
            delete tmp;
        }
    }
    static void debugPrint( const AStep* s ) {
        if( !s )
            return;
        ATRACE0( "{} OpCode {} id {}", ( void* )s, s->debugOpCode, ( void* )s->infraPtr );
        debugPrint( s->next );
    }
    CCode( const CStep& s ) {
        debugPrint( s.step );
        exec( s.step );
    }

  private:
    static CStep chain( std::function< void( void ) > fn ) {
        return CStep( fn );
    }
    static CStep chain( CStep s ) {
        return s;
    }
    template< typename... Ts >
    static CStep chain( std::function< void( void ) > fn, Ts&&... Vals ) {
        CStep s( fn );
        s.step->next = chain( Vals... ).step;
        return s;
    }
    template< typename... Ts >
    static CStep chain( CStep s, Ts&&... Vals ) {
        AASSERT4( nullptr == s.step->next );
        s.step->next = chain( Vals... ).step;
        return s;
    }

  public:
    static CStep ttry( CStep s ) {
        return s;
    }
    static CStep ttry( std::function< void( void ) > fn ) {
        return CStep( fn );
    }
    template< typename... Ts >
    static CStep ttry( CStep s, Ts&&... Vals ) {
        s.step->next = ttry( Vals... ).step;
        return s;
    }
    template< typename... Ts >
    static CStep ttry( std::function< void( void ) > fn, Ts&&... Vals ) {
        CStep s = ttry( fn );
        s.step->next = ttry( Vals... ).step;
        return s;
    }
    static CStep waitFor( const Future<Timer>& future, std::function< void( void ) > fn ) {
        AStep* a = new AStep;
        a->debugOpCode = AStep::WAIT;
        a->infraPtr = future.infraGetPtr();
        CStep s;
        s.step = a;
        future.then( [fn, a]( const std::exception* ) {
            fn();
            exec( a->next );
        } );
        return s;
    }

  private:
    static CStep infraIifImpl( const Future<bool>& b, AStep* c ) {
        AStep* a = new AStep;
        a->next = c;
        a->debugOpCode = AStep::COND;
        auto e = c->endOfChain();
        a->fn = [a, c, e, b]( const std::exception * ex ) {
            // insert active branch into exec list
            AStep* active, *end;
            if( b.value() ) {
                ATRACE0( "====IF POSITIVE====" );
                active = c;
                end = e;
            } else {
                ATRACE0( "====IF NEGATIVE====" );
                return;
            }
            // insert active branch in execution chain
            auto tmp = a->next;
            a->next = active;
            end->next = tmp;
        };
        CStep s;
        s.step = a;
        s.condition = &b;
        return s;
    }

  public:
    template< typename... Ts >
    static CStep iif( const Future<bool>& b, std::function< void( void ) > fn, Ts&&... Vals ) {
        CStep s( fn );
        s.step->next = chain( Vals... ).step;
        return infraIifImpl( b, s.step );
    }
    template< typename... Ts >
    static CStep iif( const Future<bool>& b, CStep s, Ts&&... Vals ) {
        s.step->next = chain( Vals... ).step;
        return infraIifImpl( b, s.step );
    }
    /*
    		static CStep iif( const Future<bool>& b, CStep s1, CStep s2 ) {
            AASSERT4( s1.step );
            AASSERT4( s2.step );
            CStep s;
            s.step = new AStep;
            s.step->debugOpCode = AStep::COND;
            auto e1 = s1.step->endOfChain();
            auto e2 = s2.step->endOfChain();
            s.step->fn = [s, s1, s2, e1, e2, b]( const std::exception * ex ) {
                // insert active branch into exec list
                AStep* active, *passive, *end;
                if( b.value() ) {
                    active = s1.step;
                    passive = s2.step;
                    end = e1;
                } else {
                    active = s2.step;
                    passive = s1.step;
                    end = e2;
                }
                // insert active branch in execution chain
                auto tmp = s.step->next;
                s.step->next = active;
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
    */
};

class NodeServer3 : public Node {
  public:
    void run() override {
        std::string fname( "path1" );
        Future<Timer> data( this ), data2( this ), data3( this );
        CCode code( CCode::ttry(
        [ = ]() {
            startTimeout( data, this, 5 );
            startTimeout( data3, this, 15 );
        },
        CCode::waitFor( data, [ = ]() {
            infraConsole.log( "READ1: file {}---{}", fname.c_str(), "data" );
            startTimeout( data2, this, 6 );
        } ),
        CCode::waitFor( data2, [ = ]() {
            infraConsole.log( "READ2: {} : {}", "data", "data2" );
        } ),
        CCode::waitFor( data3, [ = ]() {
            infraConsole.log( "READ3: {} : {}", "data2", "data3" );
        } ),
        [ = ]() {
            infraConsole.log( "Last step" );
        }
        ).ccatch( [ = ]( const std::exception * x ) {
            infraConsole.log( "oopsies: {}", x->what() );
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
        CCode::waitFor( data, [ = ]() {
            infraConsole.log( "READ1: file {}---{}", fname.c_str(), "data" );
            *( ( bool* )&cond.value() ) = false;
        } ),
        CCode::iif( cond,
        [ = ]() {
            startTimeout( data2, this, 6 );
            infraConsole.log( "Positive branch" );
        },
        CCode::waitFor( data2, [ = ]() {
            infraConsole.log( "READ2: {} : {}", "data", "data2" );
        } ) ).eelse(
        [ = ]() {
            startTimeout( data3, this, 7 );
            infraConsole.log( "Negative branch" );
        },
        CCode::waitFor( data3, [ = ]() {
            infraConsole.log( "READ3: {} : {}", "data", "data3" );
        } ) ),
        [ = ]() {
            infraConsole.log( "Invariant after iif" );
        }
        ).ccatch( [ = ]( const std::exception * x ) {
            infraConsole.log( "oopsies: {}", x->what() );
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
        CCode::waitFor( data, [ = ]() {
            infraConsole.log( "READ1: file {}---{}", fname.c_str(), "data" );
            *( ( bool* )&cond.value() ) = true;
        } ),

        CCode::iif( cond,
        [ = ]() {
            startTimeout( data2, this, 6 );
            infraConsole.log( "Positive branch 1" );
        },
        CCode::waitFor( data2, [ = ]() {
            infraConsole.log( "READ2: {} : {}", "data", "data2" );
        } ) ).eelse(
        [ = ]() {
            startTimeout( data3, this, 7 );
            infraConsole.log( "Negative branch 2" );
            *( ( bool* )&cond.value() ) = true;
        },
        CCode::waitFor( data3, [ = ]() {
            infraConsole.log( "READ3" );
        } ) ),
        CCode::iif( cond,
        [ = ]() {
            startTimeout( data4, this, 6 );
            infraConsole.log( "Positive branch 3" );
        },
        CCode::waitFor( data4, [ = ]() {
            infraConsole.log( "READ4" );
        } ) ).eelse(
        [ = ]() {
            startTimeout( data5, this, 7 );
            infraConsole.log( "Negative branch" );
        },
        CCode::waitFor( data5, [ = ]() {
            infraConsole.log( "READ5" );
        } ) )
        ).ccatch( [ = ]( const std::exception * x ) {
            infraConsole.log( "oopsies: {}", x->what() );
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
