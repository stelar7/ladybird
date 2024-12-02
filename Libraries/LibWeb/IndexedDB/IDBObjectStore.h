/*
 * Copyright (c) 2024, stelar7 <dudedbz@gmail.com>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include <LibGC/Heap.h>
#include <LibWeb/Bindings/PlatformObject.h>
#include <LibWeb/IndexedDB/Internal/ObjectStore.h>

namespace Web::IndexedDB {

// https://w3c.github.io/IndexedDB/#object-store-interface
class IDBObjectStore : public Bindings::PlatformObject {
    WEB_PLATFORM_OBJECT(IDBObjectStore, Bindings::PlatformObject);
    GC_DECLARE_ALLOCATOR(IDBObjectStore);

    struct IDBObjectStoreParameters {
        Optional<Variant<Empty, String, Vector<String>>> key_path;
        bool auto_increment { false };
    };

public:
    virtual ~IDBObjectStore() override;
    [[nodiscard]] static GC::Ref<IDBObjectStore> create(JS::Realm&, ObjectStore, GC::Ref<IDBTransaction>);

protected:
    explicit IDBObjectStore(JS::Realm&, ObjectStore const&, GC::Ref<IDBTransaction>);
    virtual void initialize(JS::Realm&) override;

private:
    ObjectStore m_store;
    GC::Ref<IDBTransaction> m_transaction;
};

}
