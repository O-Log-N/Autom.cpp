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

int main() {
    try {
        FS fs;
        for( ;; ) {
            Node* p = new NodeOne;
            fs.addNode( p );
            fs.run();
            fs.removeNode( p );
            delete p;
        }

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
//        AASSERT3( a > b, "---{}---{}---{}---", 1, 2, 3 );
    } catch( const std::exception& e ) {
        console.error( "std::exception '{}'", e.what() );
        return 1;
    } catch( ... ) {
        console.error( "Unknown exception!" );
        return 2;
    }

    return 0;
}
