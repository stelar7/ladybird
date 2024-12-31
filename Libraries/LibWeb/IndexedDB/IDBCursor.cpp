/*
 * Copyright (c) 2024, stelar7 <dudedbz@gmail.com>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <LibWeb/Bindings/IDBCursorPrototype.h>
#include <LibWeb/Bindings/Intrinsics.h>
#include <LibWeb/HTML/EventNames.h>
#include <LibWeb/IndexedDB/IDBCursor.h>

namespace Web::IndexedDB {

GC_DEFINE_ALLOCATOR(IDBCursor);

IDBCursor::~IDBCursor() = default;

IDBCursor::IDBCursor(JS::Realm& realm, GC::Ref<IDBTransaction> transaction)
    : PlatformObject(realm)
    , m_transaction(transaction)
{
}

GC::Ref<IDBCursor> IDBCursor::create(JS::Realm& realm, GC::Ref<IDBTransaction> transaction)
{
    return realm.create<IDBCursor>(realm, transaction);
}

void IDBCursor::initialize(JS::Realm& realm)
{
    Base::initialize(realm);
    WEB_SET_PROTOTYPE_FOR_INTERFACE(IDBCursor);
}

}
