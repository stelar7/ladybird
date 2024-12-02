/*
 * Copyright (c) 2024, stelar7 <dudedbz@gmail.com>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include <LibGC/Ptr.h>
#include <LibWeb/DOM/EventTarget.h>
#include <LibWeb/HTML/DOMStringList.h>
#include <LibWeb/IndexedDB/IDBObjectStore.h>
#include <LibWeb/IndexedDB/IDBRequest.h>
#include <LibWeb/IndexedDB/Internal/Algorithms.h>
#include <LibWeb/IndexedDB/Internal/Database.h>
#include <LibWeb/StorageAPI/StorageKey.h>

namespace Web::IndexedDB {

// https://w3c.github.io/IndexedDB/#object-store-construct
class ObjectStore : HashMap<String, GC::Root<JS::Object>> {

public:
    [[nodiscard]] String name() const { return m_name; }
    [[nodiscard]] bool auto_increment() const { return m_auto_increment; }
    [[nodiscard]] Variant<Empty, String, Vector<String>> key_path() const { return m_key_path; }

    void set_name(String name) { m_name = move(name); }
    void set_auto_increment(bool auto_increment) { m_auto_increment = auto_increment; }
    void set_key_path(Variant<Empty, String, Vector<String>> key_path) { m_key_path = move(key_path); }

    ObjectStore(String name, bool auto_increment, Variant<Empty, String, Vector<String>> key_path);
    ObjectStore(String name);

private:
    String m_name;
    bool m_auto_increment { false };
    Variant<Empty, String, Vector<String>> m_key_path;
};

}
