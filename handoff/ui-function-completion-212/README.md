# Localize spell-detail school names 212

The receiving iPad report identified an English BodyMagic label. Current character and creation pages already use the Chinese skill table, but the spell-detail school line still used the English Body Magic fallback unconditionally. It now resolves the spell's canonical magic skill and uses the existing localized skill name when present. Missing or empty entries retain the prior fallback.

Only GameplayHudOverlaySupport.cpp changes. No identity, data resource or translation-schema change. Validation is combined with211 native/unsigned generic iOS compilation, with no new App/device/campaign replay.
