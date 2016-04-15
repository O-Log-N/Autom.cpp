#include <exception>
#include <iostream>

#include "../include/aassert.h"
#include "../include/aconsole.h"
#include "../include/abuffer.h"
#include "../include/anode.h"
#include "../libsrc/infra/infraconsole.h"

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

static void test2() {
    for( ;; ) {
        FS fs;
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

static void test3() {
    uv_loop_t* loop = uv_default_loop();
    uv_timer_t timer;
    uv_timer_init( loop, &timer );
    timer.data = 0;
    uv_timer_start( &timer, test3_timer_fn, 0, 1000 );

    uv_run( loop, UV_RUN_DEFAULT );
    uv_loop_close( loop );
}

void testServer() {
    FS fs;
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
