/*
 * Copyright (c) 2024, stelar7 <dudedbz@gmail.com>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <AK/Vector.h>
#include <LibGC/Ptr.h>
#include <LibWeb/Bindings/IDBObjectStorePrototype.h>
#include <LibWeb/Bindings/Intrinsics.h>
#include <LibWeb/HTML/EventNames.h>
#include <LibWeb/IndexedDB/IDBIndex.h>
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

}
