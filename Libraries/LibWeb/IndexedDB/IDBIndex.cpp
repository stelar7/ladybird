/*
 * Copyright (c) 2024, stelar7 <dudedbz@gmail.com>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <LibWeb/Bindings/IDBIndexPrototype.h>
#include <LibWeb/Bindings/Intrinsics.h>
#include <LibWeb/HTML/EventNames.h>
#include <LibWeb/IndexedDB/IDBIndex.h>
#include <LibWeb/IndexedDB/Internal/Key.h>

namespace Web::IndexedDB {

GC_DEFINE_ALLOCATOR(IDBIndex);

IDBIndex::~IDBIndex() = default;

IDBIndex::IDBIndex(JS::Realm& realm, GC::Ref<IDBObjectStore> object_store, String name, KeyPath key_path, bool unique, bool multi_entry)
    : PlatformObject(realm)
    , m_name(move(name))
    , m_unique(unique)
    , m_multi_entry(multi_entry)
    , m_object_store(object_store)
    , m_key_path(move(key_path))
{
}

GC::Ref<IDBIndex> IDBIndex::create(JS::Realm& realm, GC::Ref<IDBObjectStore> object_store, String name, KeyPath key_path, bool unique, bool multi_entry)
{
    return realm.create<IDBIndex>(realm, object_store, name, key_path, unique, multi_entry);
}

void IDBIndex::initialize(JS::Realm& realm)
{
    Base::initialize(realm);
    WEB_SET_PROTOTYPE_FOR_INTERFACE(IDBIndex);
}

GC::Ref<IDBTransaction> IDBIndex::transaction()
{
    // The transaction of an index handle is the transaction of its associated object store handle
    return m_object_store->transaction();
}

}
