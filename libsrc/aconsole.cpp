/*******************************************************************************
Copyright (C) 2016 OLogN Technologies AG
	This program is free software; you can redistribute it and/or modify
	it under the terms of the GNU General Public License version 2 as
	published by the Free Software Foundation.
	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.
	You should have received a copy of the GNU General Public License along
	with this program; if not, write to the Free Software Foundation, Inc.,
	51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
*******************************************************************************/

#include "infra/infraconsole.h"

namespace autom {
InfraConsoleWrapper infraConsole;

//NB: MSVC doesn't support single-parameter static_assert() :-(
static_assert(Console::TRACE==0,"Console::TRACE==0");
static_assert(Console::INFO==1,"Console::INFO==1");
static_assert(Console::NOTICE==2,"Console::NOTICE==2");
static_assert(Console::WARN==3,"Console::WARN==3");
static_assert(Console::ERROR==4,"Console::ERROR==4");
static_assert(Console::CRITICAL==5,"Console::CRITICAL==5");
static_assert(Console::ALERT==6,"Console::ALERT==6");

static const char* const defaultFmtStrings[] = {
    //we don't use generic "{}: {}\n" substituting "TRACE" etc.
    //  to save an indirection and few CPU cycles for free
    "TRACE: {}\n",
    "INFO: {}\n",
    "NOTICE: {}\n",
    "WARN: {}\n",
    "ERROR: {}\n",
    "CRITICAL: {}\n",
    "ALERT: {}\n"
};

void DefaultConsole::formattedWrite( WRITELEVEL lvl, const char* s ) {
    //AASSERT() is probably way too harsh here
    if(lvl >= 0 && lvl < sizeof(defaultFmtStrings)/sizeof(defaultFmtStrings[0]))
        fmt::print(lvl >= ERROR ? std::cerr : std::cout, defaultFmtStrings[lvl], s);
    else {
        fmt::print(std::cerr, "ERROR: DefaultConsole::formattedWrite(): unknown lvl={}, forced to ERROR:\n", lvl);
        fmt::print(std::cerr, "ERROR: {}\n", s);
    }
}

void FileConsole::formattedWrite( WRITELEVEL lvl, const char* s ) {
    //AASSERT() is probably way too harsh here
    if(lvl >= 0 && lvl < sizeof(defaultFmtStrings)/sizeof(defaultFmtStrings[0]))
        fmt::print(os, defaultFmtStrings[lvl], s);
    else {
        fmt::print(os, "ERROR: DefaultConsole::formattedWrite(): unknown lvl={}, forced to ERROR:\n", lvl);
        fmt::print(os, "ERROR: {}\n", s);
    }
}

Console::TimeLabel Console::timeWithLabel() {
    if(firstFreeATime == ATIMENONE) {
        auto it = aTimes.insert(aTimes.end(),PrivateATimeStoredType());
        //moved now() after insert to avoid measuring time of insert()
        it->began = Clock::now();
        return TimeLabel(it - aTimes.begin());
    }

    assert(firstFreeATime < aTimes.size());//TODO!: remove assert (see Console::time())
    size_t idx = firstFreeATime;
    auto& item = aTimes[idx];

    //removing first item from single-linked list
    firstFreeATime = item.nextFree;

    item.nextFree = ATIMENONE;
    item.began = Clock::now();
    return TimeLabel(idx);
}

void Console::timeEnd(Console::TimeLabel label, const char* text) {
    //calculating now() right here, to avoid measuring find() function
    TimePoint now = Clock::now();

    size_t idx = label.idx;
    assert(idx < aTimes.size());//TODO!: remove assert (see Console::time())
    auto& item = aTimes[idx];
    assert(item.nextFree == ATIMENONE);//TODO: AASSERT() or remove?

    write(INFO,"Console::timeEnd('{}'): {}", text, PrintableDuration(now - item.began).count());

    //{ adding item 'idx' to single-linked list
    item.nextFree = firstFreeATime;
    firstFreeATime = idx;
    //} adding item 'idx' to single-linked list
}

#ifndef ASTRIP_NODEJS_COMPAT
//{ NODE.JS COMPATIBILITY HELPERS
void Console::time(const char* label) {
    auto it = njTimes.insert(std::unordered_map<std::string, TimePoint>::value_type(label, TimePoint())).first;
    //moved now() after insert to avoid measuring time of insert()
    it->second = Clock::now();
}

void Console::timeEnd(const char* label) {
    //calculating now() right here, to avoid measuring find() function
    TimePoint now = Clock::now();

    auto found = njTimes.find(label);
    if(found == njTimes.end()) {
        write(ERROR,"Console::timeEnd(): unknown label '{}'", label);
        return;
    }

    write(INFO,"Console::timeEnd('{}'): {}", label, PrintableDuration(now - found->second).count());
    njTimes.erase(found);
}
//} NODE.JS COMPATIBILITY HELPERS
#endif

void NodeConsole::formattedWrite( WRITELEVEL lvl, const char* s ) {
	infraConsole.formattedWrite(lvl, s);
}

}//namespace autom
