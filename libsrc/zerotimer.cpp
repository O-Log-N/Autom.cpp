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
#include "../include/aassert.h"
#include "../include/zerotimer.h"
#include "../libsrc/infra/loopcontainer.h"

namespace autom {

class ZeroTimer {
  public:
    enum EventId { ID_ERROR = 1, ID_TIME };

    ZeroTimer() {
        onTime = []() {};
    }

    std::function< void( void ) > onTime;
};

struct ZeroQTimer {
    ZeroTimer* zt;
};

static void timerCloseCb( uv_handle_t* handle ) {
    delete ( uv_timer_t* )handle;
}

static void timerCb( uv_timer_t* handle ) {
    AASSERT4( handle->data );
    auto item = static_cast<const ZeroQTimer*>( handle->data );
    item->zt->onTime();
    if( !uv_timer_get_repeat( handle ) ) {
        handle->data = nullptr;
        uv_close( ( uv_handle_t* )handle, timerCloseCb );
        delete item;
    }
}

void setInterval( LoopContainer* loop, std::function< void( void ) > fn, unsigned secRepeat ) {
    AASSERT4( secRepeat > 0 );

    auto item = new ZeroQTimer;
    auto zt = new ZeroTimer;
    zt->onTime = fn;
    item->zt = zt;

    uv_timer_t* timer = new uv_timer_t;
    timer->data = item;
    uv_timer_init( loop->infraLoop(), timer );
    uv_timer_start( timer, timerCb, secRepeat * 1000, secRepeat * 1000 );
}

void startTimeout( LoopContainer* loop, std::function< void( void ) > fn, unsigned secDelay ) {
    auto item = new ZeroQTimer;
    auto zt = new ZeroTimer;
    zt->onTime = fn;
    item->zt = zt;

    uv_timer_t* timer = new uv_timer_t;
    timer->data = item;
    uv_timer_init( loop->infraLoop(), timer );
    uv_timer_start( timer, timerCb, secDelay * 1000, 0 );
}

}