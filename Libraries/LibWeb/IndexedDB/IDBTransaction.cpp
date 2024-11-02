/*
 * Copyright (c) 2024, stelar7 <dudedbz@gmail.com>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <LibWeb/Bindings/Intrinsics.h>
#include <LibWeb/HTML/EventNames.h>
#include <LibWeb/IndexedDB/IDBTransaction.h>
#include <LibWeb/IndexedDB/Internal/Algorithms.h>

namespace Web::IndexedDB {

JS_DEFINE_ALLOCATOR(IDBTransaction);

IDBTransaction::~IDBTransaction() = default;

IDBTransaction::IDBTransaction(JS::Realm& realm, JS::NonnullGCPtr<IDBDatabase> database, JS::NonnullGCPtr<IDBRequest> request)
    : EventTarget(realm)
    , m_connection(database)
    , m_associated_request(request)
{
}

JS::NonnullGCPtr<IDBTransaction> IDBTransaction::create(JS::Realm& realm, JS::NonnullGCPtr<IDBDatabase> database, JS::NonnullGCPtr<IDBRequest> request)
{
    return realm.heap().allocate<IDBTransaction>(realm, realm, database, request);
}

void IDBTransaction::initialize(JS::Realm& realm)
{
    Base::initialize(realm);
    WEB_SET_PROTOTYPE_FOR_INTERFACE(IDBTransaction);
}

void IDBTransaction::visit_edges(Visitor& visitor)
{
    Base::visit_edges(visitor);
    visitor.visit(m_connection);
    visitor.visit(m_error);
    visitor.visit(m_associated_request);
}

// https://w3c.github.io/IndexedDB/#dom-idbtransaction-abort
WebIDL::ExceptionOr<void> IDBTransaction::abort()
{
    // 1. If this's state is committing or finished, then throw an "InvalidStateError" DOMException.
    if (m_state == TransactionState::Committing || m_state == TransactionState::Finished)
        return WebIDL::InvalidStateError::create(realm(), "Transaction is in an invalid state"_string);

    // 2. Set this's state to inactive and run abort a transaction with this and null.
    dbgln("manual abort call");
    set_state(TransactionState::Inactive);
    abort_a_transaction(realm(), *this, nullptr);

    return {};
}

void IDBTransaction::set_onabort(WebIDL::CallbackType* event_handler)
{
    set_event_handler_attribute(HTML::EventNames::abort, event_handler);
}

WebIDL::CallbackType* IDBTransaction::onabort()
{
    return event_handler_attribute(HTML::EventNames::abort);
}

void IDBTransaction::set_oncomplete(WebIDL::CallbackType* event_handler)
{
    set_event_handler_attribute(HTML::EventNames::complete, event_handler);
}

WebIDL::CallbackType* IDBTransaction::oncomplete()
{
    return event_handler_attribute(HTML::EventNames::complete);
}

void IDBTransaction::set_onerror(WebIDL::CallbackType* event_handler)
{
    set_event_handler_attribute(HTML::EventNames::error, event_handler);
}

WebIDL::CallbackType* IDBTransaction::onerror()
{
    return event_handler_attribute(HTML::EventNames::error);
}

}
