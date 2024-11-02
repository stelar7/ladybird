/*
 * Copyright (c) 2024, stelar7 <dudedbz@gmail.com>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <LibJS/Runtime/VM.h>
#include <LibWeb/DOM/EventDispatcher.h>
#include <LibWeb/HTML/EventNames.h>
#include <LibWeb/IndexedDB/IDBDatabase.h>
#include <LibWeb/IndexedDB/IDBRequest.h>
#include <LibWeb/IndexedDB/IDBTransaction.h>
#include <LibWeb/IndexedDB/IDBVersionChangeEvent.h>
#include <LibWeb/IndexedDB/Internal/Algorithms.h>
#include <LibWeb/IndexedDB/Internal/ConnectionQueueHandler.h>
#include <LibWeb/IndexedDB/Internal/Database.h>
#include <LibWeb/StorageAPI/StorageKey.h>

namespace Web::IndexedDB {

// https://w3c.github.io/IndexedDB/#open-a-database-connection
WebIDL::ExceptionOr<JS::NonnullGCPtr<IDBDatabase>> open_a_database_connection(JS::Realm& realm, StorageAPI::StorageKey storage_key, String name, Optional<u64> maybe_version, JS::NonnullGCPtr<IDBRequest> request)
{
    // 1. Let queue be the connection queue for storageKey and name.
    auto& queue = ConnectionQueueHandler::for_key_and_name(storage_key, name);

    // 2. Add request to queue.
    queue.append(request);

    // 3. Wait until all previous requests in queue have been processed.
    HTML::main_thread_event_loop().spin_until(JS::create_heap_function(realm.vm().heap(), [queue, request]() {
        dbgln("wait 1");
        return queue.all_previous_requests_processed(request);
    }));

    // 4. Let db be the database named name in storageKey, or null otherwise.
    JS::GCPtr<Database> db;
    auto maybe_db = Database::for_key_and_name(storage_key, name);
    if (maybe_db.has_value()) {
        db = maybe_db.value();
    }

    // 5. If version is undefined, let version be 1 if db is null, or db’s version otherwise.
    auto version = maybe_version.value_or(maybe_db.has_value() ? maybe_db.value()->version() : 1);

    // 6. If db is null, let db be a new database with name name, version 0 (zero), and with no object stores.
    // If this fails for any reason, return an appropriate error (e.g. a "QuotaExceededError" or "UnknownError" DOMException).
    if (!maybe_db.has_value()) {
        auto maybe_database = Database::create_for_key_and_name(realm, storage_key, name);

        if (maybe_database.is_error()) {
            return WebIDL::OperationError::create(realm, "Unable to create a new database"_string);
        }

        db = maybe_database.release_value();
    }

    // 7. If db’s version is greater than version, return a newly created "VersionError" DOMException and abort these steps.
    if (db->version() > version) {
        return WebIDL::VersionError::create(realm, "Database version is greater than the requested version"_string);
    }

    // 8. Let connection be a new connection to db.
    auto connection = IDBDatabase::create(realm, *db);

    // 9. Set connection’s version to version.
    connection->set_version(version);

    // 10. If db’s version is less than version, then:
    if (db->version() < version) {
        // 1. Let openConnections be the set of all connections, except connection, associated with db.
        auto open_connections = db->associated_connections_except(connection);

        // 2. For each entry of openConnections that does not have its close pending flag set to true,
        //    queue a task to fire a version change event named versionchange at entry with db’s version and version.
        u32 events_to_fire = open_connections.size();
        u32 events_fired = 0;
        for (auto& entry : open_connections) {
            if (!entry->close_pending()) {
                HTML::queue_a_task(HTML::Task::Source::DatabaseAccess, nullptr, nullptr, JS::create_heap_function(realm.vm().heap(), [&realm, entry, db, version, &events_fired]() {
                    fire_a_version_change_event(realm, HTML::EventNames::versionchange, *entry, db->version(), version);
                    events_fired++;
                }));
            } else {
                events_fired++;
            }
        }

        // 3. Wait for all of the events to be fired.
        HTML::main_thread_event_loop().spin_until(JS::create_heap_function(realm.vm().heap(), [&events_to_fire, &events_fired]() {
            dbgln("wait 2");
            return events_fired == events_to_fire;
        }));

        // 4. If any of the connections in openConnections are still not closed,
        //    queue a task to fire a version change event named blocked at request with db’s version and version.
        for (auto& entry : open_connections) {
            if (entry->state() != IDBDatabase::ConnectionState::Closed) {
                HTML::queue_a_task(HTML::Task::Source::DatabaseAccess, nullptr, nullptr, JS::create_heap_function(realm.vm().heap(), [&realm, entry, db, version]() {
                    fire_a_version_change_event(realm, HTML::EventNames::blocked, *entry, db->version(), version);
                }));
            }
        }

        // 5. Wait until all connections in openConnections are closed.
        HTML::main_thread_event_loop().spin_until(JS::create_heap_function(realm.vm().heap(), [open_connections]() {
            dbgln("wait 3");
            for (auto const& entry : open_connections) {
                if (entry->state() != IDBDatabase::ConnectionState::Closed) {
                    return false;
                }
            }

            return true;
        }));

        // 6. Run upgrade a database using connection, version and request.
        bool was_upgrade_aborted = upgrade_a_database(realm, connection, version, request);

        // 7. If connection was closed, return a newly created "AbortError" DOMException and abort these steps.
        if (connection->state() == IDBDatabase::ConnectionState::Closed)
            return WebIDL::AbortError::create(realm, "Connection was closed"_string);

        // 8. If the upgrade transaction was aborted, run the steps to close a database connection with connection,
        //    return a newly created "AbortError" DOMException and abort these steps.
        if (was_upgrade_aborted) {
            dbgln("Upgrade transaction was aborted");
            close_a_database_connection(*connection);
            return WebIDL::AbortError::create(realm, "Upgrade transaction was aborted"_string);
        } else {
            dbgln("no abort :c");
        }
    }

    // 11. Return connection.
    return connection;
}

bool fire_a_version_change_event(JS::Realm& realm, FlyString const& event_name, JS::NonnullGCPtr<DOM::EventTarget> target, u64 old_version, Optional<u64> new_version)
{
    IDBVersionChangeEventInit event_init = {};
    // 4. Set event’s oldVersion attribute to oldVersion.
    event_init.old_version = old_version;
    // 5. Set event’s newVersion attribute to newVersion.
    event_init.new_version = new_version;

    // 1. Let event be the result of creating an event using IDBVersionChangeEvent.
    // 2. Set event’s type attribute to e.
    auto event = IDBVersionChangeEvent::create(realm, event_name, event_init);

    // 3. Set event’s bubbles and cancelable attributes to false.
    event->set_bubbles(false);
    event->set_cancelable(false);

    // 6. Let legacyOutputDidListenersThrowFlag be false.
    auto legacy_output_did_listeners_throw_flag = false;

    // 7. Dispatch event at target with legacyOutputDidListenersThrowFlag.
    DOM::EventDispatcher::dispatch(target, *event, false, legacy_output_did_listeners_throw_flag);

    // 8. Return legacyOutputDidListenersThrowFlag.
    return legacy_output_did_listeners_throw_flag;
}

WebIDL::ExceptionOr<u64> delete_a_database(JS::Realm& realm, StorageAPI::StorageKey storage_key, String name, JS::NonnullGCPtr<IDBRequest> request)
{
    // 1. Let queue be the connection queue for storageKey and name.
    auto& queue = ConnectionQueueHandler::for_key_and_name(storage_key, name);

    // 2. Add request to queue.
    queue.append(request);

    // 3. Wait until all previous requests in queue have been processed.
    HTML::main_thread_event_loop().spin_until(JS::create_heap_function(realm.vm().heap(), [queue, request]() {
        dbgln("wait 4");
        return queue.all_previous_requests_processed(request);
    }));

    // 4. Let db be the database named name in storageKey, if one exists. Otherwise, return 0 (zero).
    auto maybe_db = Database::for_key_and_name(storage_key, name);
    if (!maybe_db.has_value())
        return 0;

    auto db = maybe_db.value();

    // 5. Let openConnections be the set of all connections associated with db.
    auto open_connections = db->associated_connections();

    // 6. For each entry of openConnections that does not have its close pending flag set to true,
    //    queue a task to fire a version change event named versionchange at entry with db’s version and null.
    u32 events_to_fire = open_connections.size();
    u32 events_fired = 0;
    for (auto const& entry : open_connections) {
        if (!entry->close_pending()) {
            HTML::queue_a_task(HTML::Task::Source::DatabaseAccess, nullptr, nullptr, JS::create_heap_function(realm.vm().heap(), [&realm, entry, db, &events_fired]() {
                fire_a_version_change_event(realm, HTML::EventNames::versionchange, *entry, db->version(), {});
                events_fired++;
            }));
        } else {
            events_fired++;
        }
    }

    // 7. Wait for all of the events to be fired.
    HTML::main_thread_event_loop().spin_until(JS::create_heap_function(realm.vm().heap(), [&events_to_fire, &events_fired]() {
        dbgln("wait 5");
        return events_fired == events_to_fire;
    }));

    // 8. If any of the connections in openConnections are still not closed,
    //    queue a task to fire a version change event named blocked at request with db’s version and null.
    for (auto const& entry : open_connections) {
        if (entry->state() != IDBDatabase::ConnectionState::Closed) {
            HTML::queue_a_task(HTML::Task::Source::DatabaseAccess, nullptr, nullptr, JS::create_heap_function(realm.vm().heap(), [&realm, entry, db]() {
                fire_a_version_change_event(realm, HTML::EventNames::blocked, *entry, db->version(), {});
            }));
        }
    }

    // 9. Wait until all connections in openConnections are closed.
    HTML::main_thread_event_loop().spin_until(JS::create_heap_function(realm.vm().heap(), [&]() {
        dbgln("wait 6");
        for (auto const& entry : open_connections) {
            if (entry->state() != IDBDatabase::ConnectionState::Closed) {
                return false;
            }
        }

        return true;
    }));

    // 10. Let version be db’s version.
    auto version = db->version();

    // 11. Delete db. If this fails for any reason, return an appropriate error (e.g. "QuotaExceededError" or "UnknownError" DOMException).
    auto maybe_deleted = Database::delete_for_key_and_name(storage_key, name);
    if (maybe_deleted.is_error())
        return WebIDL::OperationError::create(realm, "Unable to delete the database"_string);

    // 12. Return version.
    return version;
}

void close_a_database_connection(IDBDatabase& connection, bool forced)
{
    // 1. Set connection’s close pending flag to true.
    connection.set_close_pending(true);

    // FIXME: 2. If the forced flag is true, then for each transaction created using connection run abort a transaction with transaction and newly created "AbortError" DOMException.
    // FIXME: 3. Wait for all transactions created using connection to complete. Once they are complete, connection is closed.
    connection.set_state(IDBDatabase::ConnectionState::Closed);
    connection.disassociate();

    // 4. If the forced flag is true, then fire an event named close at connection.
    if (forced)
        connection.dispatch_event(DOM::Event::create(connection.realm(), HTML::EventNames::close));
}

bool upgrade_a_database(JS::Realm& realm, JS::NonnullGCPtr<IDBDatabase> connection, u64 version, JS::NonnullGCPtr<IDBRequest> request)
{
    // 1. Let db be connection’s database.
    auto db = connection->associated_database();

    // 2. Let transaction be a new upgrade transaction with connection used as connection.
    auto transaction = IDBTransaction::create(realm, connection, request);

    // NOTE: An upgrade transaction is automatically created when running the steps to upgrade a database.
    transaction->set_mode(Bindings::IDBTransactionMode::Versionchange);

    // FIXME: 3. Set transaction’s scope to connection’s object store set.

    // 4. Set db’s upgrade transaction to transaction.
    db->set_upgrade_transaction(transaction);

    // 5. Set transaction’s state to inactive.
    transaction->set_state(IDBTransaction::TransactionState::Inactive);

    // FIXME: 6. Start transaction.

    // 7. Let old version be db’s version.
    auto old_version = db->version();

    // 8. Set db’s version to version. This change is considered part of the transaction, and so if the transaction is aborted, this change is reverted.
    // FIXME: This should be done as part of the transactions change, not directly
    db->set_version(version);

    // 9. Set request’s processed flag to true.
    request->set_processed(true);

    bool is_finished = false;

    // 10. Queue a task to run these steps:
    HTML::queue_a_task(HTML::Task::Source::DatabaseAccess, nullptr, nullptr, JS::create_heap_function(realm.vm().heap(), [&realm, request, connection, transaction, old_version, version, &is_finished]() {
        // 1. Set request’s result to connection.
        request->set_result(connection);

        // 2. Set request’s transaction to transaction.
        request->set_transaction(transaction);

        // 3. Set request’s done flag to true.
        request->set_done(true);

        // 4. Set transaction’s state to active.
        transaction->set_state(IDBTransaction::TransactionState::Active);

        // 5. Let didThrow be the result of firing a version change event named upgradeneeded at request with old version and version.
        auto did_throw = fire_a_version_change_event(realm, HTML::EventNames::upgradeneeded, request, old_version, version);

        // 6. Set transaction’s state to inactive.
        transaction->set_state(IDBTransaction::TransactionState::Inactive);

        // 7. If didThrow is true, run abort a transaction with transaction and a newly created "AbortError" DOMException.
        if (did_throw)
            abort_a_transaction(realm, transaction, WebIDL::AbortError::create(realm, "An error occurred while firing the upgradeneeded event"_string));

        is_finished = true;
    }));

    // 11. Wait for transaction to finish.
    HTML::main_thread_event_loop().spin_until(JS::create_heap_function(realm.vm().heap(), [&is_finished]() {
        dbgln("wait 7");
        return is_finished;
    }));

    // NOTE: Not in spec, but we return the transactions abort state
    return transaction->was_aborted();
}

void abort_a_transaction(JS::Realm& realm, JS::NonnullGCPtr<IDBTransaction> transaction, JS::GCPtr<WebIDL::DOMException> error)
{
    // NOTE: This is not in the spec, but we manually mark the transaction as aborted here.
    transaction->set_aborted(true);

    dbgln("in aborting transaction");
    // FIXME: 1. All the changes made to the database by the transaction are reverted.
    //    For upgrade transactions this includes changes to the set of object stores and indexes, as well as the change to the version.
    //    Any object stores and indexes which were created during the transaction are now considered deleted for the purposes of other algorithms.

    // 2. If transaction is an upgrade transaction, run the steps to abort an upgrade transaction with transaction.
    if (transaction->is_upgrade_transaction()) {
        // FIXME: abort_an_upgrade_transaction(realm, transaction);
    }

    // 3. Set transaction’s state to finished.
    transaction->set_state(IDBTransaction::TransactionState::Finished);

    // 4. If error is not null, set transaction’s error to error.
    if (error)
        transaction->set_error(error);

    // FIXME: 5. For each request of transaction’s request list, abort the steps to asynchronously execute a request for request,
    //    set request’s processed flag to true, and queue a task to run these steps:

    // 6. Queue a task to run these steps:
    HTML::queue_a_task(HTML::Task::Source::DatabaseAccess, nullptr, nullptr, JS::create_heap_function(realm.vm().heap(), [&realm, transaction]() {
        // 1. If transaction is an upgrade transaction, then set transaction’s connection's associated database's upgrade transaction to null.
        if (transaction->is_upgrade_transaction() && transaction->connection()->associated_database())
            transaction->connection()->associated_database()->set_upgrade_transaction(nullptr);

        // 2. Fire an event named abort at transaction with its bubbles attribute initialized to true.
        transaction->dispatch_event(DOM::Event::create(realm, HTML::EventNames::abort, { .bubbles = true }));

        dbgln("is upgrade: {}", transaction->is_upgrade_transaction());

        // 3. If transaction is an upgrade transaction, then:
        if (transaction->is_upgrade_transaction()) {
            // 1. Let request be the open request associated with transaction.
            auto request = transaction->associated_request();

            // 2. Set request’s transaction to null.
            request->set_transaction(nullptr);

            // 3. Set request’s result to undefined.
            request->set_result(JS::js_undefined());

            // 4. Set request’s processed flag to false.
            request->set_processed(true);

            // 5. Set request’s done flag to false.
            request->set_done(false);
        }
    }));
}

}
