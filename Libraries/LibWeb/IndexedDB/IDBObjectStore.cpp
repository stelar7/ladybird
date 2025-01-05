/*
 * Copyright (c) 2024, stelar7 <dudedbz@gmail.com>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <AK/Optional.h>
#include <AK/QuickSort.h>
#include <AK/Vector.h>
#include <LibGC/Function.h>
#include <LibGC/Ptr.h>
#include <LibJS/Runtime/Value.h>
#include <LibWeb/Bindings/IDBObjectStorePrototype.h>
#include <LibWeb/Bindings/Intrinsics.h>
#include <LibWeb/HTML/EventNames.h>
#include <LibWeb/IndexedDB/IDBIndex.h>
#include <LibWeb/IndexedDB/IDBObjectStore.h>
#include <LibWeb/IndexedDB/IDBRequest.h>
#include <LibWeb/IndexedDB/IDBTransaction.h>
#include <LibWeb/IndexedDB/Internal/Algorithms.h>
#include <LibWeb/IndexedDB/Internal/Key.h>
#include <LibWeb/WebIDL/ExceptionOr.h>

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
    transaction->add_to_scope(*this);
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

void IDBObjectStore::visit_edges(Visitor& visitor)
{
    Base::visit_edges(visitor);
    visitor.visit(m_transaction);
    visitor.visit(m_indexes);
}

WebIDL::ExceptionOr<GC::Ref<IDBIndex>> IDBObjectStore::create_index(String const& name, KeyPath key_path, IDBIndexParameters options)
{
    auto& realm = this->realm();

    // 1. Let transaction be this's transaction.
    auto transaction = this->transaction();

    // 2. Let store be this's object store.
    auto& store = *this;

    // 3. If transaction is not an upgrade transaction, throw an "InvalidStateError" DOMException.
    if (transaction->mode() != Bindings::IDBTransactionMode::Versionchange)
        return WebIDL::InvalidStateError::create(realm, "Transaction is not an upgrade transaction"_string);

    // FIXME: 4. If store has been deleted, throw an "InvalidStateError" DOMException.

    // 5. If transaction’s state is not active, then throw a "TransactionInactiveError" DOMException.
    if (transaction->state() != IDBTransaction::TransactionState::Active)
        return WebIDL::TransactionInactiveError::create(realm, "Transaction is not active"_string);

    // 6. If an index named name already exists in store, throw a "ConstraintError" DOMException.
    for (auto const& index : store.index_set()) {
        if (index->name() == name)
            return WebIDL::ConstraintError::create(realm, "An index with the given name already exists"_string);
    }

    // 7. If keyPath is not a valid key path, throw a "SyntaxError" DOMException.
    if (!is_valid_key_path(key_path))
        return WebIDL::SyntaxError::create(realm, "Key path is not valid"_string);

    // 8. Let unique be options’s unique member.
    auto unique = options.unique;

    // 9. Let multiEntry be options’s multiEntry member.
    auto multi_entry = options.multi_entry;

    // 10. If keyPath is a sequence and multiEntry is true, throw an "InvalidAccessError" DOMException.
    if (key_path.has<Vector<String>>() && multi_entry)
        return WebIDL::InvalidAccessError::create(realm, "Key path is a sequence and multiEntry is true"_string);

    // 11. Let index be a new index in store.
    //     Set index’s name to name, key path to keyPath, unique flag to unique, and multiEntry flag to multiEntry.
    auto index = IDBIndex::create(realm, store, name, key_path, unique, multi_entry);

    // 12. Add index to this's index set.
    this->add_index(index);

    // 13. Return a new index handle associated with index and this.
    return index;
}

// https://w3c.github.io/IndexedDB/#dom-idbobjectstore-indexnames
GC::Ref<HTML::DOMStringList> IDBObjectStore::index_names()
{
    // 1. Let names be a list of the names of the indexes in this's index set.
    Vector<String> names;
    for (auto const& index : m_indexes)
        names.append(index->name());

    // 2. Return the result (a DOMStringList) of creating a sorted name list with names.
    return create_a_sorted_name_list(realm(), names);
}

// https://w3c.github.io/IndexedDB/#add-or-put
WebIDL::ExceptionOr<GC::Ref<IDBRequest>> IDBObjectStore::add_or_put(GC::Ref<IDBObjectStore> handle, JS::Value value, Optional<JS::Value> const& key, bool no_overwrite)
{
    auto& realm = this->realm();

    // 1. Let transaction be handle’s transaction.
    auto transaction = handle->transaction();

    // 2. Let store be handle’s object store.
    auto& store = *handle;

    // FIXME: 3. If store has been deleted, throw an "InvalidStateError" DOMException.

    // 4. If transaction’s state is not active, then throw a "TransactionInactiveError" DOMException.
    if (transaction->state() != IDBTransaction::TransactionState::Active)
        return WebIDL::TransactionInactiveError::create(realm, "Transaction is not active"_string);

    // 5. If transaction is a read-only transaction, throw a "ReadOnlyError" DOMException.
    if (transaction->is_readonly())
        return WebIDL::ReadOnlyError::create(realm, "Transaction is read-only"_string);

    // 6. If store uses in-line keys and key was given, throw a "DataError" DOMException.
    if (store.uses_inline_keys() && key.has_value())
        return WebIDL::DataError::create(realm, "Store uses in-line keys and key was given"_string);

    // 7. If store uses out-of-line keys and has no key generator and key was not given, throw a "DataError" DOMException.
    if (store.uses_out_of_line_keys() && !store.key_generator().has_value() && !key.has_value())
        return WebIDL::DataError::create(realm, "Store uses out-of-line keys and has no key generator and key was not given"_string);

    GC::Ptr<Key> key_value;
    // 8. If key was given, then:
    if (key.has_value()) {
        // 1. Let r be the result of converting a value to a key with key. Rethrow any exceptions.
        auto maybe_key = convert_a_value_to_a_key(realm, key.value());
        // 2. If r is invalid, throw a "DataError" DOMException.
        if (maybe_key.is_error())
            return WebIDL::DataError::create(realm, "Key is invalid"_string);

        // 3. Let key be r.
        key_value = maybe_key.release_value();
    }

    // 9. Let targetRealm be a user-agent defined Realm.
    auto& target_realm = realm;

    // 10. Let clone be a clone of value in targetRealm during transaction. Rethrow any exceptions.
    auto clone = TRY(clone_in_realm(target_realm, value, transaction));

    // 11. If store uses in-line keys, then:
    if (store.uses_inline_keys()) {
        // 1. Let kpk be the result of extracting a key from a value using a key path with clone and store’s key path. Rethrow any exceptions.
        auto kpk = extract_a_key_from_a_value_using_a_key_path(realm, clone, store.internal_key_path().value());

        // 2. If kpk is invalid, throw a "DataError" DOMException.
        if (kpk.is_error())
            return WebIDL::DataError::create(realm, "Key path is invalid"_string);

        auto maybe_kpk = kpk.release_value();

        // 3. If kpk is not failure, let key be kpk.
        if (!maybe_kpk.is_error()) {
            key_value = maybe_kpk.release_value();
        }

        // 4. Otherwise (kpk is failure):
        else {
            // 1. If store does not have a key generator, throw a "DataError" DOMException.
            if (!store.key_generator().has_value())
                return WebIDL::DataError::create(realm, "Store does not have a key generator"_string);

            // 2. Otherwise, if check that a key could be injected into a value with clone and store’s key path return false, throw a "DataError" DOMException.
            if (!check_that_a_key_could_be_injected_into_a_value(realm, clone, store.internal_key_path().value()))
                return WebIDL::DataError::create(realm, "Key could not be injected into value"_string);
        }
    }

    // 12. Let operation be an algorithm to run store a record into an object store with store, clone, key, and no-overwrite flag.
    auto operation = GC::Function<WebIDL::ExceptionOr<JS::Value>()>::create(realm.heap(), [&realm, &store, clone, key_value, no_overwrite] {
        auto maybe_key = store_a_record_into_an_object_store(realm, store, clone, key_value, no_overwrite);
        if (maybe_key.is_error())
            return WebIDL::ExceptionOr<JS::Value>(maybe_key.release_error());

        auto optional_key = maybe_key.release_value();
        if (optional_key == nullptr)
            return WebIDL::ExceptionOr<JS::Value>(JS::js_undefined());

        return WebIDL::ExceptionOr<JS::Value>(convert_a_key_to_a_value(realm, GC::Ref(*optional_key)));
    });

    // 13. Return the result (an IDBRequest) of running asynchronously execute a request with handle and operation.
    return asynchronously_execute_a_request(realm, handle, operation);
}

// https://w3c.github.io/IndexedDB/#dom-idbobjectstore-count
WebIDL::ExceptionOr<GC::Ref<IDBRequest>> IDBObjectStore::count(Optional<JS::Value> query)
{
    // 1. Let transaction be this's transaction.
    auto transaction = this->transaction();

    // 2. Let store be this's object store.
    auto& store = *this;

    // FIXME: 3. If store has been deleted, throw an "InvalidStateError" DOMException.

    // 4. If transaction’s state is not active, then throw a "TransactionInactiveError" DOMException.
    if (transaction->state() != IDBTransaction::TransactionState::Active)
        return WebIDL::TransactionInactiveError::create(realm(), "Transaction is not active"_string);

    // 5. Let range be the result of converting a value to a key range with query. Rethrow any exceptions.
    auto range = TRY(convert_a_value_to_a_key_range(realm(), move(query)));

    // 6. Let operation be an algorithm to run count the records in a range with store and range.
    auto operation = GC::Function<WebIDL::ExceptionOr<JS::Value>()>::create(realm().heap(), [&store, range] {
        return count_the_records_in_a_range(store, range);
    });

    // 7. Return the result (an IDBRequest) of running asynchronously execute a request with this and operation.
    return asynchronously_execute_a_request(realm(), GC::Ref(*this), operation);
}

// https://w3c.github.io/IndexedDB/#dom-idbobjectstore-add
WebIDL::ExceptionOr<GC::Ref<IDBRequest>> IDBObjectStore::add(JS::Value value, Optional<JS::Value> const& key)
{
    // The add(value, key) method steps are to return the result of running add or put with this, value, key and the no-overwrite flag true.
    return add_or_put(*this, value, key, true);
}

// https://w3c.github.io/IndexedDB/#dom-idbobjectstore-put
WebIDL::ExceptionOr<GC::Ref<IDBRequest>> IDBObjectStore::put(JS::Value value, Optional<JS::Value> const& key)
{
    // The put(value, key) method steps are to return the result of running add or put with this, value, key and the no-overwrite flag false.
    return add_or_put(*this, value, key, false);
}

bool IDBObjectStore::has_record_with_key(GC::Ref<Key> key)
{
    auto index = m_records.find_if([&key](auto const& record) {
        return record.key == key;
    });

    return index != m_records.end();
}

void IDBObjectStore::remove_records_in_range(GC::Ref<IDBKeyRange> range)
{
    m_records.remove_all_matching([&](auto const& record) {
        return range->is_in_range(record.key);
    });
}

void IDBObjectStore::store_a_record(Record const& record)
{
    m_records.append(record);

    // The record is stored in the object store’s list of records such that the list is sorted according to the key of the records in ascending order.
    AK::quick_sort(m_records, [](auto const& a, auto const& b) {
        return a.key < b.key;
    });
}

u64 IDBObjectStore::count_records_in_range(GC::Ref<IDBKeyRange> range)
{
    u64 count = 0;
    for (auto const& record : m_records) {
        if (range->is_in_range(record.key))
            ++count;
    }

    return count;
}

}
