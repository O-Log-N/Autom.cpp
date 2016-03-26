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

#include "../include/aconsole.h"

namespace autom {
ConsoleWrapper console;

//NB: MSVC doesn't support single-parameter static_assert() :-(
static_assert(Console::TRACE==0,"Console::TRACE==0");
static_assert(Console::INFO==1,"Console::INFO==1");
static_assert(Console::NOTICE==2,"Console::NOTICE==2");
static_assert(Console::WARN==3,"Console::WARN==3");
static_assert(Console::ERROR==4,"Console::ERROR==4");
static_assert(Console::CRITICAL==5,"Console::CRITICAL==5");
static_assert(Console::ALERT==6,"Console::ALERT==6");

static const char* defaultFmtStrings[] = {
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

void DefaultConsole::formattedWrite( WRITELEVEL lvl, const char* s ) override {
	//AASSERT() is probably way too harsh here
	if(lvl >= 0 && lvl <= sizeof(defaultFmtStrings)/sizeof(defaultFmtStrings[0]))
	        fmt::print(lvl >= ERROR ? std::cerr : std::cout, defaultFmtStrings[lvl], s);
	else {
		fmt::print(std::cerr, "ERROR: DefaultConsole::formattedWrite(): unknown lvl={}, forced to ERROR:\n", lvl);
		fmt::print(std::cerr, "ERROR: {}\n", s);
	}
}

void FileConsole::formattedWrite( WRITELEVEL lvl, const char* s ) override {
	//AASSERT() is probably way too harsh here
	if(lvl >= 0 && lvl <= sizeof(defaultFmtStrings)/sizeof(defaultFmtStrings[0]))
	        fmt::print(os, defaultFmtStrings[lvl], s);
	else {
		fmt::print(os, "ERROR: DefaultConsole::formattedWrite(): unknown lvl={}, forced to ERROR:\n", lvl);
		fmt::print(os, "ERROR: {}\n", s);
	}
}

}//namespace autom
