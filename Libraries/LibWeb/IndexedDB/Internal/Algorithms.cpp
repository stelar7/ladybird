/*
 * Copyright (c) 2024, stelar7 <dudedbz@gmail.com>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <AK/Assertions.h>
#include <AK/Error.h>
#include <AK/QuickSort.h>
#include <LibJS/Runtime/AbstractOperations.h>
#include <LibJS/Runtime/Array.h>
#include <LibJS/Runtime/ArrayBuffer.h>
#include <LibJS/Runtime/DataView.h>
#include <LibJS/Runtime/Date.h>
#include <LibJS/Runtime/Object.h>
#include <LibJS/Runtime/PrimitiveString.h>
#include <LibJS/Runtime/PropertyKey.h>
#include <LibJS/Runtime/Realm.h>
#include <LibJS/Runtime/TypedArray.h>
#include <LibJS/Runtime/VM.h>
#include <LibJS/Runtime/Value.h>
#include <LibWeb/DOM/EventDispatcher.h>
#include <LibWeb/FileAPI/Blob.h>
#include <LibWeb/FileAPI/File.h>
#include <LibWeb/HTML/EventNames.h>
#include <LibWeb/HTML/StructuredSerialize.h>
#include <LibWeb/IndexedDB/IDBDatabase.h>
#include <LibWeb/IndexedDB/IDBRequest.h>
#include <LibWeb/IndexedDB/IDBTransaction.h>
#include <LibWeb/IndexedDB/IDBVersionChangeEvent.h>
#include <LibWeb/IndexedDB/Internal/Algorithms.h>
#include <LibWeb/IndexedDB/Internal/ConnectionQueueHandler.h>
#include <LibWeb/IndexedDB/Internal/Database.h>
#include <LibWeb/IndexedDB/Internal/Key.h>
#include <LibWeb/Infra/Strings.h>
#include <LibWeb/StorageAPI/StorageKey.h>
#include <LibWeb/WebIDL/AbstractOperations.h>
#include <LibWeb/WebIDL/Buffers.h>

namespace Web::IndexedDB {

// https://w3c.github.io/IndexedDB/#open-a-database-connection
WebIDL::ExceptionOr<GC::Ref<IDBDatabase>> open_a_database_connection(JS::Realm& realm, StorageAPI::StorageKey storage_key, String name, Optional<u64> maybe_version, GC::Ref<IDBRequest> request)
{
    // 1. Let queue be the connection queue for storageKey and name.
    auto& queue = ConnectionQueueHandler::for_key_and_name(storage_key, name);

    // 2. Add request to queue.
    queue.append(request);

    // 3. Wait until all previous requests in queue have been processed.
    HTML::main_thread_event_loop().spin_until(GC::create_function(realm.vm().heap(), [queue, request]() {
        return queue.all_previous_requests_processed(request);
    }));

    // 4. Let db be the database named name in storageKey, or null otherwise.
    GC::Ptr<Database> db;
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
        IGNORE_USE_IN_ESCAPING_LAMBDA u32 events_to_fire = open_connections.size();
        IGNORE_USE_IN_ESCAPING_LAMBDA u32 events_fired = 0;
        for (auto const& entry : open_connections) {
            if (!entry->close_pending()) {
                HTML::queue_a_task(HTML::Task::Source::DatabaseAccess, nullptr, nullptr, GC::create_function(realm.vm().heap(), [&realm, entry, db, version, &events_fired]() {
                    fire_a_version_change_event(realm, HTML::EventNames::versionchange, *entry, db->version(), version);
                    events_fired++;
                }));
            } else {
                events_fired++;
            }
        }

        // 3. Wait for all of the events to be fired.
        HTML::main_thread_event_loop().spin_until(GC::create_function(realm.vm().heap(), [&events_to_fire, &events_fired]() {
            return events_fired == events_to_fire;
        }));

        // 4. If any of the connections in openConnections are still not closed,
        //    queue a task to fire a version change event named blocked at request with db’s version and version.
        for (auto const& entry : open_connections) {
            if (entry->state() != IDBDatabase::ConnectionState::Closed) {
                HTML::queue_a_task(HTML::Task::Source::DatabaseAccess, nullptr, nullptr, GC::create_function(realm.vm().heap(), [&realm, entry, db, version]() {
                    fire_a_version_change_event(realm, HTML::EventNames::blocked, *entry, db->version(), version);
                }));
            }
        }

        // 5. Wait until all connections in openConnections are closed.
        HTML::main_thread_event_loop().spin_until(GC::create_function(realm.vm().heap(), [open_connections]() {
            for (auto const& entry : open_connections) {
                if (entry->state() != IDBDatabase::ConnectionState::Closed) {
                    return false;
                }
            }

            return true;
        }));

        // 6. Run upgrade a database using connection, version and request.
        upgrade_a_database(realm, connection, version, request);

        // 7. If connection was closed, return a newly created "AbortError" DOMException and abort these steps.
        if (connection->state() == IDBDatabase::ConnectionState::Closed) {
            return WebIDL::AbortError::create(realm, "Connection was closed"_string);
        }

        // 8. If the upgrade transaction was aborted, run the steps to close a database connection with connection,
        //    return a newly created "AbortError" DOMException and abort these steps.
        auto transaction = connection->associated_database()->upgrade_transaction();
        if (transaction->aborted()) {
            close_a_database_connection(*connection, true);
            return WebIDL::AbortError::create(realm, "Upgrade transaction was aborted"_string);
        }
    }

    // 11. Return connection.
    return connection;
}

bool fire_a_version_change_event(JS::Realm& realm, FlyString const& event_name, GC::Ref<DOM::EventTarget> target, u64 old_version, Optional<u64> new_version)
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

// https://w3c.github.io/IndexedDB/#convert-value-to-key
ErrorOr<GC::Ref<Key>> convert_a_value_to_a_key(JS::Realm& realm, JS::Value input, Vector<JS::Value> seen)
{
    // 1. If seen was not given, then let seen be a new empty set.
    // NOTE: This is handled by the caller.

    // 2. If seen contains input, then return invalid.
    if (seen.contains_slow(input))
        return Error::from_string_literal("Already seen key");

    // 3. Jump to the appropriate step below:

    // - If Type(input) is Number
    if (input.is_number()) {

        // 1. If input is NaN then return invalid.
        if (input.is_nan())
            return Error::from_string_literal("NaN key");

        // 2. Otherwise, return a new key with type number and value input.
        return Key::create_number(realm, input.as_double());
    }

    // - If input is a Date (has a [[DateValue]] internal slot)
    if (input.is_object() && is<JS::Date>(input.as_object())) {

        // 1. Let ms be the value of input’s [[DateValue]] internal slot.
        auto& date = static_cast<JS::Date&>(input.as_object());
        auto ms = date.date_value();

        // 2. If ms is NaN then return invalid.
        if (isnan(ms))
            return Error::from_string_literal("NaN key");

        // 3. Otherwise, return a new key with type date and value ms.
        return Key::create_date(realm, ms);
    }

    // - If Type(input) is String
    if (input.is_string()) {

        // 1. Return a new key with type string and value input.
        return Key::create_string(realm, input.as_string().utf8_string());
    }

    // - If input is a buffer source type
    if (input.is_object() && (is<JS::TypedArrayBase>(input.as_object()) || is<JS::ArrayBuffer>(input.as_object()) || is<JS::DataView>(input.as_object()))) {

        // 1. If input is [detached] then return invalid.
        if (WebIDL::is_buffer_source_detached(input))
            return Error::from_string_literal("Detached buffer is not supported as key");

        // 2. Let bytes be the result of getting a copy of the bytes held by the buffer source input.
        auto data_buffer = TRY(WebIDL::get_buffer_source_copy(input.as_object()));

        // 3. Return a new key with type binary and value bytes.
        return Key::create_binary(realm, data_buffer);
    }

    // - If input is an Array exotic object
    if (input.is_object() && is<JS::Array>(input.as_object())) {

        // 1. Let len be ? ToLength( ? Get(input, "length")).
        auto maybe_length = length_of_array_like(realm.vm(), input.as_object());
        if (maybe_length.is_error())
            return Error::from_string_literal("Failed to get length of array-like object");

        auto length = maybe_length.release_value();

        // 2. Append input to seen.
        seen.append(input);

        // 3. Let keys be a new empty list.
        Vector<GC::Root<Key>> keys;

        // 4. Let index be 0.
        u64 index = 0;

        // 5. While index is less than len:
        while (index < length) {
            // 1. Let hop be ? HasOwnProperty(input, index).
            auto maybe_hop = input.as_object().has_own_property(index);
            if (maybe_hop.is_error())
                return Error::from_string_literal("Failed to check if array-like object has property");

            auto hop = maybe_hop.release_value();

            // 2. If hop is false, return invalid.
            if (!hop)
                return Error::from_string_literal("Array-like object has no property");

            // 3. Let entry be ? Get(input, index).
            auto maybe_entry = input.as_object().get(index);
            if (maybe_entry.is_error())
                return Error::from_string_literal("Failed to get property of array-like object");

            // 4. Let key be the result of converting a value to a key with arguments entry and seen.
            auto maybe_key = convert_a_value_to_a_key(realm, maybe_entry.release_value(), seen);

            // 5. ReturnIfAbrupt(key).
            // 6. If key is invalid abort these steps and return invalid.
            if (maybe_key.is_error())
                return maybe_key.release_error();

            auto key = maybe_key.release_value();

            // 7. Append key to keys.
            keys.append(key);

            // 8. Increase index by 1.
            index++;
        }

        // 6. Return a new array key with value keys.
        return Key::create_array(realm, keys);
    }

    // - Otherwise
    // 1. Return invalid.
    return Error::from_string_literal("Unknown key type");
}

// https://w3c.github.io/IndexedDB/#close-a-database-connection
void close_a_database_connection(IDBDatabase& connection, bool forced)
{
    // 1. Set connection’s close pending flag to true.
    connection.set_close_pending(true);

    // FIXME: 2. If the forced flag is true, then for each transaction created using connection run abort a transaction with transaction and newly created "AbortError" DOMException.
    // FIXME: 3. Wait for all transactions created using connection to complete. Once they are complete, connection is closed.
    connection.set_state(IDBDatabase::ConnectionState::Closed);

    // 4. If the forced flag is true, then fire an event named close at connection.
    if (forced)
        connection.dispatch_event(DOM::Event::create(connection.realm(), HTML::EventNames::close));
}

// https://w3c.github.io/IndexedDB/#upgrade-a-database
void upgrade_a_database(JS::Realm& realm, GC::Ref<IDBDatabase> connection, u64 version, GC::Ref<IDBRequest> request)
{
    // 1. Let db be connection’s database.
    auto db = connection->associated_database();

    // 2. Let transaction be a new upgrade transaction with connection used as connection.
    auto transaction = IDBTransaction::create(realm, connection);

    // 3. Set transaction’s scope to connection’s object store set.
    for (auto const& object_store : connection->object_store_set())
        transaction->add_to_scope(object_store);

    // 4. Set db’s upgrade transaction to transaction.
    db->set_upgrade_transaction(transaction);

    // 5. Set transaction’s state to inactive.
    transaction->set_state(IDBTransaction::TransactionState::Inactive);

    // FIXME: 6. Start transaction.

    // 7. Let old version be db’s version.
    auto old_version = db->version();

    // 8. Set db’s version to version. This change is considered part of the transaction, and so if the transaction is aborted, this change is reverted.
    db->set_version(version);

    // 9. Set request’s processed flag to true.
    request->set_processed(true);

    // 10. Queue a task to run these steps:
    IGNORE_USE_IN_ESCAPING_LAMBDA bool wait_for_transaction = true;
    HTML::queue_a_task(HTML::Task::Source::DatabaseAccess, nullptr, nullptr, GC::create_function(realm.vm().heap(), [&realm, request, connection, transaction, old_version, version, &wait_for_transaction]() {
        // 1. Set request’s result to connection.
        request->set_result(connection);

        // 2. Set request’s transaction to transaction.
        // NOTE: We need to do a two-way binding here.
        request->set_transaction(transaction);
        transaction->set_associated_request(request);

        // 3. Set request’s done flag to true.
        request->set_done(true);

        // 4. Set transaction’s state to active.
        transaction->set_state(IDBTransaction::TransactionState::Active);

        // 5. Let didThrow be the result of firing a version change event named upgradeneeded at request with old version and version.
        [[maybe_unused]] auto did_throw = fire_a_version_change_event(realm, HTML::EventNames::upgradeneeded, request, old_version, version);

        // 6. Set transaction’s state to inactive.
        transaction->set_state(IDBTransaction::TransactionState::Inactive);

        // FIXME: 7. If didThrow is true, run abort a transaction with transaction and a newly created "AbortError" DOMException.

        wait_for_transaction = false;
    }));

    // 11. Wait for transaction to finish.
    HTML::main_thread_event_loop().spin_until(GC::create_function(realm.vm().heap(), [&wait_for_transaction]() {
        return !wait_for_transaction;
    }));
}

// https://w3c.github.io/IndexedDB/#deleting-a-database
WebIDL::ExceptionOr<u64> delete_a_database(JS::Realm& realm, StorageAPI::StorageKey storage_key, String name, GC::Ref<IDBRequest> request)
{
    // 1. Let queue be the connection queue for storageKey and name.
    auto& queue = ConnectionQueueHandler::for_key_and_name(storage_key, name);

    // 2. Add request to queue.
    queue.append(request);

    // 3. Wait until all previous requests in queue have been processed.
    HTML::main_thread_event_loop().spin_until(GC::create_function(realm.vm().heap(), [queue, request]() {
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
    IGNORE_USE_IN_ESCAPING_LAMBDA u32 events_to_fire = open_connections.size();
    IGNORE_USE_IN_ESCAPING_LAMBDA u32 events_fired = 0;
    for (auto const& entry : open_connections) {
        if (!entry->close_pending()) {
            HTML::queue_a_task(HTML::Task::Source::DatabaseAccess, nullptr, nullptr, GC::create_function(realm.vm().heap(), [&realm, entry, db, &events_fired]() {
                fire_a_version_change_event(realm, HTML::EventNames::versionchange, *entry, db->version(), {});
                events_fired++;
            }));
        } else {
            events_fired++;
        }
    }

    // 7. Wait for all of the events to be fired.
    HTML::main_thread_event_loop().spin_until(GC::create_function(realm.vm().heap(), [&events_to_fire, &events_fired]() {
        return events_fired == events_to_fire;
    }));

    // 8. If any of the connections in openConnections are still not closed, queue a task to fire a version change event named blocked at request with db’s version and null.
    for (auto const& entry : open_connections) {
        if (entry->state() != IDBDatabase::ConnectionState::Closed) {
            HTML::queue_a_task(HTML::Task::Source::DatabaseAccess, nullptr, nullptr, GC::create_function(realm.vm().heap(), [&realm, entry, db]() {
                fire_a_version_change_event(realm, HTML::EventNames::blocked, *entry, db->version(), {});
            }));
        }
    }

    // 9. Wait until all connections in openConnections are closed.
    HTML::main_thread_event_loop().spin_until(GC::create_function(realm.vm().heap(), [open_connections]() {
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
        return WebIDL::OperationError::create(realm, "Unable to delete database"_string);

    // 12. Return version.
    return version;
}

// https://w3c.github.io/IndexedDB/#abort-a-transaction
void abort_a_transaction(IDBTransaction& transaction, GC::Ptr<WebIDL::DOMException> error)
{
    // NOTE: This is not spec'ed anywhere, but we need to know IF the transaction was aborted.
    transaction.set_aborted(true);

    // FIXME: 1. All the changes made to the database by the transaction are reverted.
    // For upgrade transactions this includes changes to the set of object stores and indexes, as well as the change to the version.
    // Any object stores and indexes which were created during the transaction are now considered deleted for the purposes of other algorithms.

    // FIXME: 2. If transaction is an upgrade transaction, run the steps to abort an upgrade transaction with transaction.
    // if (transaction.is_upgrade_transaction())
    //     abort_an_upgrade_transaction(transaction);

    // 3. Set transaction’s state to finished.
    transaction.set_state(IDBTransaction::TransactionState::Finished);

    // 4. If error is not null, set transaction’s error to error.
    if (error)
        transaction.set_error(error);

    // FIXME: 5. For each request of transaction’s request list, abort the steps to asynchronously execute a request for request,
    //           set request’s processed flag to true, and queue a task to run these steps:
    // FIXME: 1. Set request’s done flag to true.
    // FIXME: 2. Set request’s result to undefined.
    // FIXME: 3. Set request’s error to a newly created "AbortError" DOMException.
    // FIXME: 4. Fire an event named error at request with its bubbles and cancelable attributes initialized to true.

    // 6. Queue a task to run these steps:
    HTML::queue_a_task(HTML::Task::Source::DatabaseAccess, nullptr, nullptr, GC::create_function(transaction.realm().vm().heap(), [&transaction]() {
        // 1. If transaction is an upgrade transaction, then set transaction’s connection's associated database's upgrade transaction to null.
        if (transaction.is_upgrade_transaction())
            transaction.connection()->associated_database()->set_upgrade_transaction(nullptr);

        // 2. Fire an event named abort at transaction with its bubbles attribute initialized to true.
        transaction.dispatch_event(DOM::Event::create(transaction.realm(), HTML::EventNames::abort, { .bubbles = true }));

        // 3. If transaction is an upgrade transaction, then:
        if (transaction.is_upgrade_transaction()) {
            // 1. Let request be the open request associated with transaction.
            auto request = transaction.associated_request();

            // 2. Set request’s transaction to null.
            // NOTE: Clear the two-way binding.
            request->set_transaction(nullptr);
            transaction.set_associated_request(nullptr);

            // 3. Set request’s result to undefined.
            request->set_result(JS::js_undefined());

            // 4. Set request’s processed flag to false.
            request->set_processed(false);

            // 5. Set request’s done flag to false.
            request->set_done(false);
        }
    }));
}

// https://w3c.github.io/IndexedDB/#convert-a-key-to-a-value
JS::Value convert_a_key_to_a_value(JS::Realm& realm, GC::Ref<Key> key)
{
    // 1. Let type be key’s type.
    auto type = key->type();

    // 2. Let value be key’s value.
    auto value = key->value();

    // 3. Switch on type:
    switch (type) {
    case Key::KeyType::Number: {
        // Return an ECMAScript Number value equal to value
        return JS::Value(key->value_as_double());
    }

    case Key::KeyType::String: {
        // Return an ECMAScript String value equal to value
        return JS::PrimitiveString::create(realm.vm(), key->value_as_string());
    }

    case Key::KeyType::Date: {
        // 1. Let date be the result of executing the ECMAScript Date constructor with the single argument value.
        auto date = JS::Date::create(realm, key->value_as_double());

        // 2. Assert: date is not an abrupt completion.
        // NOTE: This is not possible in our implementation.

        // 3. Return date.
        return date;
    }

    case Key::KeyType::Binary: {
        auto buffer = key->value_as_byte_buffer();

        // 1. Let len be value’s length.
        auto len = buffer.size();

        // 2. Let buffer be the result of executing the ECMAScript ArrayBuffer constructor with len.
        // 3. Assert: buffer is not an abrupt completion.
        auto array_buffer = MUST(JS::ArrayBuffer::create(realm, len));

        // 4. Set the entries in buffer’s [[ArrayBufferData]] internal slot to the entries in value.
        buffer.span().copy_to(array_buffer->buffer());

        // 5. Return buffer.
        return array_buffer;
    }

    case Key::KeyType::Array: {
        auto data = key->value_as_vector();

        // 1. Let array be the result of executing the ECMAScript Array constructor with no arguments.
        // 2. Assert: array is not an abrupt completion.
        auto array = MUST(JS::Array::create(realm, 0));

        // 3. Let len be value’s size.
        auto len = data.size();

        // 4. Let index be 0.
        u64 index = 0;

        // 5. While index is less than len:
        while (index < len) {
            // 1. Let entry be the result of converting a key to a value with value[index].
            auto entry = convert_a_key_to_a_value(realm, *data[index]);

            // 2. Let status be CreateDataProperty(array, index, entry).
            auto status = MUST(array->create_data_property(index, entry));

            // 3. Assert: status is true.
            VERIFY(status);

            // 4. Increase index by 1.
            index++;
        }

        // 6. Return array.
        return array;
    }
    }

    VERIFY_NOT_REACHED();
}

// https://w3c.github.io/IndexedDB/#valid-key-path
bool is_valid_key_path(KeyPath path)
{
    // A valid key path is one of:
    return path.visit(
        [](String const& value) -> bool {
            // * An empty string.
            if (value.is_empty())
                return true;

            // FIXME: * An identifier, which is a string matching the IdentifierName production from the ECMAScript Language Specification [ECMA-262].
            return true;

            // FIXME: * A string consisting of two or more identifiers separated by periods (U+002E FULL STOP).
            return true;

            return false;
        },
        [](Vector<String> const& values) -> bool {
            // * A non-empty list containing only strings conforming to the above requirements.
            if (values.is_empty())
                return false;

            for (auto const& value : values) {
                if (!is_valid_key_path(value))
                    return false;
            }

            return true;
        });
}

// https://w3c.github.io/IndexedDB/#create-a-sorted-name-list
GC::Ref<HTML::DOMStringList> create_a_sorted_name_list(JS::Realm& realm, Vector<String> names)
{
    // 1. Let sorted be names sorted in ascending order with the code unit less than algorithm.
    quick_sort(names, [](auto const& a, auto const& b) {
        return Infra::code_unit_less_than(a, b);
    });

    // 2. Return a new DOMStringList associated with sorted.
    return HTML::DOMStringList::create(realm, names);
}

// https://w3c.github.io/IndexedDB/#clone
WebIDL::ExceptionOr<JS::Value> clone_in_realm(JS::Realm& target_realm, JS::Value value, GC::Ref<IDBTransaction> transaction)
{
    // 1. Assert: transaction’s state is active.
    VERIFY(transaction->state() == IDBTransaction::TransactionState::Active);

    // 2. Set transaction’s state to inactive.
    transaction->set_state(IDBTransaction::TransactionState::Inactive);

    // 3. Let serialized be ? StructuredSerializeForStorage(value).
    auto serialized = TRY(HTML::structured_serialize_for_storage(target_realm.vm(), value));

    // 4. Let clone be ? StructuredDeserialize(serialized, targetRealm).
    auto clone = TRY(HTML::structured_deserialize(target_realm.vm(), serialized, target_realm));

    // 5. Set transaction’s state to active.
    transaction->set_state(IDBTransaction::TransactionState::Active);

    // 6. Return clone.
    return clone;
}

// https://w3c.github.io/IndexedDB/#extract-a-key-from-a-value-using-a-key-path
WebIDL::ExceptionOr<ErrorOr<Key>> extract_a_key_from_a_value_using_a_key_path(JS::Realm& realm, JS::Value value, KeyPath key_path, bool multi_entry)
{
    // 1. Let r be the result of evaluating a key path on a value with value and keyPath. Rethrow any exceptions.
    // 2. If r is failure, return failure.
    auto r = TRY(TRY(evaluate_key_path_on_a_value(realm, value, key_path)));

    // 3. Let key be the result of converting a value to a key with r if the multiEntry flag is false,
    //    and the result of converting a value to a multiEntry key with r otherwise. Rethrow any exceptions.
    // 4. If key is invalid, return invalid.
    Optional<Key> key;
    if (multi_entry) {
        key = TRY(convert_a_value_to_a_multi_entry_key(realm, r));
    } else {
        key = TRY(convert_a_value_to_a_key(realm, r));
    }

    // 5. Return key.
    return key.value();
}

// https://w3c.github.io/IndexedDB/#evaluate-a-key-path-on-a-value
WebIDL::ExceptionOr<ErrorOr<JS::Value>> evaluate_key_path_on_a_value(JS::Realm& realm, JS::Value value, KeyPath key_path)
{
    // 1. If keyPath is a list of strings, then:
    if (key_path.has<Vector<String>>()) {
        auto& key_path_list = key_path.get<Vector<String>>();

        // 1. Let result be a new Array object created as if by the expression [].
        auto result = MUST(JS::Array::create(realm, 0));

        // 2. Let i be 0.
        u64 i = 0;

        // 3. For each item of keyPath:
        for (auto const& item : key_path_list) {
            // 1. Let key be the result of recursively evaluating a key path on a value with item and value.
            // 2. Assert: key is not an abrupt completion.
            // 3. If key is failure, abort the overall algorithm and return failure.
            auto key = TRY(TRY(evaluate_key_path_on_a_value(realm, value, item)));

            // 4. Let p be ! ToString(i).
            auto p = JS::PropertyKey { i };

            // 5. Let status be CreateDataProperty(result, p, key).
            auto status = result->create_data_property(p, key);

            // 6. Assert: status is true.
            MUST(status);

            // 7. Increase i by 1.
            i++;
        }

        // 4. Return result.
        return result;
    }

    auto key_path_string = key_path.get<String>();

    // 2. If keyPath is the empty string, return value and skip the remaining steps.
    if (key_path_string.is_empty())
        return value;

    // 3. Let identifiers be the result of strictly splitting keyPath on U+002E FULL STOP characters (.).
    auto identifiers = MUST(key_path_string.split('.'));

    // 4. For each identifier of identifiers, jump to the appropriate step below:

    for (auto const& identifier : identifiers) {
        // If Type(value) is String, and identifier is "length"
        if (value.is_string() && identifier == "length") {
            // Let value be a Number equal to the number of elements in value.
            value = JS::Value(value.as_string().utf16_string_view().length_in_code_units());
        }

        // If value is an Array and identifier is "length"
        else if (value.is_object() && is<JS::Array>(value.as_object()) && identifier == "length") {
            // Let value be ! ToLength(! Get(value, "length")).
            value = JS::Value(MUST(length_of_array_like(realm.vm(), value.as_object())));
        }

        // If value is a Blob and identifier is "size"
        else if (value.is_object() && is<FileAPI::Blob>(value.as_object()) && identifier == "size") {
            // Let value be value’s size.
            value = JS::Value(static_cast<FileAPI::Blob&>(value.as_object()).size());
        }

        // If value is a Blob and identifier is "type"
        else if (value.is_object() && is<FileAPI::Blob>(value.as_object()) && identifier == "type") {
            // Let value be a String equal to value’s type.
            value = JS::PrimitiveString::create(realm.vm(), static_cast<FileAPI::Blob&>(value.as_object()).type());
        }

        // If value is a File and identifier is "name"
        else if (value.is_object() && is<FileAPI::File>(value.as_object()) && identifier == "name") {
            // Let value be a String equal to value’s name.
            value = JS::PrimitiveString::create(realm.vm(), static_cast<FileAPI::File&>(value.as_object()).name());
        }

        // If value is a File and identifier is "lastModified"
        else if (value.is_object() && is<FileAPI::File>(value.as_object()) && identifier == "lastModified") {
            // Let value be a Number equal to value’s lastModified.
            value = JS::Value(static_cast<double>(static_cast<FileAPI::File&>(value.as_object()).last_modified()));
        }

        // Otherwise
        else {
            // 1. If Type(value) is not Object, return failure.
            if (!value.is_object())
                return Error::from_string_literal("Value is not an object");

            // 2. Let hop be ! HasOwnProperty(value, identifier).
            auto hop = MUST(value.as_object().has_own_property(identifier.to_byte_string()));

            // 3. If hop is false, return failure.
            if (!hop)
                return Error::from_string_literal("Property does not exist");

            // 4. Let value be ! Get(value, identifier).
            value = MUST(value.as_object().get(identifier.to_byte_string()));

            // 5. If value is undefined, return failure.
            if (value.is_undefined())
                return Error::from_string_literal("Value is undefined");
        }
    }

    // 5. Assert: value is not an abrupt completion.
    // NOTE: This is not possible in our implementation.

    // 6. Return value.
    return value;
}

// https://w3c.github.io/IndexedDB/#convert-a-value-to-a-multientry-key
ErrorOr<Key> convert_a_value_to_a_multi_entry_key(JS::Realm& realm, JS::Value value)
{
    // 1. If input is an Array exotic object, then:
    if (value.is_object() && is<JS::Array>(value.as_object())) {
        // 1. Let len be ? ToLength( ? Get(input, "length")).
        auto maybe_length = length_of_array_like(realm.vm(), value.as_object());
        if (maybe_length.is_error())
            return Error::from_string_literal("Failed to get length of array-like object");

        auto length = maybe_length.release_value();

        // 2. Let seen be a new set containing only input.
        Vector<JS::Value> seen { value };

        // 3. Let keys be a new empty list.
        Vector<Key> keys;

        // 4. Let index be 0.
        u64 index = 0;

        // 5. While index is less than len:
        while (index < length) {
            // 1. Let entry be Get(input, index).
            auto maybe_entry = value.as_object().get(index);

            // 2. If entry is not an abrupt completion, then:
            if (!maybe_entry.is_error()) {
                // 1. Let key be the result of converting a value to a key with arguments entry and seen.
                auto maybe_key = convert_a_value_to_a_key(realm, maybe_entry.release_value(), seen);

                // 2. If key is not invalid or an abrupt completion, and there is no item in keys equal to key, then append key to keys.
                if (!maybe_key.is_error()) {
                    auto key = maybe_key.release_value();
                    if (!keys.contains_slow(key))
                        keys.append(key);
                }
            }

            // 3. Increase index by 1.
            index++;
        }

        // 6. Return a new array key with value set to keys.
        return Key::create_array(keys);
    }

    // 2. Otherwise, return the result of converting a value to a key with argument input. Rethrow any exceptions.
    return convert_a_value_to_a_key(realm, value);
}

// https://w3c.github.io/IndexedDB/#check-that-a-key-could-be-injected-into-a-value
bool check_that_a_key_could_be_injected_into_a_value(JS::Realm& realm, JS::Value value, KeyPath key_path)
{
    // NOTE: The key paths used in this section are always strings and never sequences

    // 1. Let identifiers be the result of strictly splitting keyPath on U+002E FULL STOP characters (.).
    auto identifiers = MUST(key_path.get<String>().split('.'));

    // 2. Assert: identifiers is not empty.
    VERIFY(!identifiers.is_empty());

    // 3. Remove the last item of identifiers.
    identifiers.take_last();

    // 4. For each remaining identifier of identifiers, if any:
    for (auto const& identifier : identifiers) {
        // 1. If value is not an Object or an Array, return false.
        if (!(value.is_object() || MUST(value.is_array(realm.vm()))))
            return false;

        // 2. Let hop be ! HasOwnProperty(value, identifier).
        auto hop = MUST(value.as_object().has_own_property(identifier.to_byte_string()));

        // 3. If hop is false, return true.
        if (!hop)
            return true;

        // 4. Let value be ! Get(value, identifier).
        value = MUST(value.as_object().get(identifier.to_byte_string()));
    }

    // 5. Return true if value is an Object or an Array, or false otherwise.
    return value.is_object() || MUST(value.is_array(realm.vm()));
}

// https://w3c.github.io/IndexedDB/#generate-a-key
WebIDL::ExceptionOr<u64> generate_a_key(GC::Ref<IDBObjectStore> store)
{
    // 1. Let generator be store’s key generator.
    auto generator = store->key_generator().value();

    // 2. Let key be generator’s current number.
    auto key = generator.current_number();

    // 3. If key is greater than 2^53 (9007199254740992), then return failure.
    if (key > 9007199254740992)
        return WebIDL::ConstraintError::create(store->realm(), "Key is greater than 2^53"_string);

    // 4. Increase generator’s current number by 1.
    generator.increment(1);

    // 5. Return key.
    return key;
}

// https://w3c.github.io/IndexedDB/#inject-a-key-into-a-value-using-a-key-path
void inject_a_key_into_a_value_using_a_key_path(JS::Realm& realm, JS::Value value, Key key, KeyPath key_path)
{
    // 1. Let identifiers be the result of strictly splitting keyPath on U+002E FULL STOP characters (.).
    auto identifiers = MUST(key_path.get<String>().split('.'));

    // 2. Assert: identifiers is not empty.
    VERIFY(!identifiers.is_empty());

    // 3. Let last be the last item of identifiers and remove it from the list.
    auto last = identifiers.take_last();

    // 4. For each remaining identifier of identifiers:
    for (auto const& identifier : identifiers) {
        // 1. Assert: value is an Object or an Array.
        VERIFY(value.is_object() || MUST(value.is_array(realm.vm())));

        // 2. Let hop be ! HasOwnProperty(value, identifier).
        auto hop = MUST(value.as_object().has_own_property(identifier.to_byte_string()));

        // 3. If hop is false, then:
        if (!hop) {
            // 1. Let o be a new Object created as if by the expression ({}).
            auto o = JS::Object::create(realm, realm.intrinsics().object_prototype());

            // 2. Let status be CreateDataProperty(value, identifier, o).
            auto status = MUST(value.as_object().create_data_property(identifier.to_byte_string(), o));

            // 3. Assert: status is true.
            VERIFY(status);
        }

        // 4. Let value be ! Get(value, identifier).
        value = MUST(value.as_object().get(identifier.to_byte_string()));
    }

    // 5. Assert: value is an Object or an Array.
    VERIFY(value.is_object() || MUST(value.is_array(realm.vm())));

    // 6. Let keyValue be the result of converting a key to a value with key.
    auto key_value = convert_a_key_to_a_value(realm, move(key));

    // 7. Let status be CreateDataProperty(value, last, keyValue).
    auto status = MUST(value.as_object().create_data_property(last.to_byte_string(), key_value));

    // 8. Assert: status is true.
    VERIFY(status);
}

// https://w3c.github.io/IndexedDB/#convert-a-key-to-a-value
JS::Value convert_a_key_to_a_value(JS::Realm& realm, Key key)
{
    // 1. Let type be key’s type.
    auto type = key.type();

    // 2. Let value be key’s value.
    auto value = key.value();

    // 3. Switch on type:
    switch (type) {
    case Key::KeyType::Number: {
        // Return an ECMAScript Number value equal to value
        return JS::Value(key.value_as_double());
    }

    case Key::KeyType::String: {
        // Return an ECMAScript String value equal to value
        return JS::PrimitiveString::create(realm.vm(), key.value_as_string());
    }

    case Key::KeyType::Date: {
        // 1. Let date be the result of executing the ECMAScript Date constructor with the single argument value.
        auto date = JS::Date::create(realm, key.value_as_double());

        // 2. Assert: date is not an abrupt completion.
        // NOTE: This is not possible in our implementation.

        // 3. Return date.
        return date;
    }

    case Key::KeyType::Binary: {
        auto buffer = key.value_as_byte_buffer();

        // 1. Let len be value’s length.
        auto len = buffer.size();

        // 2. Let buffer be the result of executing the ECMAScript ArrayBuffer constructor with len.
        // 3. Assert: buffer is not an abrupt completion.
        auto array_buffer = MUST(JS::ArrayBuffer::create(realm, len));

        // 4. Set the entries in buffer’s [[ArrayBufferData]] internal slot to the entries in value.
        buffer.span().copy_to(array_buffer->buffer());

        // 5. Return buffer.
        return array_buffer;
    }

    case Key::KeyType::Array: {
        auto data = key.value_as_vector();

        // 1. Let array be the result of executing the ECMAScript Array constructor with no arguments.
        // 2. Assert: array is not an abrupt completion.
        auto array = MUST(JS::Array::create(realm, 0));

        // 3. Let len be value’s size.
        auto len = data.size();

        // 4. Let index be 0.
        u64 index = 0;

        // 5. While index is less than len:
        while (index < len) {
            // 1. Let entry be the result of converting a key to a value with value[index].
            auto entry = convert_a_key_to_a_value(realm, data[index]);

            // 2. Let status be CreateDataProperty(array, index, entry).
            auto status = MUST(array->create_data_property(index, entry));

            // 3. Assert: status is true.
            VERIFY(status);

            // 4. Increase index by 1.
            index++;
        }

        // 6. Return array.
        return array;
    }
    }

    VERIFY_NOT_REACHED();
}

}
