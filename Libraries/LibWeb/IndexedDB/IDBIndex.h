/*
 * Copyright (c) 2024, stelar7 <dudedbz@gmail.com>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include <LibGC/Heap.h>
#include <LibGC/Ptr.h>
#include <LibWeb/Bindings/PlatformObject.h>
#include <LibWeb/IndexedDB/IDBObjectStore.h>
#include <LibWeb/IndexedDB/Internal/Algorithms.h>

namespace Web::IndexedDB {

// https://w3c.github.io/IndexedDB/#index-interface
class IDBIndex : public Bindings::PlatformObject {
    WEB_PLATFORM_OBJECT(IDBIndex, Bindings::PlatformObject);
    GC_DECLARE_ALLOCATOR(IDBIndex);

public:
    virtual ~IDBIndex() override;
    [[nodiscard]] static GC::Ref<IDBIndex> create(JS::Realm&, GC::Ref<IDBObjectStore>, String, KeyPath, bool, bool);

    GC::Ref<IDBTransaction> transaction();

    String name() const { return m_name; }
    void set_name(String name) { m_name = move(name); }
    bool unique() const { return m_unique; }
    bool multi_entry() const { return m_multi_entry; }
    GC::Ref<IDBObjectStore> object_store() { return m_object_store; }
    JS::Value key_path() const
    {
        return m_key_path.visit(
            [&](String const& value) -> JS::Value { return JS::PrimitiveString::create(realm().vm(), value); },
            [&](Vector<String> const& value) -> JS::Value { return JS::Array::create_from<String>(realm(), value.span(), [&](auto const& entry) -> JS::Value {
                                                                return JS::PrimitiveString::create(realm().vm(), entry);
                                                            }); });
    }

protected:
    explicit IDBIndex(JS::Realm&, GC::Ref<IDBObjectStore>, String, KeyPath, bool, bool);
    virtual void initialize(JS::Realm&) override;

private:
    // An index has a name, which is a name.
    String m_name;

    // An index has a unique flag. When true, the index enforces that no two records in the index has the same key.
    bool m_unique { false };

    // An index has a multiEntry flag. This flag affects how the index behaves when the result of evaluating the index’s key path yields an array key.
    bool m_multi_entry { false };

    // An index handle has an associated object store handle.
    GC::Ref<IDBObjectStore> m_object_store;

    // The keys are derived from the referenced object store’s values using a key path.
    KeyPath m_key_path;
};

}
