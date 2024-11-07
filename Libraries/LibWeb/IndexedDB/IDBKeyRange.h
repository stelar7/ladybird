/*
 * Copyright (c) 2024, stelar7 <dudedbz@gmail.com>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include <LibJS/Heap/Heap.h>
#include <LibWeb/Bindings/PlatformObject.h>
#include <LibWeb/IndexedDB/Internal/Key.h>

namespace Web::IndexedDB {

// https://w3c.github.io/IndexedDB/#keyrange
class IDBKeyRange : public Bindings::PlatformObject {
    WEB_PLATFORM_OBJECT(IDBKeyRange, Bindings::PlatformObject);
    JS_DECLARE_ALLOCATOR(IDBKeyRange);

public:
    virtual ~IDBKeyRange() override;
    [[nodiscard]] static JS::NonnullGCPtr<IDBKeyRange> create(JS::Realm&, JS::GCPtr<Key> lower_bound, JS::GCPtr<Key> upper_bound, bool lower_open, bool upper_open);
    [[nodiscard]] static WebIDL::ExceptionOr<JS::NonnullGCPtr<IDBKeyRange>> only(JS::VM& vm, JS::Value value);
    [[nodiscard]] static WebIDL::ExceptionOr<JS::NonnullGCPtr<IDBKeyRange>> lower_bound(JS::VM& vm, JS::Value value, bool open = false);
    [[nodiscard]] static WebIDL::ExceptionOr<JS::NonnullGCPtr<IDBKeyRange>> upper_bound(JS::VM& vm, JS::Value value, bool open = false);
    [[nodiscard]] static WebIDL::ExceptionOr<JS::NonnullGCPtr<IDBKeyRange>> bound(JS::VM& vm, JS::Value lower, JS::Value upper, bool lower_open = false, bool upper_open = false);
    [[nodiscard]] WebIDL::ExceptionOr<bool> includes(JS::Value key) const;

    [[nodiscard]] JS::Value lower() const { return m_lower_bound->as_js_value(realm()); }
    [[nodiscard]] JS::Value upper() const { return m_upper_bound->as_js_value(realm()); }
    [[nodiscard]] bool lower_open() const { return m_lower_open; }
    [[nodiscard]] bool upper_open() const { return m_upper_open; }

protected:
    explicit IDBKeyRange(JS::Realm&, JS::GCPtr<Key> lower_bound, JS::GCPtr<Key> upper_bound, bool lower_open, bool upper_open);
    virtual void initialize(JS::Realm&) override;
    virtual void visit_edges(Visitor& visitor) override;

private:
    JS::GCPtr<Key> m_lower_bound;
    JS::GCPtr<Key> m_upper_bound;
    bool m_lower_open { false };
    bool m_upper_open { false };
};

}
