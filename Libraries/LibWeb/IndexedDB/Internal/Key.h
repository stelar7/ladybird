/*
 * Copyright (c) 2024, stelar7 <dudedbz@gmail.com>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include <AK/ByteBuffer.h>
#include <AK/String.h>
#include <AK/Variant.h>
#include <AK/Vector.h>
#include <LibJS/Heap/GCPtr.h>
#include <LibWeb/Bindings/PlatformObject.h>

namespace Web::IndexedDB {

// https://w3c.github.io/IndexedDB/#key-construct
class Key : public Bindings::PlatformObject {
    // A key also has an associated value, which will be either:
    // * an unrestricted double if type is number or date,
    // * a DOMString if type is string,
    // * a byte sequence if type is binary,
    // * a list of other keys if type is array.
    using KeyValue = Variant<double, AK::String, ByteBuffer, Vector<JS::Handle<Key>>>;

    WEB_PLATFORM_OBJECT(Key, Bindings::PlatformObject);
    JS_DECLARE_ALLOCATOR(Key);

public:
    // A key has an associated type which is one of: number, date, string, binary, or array.
    enum KeyType {
        Number,
        Date,
        String,
        Binary,
        Array,
    };

    [[nodiscard]] static JS::NonnullGCPtr<Key> create(JS::Realm&, KeyType, KeyValue);

    [[nodiscard]] KeyType type() { return m_type; }
    [[nodiscard]] KeyValue value() { return m_value; }
    [[nodiscard]] JS::Value as_js_value(JS::Realm& realm);

    [[nodiscard]] static i8 compare_two_keys(JS::NonnullGCPtr<Key> a, JS::NonnullGCPtr<Key> b);

protected:
    explicit Key(JS::Realm& realm, KeyType type, KeyValue value)
        : PlatformObject(realm)
        , m_type(type)
        , m_value(value)
    {
    }

    virtual void initialize(JS::Realm&) override;
    virtual void visit_edges(Visitor& visitor) override;

private:
    KeyType m_type;
    KeyValue m_value;

    JS::Value cached_js_value;
};

}
