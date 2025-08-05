/*
 * Copyright (c) 2025, stelar7 <dudedbz@gmail.com>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include <AK/Optional.h>
#include <AK/String.h>
#include <AK/Types.h>
#include <AK/Vector.h>
#include <LibURL/URL.h>
#include <LibWeb/EncryptedMediaExtensions/KeySystem.h>

namespace Web::EncryptedMediaExtensions {

bool is_supported_key_system(Utf16String const& key_system);
NonnullOwnPtr<KeySystem> key_system_from_string(Utf16String const& key_system);
bool supports_container(String const& container);
bool is_persistent_session_type(Utf16String const& session_type);
Optional<Vector<Bindings::MediaKeySystemMediaCapability>> get_supported_capabilities_for_audio_video_type(KeySystem const&, CapabilitiesType, Vector<Bindings::MediaKeySystemMediaCapability>, Bindings::MediaKeySystemConfiguration const&, MediaKeyRestrictions);
ConsentStatus get_consent_status(Bindings::MediaKeySystemConfiguration const&, MediaKeyRestrictions&, URL::Origin const&);
Optional<ConsentConfiguration> get_supported_configuration_and_consent(KeySystem const&, Bindings::MediaKeySystemConfiguration const&, MediaKeyRestrictions&, URL::Origin const&);
Optional<ConsentConfiguration> get_supported_configuration(KeySystem const&, Bindings::MediaKeySystemConfiguration const&, URL::Origin const&);

}
