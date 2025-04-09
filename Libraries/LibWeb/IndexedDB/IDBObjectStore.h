/*
 * Copyright (c) 2024-2025, stelar7 <dudedbz@gmail.com>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include <AK/Forward.h>
#include <AK/HashMap.h>
#include <AK/Optional.h>
#include <AK/String.h>
#include <LibGC/Heap.h>
#include <LibGC/Ptr.h>
#include <LibJS/Runtime/Array.h>
#include <LibJS/Runtime/PrimitiveString.h>
#include <LibJS/Runtime/Value.h>
#include <LibWeb/Bindings/IDBCursorPrototype.h>
#include <LibWeb/Bindings/PlatformObject.h>
#include <LibWeb/HTML/StructuredSerializeTypes.h>
#include <LibWeb/IndexedDB/IDBRequest.h>
#include <LibWeb/IndexedDB/IDBTransaction.h>
#include <LibWeb/IndexedDB/Internal/Algorithms.h>
#include <LibWeb/IndexedDB/Internal/Key.h>
#include <LibWeb/IndexedDB/Internal/KeyGenerator.h>
#include <LibWeb/IndexedDB/Internal/ObjectStore.h>

namespace Web::IndexedDB {

using KeyPath = Variant<String, Vector<String>>;

struct IDBIndexParameters {
    bool unique { false };
    bool multi_entry { false };
};

// https://w3c.github.io/IndexedDB/#object-store-interface
// https://w3c.github.io/IndexedDB/#object-store-handle-construct
class IDBObjectStore : public Bindings::PlatformObject {
    WEB_PLATFORM_OBJECT(IDBObjectStore, Bindings::PlatformObject);
    GC_DECLARE_ALLOCATOR(IDBObjectStore);

public:
    [[nodiscard]] static GC::Ref<IDBObjectStore> create(JS::Realm&, GC::Ref<ObjectStore>, GC::Ref<IDBTransaction>);

    String name() const { return m_name; }
    WebIDL::ExceptionOr<void> set_name(String const& value);
    GC::Ref<IDBTransaction> transaction() const { return m_transaction; }
    GC::Ref<ObjectStore> store() const { return m_store; }
    AK::HashMap<String, GC::Ref<Index>>& index_set() { return m_indexes; }

    WebIDL::ExceptionOr<GC::Ref<IDBIndex>> create_index(String const&, KeyPath, IDBIndexParameters options);
    [[nodiscard]] GC::Ref<HTML::DOMStringList> index_names();
    WebIDL::ExceptionOr<GC::Ref<IDBIndex>> index(String const&);
    WebIDL::ExceptionOr<void> delete_index(String const&);

    bool auto_increment() const;
    JS::Value key_path() const;

    // If the object store has a key path it is said to use in-line keys. Otherwise it is said to use out-of-line keys.
    bool uses_inline_keys() const;
    bool uses_out_of_line_keys() const;

    void set_transaction(GC::Ref<IDBTransaction> transaction) { m_transaction = transaction; }
    GC::Ref<IDBTransaction> transaction() { return m_transaction; }
    GC::Ref<ObjectStore> store() { return m_store; }
    AK::ReadonlySpan<GC::Ref<Index>> index_set() const { return m_indexes; }
    void add_index(GC::Ref<Index> index) { m_indexes.append(index); }

    [[nodiscard]] WebIDL::ExceptionOr<GC::Ref<IDBRequest>> count(Optional<JS::Value>);
    WebIDL::ExceptionOr<GC::Ref<IDBIndex>> create_index(String const&, KeyPath, IDBIndexParameters options = {});
    [[nodiscard]] GC::Ref<HTML::DOMStringList> index_names();
    [[nodiscard]] WebIDL::ExceptionOr<GC::Ref<IDBRequest>> add_or_put(GC::Ref<IDBObjectStore>, JS::Value, Optional<JS::Value> const&, bool);
    [[nodiscard]] WebIDL::ExceptionOr<GC::Ref<IDBRequest>> add(JS::Value value, Optional<JS::Value> const& key);
    [[nodiscard]] WebIDL::ExceptionOr<GC::Ref<IDBRequest>> put(JS::Value value, Optional<JS::Value> const& key);
    [[nodiscard]] WebIDL::ExceptionOr<GC::Ref<IDBRequest>> open_cursor(JS::Value, Bindings::IDBCursorDirection = Bindings::IDBCursorDirection::Next);
    [[nodiscard]] WebIDL::ExceptionOr<GC::Ref<IDBRequest>> get(JS::Value);

    virtual ~IDBObjectStore() override;
    [[nodiscard]] static GC::Ref<IDBObjectStore> create(JS::Realm&, String, bool, Optional<KeyPath> const&, GC::Ref<IDBTransaction>);

    [[nodiscard]] bool has_record_with_key(GC::Ref<Key> key);
    void remove_records_in_range(GC::Ref<IDBKeyRange>);
    void store_a_record(Record const&);

protected:
    explicit IDBObjectStore(JS::Realm&, GC::Ref<ObjectStore>, GC::Ref<IDBTransaction>);
    virtual void initialize(JS::Realm&) override;
    virtual void visit_edges(Visitor& visitor) override;

private:
    // An object store handle has an index set
    Vector<GC::Ref<Index>> m_indexes;

    // An object store handle has an associated object store and an associated transaction.
    GC::Ref<ObjectStore> m_store;
    GC::Ref<IDBTransaction> m_transaction;

    // An object store handle has a name, which is initialized to the name of the associated object store when the object store handle is created.
    String m_name;

    // An object store handle has an index set
    AK::HashMap<String, GC::Ref<Index>> m_indexes;
};

}
