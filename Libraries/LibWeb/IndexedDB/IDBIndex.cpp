/*
 * Copyright (c) 2024, stelar7 <dudedbz@gmail.com>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <AK/QuickSort.h>
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

void IDBIndex::store_a_record(IndexRecord const& record)
{
    m_records.append(record);

    // The records are stored in index’s list of records such that the list is sorted primarily on the records keys, and secondarily on the records values, in ascending order.
    AK::quick_sort(m_records, [](IndexRecord const& a, IndexRecord const& b) {
        auto key_compare = Key::compare_two_keys(a.key, b.key);

        if (key_compare != 0)
            return key_compare < 0;

        return Key::compare_two_keys(a.value, b.value) < 0;
    });
}

bool IDBIndex::has_record_with_key(GC::Ref<Key> key)
{
    auto index = m_records.find_if([&key](auto const& record) {
        return record.key == key;
    });

    return index != m_records.end();
}

}
