#ifndef CPPREF_RH_UNORDERD_MAP_H
#define CPPREF_RH_UNORDERD_MAP_H

#include <memory>
#include <string>
#include <utility>

namespace {

constexpr std::uint32_t MOD_ADLER = 65521;

inline uint32_t
adler32(const uint8_t* bytes, std::size_t len)
{
  std::uint32_t a = 1, b = 0;
  const auto end = bytes + len;
  while (bytes < end) {
    a = (a + *bytes++) % MOD_ADLER;
    b = (b + a) % MOD_ADLER;
  }
  return (b << 16) | a;
}

inline std::size_t
hash(const void* bytes, std::size_t len)
{
  return adler32(static_cast<const uint8_t*>(bytes), len);
}

inline std::size_t
to_index(std::size_t hash, std::size_t bucket_size)
{
  /* TODO: remove modular operator? */
  return (hash >> 7) % bucket_size;
}

inline std::size_t
to_metadata(std::size_t hash)
{
  /* TODO: remove modular operator? */
  return hash & 0x7F;
}

inline std::size_t
linear_probe_next(std::size_t index, std::size_t bucket_size)
{
  /* TODO: remove modular operator. */
  return (index + 1) % bucket_size;
}

/*
 * https://www.youtube.com/watch?v=M2fKMP47slQ&list=WL&index=22&t=810s
 * https://www.youtube.com/watch?v=ncHmEUmJZf4&t=1730s
 * enum ctrl_t {
 *   kEmpty = 0b1000'0000
 *   kDeleted = 0b1111'1110
 *   kSentinel = 0b1111'1111
 *   kFull = 0b0xxx'xxxx
 * }
 *
 * // Position in array (57-bits)
 * size_t H1(size_t hash) { return hash >> 7; }
 * // Metadata (7-bits)
 * size_t H2(size_t hash) { return hash & 0x7F; }
 *
 * iterator find(const K& key, size_t hash) const {
 *   size_t pos = H1(hash) % size;
 *   while (true) {
 *     if (H2(hash) == ctrl[pos] && key == slots[pos] {
 *       return iterator_at(pos);
 *     }
 *     if (ctrl[pos] == kEmpty) {
 *       return end();
 *     }
 *     pos = (pos + 1) % size; // linear probing
 *   }
 * }
 *
 * IMPLEMENTATION DETAILS
 *
 * The table stores elements inline in a slot array. In addition to the slot
 * array the table maintains some control state per slot. The extra state is one
 * byte per slot and stores empty or deleted marks, or alternatively 7 bits from
 * the hash of an occupied slot. The table is split into logical groups of
 * slots, like so:
 *
 *      Group 1         Group 2        Group 3
 * +---------------+---------------+---------------+
 * | | | | | | | | | | | | | | | | | | | | | | | | |
 * +---------------+---------------+---------------+
 * |s|s|s|s|s|s|s|s|s|s|s|s|s|s|s|s|s|s|s|s|s|s|s|s|
 * +---------------+---------------+---------------+
 * s: empty | deleted | 7 bits of hash (occupied)
 *
 * Robin Hood Hash Table
 * template<typename T>
 * struct hashtable_entry {
 *   int8_t distance_from_desired = -1;
 *   union { T value; };
 * }
 *
 * Google's new flat_hash
 * template<typename T>
 * struct hashtable_entry {
 *   union { T value; };
 * }
 * Separated metadata + 7bits of hash
 *
 * On lookup the hash is split into two parts:
 * - H2: 7 bits (those stored in the control bytes)
 * - H1: the rest of the bits
 * The groups are probed using H1. For each group the slots are matched to H2 in
 * parallel. Because H2 is 7 bits (128 states) and the number of slots per group
 * is low (8 or 16) in almost all cases a match in H2 is also a lookup hit.
 *
 * On insert, once the right group is found (as in lookup), its slots are
 * filled in order.
 *
 * On erase a slot is cleared. In case the group did not have any empty slots
 * before the erase, the erased slot is marked as deleted.
 *
 * Groups without empty slots (but maybe with deleted slots) extend the probe
 * sequence. The probing algorithm is quadratic. Given N the number of groups,
 * the probing function for the i'th probe is:
 *
 *   P(0) = H1 % N
 *
 *   P(i) = (P(i - 1) + i) % N
 *
 * This probing function guarantees that after N probes, all the groups of the
 * table will be probed exactly once.
 */
enum class Control
{
  KeyEmpty = 0b1000'0000,
  KeyDelete = 0b1111'1110,
  KeySentinel = 0b1111'1111,
  // KeyFull = 0b0xxx'xxxx
};

/* TODO: try using 'concept'. */
template<typename KeyType, typename ValueType, typename Bucket>
bool
insert_impl(KeyType&& key, ValueType&& value, Bucket buckets, std::size_t bucket_size)
{
  auto size = bucket_size;
  auto h = hash(key.data(), key.size());
  auto metadata = to_metadata(h);

  for (auto index = to_index(h, size);; index = linear_probe_next(index, size)) {
    auto state = ctrl[index];
    auto bucket = buckets[index];
    if (state == metadata && key == bucket) {
      bucket.first = std::forward<KeyType>(key);
      bucket.second = std::forward<ValueType>(value);
      return true;
    }
    if (state == Control::KeyEmpty) {
      bucket.second = std::forward<ValueType>(value);
      return false;
    }
  }
}
}

namespace cppref {
class unordered_map
{
public:
  using key_t = std::string;
  using value_t = int;
  using bucket_t = std::pair<key_t, value_t>[];
  inline static constexpr auto load_factor = 0.875;
  inline static constexpr auto default_table_size = 10;

private:
  std::unique_ptr<bucket_t> buckets;
  /* Should be 16 bytes (SSE instructions operate on 16 bytes: SIMD) */
  std::unique_ptr<uint64> ctrl;

  std::size_t bucket_size{ 0 };
  std::size_t count{ 0 };

public:
  unordered_map()
    : buckets(std::make_unique<bucket_t>(default_table_size))
  {}

  /*
   * TODO: 'insert' and 'find' are similar.
   */
  void insert(const std::pair<key_t, value_t>& kv)
  {
    /* TODO: micro optimization required */
    if (1.0 * count / bucket_size > load_factor) {
      rehash(bucket_size << 1);
    }

    count += insert_impl(kv.first, kv.second, buckets.get(), bucket_size);
  }

  [[nodiscard]] std::optional<value_t> find(const key_t& key) const
  {
    auto size = bucket_size;
    for (auto index = to_index(key.data(), key.size(), size);;
         index = linear_probe_next(index, size)) {
      auto& bucket = buckets[index];
      /* TODO: equal_to */
      if (bucket.first == key) {
        return bucket.second;
      }
      /* TODO: no key */
      if (bucket.first.empty()) {
        return std::optional<value_t>{};
      }
    }
  }

private:
  void rehash(std::size_t new_size)
  {
    auto new_bucket = std::make_unique<bucket_t>(new_size);

    auto cursor = buckets.get();
    const auto end = cursor + bucket_size;
    while (cursor < end) {
      /* If key is not empty, move. */
      if (!cursor->first.empty()) {
        insert_impl(
          std::move(cursor->first), std::move(cursor->second), new_bucket.get(), new_size);
      }
      ++cursor;
    }

    buckets.swap(new_bucket);
    bucket_size = new_size;
  }
};

}

#endif // CPPREF_RH_UNORDERD_MAP_H
