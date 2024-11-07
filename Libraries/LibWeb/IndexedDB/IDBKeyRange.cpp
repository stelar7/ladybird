/*
 * Copyright (c) 2024, stelar7 <dudedbz@gmail.com>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <LibJS/Heap/GCPtr.h>
#include <LibJS/Runtime/VM.h>
#include <LibJS/Runtime/Value.h>
#include <LibWeb/Bindings/IDBKeyRangePrototype.h>
#include <LibWeb/Bindings/Intrinsics.h>
#include <LibWeb/IndexedDB/IDBKeyRange.h>
#include <LibWeb/IndexedDB/Internal/Algorithms.h>
#include <LibWeb/IndexedDB/Internal/Key.h>
#include <LibWeb/WebIDL/DOMException.h>

namespace Web::IndexedDB {

JS_DEFINE_ALLOCATOR(IDBKeyRange);

IDBKeyRange::~IDBKeyRange() = default;

IDBKeyRange::IDBKeyRange(JS::Realm& realm, JS::GCPtr<Key> lower_bound, JS::GCPtr<Key> upper_bound, bool lower_open, bool upper_open)
    : PlatformObject(realm)
    , m_lower_bound(lower_bound)
    , m_upper_bound(upper_bound)
    , m_lower_open(lower_open)
    , m_upper_open(upper_open)
{
}

JS::NonnullGCPtr<IDBKeyRange> IDBKeyRange::create(JS::Realm& realm, JS::GCPtr<Key> lower_bound, JS::GCPtr<Key> upper_bound, bool lower_open, bool upper_open)
{
    return realm.heap().allocate<IDBKeyRange>(realm, realm, lower_bound, upper_bound, lower_open, upper_open);
}

void IDBKeyRange::initialize(JS::Realm& realm)
{
    Base::initialize(realm);
    WEB_SET_PROTOTYPE_FOR_INTERFACE(IDBKeyRange);
}

void IDBKeyRange::visit_edges(Visitor& visitor)
{
    Base::visit_edges(visitor);
    visitor.visit(m_lower_bound);
    visitor.visit(m_upper_bound);
}

WebIDL::ExceptionOr<JS::NonnullGCPtr<IDBKeyRange>> IDBKeyRange::only(JS::VM& vm, JS::Value value)
{
    auto const& realm = vm.current_realm();

    // 1. Let key be the result of converting a value to a key with value. Rethrow any exceptions.
    auto maybe_key = convert_a_value_to_a_key(*realm, value);

    // 2. If key is invalid, throw a "DataError" DOMException.
    if (maybe_key.is_error())
        return WebIDL::DataError::create(*realm, "Failed to convert a value to a key"_string);

    // 3. Create and return a new key range containing only key.
    auto key = maybe_key.release_value();
    return IDBKeyRange::create(*realm, key, key, false, false);
}

WebIDL::ExceptionOr<JS::NonnullGCPtr<IDBKeyRange>> IDBKeyRange::lower_bound(JS::VM& vm, JS::Value value, bool open)
{
    auto const& realm = vm.current_realm();

    // 1. Let lowerKey be the result of converting a value to a key with lower. Rethrow any exceptions.
    auto maybe_lower_key = convert_a_value_to_a_key(*realm, value);

    // 2. If lowerKey is invalid, throw a "DataError" DOMException.
    if (maybe_lower_key.is_error())
        return WebIDL::DataError::create(*realm, "Failed to convert a value to a key"_string);

    // 3. Create and return a new key range with lower bound set to lowerKey, lower open flag set to open, upper bound set to null, and upper open flag set to true.
    auto lower_key = maybe_lower_key.release_value();
    return IDBKeyRange::create(*realm, lower_key, nullptr, open, true);
}

WebIDL::ExceptionOr<JS::NonnullGCPtr<IDBKeyRange>> IDBKeyRange::upper_bound(JS::VM& vm, JS::Value value, bool open)
{
    auto const& realm = vm.current_realm();

    // 1. Let upperKey be the result of converting a value to a key with upper. Rethrow any exceptions.
    auto maybe_upper_key = convert_a_value_to_a_key(*realm, value);

    // 2. If upperKey is invalid, throw a "DataError" DOMException.
    if (maybe_upper_key.is_error())
        return WebIDL::DataError::create(*realm, "Failed to convert a value to a key"_string);

    // 3. Create and return a new key range with lower bound set to null, lower open flag set to true, upper bound set to upperKey, and upper open flag set to open.
    auto upper_key = maybe_upper_key.release_value();
    return IDBKeyRange::create(*realm, nullptr, upper_key, true, open);
}

WebIDL::ExceptionOr<JS::NonnullGCPtr<IDBKeyRange>> IDBKeyRange::bound(JS::VM& vm, JS::Value lower, JS::Value upper, bool lower_open, bool upper_open)
{
    auto const& realm = vm.current_realm();

    // 1. Let lowerKey be the result of converting a value to a key with lower. Rethrow any exceptions.
    auto maybe_lower_key = convert_a_value_to_a_key(*realm, lower);

    // 2. If lowerKey is invalid, throw a "DataError" DOMException.
    if (maybe_lower_key.is_error())
        return WebIDL::DataError::create(*realm, "Failed to convert a value to a key"_string);

    // 3. Let upperKey be the result of converting a value to a key with upper. Rethrow any exceptions.
    auto maybe_upper_key = convert_a_value_to_a_key(*realm, upper);

    // 4. If upperKey is invalid, throw a "DataError" DOMException.
    if (maybe_upper_key.is_error())
        return WebIDL::DataError::create(*realm, "Failed to convert a value to a key"_string);

    auto lower_key = maybe_lower_key.release_value();
    auto upper_key = maybe_upper_key.release_value();

    // 5. If lowerKey is greater than upperKey, throw a "DataError" DOMException.
    if (Key::compare_two_keys(lower_key, upper_key) > 0)
        return WebIDL::DataError::create(*realm, "Failed to convert a value to a key"_string);

    // 6. Create and return a new key range with lower bound set to lowerKey, lower open flag set to lowerOpen, upper bound set to upperKey and upper open flag set to upperOpen.
    return IDBKeyRange::create(*realm, lower_key, upper_key, lower_open, upper_open);
}

WebIDL::ExceptionOr<bool> IDBKeyRange::includes(JS::Value key) const
{
    // 1. Let k be the result of converting a value to a key with key. Rethrow any exceptions.
    auto maybe_k = convert_a_value_to_a_key(realm(), key);

    // 2. If k is invalid, throw a "DataError" DOMException.
    if (maybe_k.is_error())
        return WebIDL::DataError::create(realm(), "Failed to convert a value to a key"_string);

    // 3. Return true if k is in this range, and false otherwise.
    auto k = maybe_k.release_value();

    // NOTE: A key is in a key range range if both of the following conditions are fulfilled:
    // * The range’s lower bound is null, or it is less than key, or it is both equal to key and the range’s lower open flag is false.
    auto lower_comparison = Key::compare_two_keys(*m_lower_bound, k);
    if (!m_lower_bound || lower_comparison < 0 || (lower_comparison == 0 && !m_lower_open)) {

        // * The range’s upper bound is null, or it is greater than key, or it is both equal to key and the range’s upper open flag is false.
        auto upper_comparison = Key::compare_two_keys(*m_upper_bound, k);
        return !m_upper_bound || upper_comparison > 0 || (upper_comparison == 0 && !m_upper_open);
    }

    return false;
}

}
