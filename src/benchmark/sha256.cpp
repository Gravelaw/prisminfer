#include "prisminfer/sha256.h"

#include <array>
#include <cstdint>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <vector>

namespace prisminfer {
namespace {
constexpr std::array<std::uint32_t, 64> kRoundConstants{
    0x428a2f98U,0x71374491U,0xb5c0fbcfU,0xe9b5dba5U,0x3956c25bU,0x59f111f1U,0x923f82a4U,0xab1c5ed5U,
    0xd807aa98U,0x12835b01U,0x243185beU,0x550c7dc3U,0x72be5d74U,0x80deb1feU,0x9bdc06a7U,0xc19bf174U,
    0xe49b69c1U,0xefbe4786U,0x0fc19dc6U,0x240ca1ccU,0x2de92c6fU,0x4a7484aaU,0x5cb0a9dcU,0x76f988daU,
    0x983e5152U,0xa831c66dU,0xb00327c8U,0xbf597fc7U,0xc6e00bf3U,0xd5a79147U,0x06ca6351U,0x14292967U,
    0x27b70a85U,0x2e1b2138U,0x4d2c6dfcU,0x53380d13U,0x650a7354U,0x766a0abbU,0x81c2c92eU,0x92722c85U,
    0xa2bfe8a1U,0xa81a664bU,0xc24b8b70U,0xc76c51a3U,0xd192e819U,0xd6990624U,0xf40e3585U,0x106aa070U,
    0x19a4c116U,0x1e376c08U,0x2748774cU,0x34b0bcb5U,0x391c0cb3U,0x4ed8aa4aU,0x5b9cca4fU,0x682e6ff3U,
    0x748f82eeU,0x78a5636fU,0x84c87814U,0x8cc70208U,0x90befffaU,0xa4506cebU,0xbef9a3f7U,0xc67178f2U};
std::uint32_t rotate_right(std::uint32_t value, std::uint32_t bits) {
  return (value >> bits) | (value << (32U - bits));
}
void process_block(const std::uint8_t* block, std::array<std::uint32_t, 8>* state) {
  std::array<std::uint32_t, 64> words{};
  for (std::size_t i = 0; i < 16; ++i) words[i] = (static_cast<std::uint32_t>(block[4*i]) << 24U) | (static_cast<std::uint32_t>(block[4*i+1]) << 16U) | (static_cast<std::uint32_t>(block[4*i+2]) << 8U) | block[4*i+3];
  for (std::size_t i = 16; i < words.size(); ++i) {
    const auto s0 = rotate_right(words[i-15], 7) ^ rotate_right(words[i-15], 18) ^ (words[i-15] >> 3U);
    const auto s1 = rotate_right(words[i-2], 17) ^ rotate_right(words[i-2], 19) ^ (words[i-2] >> 10U);
    words[i] = words[i-16] + s0 + words[i-7] + s1;
  }
  auto a=(*state)[0], b=(*state)[1], c=(*state)[2], d=(*state)[3], e=(*state)[4], f=(*state)[5], g=(*state)[6], h=(*state)[7];
  for (std::size_t i=0;i<64;++i) {
    const auto s1=rotate_right(e,6)^rotate_right(e,11)^rotate_right(e,25);
    const auto choose=(e&f)^((~e)&g);
    const auto t1=h+s1+choose+kRoundConstants[i]+words[i];
    const auto s0=rotate_right(a,2)^rotate_right(a,13)^rotate_right(a,22);
    const auto majority=(a&b)^(a&c)^(b&c);
    const auto t2=s0+majority;
    h=g;g=f;f=e;e=d+t1;d=c;c=b;b=a;a=t1+t2;
  }
  (*state)[0]+=a;(*state)[1]+=b;(*state)[2]+=c;(*state)[3]+=d;(*state)[4]+=e;(*state)[5]+=f;(*state)[6]+=g;(*state)[7]+=h;
}
}  // namespace

bool sha256_file(const std::filesystem::path& path, std::string* digest, std::string* error) {
  std::ifstream input(path, std::ios::binary);
  if (!input) { if (error) *error="sha256_input_open_failed"; return false; }
  const std::vector<std::uint8_t> data((std::istreambuf_iterator<char>(input)),
                                       std::istreambuf_iterator<char>());
  if (input.bad()) { if (error) *error="sha256_input_read_failed"; return false; }
  std::array<std::uint32_t,8> state{0x6a09e667U,0xbb67ae85U,0x3c6ef372U,0xa54ff53aU,0x510e527fU,0x9b05688cU,0x1f83d9abU,0x5be0cd19U};
  std::array<std::uint8_t,64> block{};
  std::size_t position=0;
  while (position + 64U <= data.size()) { process_block(data.data()+position, &state); position += 64U; }
  const std::size_t remaining=data.size()-position;
  block.fill(0); for (std::size_t i=0;i<remaining;++i) block[i]=data[position+i];
  block[remaining]=0x80U;
  if (remaining >= 56U) { process_block(block.data(), &state); block.fill(0); }
  const std::uint64_t bit_count=static_cast<std::uint64_t>(data.size())*8U;
  for (std::size_t i=0;i<8;++i) block[63U-i]=static_cast<std::uint8_t>(bit_count>>(8U*i));
  process_block(block.data(), &state);
  std::ostringstream out; out<<std::hex<<std::setfill('0');
  for (const auto value:state) out<<std::setw(8)<<value;
  *digest=out.str(); return true;
}
}  // namespace prisminfer
