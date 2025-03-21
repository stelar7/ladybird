/*
 * Copyright (c) 2024, stelar7 <dudedbz@gmail.com>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <LibGC/Ptr.h>
#include <LibJS/Runtime/Value.h>
#include <LibWeb/Bindings/IDBCursorPrototype.h>
#include <LibWeb/Bindings/Intrinsics.h>
#include <LibWeb/HTML/EventNames.h>
#include <LibWeb/IndexedDB/IDBCursor.h>
#include <LibWeb/IndexedDB/Internal/Algorithms.h>

namespace Web::IndexedDB {

GC_DEFINE_ALLOCATOR(IDBCursor);

IDBCursor::~IDBCursor() = default;

IDBCursor::IDBCursor(JS::Realm& realm, GC::Ref<IDBTransaction> transaction, GC::Ptr<Key> position, Bindings::IDBCursorDirection direction, bool got_value, GC::Ptr<Key> key, JS::Value value, CursorSource source, GC::Ref<IDBKeyRange> range, bool key_only)
    : PlatformObject(realm)
    , m_transaction(transaction)
    , m_position(position)
    , m_direction(direction)
    , m_got_value(got_value)
    , m_key(key)
    , m_value(value)
    , m_source(source)
    , m_range(range)
    , m_key_only(key_only)
{
}

GC::Ref<IDBCursor> IDBCursor::create(JS::Realm& realm, GC::Ref<IDBTransaction> transaction, GC::Ptr<Key> position, Bindings::IDBCursorDirection direction, bool got_value, GC::Ptr<Key> key, JS::Value value, CursorSource source, GC::Ref<IDBKeyRange> range, bool key_only)
{
    return realm.create<IDBCursor>(realm, transaction, position, direction, got_value, key, value, source, range, key_only);
}

void IDBCursor::initialize(JS::Realm& realm)
{
    Base::initialize(realm);
    WEB_SET_PROTOTYPE_FOR_INTERFACE(IDBCursor);
}

// https://w3c.github.io/IndexedDB/#dom-idbcursor-key
JS::Value IDBCursor::key()
{
    // The key getter steps are to return the result of converting a key to a value with the cursor’s current key.
    return convert_a_key_to_a_value(realm(), *m_key);
}

}
