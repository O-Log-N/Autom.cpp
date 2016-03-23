#include <exception>
#include "../3rdparty/cppformat/cppformat/format.h"

#include "../include/aasert.h"
#include "../include/atrace.h"

int main()
{
    try {

        return 0;
    }
    catch (const std::exception& e) {
        fmt::print(stderr, "Exception!");
        fmt::print(stderr, e.what());
        return 1;
    }
    catch (...) {
        fmt::print(stderr, "Unknown exception!");
        return 1;
    }
}

