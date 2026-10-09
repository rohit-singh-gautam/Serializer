#include <rohit/digest.hpp>

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

namespace {
using rohit::digest_algorithm;

struct digest_fixture {
  digest_algorithm algorithm;
  std::string_view empty_hex;
  std::string_view abc_hex;
  std::string_view binary_hex;
  std::array<std::size_t, 5> boundary_lengths;
  std::array<std::string_view, 5> boundary_hex;
};

// Empty/abc known answers match RFC 1321 and NIST SHA examples. Binary and padding answers were
// independently produced with Python hashlib; hashing implementations are not shared with runtime.
constexpr auto fixtures = std::to_array<digest_fixture>({
    {digest_algorithm::md5,
     "d41d8cd98f00b204e9800998ecf8427e",
     "900150983cd24fb0d6963f7d28e17f72",
     "82db13df4a178acb69af07fe121c9c7e",
     {55, 56, 63, 64, 65},
     {"96799d8bf84d94466fb1d4785f0eae6c", "bf9c6bb58c938814a38d99d564393a53",
      "35692891f16306d06ba365a887de5dd3", "c99cacee8808c930ea7a2a5f79a58599",
      "99f97c2c46e2953fc2c2b315db172a78"}},
    {digest_algorithm::sha1,
     "da39a3ee5e6b4b0d3255bfef95601890afd80709",
     "a9993e364706816aba3e25717850c26c9cd0d89d",
     "66e98b6b750b34c7dd4cf089bc447890a20d5461",
     {55, 56, 63, 64, 65},
     {"c460dda9725993db7b635e25646d4fcd9bd30871", "1794a43ae77e95e8cc39a31e2727c58c718ce53f",
      "5274fdff2cc6384f500028c1e794d62aed870966", "8477cdffd4b543880bcf926612a161e4a08abfbf",
      "180b0aab8816d0ef092f1224dfc0568d08c22fe9"}},
    {digest_algorithm::sha224,
     "d14a028c2a3a2bc9476102bb288234c415a2b01f828ea62ac5b3e42f",
     "23097d223405d8228642a477bda255b32aadbce4bda0b3f7e36c9da7",
     "37a06b082138c2de25b7acc3dec41c9c7e6ac05414bf64f7274f2bfd",
     {55, 56, 63, 64, 65},
     {"381c0deaf5c52249dfbe3cb0a6990666db4beb9a019731537475a691",
      "a42264f56849ca940423b53cfafb801e85c4e378b7fb7c5f98bbfae8",
      "aa8f395dad99f4a1161e2135bf6582fc2c7005f04777f793d638ad0d",
      "37c92f6bdd9888e11ed3a5ff54f56dee03b5870221b0004facfde880",
      "5d6a9d0f779f7b0a3d1d0a28385b145bca4afc2ad1a413a1a2038e44"}},
    {digest_algorithm::sha256,
     "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855",
     "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad",
     "2ef02ebdb78dc5c7af2a6e09ce3296cc75a57ca4c880ddededf4cb356e5fea82",
     {55, 56, 63, 64, 65},
     {"59aaae80b8e7958f5757b4ac9274f1fd57ec0dc7aa8349319102316781b92006",
      "dca902d31487ffab357ce36cc5abc6947fbb69127ec04c468ba5687268c4bf7c",
      "b8e655e9e7ad413b96f0a371eb1d6db71dcca27f4eee35892517cbe4bfaa6f9a",
      "e5146be62accc56709594cb45c651b361df94f622cbb09b91ea3ca7a2060bd53",
      "4b6c6774f0cbcd776a90e187b9494e04ca880c0afe3ea1c5cad51c8bbcb41f74"}},
    {digest_algorithm::sha384,
     "38b060a751ac96384cd9327eb1b1e36a21fdb71114be07434c0cc7bf63f6e1da274edebfe76f65fbd51ad2f14898b"
     "95b",
     "cb00753f45a35e8bb5a03d699ac65007272c32ab0eded1631a8b605a43ff5bed8086072ba1e7cc2358baeca134c82"
     "5a7",
     "7898c80960393c371374684faf9b63a46fa723e1da458b32be10d2e0ffb8aeaa110713e63bd16c65f9e4f434322cd"
     "a08",
     {111, 112, 127, 128, 129},
     {"ae05be3f17ff7bc9642dd0c02073b70932ad8516fdb8a8e2148098f163de55fb0df302f835d4232ed64364f7c5f1"
      "f505",
      "bfdd3ba274db0213cfd756eba1ca6b1180d44ba2428133f8ab37de72772588022f1d6b0381eb355b091999a01499"
      "98fa",
      "2da2c082e24e64528c01224f002a87a8f6c1e2abb109c4138ea3738f0b3dd3546750d82a9816bbb0edaba0986aef"
      "7d7d",
      "ea0b611956177051328ae3750bfad5f0dde40d20b602f2a406f179be319b99a9eb1f8af9ebf572e09b5ee90aa7b8"
      "1a11",
      "8391543cf5fef95ee06b21011aad4749af279f4f17a169311fa3879ab5e722ea6dcdd69b3ec58368499f364f4517"
      "78b0"}},
    {digest_algorithm::sha512,
     "cf83e1357eefb8bdf1542850d66d8007d620e4050b5715dc83f4a921d36ce9ce47d0d13c5d85f2b0ff8318d2877ee"
     "c2f63b931bd47417a81a538327af927da3e",
     "ddaf35a193617abacc417349ae20413112e6fa4e89a97ea20a9eeee64b55d39a2192992a274fc1a836ba3c23a3fee"
     "bbd454d4423643ce80e2a9ac94fa54ca49f",
     "2090bcb0fe37ec3a3946bc445baaa9917872a97262f95cf447aa533f79f3c3aae674cb782cbebd74b7301a6fac602"
     "f975a465bf6b02aff880238b0fe9b1835d7",
     {111, 112, 127, 128, 129},
     {"e914b739fe0b5e807bcd0966007596551f6395d312b8b26b255749b9a3c0550da197325745c05fd3f19b6b9709af"
      "9fc4a681856adbe312bdbe4f18f94ff523a3",
      "d5b97c9cef538d686c72c80befed51a66281b3ad4cf70a75f3812015e7f60bb0ca529f8d58661a517263a0ffaf59"
      "a8e05aee80bc0588a130f6295a94c7638657",
      "a10a2850cc7a101c1877907254550f8beb1a4c7da9d372ebf0ecacc0169fbce842471cd18da009aa68513a05dc72"
      "c0d6ceaaf704060fb4632be0a0901215564f",
      "3b6509e01d3962462c2321ed30af8a49fd06ad2ed8c15a6310236fd1309508adcbe0c72404d88570f2b625a70314"
      "b5728c5b3476b2160cc93cc0e448229909aa",
      "1e760fbfdd903458c19c2017c624e7bed001aeb12bb259f45b74ad307d1a59930100c898e514028f1b1c0a85f195"
      "72813d8547ae08e02487a7090bad9220d8ce"}},
    {digest_algorithm::sha512_224,
     "6ed0dd02806fa89e25de060c19d3ac86cabb87d6a0ddd05c333b84f4",
     "4634270f707b6a54daae7530460842e20e37ed265ceee9a43e8924aa",
     "1736c68c81ff864b1f5595b14903c70ecb03fc23633a25feb25588a5",
     {111, 112, 127, 128, 129},
     {"c183f06a1414d23559a0e655afbb7934c1c8ebc49ca296ba54830956",
      "cb706b234e9e4ad86f5a56ab3557554e09945ae54db3a33610e3a681",
      "46f4901b8e55d0681b58e453364499a043cec91ce6fe0e542e942181",
      "f7584f4d67401f0434fbf8e75f072850100929fe7c4748d373cffa54",
      "b3706775979f05dd639beae9e7f731a685327f89a315a4dd878b5ab5"}},
    {digest_algorithm::sha512_256,
     "c672b8d1ef56ed28ab87c3622c5114069bdd3ad7b8f9737498d0c01ecef0967a",
     "53048e2681941ef99b2e29b76b4c7dabe4c2d0c634fc6d46e0e2f13107e7af23",
     "f44dd8a6a73039d0b553f3acd9b6bf30d7542d41e4f6223f6cb3fee0aa400db5",
     {111, 112, 127, 128, 129},
     {"de114df742012e602a32aa40b6a6c1de8fd34c74f9e74f7f9f1e3e0edc152443",
      "13fd64ced5c4007ee2075ab781338c3c59dae74c5dad2674ef702ac12a67a356",
      "f81b8479c9223dc4fd3fbe6819be5b01f95810faa429bf933fa28281de1bd836",
      "b90f35aa6e60aefae175c9bb9279ec9649e0f9e549449bbb0c3feacf9a648716",
      "902abe99d326d142e96612195bd8221ec0edb50ba73a1a5a2f3d6e0a720290af"}},
    {digest_algorithm::sha3_224,
     "6b4e03423667dbb73b6e15454f0eb1abd4597f9a1b078e3f5b5a6bc7",
     "e642824c3f8cf24ad09234ee7d3c766fc9a3a5168d0c94ad73b46fdf",
     "88010e7e8e53829ab67d39e1178bb6fae6faf866c1358d6e9e526273",
     {143, 144, 145, 287, 288},
     {"c0b093117103b2f35b0907dfb9d61fd4b1b8bfc8c6f3d05c602317d8",
      "81f8eb84f46641af85b3d8a4cc00a039ec279afc06d2e3df89b35627",
      "fce57da891d2c86a0ba482622d29fba6317da0874103803041085264",
      "1fc1bbe023631fdf85c4babbc58da3bab890c8df3016c38fe2360869",
      "0763fc0dc1a9dbeb913e34c27464230bf85081622c673b03f6a128dd"}},
    {digest_algorithm::sha3_256,
     "a7ffc6f8bf1ed76651c14756a061d662f580ff4de43b49fa82d80a4b80f8434a",
     "3a985da74fe225b2045c172d6bd390bd855f086e3e9d525b46bfe24511431532",
     "3a1a1e2415d6f415d9e6205155f34b2c27d9ba2f2e88fd58d4ad5304c49985e0",
     {135, 136, 137, 271, 272},
     {"7de7585833b3fa0a7bfdfd08e178fa026bd301e151b07b8e2a4304d13b2723b5",
      "a0cf25dfe09b49b63609a1779dc9e640d6f809debc8d2c8abc19ac79829b4715",
      "2db99aafa856723998c7e92b4ed293c01ace3aab3f61b0f4f8b2402cfedd6410",
      "29ee8d293f45146ac4869de9576904038e034ff52e62fd180b03b8ee10a5547c",
      "23dcf09d75f3e1d7d394aabc30ff7effdafc001597c229d9e47a74798a2436be"}},
    {digest_algorithm::sha3_384,
     "0c63a75b845e4f7d01107d852e4c2485c51a50aaaa94fc61995e71bbee983a2ac3713831264adb47fb6bd1e058d5f"
     "004",
     "ec01498288516fc926459f58e2c6ad8df9b473cb0fc08c2596da7cf0e49be4b298d88cea927ac7f539f1edf228376"
     "d25",
     "5f105499bf5a688fa8989932b68607d7a9680d168c053623375d567aa8cf941410fa5c70b1a14a648bfdbd95560db"
     "ae3",
     {103, 104, 105, 207, 208},
     {"e6678dd5001e2ae82a1e5190f42dc64caaee8defaf7c64e8f596f8c142ee29ad9918908eb2a44e8fe12f0ab1ab08"
      "510e",
      "f804e0e24da80e898b357fadb8dc5e83c844bde22a9731cbcb9a75e4944b602072a6590de6e14c4bf50154e6ffc0"
      "5f8e",
      "eb0a81e8d6b0a96f690045c6b072e1da40a8060572d659c877caaee8dbead773e1a8b0197c8d6175acc9211b1202"
      "d42a",
      "8c6f296824b0c28d3c07551df6d8fdaec680ae18dba8c8f4834b6d50a6f312a39854bbce03f574fa421cc5464fad"
      "b704",
      "ce32472f67fcb6db84cf6ceb960d74338fa2bcb6dc8b457cba9de48bf663575c70c64c555bf97f66fa07614c1329"
      "5737"}},
    {digest_algorithm::sha3_512,
     "a69f73cca23a9ac5c8b567dc185a756e97c982164fe25859e0d1dcc1475c80a615b2123af1f5f94c11e3e9402c3ac"
     "558f500199d95b6d3e301758586281dcd26",
     "b751850b1a57168a5693cd924b6b096e08f621827444f70d884f5d0240d2712e10e116e9192af3c91a7ec57647e39"
     "34057340b4cf408d5a56592f8274eec53f0",
     "aa1a79a0ccb3a676edba38c53b9e4726fe8975d3ca6c8d1846988a34dda4941fdbe53c165d657695deacead834007"
     "c94bdb81b1a2e332688e3700ab3fca3b390",
     {71, 72, 73, 143, 144},
     {"c904f58db0da5f77474a69124bdb36a65f515193db08ba0fea5cf347efafb33a717cfa32de443b6335b66247d7ff"
      "5bdca175efd9297ce911fc92bba79bbd5954",
      "fe131baaee921607c4a67a82f7ef3d66521031a43997b4bbf320265a27fdd043f7d59408c68ac380e2c54ecc4c75"
      "271e6a91a151720478253011a4d1cfa42919",
      "77de345b2922cdb528763413ade641052c7a1029e207492f4cc06e87883cef305d1fc9c67eca3895a3a29d229be4"
      "b16c2c7ed19f65b12dc04563fca21b6c668f",
      "1276b3ed3749316dce536bd7c02361e607430bb0919c8b16a3cca66bd574b3af781689ac3f93a883e42f379adfe1"
      "5cb4c851891936b213a98315a8955a960b13",
      "27b0c4142e7ee5a4f6ccb40566db23225f63107278a0106468331dde9b9aac4ac2eeca81cc2f4476a2f892ad32db"
      "71ca4db0911e66eb81182f22089e66cb814c"}},
});

// Include every byte value in a deterministic order, including embedded zeros and high bytes.
std::vector<std::uint8_t> make_binary_input(std::size_t length) {
  std::vector<std::uint8_t> result(length);
  for (std::size_t index = 0; index < length; ++index) {
    result[index] = static_cast<std::uint8_t>(index * 131 + 17);
  }
  return result;
}

// Compute dynamically and compare the exact digest against an independently frozen fixture.
void expect_digest(digest_algorithm algorithm, std::span<const std::uint8_t> input,
                   std::string_view expected) {
  std::vector<std::uint8_t> output(rohit::digest_size(algorithm), 0xff);
  rohit::compute_digest(algorithm, input, output);
  EXPECT_EQ(rohit::digest_to_hex(output), expected);
}

// Exercise each compile-time helper and its exact fixed byte type against the same known answer.
template <digest_algorithm Algorithm>
void expect_fixed_digest(std::string_view expected) {
  const auto result = rohit::make_digest<Algorithm>(std::string_view{"abc"});
  static_assert(std::same_as<std::remove_cv_t<decltype(result)>,
                             std::array<std::uint8_t, rohit::digest_size(Algorithm)>>);
  EXPECT_EQ(rohit::digest_to_hex(result), expected);
  constexpr auto input = std::to_array<std::uint8_t>({'a', 'b', 'c'});
  EXPECT_EQ(rohit::make_digest<Algorithm>(std::span<const std::uint8_t>{input}), result);
}
} // namespace

// Check published empty/abc answers and binary multiblock messages for all supported algorithms.
TEST(digest, known_answers) {
  constexpr auto abc = std::to_array<std::uint8_t>({'a', 'b', 'c'});
  const auto binary = make_binary_input(512);
  for (const auto& fixture : fixtures) {
    SCOPED_TRACE(static_cast<int>(fixture.algorithm));
    expect_digest(fixture.algorithm, {}, fixture.empty_hex);
    expect_digest(fixture.algorithm, abc, fixture.abc_hex);
    expect_digest(fixture.algorithm, binary, fixture.binary_hex);
  }
}

// Force one/two padding blocks and SHA-3's suffix/rate boundary for every algorithm family.
TEST(digest, padding_and_rate_boundaries) {
  for (const auto& fixture : fixtures) {
    SCOPED_TRACE(static_cast<int>(fixture.algorithm));
    for (std::size_t index = 0; index < fixture.boundary_lengths.size(); ++index) {
      SCOPED_TRACE(fixture.boundary_lengths[index]);
      expect_digest(fixture.algorithm, make_binary_input(fixture.boundary_lengths[index]),
                    fixture.boundary_hex[index]);
    }
  }
}

// Require all explicitly named fixed helpers to compute the specified algorithm and byte width.
TEST(digest, fixed_size_helpers) {
  expect_fixed_digest<digest_algorithm::md5>(fixtures[0].abc_hex);
  expect_fixed_digest<digest_algorithm::sha1>(fixtures[1].abc_hex);
  expect_fixed_digest<digest_algorithm::sha224>(fixtures[2].abc_hex);
  expect_fixed_digest<digest_algorithm::sha256>(fixtures[3].abc_hex);
  expect_fixed_digest<digest_algorithm::sha384>(fixtures[4].abc_hex);
  expect_fixed_digest<digest_algorithm::sha512>(fixtures[5].abc_hex);
  expect_fixed_digest<digest_algorithm::sha512_224>(fixtures[6].abc_hex);
  expect_fixed_digest<digest_algorithm::sha512_256>(fixtures[7].abc_hex);
  expect_fixed_digest<digest_algorithm::sha3_224>(fixtures[8].abc_hex);
  expect_fixed_digest<digest_algorithm::sha3_256>(fixtures[9].abc_hex);
  expect_fixed_digest<digest_algorithm::sha3_384>(fixtures[10].abc_hex);
  expect_fixed_digest<digest_algorithm::sha3_512>(fixtures[11].abc_hex);
}

// Reject invalid selections and output lengths without touching caller-owned destination bytes.
TEST(digest, invalid_algorithm_and_extent_are_transactional) {
  std::array<std::uint8_t, 64> output{};
  output.fill(0xa5);
  const auto before = output;
  EXPECT_THROW(rohit::compute_digest(digest_algorithm::none, {}, output), std::invalid_argument);
  EXPECT_THROW(rohit::compute_digest(static_cast<digest_algorithm>(255), {}, output),
               std::invalid_argument);
  for (const auto& fixture : fixtures) {
    const auto size = rohit::digest_size(fixture.algorithm);
    EXPECT_THROW(rohit::compute_digest(fixture.algorithm, {}, std::span{output}.first(size - 1)),
                 std::invalid_argument);
  }
  EXPECT_EQ(output, before);
  EXPECT_EQ(rohit::digest_size(digest_algorithm::none), 0u);
  EXPECT_EQ(rohit::digest_size(static_cast<digest_algorithm>(255)), 0u);
}

// Permit an output span to replace bytes inside its input only after all message blocks are read.
TEST(digest, overlapping_input_and_output) {
  for (const auto& fixture : fixtures) {
    auto input = make_binary_input(512);
    rohit::compute_digest(fixture.algorithm, input,
                          std::span{input}.subspan(17, rohit::digest_size(fixture.algorithm)));
    EXPECT_EQ(
        rohit::digest_to_hex(std::span{input}.subspan(17, rohit::digest_size(fixture.algorithm))),
        fixture.binary_hex);
  }
}

// Preserve embedded zero and non-ASCII bytes when adapting a string_view to a byte span.
TEST(digest, string_view_preserves_binary_input) {
  const auto input = make_binary_input(512);
  const std::string_view text{reinterpret_cast<const char*>(input.data()), input.size()};
  EXPECT_EQ(rohit::digest_to_hex(rohit::make_digest<digest_algorithm::sha256>(text)),
            fixtures[3].binary_hex);
}

// Decode both ASCII cases at exactly the expected width, with empty caller-provided data allowed.
TEST(digest, hexadecimal_round_trip) {
  const auto input = make_binary_input(256);
  const auto lower = rohit::digest_to_hex(input);
  auto upper = lower;
  std::transform(upper.begin(), upper.end(), upper.begin(), [](char value) {
    return value >= 'a' && value <= 'f' ? static_cast<char>(value - 'a' + 'A') : value;
  });
  std::vector<std::uint8_t> output(input.size());
  EXPECT_TRUE(rohit::digest_from_hex(lower, output));
  EXPECT_EQ(output, input);
  output.assign(output.size(), 0);
  EXPECT_TRUE(rohit::digest_from_hex(upper, output));
  EXPECT_EQ(output, input);
  EXPECT_TRUE(rohit::digest_from_hex({}, {}));
  EXPECT_TRUE(rohit::digest_to_hex({}).empty());
}

// Reject malformed/short/long/prefixed hexadecimal before writing any caller-provided byte.
TEST(digest, invalid_hexadecimal_is_transactional) {
  std::array<std::uint8_t, 3> output{0xab, 0xcd, 0xef};
  const auto before = output;
  for (const std::string_view text : {"", "abc", "0011", "00112233", "0x1122", "00112g", "00112 ",
                                      "0011\n2", "0011/2", "0011:2"}) {
    EXPECT_FALSE(rohit::digest_from_hex(text, output)) << text;
    EXPECT_EQ(output, before);
  }
  const std::string non_ascii{"00112" + std::string(1, static_cast<char>(0xff))};
  EXPECT_FALSE(rohit::digest_from_hex(non_ascii, output));
  EXPECT_EQ(output, before);
}
