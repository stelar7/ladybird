/*
 * Copyright (c) 2024, stelar7 <dudedbz@gmail.com>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <LibGC/Ptr.h>
#include <LibWeb/Bindings/IDBObjectStorePrototype.h>
#include <LibWeb/Bindings/Intrinsics.h>
#include <LibWeb/HTML/EventNames.h>
#include <LibWeb/IndexedDB/IDBObjectStore.h>
#include <LibWeb/IndexedDB/IDBTransaction.h>
#include <LibWeb/IndexedDB/Internal/Algorithms.h>

namespace Web::IndexedDB {

GC_DEFINE_ALLOCATOR(IDBObjectStore);

IDBObjectStore::~IDBObjectStore() = default;

IDBObjectStore::IDBObjectStore(JS::Realm& realm, String name, bool auto_increment, Optional<KeyPath> const& key_path, GC::Ref<IDBTransaction> transaction)
    : PlatformObject(realm)
    , m_name(move(name))
    , m_key_path(key_path)
    , m_auto_increment(auto_increment)
    , m_transaction(transaction)
{
}

GC::Ref<IDBObjectStore> IDBObjectStore::create(JS::Realm& realm, String name, bool auto_increment, Optional<KeyPath> const& key_path, GC::Ref<IDBTransaction> transaction)
{
    return realm.create<IDBObjectStore>(realm, name, auto_increment, key_path, transaction);
}

void IDBObjectStore::initialize(JS::Realm& realm)
{
    Base::initialize(realm);
    WEB_SET_PROTOTYPE_FOR_INTERFACE(IDBObjectStore);
}

}
