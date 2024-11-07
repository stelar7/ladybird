/*
 * Copyright (c) 2024, stelar7 <dudedbz@gmail.com>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include <LibJS/Heap/Heap.h>
#include <LibWeb/Bindings/PlatformObject.h>

namespace Web::IndexedDB {

// https://w3c.github.io/IndexedDB/#object-store-interface
class IDBObjectStore : public Bindings::PlatformObject {
    WEB_PLATFORM_OBJECT(IDBObjectStore, Bindings::PlatformObject);
    JS_DECLARE_ALLOCATOR(IDBObjectStore);

public:
    virtual ~IDBObjectStore() override;
    [[nodiscard]] static JS::NonnullGCPtr<IDBObjectStore> create(JS::Realm&);

protected:
    explicit IDBObjectStore(JS::Realm&);
    virtual void initialize(JS::Realm&) override;
};

}
