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

#include "..\include\timer.h"
#include "..\include\anode.h"
#include "infra\nodecontainer.h"

using namespace autom;

static void timerCloseCb( uv_handle_t* handle ) {
	delete (uv_timer_t*)handle;
}

static void timerCb( uv_timer_t* handle ) {
	AASSERT4( handle->data );
	auto* item = static_cast<const NodeQTimer*>( handle->data );
	item->node->infraProcessTimer( *item );
	if( !uv_timer_get_repeat( handle ) ) {
		handle->data = nullptr;
		uv_close( (uv_handle_t*)handle, timerCloseCb );
		delete item;
	}
}

autom::Future< Timer > autom::startTimout( Node* node, unsigned secDelay ) {
	Future< Timer > future( node );

	NodeQTimer* item = new NodeQTimer;
	item->id = future.infraGetId();
	item->node = node;

	uv_timer_t* timer = new uv_timer_t;
	uv_timer_init( node->parentFS->infraLoop(), timer );
	timer->data = item;
	uv_timer_start( timer, timerCb, secDelay * 1000, 0 );

	return future;
}

MultiFuture< Timer > autom::setInterval( Node* node, unsigned secRepeat ) {
	AASSERT4( secRepeat > 0 );
	MultiFuture< Timer > future( node );

	NodeQTimer* item = new NodeQTimer;
	item->id = future.infraGetId();
	item->node = node;

	uv_timer_t* timer = new uv_timer_t;
	uv_timer_init( node->parentFS->infraLoop(), timer );
	timer->data = item;
	uv_timer_start( timer, timerCb, secRepeat * 1000, secRepeat * 1000 );

	return future;
}
