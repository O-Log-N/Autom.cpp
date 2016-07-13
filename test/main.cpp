#include <exception>
#include <iostream>

#include "../include/aassert.h"
#include "../include/aconsole.h"
#include "../include/abuffer.h"
#include "../include/zeronet.h"
#include "../include/anode.h"
#include "../include/net.h"
#include "../include/zerotimer.h"
#include "../include/timer.h"
#include "../libsrc/infra/infraconsole.h"
#include "../libsrc/infra/nodecontainer.h"
#include "../libsrc/infra/loopcontainer.h"

using namespace std;
using namespace autom;

NodeConsoleWrapper console;
/*static void test1() {
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
		s.onEach( [=]( const std::exception * ex ) {
			if( ex ) {
				infraConsole.log( "Accept error '{}'", ex->what() );
				return;
			}
			infraConsole.log( "Connection accepted" );
			auto fromNet = s.value().read( this );
			fromNet.onEach( [=]( const std::exception * ex ) {
				if( ex ) {
					s.value().disconnect( this );
					infraConsole.log( "Read error '{}'", ex->what() );
					return;
				}
				INFRATRACE0( "Received '{}'", fromNet.value().toString() );
			} );
			auto end = s.value().closed( this );
			end.then( [=]( const std::exception * ex ) {
				INFRATRACE0( "Disconnected" );
			} );
		} );

		auto data = setInterval( this, 5 );
		data.onEach( [=]( const std::exception * ex ) {
			INFRATRACE0( "connecting {}", connectToPort );
			auto c = net::connect( this, connectToPort );
			c.then( [=]( const std::exception * ex ) {
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
		s.onEach( [=]( const std::exception * ex ) {
			infraConsole.log( "Connection accepted" );
			auto fromNet = s.value().read( this );
			fromNet.onEach( [=]( const std::exception * ex ) {
				INFRATRACE0( "Received '{}'", fromNet.value().toString() );
			} );
			auto end = s.value().closed( this );
			end.then( [=]( const std::exception * ex ) {
				INFRATRACE0( "Disconnected" );
			} );
		} );

		auto t = autom::startTimeout( this, 10 );
		t.then( [=]( const std::exception * ex ) {
			auto con = net::connect( this, connectToPort );
			con.then( [=]( const std::exception * ex ) {
				INFRATRACE0( "connected {}", connectToPort );
				auto t2 = setInterval( this, 5 );
				t2.onEach( [=]( const std::exception * ex ) {
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
			[=]() {
			startTimeout( data1, this, 15 );
			startTimeout( data3, this, 5 );
			infraConsole.log( "timers 1 and 3 started" );
		},
			CCode::waitFor( data1 ),
			[=]() {
			infraConsole.log( "Timer1: file {}", fname.c_str() );
			startTimeout( data2, this, 6 );
			infraConsole.log( "timer 2 started" );
		},
			[=]() {
			infraConsole.log( "After Timer1" );
		},
			CCode::waitFor( data2 ),
			[=]() {
			infraConsole.log( "TIMER2:" );
		},
			[=]() {
			infraConsole.log( "After Timer2" );
		},
			CCode::waitFor( data3 ),
			[=]() {
			infraConsole.log( "TIMER3" );
		},
			[=]() {
			infraConsole.log( "After Timer3" );
		},
			[=]() {
			infraConsole.log( "Last step" );
		}
			).ccatch( [=]( const std::exception & x ) {
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
			[=]() {
			startTimeout( data, this, 5 );
		},
			CCode::waitFor( data ),
			[=]() {
			infraConsole.log( "READ1: file {}---{}", fname.c_str(), "data" );
			cond.setValue( false );
		},
			CCode::iif( cond,
				[=]() {
			startTimeout( data2, this, 6 );
			infraConsole.log( "Positive branch" );
		},
				CCode::waitFor( data2 ),
			[=]() {
			infraConsole.log( "READ2: {} : {}", "data", "data2" );
		} ).eelse(
			[=]() {
			startTimeout( data3, this, 7 );
			infraConsole.log( "Negative branch" );
		},
			CCode::waitFor( data3 ),
			[=]() {
			infraConsole.log( "READ3: {} : {}", "data", "data3" );
		} ),
			[=]() {
			infraConsole.log( "Invariant after iif" );
		}
			).ccatch( [=]( const std::exception & x ) {
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

		CCODE {
			TTRY {
			startTimeout( data, this, 5 );
		AWAIT( data );
		infraConsole.log( "READ1: file {}---{}", fname.c_str(), "data" );
		cond.setValue( true );
		WWHILE( cond ) {
			static int cnt = 0;
			cnt++;
			infraConsole.log( "wwhile loop" );
			if( cnt > 10 )
				cond.setValue( false );
		}
		ENDWWHILE
			IIF( cond ) {
			startTimeout( data2, this, 6 );
			infraConsole.log( "Positive branch 1" );
			AWAIT( data2 );
			infraConsole.log( "READ2: {} : {}", "data", "data2" );
			cond.setValue( false );
			IIF( cond ) {
				infraConsole.log( "nested iif +" );
			}
			EELSE {
				infraConsole.log( "nested iif -" );
			}
				ENDIIF
		}
		EELSE {
			TTRY {
			startTimeout( data3, this, 7 );
		infraConsole.log( "Negative branch 2" );
		cond.setValue( true );
		AWAIT( data3 );
		}
			CCATCH( const std::exception & x ) {
			infraConsole.log( "nested catch" );
		}
		ENDTTRY
			infraConsole.log( "READ3" );
		}
			ENDIIF
			IIF( cond ) {
			startTimeout( data4, this, 6 );
			infraConsole.log( "Positive branch 3" );
			AWAIT( data4 );
			infraConsole.log( "READ4" );
		}
		EELSE {
			startTimeout( data5, this, 7 );
		infraConsole.log( "Negative branch 3" );
		AWAIT( data5 );
		infraConsole.log( "READ5" );
		}
			ENDIIF
		}
			CCATCH( const std::exception & x ) {
			infraConsole.log( "oopsies: {}", x.what() );
		}
		ENDTTRY
			ENDCCODE
		}
	};
*/
class ZeroServer0 {
  public:
    void run( LoopContainer& loop ) {
        auto server = net::createServer( &loop );
        server->on( TcpZeroServer::ID_CONNECT, [ = ]( TcpZeroSocket * sock ) {
            console.log( "connected" );
            sock->on( TcpZeroSocket::ID_DATA, [ = ]( const NetworkBuffer * buff ) {
                console.log( "read '{}'", buff->c_str() );
                if( buff->c_str()[0] == 'Q' ) {
                    sock->close();
                    console.log( "Exit" );
                } else {
                    std::string s( "\r\nyou wrote: '" );
                    s += buff->c_str();
                    s += "'\r\n";
                    sock->write( s.c_str(), s.length() );
                }
            } );
            sock->on( TcpZeroSocket::ID_CLOSED, [ = ]() {
                console.log( "closed" );
            } );
            sock->read();
        } );
        server->on( TcpZeroServer::ID_ERROR, [ = ]() {
            console.log( "server error" );
        } );
        server->listen( 8080 );
    }
};

class NodeServer0 : public Node {
  public:
    void run() override {
        auto server = net::createServer( this );
        auto futureSock = server->listen( 8080 );
        futureSock.onEach( [ = ]( const std::exception * err ) {
            console.log( "Future Connected" );
            auto futureData = futureSock.value().read();
            futureData.onEach( [ = ]( const std::exception * err ) {
                if( err ) {
                    console.log( "Connection closed" );
                } else {
                    string s = futureData.value().toString();
                    console.log( "Future Data {}", s.c_str() );
                    if( s[0] == 'Q' ) {
                        futureSock.value().close();
                        console.log( "Exit" );
                    } else {
                        s = "You wrote: " + s + "\r\n";
                        futureSock.value().write( s.c_str(), s.length() );
                    }
                }
            } );
        } );
    }
};

class ZeroServer1 {
  public:
    void run( LoopContainer& loop ) {
        startTimeout( &loop, [ = ]() {
            console.log( "Timeout" );
        }, 7 );
        setInterval( &loop, [ = ]() {
            static int n = 0;
            n++;
            console.log( "Interval {}", n );
        }, 5 );
    }
};

class NodeServer1 : public Node {
    void run() override {
        Future<Timer> t1 = startTimeout( this, 7 );
        t1.then( [ = ]( const exception * ex ) {
            console.log( "future timeout" );
        } );
        MultiFuture<Timer> t2 = setInterval( this, 5 );
        t2.onEach( [ = ]( const exception * ex ) {
            console.log( "future interval" );
        } );
    }
};

class NodeClient0 : public Node {
  public:
    void run() override {
        Future< TcpSocket > futureClient = net::connect( this, "127.0.0.1", 8080 );
        futureClient.then( [ = ]( const std::exception * ex ) {
            if( ex ) {
                console.log( "Client Error" );
                return;
            }
            console.log( "Client Connected" );
            for( int i = 0; i < 1000; i++ ) {
                std::string s( "Hello " );
                s += '0' + i % 10;
                futureClient.value().write( s.c_str(), s.length() );
            }
            futureClient.value().close();
        } );
    }
};

static void testServerZero() {
    LoopContainer loop;
    auto p = new ZeroServer0;
    p->run( loop );
    loop.run();
    delete p;
}

static void testServer() {
    LoopContainer lc;
    InfraNodeContainer container( &lc );
    Node* p = new NodeServer0;
    container.addNode( p );
    container.run();
    container.removeNode( p );
    delete p;
}

static void testClient() {
    LoopContainer lc;
    InfraNodeContainer container( &lc );
    Node* p = new NodeClient0;
    container.addNode( p );
    container.run();
    container.removeNode( p );
    delete p;
}

int main( int argc, const char** argv ) {
    try {
        if( argc > 1 && 0 == strcmp( argv[1], "-c" ) )
            testClient();
        else
            testServer();
    } catch( const std::exception& e ) {
        console.error( "std::exception '{}'", e.what() );
        return 1;
    } catch( ... ) {
        console.error( "Unknown exception!" );
        return 2;
    }

    return 0;
}
