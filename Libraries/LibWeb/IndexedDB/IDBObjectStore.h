/*
 * Copyright (c) 2024, stelar7 <dudedbz@gmail.com>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include <AK/Forward.h>
#include <AK/Optional.h>
#include <AK/String.h>
#include <LibGC/Heap.h>
#include <LibGC/Ptr.h>
#include <LibJS/Runtime/Array.h>
#include <LibJS/Runtime/PrimitiveString.h>
#include <LibJS/Runtime/Value.h>
#include <LibWeb/Bindings/PlatformObject.h>
#include <LibWeb/HTML/StructuredSerializeTypes.h>
#include <LibWeb/IndexedDB/IDBIndex.h>
#include <LibWeb/IndexedDB/IDBRequest.h>
#include <LibWeb/IndexedDB/IDBTransaction.h>
#include <LibWeb/IndexedDB/Internal/Algorithms.h>
#include <LibWeb/IndexedDB/Internal/Key.h>
#include <LibWeb/IndexedDB/Internal/KeyGenerator.h>

namespace Web::IndexedDB {

struct IDBIndexParameters {
    bool unique { false };
    bool multi_entry { false };
};

// https://w3c.github.io/IndexedDB/#object-store-record
struct Record {
    GC::Ref<Key> key;
    HTML::SerializationRecord value;
};

// https://w3c.github.io/IndexedDB/#object-store-interface
class IDBObjectStore : public Bindings::PlatformObject {
    WEB_PLATFORM_OBJECT(IDBObjectStore, Bindings::PlatformObject);
    GC_DECLARE_ALLOCATOR(IDBObjectStore);

public:
    String name() const { return m_name; }
    void set_name(String name) { m_name = move(name); }

    bool auto_increment() const { return m_auto_increment; }
    Optional<KeyPath> internal_key_path() const { return m_key_path; }
    JS::Value key_path() const
    {
        if (!m_key_path.has_value())
            return JS::js_null();

        return m_key_path.value().visit(
            [&](String const& value) -> JS::Value { return JS::PrimitiveString::create(realm().vm(), value); },
            [&](Vector<String> const& value) -> JS::Value { return JS::Array::create_from<String>(realm(), value.span(), [&](auto const& entry) -> JS::Value {
                                                                return JS::PrimitiveString::create(realm().vm(), entry);
                                                            }); });
    }

    // If the object store has a key path it is said to use in-line keys. Otherwise it is said to use out-of-line keys.
    bool uses_inline_keys() const { return m_key_path.has_value(); }
    bool uses_out_of_line_keys() const { return !m_key_path.has_value(); }

    Optional<KeyGenerator> key_generator() const { return m_key_generator; }

    void set_transaction(GC::Ref<IDBTransaction> transaction) { m_transaction = transaction; }
    GC::Ref<IDBTransaction> transaction() { return m_transaction; }
    AK::ReadonlySpan<GC::Ref<IDBIndex>> index_set() const { return m_indexes; }
    void add_index(GC::Ref<IDBIndex> index) { m_indexes.append(index); }

    [[nodiscard]] WebIDL::ExceptionOr<GC::Ref<IDBRequest>> count(Optional<JS::Value>);
    WebIDL::ExceptionOr<GC::Ref<IDBIndex>> create_index(String const&, KeyPath, IDBIndexParameters options = {});
    [[nodiscard]] GC::Ref<HTML::DOMStringList> index_names();
    [[nodiscard]] WebIDL::ExceptionOr<GC::Ref<IDBRequest>> add_or_put(GC::Ref<IDBObjectStore>, JS::Value, Optional<JS::Value> const&, bool);
    [[nodiscard]] WebIDL::ExceptionOr<GC::Ref<IDBRequest>> add(JS::Value value, Optional<JS::Value> const& key);
    [[nodiscard]] WebIDL::ExceptionOr<GC::Ref<IDBRequest>> put(JS::Value value, Optional<JS::Value> const& key);

    virtual ~IDBObjectStore() override;
    [[nodiscard]] static GC::Ref<IDBObjectStore> create(JS::Realm&, String, bool, Optional<KeyPath> const&, GC::Ref<IDBTransaction>);

    [[nodiscard]] bool has_record_with_key(GC::Ref<Key> key);
    void remove_records_in_range(GC::Ref<IDBKeyRange>);
    void store_a_record(Record const&);
    [[nodiscard]] u64 count_records_in_range(GC::Ref<IDBKeyRange>);

protected:
    explicit IDBObjectStore(JS::Realm&, String, bool, Optional<KeyPath> const&, GC::Ref<IDBTransaction>);
    virtual void initialize(JS::Realm&) override;
    virtual void visit_edges(Visitor& visitor) override;

private:
    // An object store has a name, which is a name. At any one time, the name is unique within the database to which it belongs.
    String m_name;

    // An object store optionally has a key path.
    Optional<KeyPath> m_key_path;

    // If autoIncrement is true, then the created object store uses a key generator.
    bool m_auto_increment { false };

    // An object store optionally has a key generator.
    Optional<KeyGenerator> m_key_generator;

    // An object store handle has an associated transaction.
    GC::Ref<IDBTransaction> m_transaction;

    // An object store handle has an index set
    Vector<GC::Ref<IDBIndex>> m_indexes;

    // An object store has a list of records
    Vector<Record> m_records;
};

}
