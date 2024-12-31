/*
 * Copyright (c) 2024, stelar7 <dudedbz@gmail.com>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include <LibGC/Heap.h>
#include <LibWeb/Bindings/PlatformObject.h>

namespace Web::IndexedDB {

// https://w3c.github.io/IndexedDB/#cursor-interface
class IDBCursor : public Bindings::PlatformObject {
    WEB_PLATFORM_OBJECT(IDBCursor, Bindings::PlatformObject);
    GC_DECLARE_ALLOCATOR(IDBCursor);

public:
    GC::Ref<IDBTransaction> transaction() { return m_transaction; }

    virtual ~IDBCursor() override;
    [[nodiscard]] static GC::Ref<IDBCursor> create(JS::Realm&, GC::Ref<IDBTransaction>);

protected:
    explicit IDBCursor(JS::Realm&, GC::Ref<IDBTransaction>);
    virtual void initialize(JS::Realm&) override;

private:
    // A cursor has a transaction, the transaction that was active when the cursor was created.
    GC::Ref<IDBTransaction> m_transaction;
};
}
