#include "test_framework.h"

#include "Sha256Logic.h"

#include <cstdint>
#include <string>
#include <vector>

using dvb::Sha256Logic::Hex;

namespace
{
	std::string HexOf(const std::string& a_text)
	{
		return Hex({ reinterpret_cast<const std::uint8_t*>(a_text.data()), a_text.size() });
	}
}

// The comparison a caller makes is against a hash computed elsewhere (python hashlib, shasum), so these are the published
// FIPS 180-4 / NIST vectors, not values this code produced.
TEST_CASE("sha256 of the empty input and of abc match the published vectors")
{
	CHECK(HexOf("") == "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");
	CHECK(HexOf("abc") == "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
}

TEST_CASE("sha256 handles the padding boundaries: 55, 56, 63, 64 and 65 bytes")
{
	// 55 bytes still fits one padded block, 56 forces a second block; 64 is exactly one data block plus a padding block.
	CHECK(HexOf(std::string(55, 'a')) == "9f4390f8d30c2dd92ec9f095b65e2b9ae9b0a925a5258e241c9f1e910f734318");
	CHECK(HexOf(std::string(56, 'a')) == "b35439a4ac6f0948b6d6f9e3c6af0f5f590ce20f1bde7090ef7970686ec6738a");
	CHECK(HexOf(std::string(63, 'a')) == "7d3e74a05d7db15bce4ad9ec0658ea98e3f06eeecf16b4c6fff2da457ddc2f34");
	CHECK(HexOf(std::string(64, 'a')) == "ffe054fe7ae0cb6dc65c3af9b61d5209f439851db43d0ba5997337df154668eb");
	CHECK(HexOf(std::string(65, 'a')) == "635361c48bb9eab14198e76ea8ab7f1a41685d6ad62aa9146d301d4f17eb0ae0");
}

TEST_CASE("sha256 of the two-block NIST message and of one million a")
{
	CHECK(HexOf("abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq") == "248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1");
	CHECK(HexOf(std::string(1000000, 'a')) == "cdc76e5c9914fb9281a1c7e284d73e67f1809a48a497200e046d39ccc7112cd0");
}

TEST_CASE("sha256 hashes binary data containing zero bytes")
{
	const std::vector<std::uint8_t> zeros(4, 0);
	CHECK(Hex(zeros) == "df3f619804a92fdb4057192dc43dd748ea778adc52bc498ce80524c014b81119");
}
