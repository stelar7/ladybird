/*
 * Copyright (c) 2024, stelar7 <dudedbz@gmail.com>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include <AK/Error.h>
#include <AK/Variant.h>
#include <AK/Vector.h>
#include <LibGC/Ptr.h>
#include <LibJS/Runtime/Realm.h>
#include <LibJS/Runtime/Value.h>
#include <LibWeb/HTML/DOMStringList.h>
#include <LibWeb/IndexedDB/IDBRequest.h>
#include <LibWeb/IndexedDB/IDBTransaction.h>
#include <LibWeb/IndexedDB/Internal/Key.h>
#include <LibWeb/StorageAPI/StorageKey.h>
#include <LibWeb/WebIDL/ExceptionOr.h>

namespace Web::IndexedDB {

using KeyPath = Variant<String, Vector<String>>;

WebIDL::ExceptionOr<GC::Ref<IDBDatabase>> open_a_database_connection(JS::Realm&, StorageAPI::StorageKey, String, Optional<u64>, GC::Ref<IDBRequest>);
bool fire_a_version_change_event(JS::Realm&, FlyString const&, GC::Ref<DOM::EventTarget>, u64, Optional<u64>);
ErrorOr<GC::Ref<Key>> convert_a_value_to_a_key(JS::Realm&, JS::Value, Vector<JS::Value> = {});
ErrorOr<Key> convert_a_value_to_a_multi_entry_key(JS::Realm&, JS::Value);
void close_a_database_connection(IDBDatabase&, bool forced = false);
void upgrade_a_database(JS::Realm&, GC::Ref<IDBDatabase>, u64, GC::Ref<IDBRequest>);
WebIDL::ExceptionOr<u64> delete_a_database(JS::Realm&, StorageAPI::StorageKey, String, GC::Ref<IDBRequest>);
void abort_a_transaction(IDBTransaction&, GC::Ptr<WebIDL::DOMException>);
JS::Value convert_a_key_to_a_value(JS::Realm&, GC::Ref<Key>);
bool is_valid_key_path(KeyPath);
GC::Ref<HTML::DOMStringList> create_a_sorted_name_list(JS::Realm&, Vector<String>);
WebIDL::ExceptionOr<JS::Value> clone_in_realm(JS::Realm&, JS::Value, GC::Ref<IDBTransaction>);
WebIDL::ExceptionOr<ErrorOr<Key>> extract_a_key_from_a_value_using_a_key_path(JS::Realm&, JS::Value, KeyPath, bool = false);
WebIDL::ExceptionOr<ErrorOr<JS::Value>> evaluate_key_path_on_a_value(JS::Realm&, JS::Value, KeyPath);
bool check_that_a_key_could_be_injected_into_a_value(JS::Realm&, JS::Value, KeyPath);
WebIDL::ExceptionOr<Optional<Key>> store_a_record_into_an_object_store(GC::Ref<IDBObjectStore>, JS::Value, Optional<Key>, bool);
WebIDL::ExceptionOr<u64> generate_a_key(GC::Ref<IDBObjectStore>);
void inject_a_key_into_a_value_using_a_key_path(JS::Realm&, JS::Value, Key, KeyPath);
JS::Value convert_a_key_to_a_value(JS::Realm&, Key);
void possibly_update_the_key_generator(GC::Ref<IDBObjectStore>, Key);
void fire_an_error_event(JS::Realm&, GC::Ref<IDBRequest>);
void fire_a_success_event(JS::Realm&, GC::Ref<IDBRequest>);

}
