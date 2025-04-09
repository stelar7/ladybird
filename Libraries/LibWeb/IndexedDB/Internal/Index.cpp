/*
 * Copyright (c) 2025, stelar7 <dudedbz@gmail.com>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <LibWeb/IndexedDB/Internal/Index.h>
#include <LibWeb/IndexedDB/Internal/ObjectStore.h>

namespace Web::IndexedDB {

GC_DEFINE_ALLOCATOR(Index);

Index::~Index() = default;

GC::Ref<Index> Index::create(JS::Realm& realm, GC::Ref<ObjectStore> store, String name, KeyPath const& key_path, bool unique, bool multi_entry)
{
    return realm.create<Index>(store, name, key_path, unique, multi_entry);
}

Index::Index(GC::Ref<ObjectStore> store, String name, KeyPath const& key_path, bool unique, bool multi_entry)
    : m_object_store(store)
    , m_name(move(name))
    , m_unique(unique)
    , m_multi_entry(multi_entry)
    , m_key_path(key_path)
{
    store->index_set().set(name, *this);
}

void Index::visit_edges(Visitor& visitor)
{
    Base::visit_edges(visitor);
    visitor.visit(m_object_store);

    for (auto& record : m_records) {
        visitor.visit(record.key);
        visitor.visit(record.value);
    }
}

void Index::store_a_record(IndexRecord const& record)
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

bool Index::has_record_with_key(GC::Ref<Key> key)
{
    auto index = m_records.find_if([&key](auto const& record) {
        return record.key == key;
    });

    return index != m_records.end();
}

void Index::set_name(String name)
{
    // NOTE: Update the key in the map so it still matches the name
    auto old_value = m_object_store->index_set().take(m_name).release_value();
    m_object_store->index_set().set(name, old_value);

    m_name = move(name);
}

}
