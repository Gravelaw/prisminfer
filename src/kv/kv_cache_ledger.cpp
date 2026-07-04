#include "prisminfer/kv_cache_ledger.h"

#include <limits>

namespace prisminfer {
namespace {

bool multiply_u64(std::uint64_t lhs, std::uint64_t rhs, std::uint64_t* out) {
  if (lhs != 0 && rhs > std::numeric_limits<std::uint64_t>::max() / lhs) {
    return false;
  }
  *out = lhs * rhs;
  return true;
}

std::uint64_t tier_total(const KvCacheSample& sample) {
  return sample.gpu_bytes + sample.host_bytes + sample.compressed_bytes +
         sample.metadata_bytes + sample.unknown_bytes;
}

}  // namespace

KvCacheLedgerResult make_kv_cache_profile(std::uint32_t layer_count,
                                          std::uint32_t kv_head_count,
                                          std::uint32_t head_dim,
                                          std::uint32_t block_tokens,
                                          std::uint32_t key_bits,
                                          std::uint32_t value_bits,
                                          KvCacheProfile* profile) {
  if (profile == nullptr) {
    return {false, "kv_profile_output_required"};
  }
  if (layer_count == 0 || kv_head_count == 0 || head_dim == 0 ||
      block_tokens == 0 || key_bits == 0 || value_bits == 0) {
    return {false, "kv_profile_metadata_incomplete"};
  }
  if ((key_bits % 8) != 0 || (value_bits % 8) != 0) {
    return {false, "kv_profile_bits_must_be_byte_aligned"};
  }

  std::uint64_t bytes_per_token = 0;
  if (!multiply_u64(layer_count, kv_head_count, &bytes_per_token) ||
      !multiply_u64(bytes_per_token, head_dim, &bytes_per_token) ||
      !multiply_u64(bytes_per_token, key_bits + value_bits,
                    &bytes_per_token)) {
    return {false, "kv_profile_size_overflow"};
  }
  bytes_per_token /= 8;

  std::uint64_t bytes_per_block = 0;
  if (!multiply_u64(bytes_per_token, block_tokens, &bytes_per_block)) {
    return {false, "kv_profile_size_overflow"};
  }

  *profile = KvCacheProfile{layer_count, kv_head_count, head_dim, block_tokens,
                            key_bits, value_bits, bytes_per_token,
                            bytes_per_block};
  return {true, ""};
}

KvCacheLedgerResult KvCacheLedger::record_profile(
    const KvCacheProfile& profile) {
  if (profile.bytes_per_token == 0 || profile.bytes_per_block == 0) {
    return {false, "kv_profile_invalid"};
  }
  profile_ = profile;
  has_profile_ = true;
  return {true, ""};
}

KvCacheLedgerResult KvCacheLedger::sample(
    const KvCacheSample& sample,
    std::uint64_t metadata_budget_bytes) {
  if (!has_profile_) {
    return {false, "kv_profile_required"};
  }
  if (metadata_budget_bytes > 0 &&
      sample.metadata_bytes > metadata_budget_bytes) {
    peak_sample_ = sample;
    return {false, "kv_metadata_budget_exceeded"};
  }
  if (tier_total(sample) > tier_total(peak_sample_)) {
    peak_sample_ = sample;
  }
  return {true, ""};
}

const KvCacheProfile& KvCacheLedger::profile() const { return profile_; }

const KvCacheSample& KvCacheLedger::peak_sample() const {
  return peak_sample_;
}

bool KvCacheLedger::has_profile() const { return has_profile_; }

}  // namespace prisminfer
