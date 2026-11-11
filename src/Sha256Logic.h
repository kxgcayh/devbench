#pragma once

#include <cstdint>
#include <span>
#include <string>

namespace dvb::Sha256Logic
{
	/// FIPS 180-4 SHA-256 of a_bytes as 64 lower-case hex characters. Pure; no allocation beyond the result.
	std::string Hex(std::span<const std::uint8_t> a_bytes);
}
