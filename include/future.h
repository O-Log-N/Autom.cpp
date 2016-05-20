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

#ifndef FUTURE_H
#define FUTURE_H

#include <unordered_map>
#include <functional>
#include <memory>

#include "aassert.h"
#include "anode.h"
#include "../libsrc/infra/infraconsole.h"

namespace autom {

class InfraFutureBase {
    bool dataReady;
    bool stepReady;

  public:
    FutureFunction fn;
    int refCount;
    bool multi;

    InfraFutureBase() : dataReady( false ), stepReady( false ) {}
    virtual ~InfraFutureBase() {}
    void setDataReady() {
        dataReady = true;
    }
    bool isDataReady() const {
        return dataReady;
    }
    void setStepReady() {
        stepReady = true;
    }
    bool isStepReady() const {
        return stepReady;
    }
    void cleanup() {
        if( multi )
            return;
        fn = FutureFunction();
        //The line above effectively destroys existing lambda it->second.fn
        //  First, we CAN do it, as we don't need second.fn anymore at all
        //  Second, we SHOULD do it, to avoid cyclical references from lambda
        //    to our InfraFutures, which will prevent futureCleanup() from
        //    destroying InfraFuture - EVER
        refCount--;
    }
    virtual void debugDump() const = 0;
};

template< typename T >
class InfraFuture : public InfraFutureBase {
    T result;

  public:
    const T& getResult() const {
        AASSERT0( isDataReady() );
        return result;
    }
    T& infraGetData() {
        return result;
    }

    void debugDump() const override {
        INFRATRACE0( "    refcnt {} {}", refCount, multi );
    }
};

class FutureBase {
  public:
    virtual InfraFutureBase* infraGetPtr() const = 0;
    virtual void then( const FutureFunction& ) const = 0;
};

template< typename T >
class Future : public FutureBase {
    FutureId futureId;
    Node* node;
    InfraFuture< T >* infraPtr;

  public:
    explicit Future( Node* );
    Future();
    Future( const Future& );
    Future( Future&& );
    Future& operator=( const Future& );
    ~Future();
    void then( const FutureFunction& ) const override;
    void setValue( const T& v ) const {
        infraPtr->infraGetData() = v;
        infraPtr->setDataReady();
    }
    FutureId infraGetId() const {
        return futureId;
    }
    InfraFutureBase* infraGetPtr() const override {
        return infraPtr;
    }
    void infraAssign( const Future& other ) const {
        AASSERT4( 0 == futureId );
        AASSERT4( node == other.node );
        *infraPtr = *other.infraPtr;
    };
    const T& value() const;
};

template< typename T >
Future< T >::Future( Node* node_ ) : node( node_ ) {
    futureId = node->nextFutureId();
    auto f = new InfraFuture< T >;
    f->refCount = 1;
    f->multi = false;
    infraPtr = static_cast<InfraFuture< T >*>( node->insertInfraFuture( futureId, f ) );
}

template<typename T>
inline Future<T>::Future() {
    futureId = 0;
    node = nullptr;
    infraPtr = nullptr;
}

template< typename T >
Future< T >::Future( const Future& other ) :
    futureId( other.futureId ), node( other.node ), infraPtr( other.infraPtr ) {
    AASSERT4( infraPtr == node->findInfraFuture( futureId ) );
    infraPtr->refCount++;
}

template< typename T >
Future< T >& Future< T >::operator=( const Future& other ) {
    futureId = other.futureId;
    node = other.node;
    infraPtr = other.infraPtr;
    AASSERT4( infraPtr == node->findInfraFuture( futureId ) );
    infraPtr->refCount++;
    return *this;
}

template< typename T >
Future< T >::Future( Future&& other ) :
    futureId( other.futureId ), node( other.node ), infraPtr( other.infraPtr ) {
    AASSERT4( infraPtr == node->findInfraFuture( futureId ) );
    infraPtr->refCount++;
}

template< typename T >
Future< T >::~Future() {
    // TODO: check ( infraPtr == node->findInfraFuture( futureId ) )
    infraPtr->refCount--;
}

template< typename T >
void Future< T >::then( const FutureFunction& fn ) const {
    AASSERT4( infraPtr == node->findInfraFuture( futureId ) );
    infraPtr->fn = fn;
    infraPtr->refCount++; // NOTE: see corresponding refCount-- in InfraFutureBase::cleanup()
}

template< typename T >
const T& Future< T >::value() const {
    AASSERT4( infraPtr == node->findInfraFuture( futureId ) );
    return infraPtr->getResult();
}

template< typename T >
class MultiFuture {
    FutureId futureId;
    Node* node;
    InfraFuture< T >* infraPtr;

  public:
    explicit MultiFuture( Node* );
    MultiFuture();
    MultiFuture( const MultiFuture& ) = default;
    MultiFuture( MultiFuture&& ) = default;
    MultiFuture& operator=( const MultiFuture& ) = default;

    void onEach( const FutureFunction& f );
    FutureId infraGetId() const {
        return futureId;
    }
    bool isOk() const {
        return !!futureId;
    }
    const T& value() const;
};

template< typename T >
MultiFuture< T >::MultiFuture( Node* node_ ) : node( node_ ) {
    futureId = node->nextFutureId();
    auto f = new InfraFuture< T >;
    f->refCount = 0;
    f->multi = true;
    infraPtr = static_cast<InfraFuture< T >*>( node->insertInfraFuture( futureId, f ) );
}

template<typename T>
inline MultiFuture<T>::MultiFuture() {
    futureId = 0;
    node = nullptr;
    infraPtr = nullptr;
}

template< typename T >
void MultiFuture< T >::onEach( const FutureFunction& fn ) {
    AASSERT4( infraPtr == node->findInfraFuture( futureId ) );
    infraPtr->fn = fn;
}

template< typename T >
const T& MultiFuture< T >::value() const {
    AASSERT4( infraPtr == node->findInfraFuture( futureId ) );
    return infraPtr->getResult();
}

}
#endif
