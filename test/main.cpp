#include <exception>
#include <iostream>

#include "../include/aassert.h"
#include "../include/aconsole.h"
#include "../include/abuffer.h"
#include "../include/anode.h"
#include "../libsrc/infra/infraconsole.h"


using namespace std;
using namespace autom;

extern autom::InfraConsoleWrapper infraConsole;

int main() {
    try {
        Buffer buf( 33 );
        infraConsole.trace( "Starting..." );
        infraConsole.assignNewConsole( unique_ptr<NodeConsole>(new NodeConsole) );
        auto idt1 = infraConsole.timeWithLabel();
        auto idt2 = infraConsole.timeWithLabel();
        infraConsole.info( "Hello, {0}{1}", "World",'!' );
        infraConsole.timeEnd( idt1, "1" );
        INFRATRACE3( "TRACE LEVEL IS {0}, line {1}", infraConsole.traceLevel(), __LINE__ );
        infraConsole.timeEnd( idt2, "2" );
        int a = 2, b = 3;
        AASSERT3( b > a );
        AASSERT3( a > b, "---{}---{}---{}---", 1, 2, 3 );
        }
    catch( const std::exception& e ) {
        infraConsole.error( "std::exception '{}'", e.what() );
        return 1;
        }
    catch( ... ) {
        infraConsole.error( "Unknown exception!" );
        return 2;
        }

    return 0;
    }
