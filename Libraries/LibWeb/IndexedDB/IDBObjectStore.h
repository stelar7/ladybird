/*
 * Copyright (c) 2024, stelar7 <dudedbz@gmail.com>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include <AK/Optional.h>
#include <AK/String.h>
#include <LibGC/Heap.h>
#include <LibGC/Ptr.h>
#include <LibJS/Runtime/Array.h>
#include <LibJS/Runtime/PrimitiveString.h>
#include <LibJS/Runtime/Value.h>
#include <LibWeb/Bindings/PlatformObject.h>
#include <LibWeb/IndexedDB/IDBTransaction.h>
#include <LibWeb/IndexedDB/Internal/Algorithms.h>
#include <LibWeb/IndexedDB/Internal/KeyGenerator.h>

namespace Web::IndexedDB {

// https://w3c.github.io/IndexedDB/#object-store-interface
class IDBObjectStore : public Bindings::PlatformObject {
    WEB_PLATFORM_OBJECT(IDBObjectStore, Bindings::PlatformObject);
    GC_DECLARE_ALLOCATOR(IDBObjectStore);

public:
    String name() const { return m_name; }
    void set_name(String name) { m_name = move(name); }

    bool auto_increment() const { return m_auto_increment; }
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

    virtual ~IDBObjectStore() override;
    [[nodiscard]] static GC::Ref<IDBObjectStore> create(JS::Realm&, String, bool, Optional<KeyPath> const&, GC::Ref<IDBTransaction>);

protected:
    explicit IDBObjectStore(JS::Realm&, String, bool, Optional<KeyPath> const&, GC::Ref<IDBTransaction>);
    virtual void initialize(JS::Realm&) override;

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
};

}
