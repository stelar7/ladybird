/*
 * Copyright (c) 2024, stelar7 <dudedbz@gmail.com>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <LibWeb/Bindings/IDBKeyRangePrototype.h>
#include <LibWeb/Bindings/Intrinsics.h>
#include <LibWeb/IndexedDB/IDBKeyRange.h>

namespace Web::IndexedDB {

JS_DEFINE_ALLOCATOR(IDBKeyRange);

IDBKeyRange::~IDBKeyRange() = default;

IDBKeyRange::IDBKeyRange(JS::Realm& realm)
    : PlatformObject(realm)
{
}

JS::NonnullGCPtr<IDBKeyRange> IDBKeyRange::create(JS::Realm& realm)
{
    return realm.heap().allocate<IDBKeyRange>(realm, realm);
}

void IDBKeyRange::initialize(JS::Realm& realm)
{
    Base::initialize(realm);
    WEB_SET_PROTOTYPE_FOR_INTERFACE(IDBKeyRange);
}

}
