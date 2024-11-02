/*
 * Copyright (c) 2024, stelar7 <dudedbz@gmail.com>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include <LibJS/Runtime/Realm.h>
#include <LibWeb/IndexedDB/IDBRequest.h>
#include <LibWeb/IndexedDB/IDBTransaction.h>
#include <LibWeb/StorageAPI/StorageKey.h>

namespace Web::IndexedDB {

WebIDL::ExceptionOr<JS::NonnullGCPtr<IDBDatabase>> open_a_database_connection(JS::Realm&, StorageAPI::StorageKey, String, Optional<u64>, JS::NonnullGCPtr<IDBRequest>);
void close_a_database_connection(IDBDatabase&, bool forced = false);
WebIDL::ExceptionOr<u64> delete_a_database(JS::Realm&, StorageAPI::StorageKey, String, JS::NonnullGCPtr<IDBRequest>);
bool upgrade_a_database(JS::Realm&, JS::NonnullGCPtr<IDBDatabase>, u64, JS::NonnullGCPtr<IDBRequest>);
void abort_a_transaction(JS::Realm&, JS::NonnullGCPtr<IDBTransaction>, JS::GCPtr<WebIDL::DOMException>);
bool fire_a_version_change_event(JS::Realm&, FlyString const&, JS::NonnullGCPtr<DOM::EventTarget>, u64, Optional<u64>);

}
