/*
 * Copyright (c) 2024, stelar7 <dudedbz@gmail.com>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include <LibWeb/IndexedDB/Internal/ObjectStore.h>

namespace Web::IndexedDB {
ObjectStore::ObjectStore(String name, bool auto_increment, Variant<Empty, String, Vector<String>> key_path)
    : m_name(move(name))
    , m_auto_increment(auto_increment)
    , m_key_path(move(key_path))
{
}

ObjectStore::ObjectStore(String name)
    : m_name(move(name))
{
}
}
