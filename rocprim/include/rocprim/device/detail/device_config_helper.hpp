// Copyright (c) 2022-2023 Advanced Micro Devices, Inc. All rights reserved.
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.

#ifndef ROCPRIM_DEVICE_DETAIL_CONFIG_HELPER_HPP_
#define ROCPRIM_DEVICE_DETAIL_CONFIG_HELPER_HPP_

#include <type_traits>

#include "../../config.hpp"
#include "../../detail/various.hpp"

#include "../../block/block_load.hpp"
#include "../../block/block_reduce.hpp"
#include "../../block/block_scan.hpp"
#include "../../block/block_store.hpp"

#include "../config_types.hpp"
#include "rocprim/block/block_radix_rank.hpp"
#include "rocprim/block/block_sort.hpp"

/// \addtogroup primitivesmodule_deviceconfigs
/// @{

BEGIN_ROCPRIM_NAMESPACE

namespace detail
{

/// \brief Default values are provided by \p merge_sort_block_sort_config_base.
struct merge_sort_block_sort_config_params
{
    kernel_config_params block_sort_config = {0, 0};
};

// Necessary to construct a parameterized type of `merge_sort_block_sort_config_params`.
// Used in passing to host-side sub-algorithms and GPU kernels so non-default parameters can be available during compile-time.
template<unsigned int BlockSize, unsigned int ItemsPerThread, rocprim::block_sort_algorithm Algo>
struct merge_sort_block_sort_config : rocprim::detail::merge_sort_block_sort_config_params
{
    using sort_config = kernel_config<BlockSize, ItemsPerThread>;
    constexpr merge_sort_block_sort_config()
        : rocprim::detail::merge_sort_block_sort_config_params{sort_config()} {};
};

constexpr unsigned int merge_sort_items_per_thread(const unsigned int item_scale)
{
    if(item_scale <= 4)
    {
        return 8;
    }
    else if(item_scale <= 64)
    {
        return 4;
    }
    return 2;
}
constexpr unsigned int merge_sort_block_size(const unsigned int item_scale)
{
    if(item_scale <= 32)
    {
        return 128;
    }
    else if(item_scale <= 128)
    {
        return 64;
    }
    return 32;
}

// Calculate kernel configurations, such that it will not exceed shared memory maximum
template<class Key, class Value>
struct merge_sort_block_sort_config_base
{
    static constexpr unsigned int item_scale = ::rocprim::max(sizeof(Key), sizeof(Value));
    // multiply by 2 to ensure block_sort's items_per_block >= block_merge's items_per_block
    static constexpr unsigned int block_size       = merge_sort_block_size(item_scale) * 2;
    static constexpr unsigned int items_per_thread = merge_sort_items_per_thread(item_scale);
    using type                                     = merge_sort_block_sort_config<block_size,
                                              items_per_thread,
                                              block_sort_algorithm::stable_merge_sort>;
};

// Calculate kernel configurations, such that it will not exceed shared memory maximum
// No radix_sort_block_sort_params and radix_sort_block_sort_config exist since the only
// configuration member is a kernel_config.
template<class Key, class Value>
struct radix_sort_block_sort_config_base
{
    static constexpr unsigned int item_scale = ::rocprim::max(sizeof(Key), sizeof(Value));

    // multiply by 2 to ensure block_sort's items_per_block >= block_merge's items_per_block
    static constexpr unsigned int block_size = merge_sort_block_size(item_scale) * 2;
    static constexpr unsigned int items_per_thread
        = rocprim::min(4u, merge_sort_items_per_thread(item_scale));
    using type = kernel_config<block_size, items_per_thread>;

    // The items per block should be a power of two, as this is a requirement for the
    // radix sort merge sort.
    static_assert(is_power_of_two(block_size * items_per_thread),
                  "Sorted items per block should be a power of two.");
};

/// \brief Default values are provided by \p merge_sort_block_merge_config_base.
struct merge_sort_block_merge_config_params
{
    kernel_config_params merge_oddeven_config             = {0, 0, 0};
    kernel_config_params merge_mergepath_partition_config = {0, 0};
    kernel_config_params merge_mergepath_config           = {0, 0};
};

// Necessary to construct a parameterized type of `merge_sort_block_merge_config_params`.
// Used in passing to host-side sub-algorithms and GPU kernels so non-default parameters can be available during compile-time.
template<unsigned int OddEvenBlockSize        = 256,
         unsigned int OddEvenItemsPerThread   = 1,
         unsigned int OddEvenSizeLimit        = (1 << 17) + 70000,
         unsigned int PartitionBlockSize      = 128,
         unsigned int MergePathBlockSize      = 128,
         unsigned int MergePathItemsPerThread = 4>
struct merge_sort_block_merge_config : rocprim::detail::merge_sort_block_merge_config_params
{
    constexpr merge_sort_block_merge_config()
        : rocprim::detail::merge_sort_block_merge_config_params{
            {OddEvenBlockSize, OddEvenItemsPerThread, OddEvenSizeLimit},
            {PartitionBlockSize, 1},
            {MergePathBlockSize, MergePathItemsPerThread}
    } {};
};

template<class Key, class Value>
struct merge_sort_block_merge_config_base
{
    static constexpr unsigned int item_scale = ::rocprim::max(sizeof(Key), sizeof(Value));

    static constexpr unsigned int block_size       = merge_sort_block_size(item_scale);
    static constexpr unsigned int items_per_thread = merge_sort_items_per_thread(item_scale);
    using type                                     = merge_sort_block_merge_config<block_size,
                                               1,
                                               (1 << 17) + 70000,
                                               128,
                                               block_size,
                                               items_per_thread>;
};

/// \brief Default values are provided by \p radix_sort_onesweep_config_base.
struct radix_sort_onesweep_config_params
{
    kernel_config_params histogram = {0, 0};
    kernel_config_params sort      = {0, 0};

    /// \brief The number of bits to sort in one onesweep iteration.
    unsigned int radix_bits_per_place = 1;

    /// \brief The internal block radix rank algorithm to use during the onesweep iteration.
    block_radix_rank_algorithm radix_rank_algorithm = block_radix_rank_algorithm::default_algorithm;
};

} // namespace detail

/// \brief Configuration of subalgorithm Onesweep.
///
/// \tparam HistogramConfig - configuration of histogram kernel.
/// \tparam SortConfig - configuration of sort kernel.
/// \tparam RadixBits - number of bits per iteration.
/// \tparam RadixRankAlgorithm - algorithm used for radix rank.
template<class HistogramConfig                = kernel_config<256, 12>,
         class SortConfig                     = kernel_config<256, 12>,
         unsigned int               RadixBits = 4,
         block_radix_rank_algorithm RadixRankAlgorithm
         = block_radix_rank_algorithm::default_algorithm>
struct radix_sort_onesweep_config : detail::radix_sort_onesweep_config_params
{
#ifndef DOXYGEN_SHOULD_SKIP_THIS
    /// \brief Configration of radix sort onesweep histogram kernel.
    using histogram = HistogramConfig;
    /// \brief Configration of radix sort onesweep sort kernel.
    using sort = SortConfig;

    constexpr radix_sort_onesweep_config()
        : radix_sort_onesweep_config_params{
            {HistogramConfig::block_size, HistogramConfig::items_per_thread},
            {     SortConfig::block_size,      SortConfig::items_per_thread},
            RadixBits,
            RadixRankAlgorithm,
    } {};
#endif
};

namespace detail
{

struct reduce_config_tag
{};

// Calculate kernel configurations, such that it will not exceed shared memory maximum
template<class Key, class Value>
struct radix_sort_onesweep_config_base
{
    static constexpr unsigned int item_scale = ::rocprim::max(sizeof(Key), sizeof(Value));

    static constexpr unsigned int block_size = merge_sort_block_size(item_scale) * 4;
    using type                               = radix_sort_onesweep_config<
        kernel_config<256, 12>,
        kernel_config<block_size, ::rocprim::max(1u, 65000u / block_size / item_scale)>,
        4>;
};

struct reduce_config_params
{
    kernel_config_params   reduce_config;
    block_reduce_algorithm block_reduce_method;
};

} // namespace detail

/// \brief Configuration of device-level reduce primitives.
///
/// \tparam BlockSize - number of threads in a block.
/// \tparam ItemsPerThread - number of items processed by each thread.
/// \tparam BlockReduceMethod - algorithm for block reduce.
/// \tparam SizeLimit - limit on the number of items reduced by a single launch
template<unsigned int                      BlockSize      = 256,
         unsigned int                      ItemsPerThread = 8,
         ::rocprim::block_reduce_algorithm BlockReduceMethod
         = ::rocprim::block_reduce_algorithm::default_algorithm,
         unsigned int SizeLimit = ROCPRIM_GRID_SIZE_LIMIT>
struct reduce_config : rocprim::detail::reduce_config_params
{
    /// \brief Identifies the algorithm associated to the config.
    using tag = detail::reduce_config_tag;
    constexpr reduce_config()
        : rocprim::detail::reduce_config_params{
            {BlockSize, ItemsPerThread, SizeLimit},
            BlockReduceMethod
    } {};
};

namespace detail
{

template<class Value>
struct default_reduce_config_base_helper
{
    static constexpr unsigned int item_scale
        = ::rocprim::detail::ceiling_div<unsigned int>(sizeof(Value), sizeof(int));

    using type = reduce_config<limit_block_size<256U, sizeof(Value), ROCPRIM_WARP_SIZE_64>::value,
                               ::rocprim::max(1u, 16u / item_scale),
                               ::rocprim::block_reduce_algorithm::using_warp_reduce>;
};

template<class Value>
struct default_reduce_config_base : default_reduce_config_base_helper<Value>::type
{};

struct scan_config_tag
{};

/// \brief Provides the kernel parameters for exclusive_scan and inclusive_scan based
///        on autotuned configurations or user-provided configurations.
struct scan_config_params
{
    kernel_config_params            kernel_config{};
    ::rocprim::block_load_method    block_load_method{};
    ::rocprim::block_store_method   block_store_method{};
    ::rocprim::block_scan_algorithm block_scan_method{};
};

} // namespace detail

/// \brief Configuration of device-level scan primitives.
///
/// \tparam BlockSize - number of threads in a block.
/// \tparam ItemsPerThread - number of items processed by each thread.
/// \tparam BlockLoadMethod - method for loading input values.
/// \tparam StoreLoadMethod - method for storing values.
/// \tparam BlockScanMethod - algorithm for block scan.
/// \tparam SizeLimit - limit on the number of items for a single scan kernel launch.
template<unsigned int                    BlockSize,
         unsigned int                    ItemsPerThread,
         ::rocprim::block_load_method    BlockLoadMethod,
         ::rocprim::block_store_method   BlockStoreMethod,
         ::rocprim::block_scan_algorithm BlockScanMethod,
         unsigned int                    SizeLimit = ROCPRIM_GRID_SIZE_LIMIT>
struct scan_config : ::rocprim::detail::scan_config_params
{
    /// \brief Identifies the algorithm associated to the config.
    using tag = detail::scan_config_tag;
#ifndef DOXYGEN_SHOULD_SKIP_THIS
    // Requirement dictated by init_lookback_scan_state_kernel.
    static_assert(BlockSize <= ROCPRIM_DEFAULT_MAX_BLOCK_SIZE,
                  "Block size should at most be ROCPRIM_DEFAULT_MAX_BLOCK_SIZE.");

    /// \brief Number of threads in a block.
    static constexpr unsigned int block_size = BlockSize;
    /// \brief Number of items processed by each thread.
    static constexpr unsigned int items_per_thread = ItemsPerThread;
    /// \brief Method for loading input values.
    static constexpr ::rocprim::block_load_method block_load_method = BlockLoadMethod;
    /// \brief Method for storing values.
    static constexpr ::rocprim::block_store_method block_store_method = BlockStoreMethod;
    /// \brief Algorithm for block scan.
    static constexpr ::rocprim::block_scan_algorithm block_scan_method = BlockScanMethod;
    /// \brief Limit on the number of items for a single scan kernel launch.
    static constexpr unsigned int size_limit = SizeLimit;

    constexpr scan_config()
        : ::rocprim::detail::scan_config_params{
            {BlockSize, ItemsPerThread, SizeLimit},
            BlockLoadMethod,
            BlockStoreMethod,
            BlockScanMethod
    } {};
#endif
};

namespace detail
{

struct scan_by_key_config_tag
{};

template<class Value>
struct default_scan_config_base_helper
{
    static constexpr unsigned int item_scale
        = ::rocprim::detail::ceiling_div<unsigned int>(sizeof(Value), sizeof(int));

    using type = scan_config<limit_block_size<256U, sizeof(Value), ROCPRIM_WARP_SIZE_64>::value,
                             ::rocprim::max(1u, 16u / item_scale),
                             ::rocprim::block_load_method::block_load_transpose,
                             ::rocprim::block_store_method::block_store_transpose,
                             ::rocprim::block_scan_algorithm::using_warp_scan>;
};

template<class Value>
struct default_scan_config_base : default_scan_config_base_helper<Value>::type
{};

/// \brief Provides the kernel parameters for exclusive_scan_by_key and inclusive_scan_by_key based
///        on autotuned configurations or user-provided configurations.
struct scan_by_key_config_params
{
    kernel_config_params            kernel_config;
    ::rocprim::block_load_method    block_load_method;
    ::rocprim::block_store_method   block_store_method;
    ::rocprim::block_scan_algorithm block_scan_method;
};

} // namespace detail

/// \brief Configuration of device-level scan-by-key operation.
///
/// \tparam BlockSize - number of threads in a block.
/// \tparam ItemsPerThread - number of items processed by each thread.
/// \tparam BlockLoadMethod - method for loading input values.
/// \tparam StoreLoadMethod - method for storing values.
/// \tparam BlockScanMethod - algorithm for block scan.
/// \tparam SizeLimit - limit on the number of items for a single scan kernel launch.
template<unsigned int                    BlockSize,
         unsigned int                    ItemsPerThread,
         ::rocprim::block_load_method    BlockLoadMethod,
         ::rocprim::block_store_method   BlockStoreMethod,
         ::rocprim::block_scan_algorithm BlockScanMethod,
         unsigned int                    SizeLimit = ROCPRIM_GRID_SIZE_LIMIT>
struct scan_by_key_config : ::rocprim::detail::scan_by_key_config_params
{
    /// \brief Identifies the algorithm associated to the config.
    using tag = detail::scan_by_key_config_tag;
#ifndef DOXYGEN_SHOULD_SKIP_THIS
    // Requirement dictated by init_lookback_scan_state_kernel.
    static_assert(BlockSize <= ROCPRIM_DEFAULT_MAX_BLOCK_SIZE,
                  "Block size should at most be ROCPRIM_DEFAULT_MAX_BLOCK_SIZE.");

    /// \brief Number of threads in a block.
    static constexpr unsigned int block_size = BlockSize;
    /// \brief Number of items processed by each thread.
    static constexpr unsigned int items_per_thread = ItemsPerThread;
    /// \brief Method for loading input values.
    static constexpr ::rocprim::block_load_method block_load_method = BlockLoadMethod;
    /// \brief Method for storing values.
    static constexpr ::rocprim::block_store_method block_store_method = BlockStoreMethod;
    /// \brief Algorithm for block scan.
    static constexpr ::rocprim::block_scan_algorithm block_scan_method = BlockScanMethod;
    /// \brief Limit on the number of items for a single scan kernel launch.
    static constexpr unsigned int size_limit = SizeLimit;

    constexpr scan_by_key_config()
        : ::rocprim::detail::scan_by_key_config_params{
            {BlockSize, ItemsPerThread, SizeLimit},
            BlockLoadMethod,
            BlockStoreMethod,
            BlockScanMethod
    } {};
#endif
};

namespace detail
{

template<class Key, class Value>
struct default_scan_by_key_config_base_helper
{
    static constexpr unsigned int item_scale = ::rocprim::detail::ceiling_div<unsigned int>(
        sizeof(Key) + sizeof(Value), 2 * sizeof(int));

    using type = scan_by_key_config<
        limit_block_size<256U, sizeof(Key) + sizeof(Value), ROCPRIM_WARP_SIZE_64>::value,
        ::rocprim::max(1u, 16u / item_scale),
        ::rocprim::block_load_method::block_load_transpose,
        ::rocprim::block_store_method::block_store_transpose,
        ::rocprim::block_scan_algorithm::using_warp_scan>;
};

template<class Key, class Value>
struct default_scan_by_key_config_base : default_scan_by_key_config_base_helper<Key, Value>::type
{};

struct transform_config_tag
{};

struct transform_config_params
{
    kernel_config_params kernel_config{};
};

} // namespace detail

/// \brief Configuration for the device-level transform operation.
/// \tparam BlockSize Number of threads in a block.
/// \tparam ItemsPerThread Number of items processed by each thread.
/// \tparam SizeLimit Limit on the number of items for a single kernel launch.
template<unsigned int BlockSize,
         unsigned int ItemsPerThread,
         unsigned int SizeLimit = ROCPRIM_GRID_SIZE_LIMIT>
struct transform_config : public detail::transform_config_params
{
    /// \brief Identifies the algorithm associated to the config.
    using tag = detail::transform_config_tag;
#ifndef DOXYGEN_SHOULD_SKIP_THIS

    /// \brief Number of threads in a block.
    static constexpr unsigned int block_size = BlockSize;

    /// \brief Number of items processed by each thread.
    static constexpr unsigned int items_per_thread = ItemsPerThread;

    /// \brief Limit on the number of items for a single kernel launch.
    static constexpr unsigned int size_limit = SizeLimit;

    constexpr transform_config()
        : detail::transform_config_params{
            {BlockSize, ItemsPerThread, SizeLimit}
    }
    {}
#endif
};

namespace detail
{

template<class Value>
struct default_transform_config_base_helper
{
    static constexpr unsigned int item_scale
        = ::rocprim::detail::ceiling_div<unsigned int>(sizeof(Value), sizeof(int));

    using type = transform_config<256, ::rocprim::max(1u, 16u / item_scale)>;
};

template<class Value>
struct default_transform_config_base : default_transform_config_base_helper<Value>::type
{};

struct binary_search_config_tag : public transform_config_tag
{};
struct upper_bound_config_tag : public transform_config_tag
{};
struct lower_bound_config_tag : public transform_config_tag
{};

} // namespace detail

/// \brief Configuration for the device-level binary search operation.
/// \tparam BlockSize Number of threads in a block.
/// \tparam ItemsPerThread Number of items processed by each thread.
/// \tparam SizeLimit Limit on the number of items for a single kernel launch.
template<unsigned int BlockSize,
         unsigned int ItemsPerThread,
         unsigned int SizeLimit = ROCPRIM_GRID_SIZE_LIMIT>
struct binary_search_config : transform_config<BlockSize, ItemsPerThread, SizeLimit>
{
    /// \brief Identifies the algorithm associated to the config.
    using tag = detail::binary_search_config_tag;
};

/// \brief Configuration for the device-level upper bound operation.
/// \tparam BlockSize Number of threads in a block.
/// \tparam ItemsPerThread Number of items processed by each thread.
/// \tparam SizeLimit Limit on the number of items for a single kernel launch.
template<unsigned int BlockSize,
         unsigned int ItemsPerThread,
         unsigned int SizeLimit = ROCPRIM_GRID_SIZE_LIMIT>
struct upper_bound_config : transform_config<BlockSize, ItemsPerThread, SizeLimit>
{
    /// \brief Identifies the algorithm associated to the config.
    using tag = detail::upper_bound_config_tag;
};

/// \brief Configuration for the device-level lower bound operation.
/// \tparam BlockSize Number of threads in a block.
/// \tparam ItemsPerThread Number of items processed by each thread.
/// \tparam SizeLimit Limit on the number of items for a single kernel launch.
template<unsigned int BlockSize,
         unsigned int ItemsPerThread,
         unsigned int SizeLimit = ROCPRIM_GRID_SIZE_LIMIT>
struct lower_bound_config : transform_config<BlockSize, ItemsPerThread, SizeLimit>
{
    /// \brief Identifies the algorithm associated to the config.
    using tag = detail::lower_bound_config_tag;
};

namespace detail
{

struct histogram_config_tag
{};

template<class Value, class Output>
struct default_binary_search_config_base
    : binary_search_config<
          limit_block_size<256U, sizeof(Value) + sizeof(Output), ROCPRIM_WARP_SIZE_64>::value,
          1>
{};

/// \brief Provides the kernel parameters for histogram_even, multi_histogram_even,
///        histogram_range, and multi_histogram_range based on autotuned configurations or
///        user-provided configurations.
struct histogram_config_params
{
    kernel_config_params histogram_config = {0, 0};

    unsigned int max_grid_size          = 0;
    unsigned int shared_impl_max_bins   = 0;
    unsigned int shared_impl_histograms = 0;
};

} // namespace detail

/// \brief Configuration of device-level histogram operation.
///
/// \tparam HistogramConfig - configuration of histogram kernel. Must be \p kernel_config.
/// \tparam MaxGridSize - maximum number of blocks to launch.
/// \tparam SharedImplMaxBins - maximum total number of bins for all active channels
/// for the shared memory histogram implementation (samples -> shared memory bins -> global memory bins),
/// when exceeded the global memory implementation is used (samples -> global memory bins).
/// \tparam SharedImplHistograms - number of histograms in the shared memory to reduce bank conflicts
/// for atomic operations with narrow sample distributions. Sweetspot for 9xx and 10xx is 3.
template<class HistogramConfig,
         unsigned int MaxGridSize          = 1024,
         unsigned int SharedImplMaxBins    = 2048,
         unsigned int SharedImplHistograms = 3>
struct histogram_config : detail::histogram_config_params
{
    /// \brief Identifies the algorithm associated to the config.
    using tag = detail::histogram_config_tag;
#ifndef DOXYGEN_SHOULD_SKIP_THIS
    using histogram = HistogramConfig;

    static constexpr unsigned int max_grid_size          = MaxGridSize;
    static constexpr unsigned int shared_impl_max_bins   = SharedImplMaxBins;
    static constexpr unsigned int shared_impl_histograms = SharedImplHistograms;

    constexpr histogram_config()
        : detail::histogram_config_params{
            HistogramConfig{}, MaxGridSize, SharedImplMaxBins, SharedImplHistograms} {};
#endif
};

namespace detail
{

template<class Sample, unsigned int Channels, unsigned int ActiveChannels>
struct default_histogram_config_base_helper
{
    static constexpr unsigned int item_scale
        = ::rocprim::detail::ceiling_div(sizeof(Sample), sizeof(int));

    using type
        = histogram_config<kernel_config<256, ::rocprim::max(8u / Channels / item_scale, 1u)>>;
};

template<class Sample, unsigned int Channels, unsigned int ActiveChannels>
struct default_histogram_config_base
    : default_histogram_config_base_helper<Sample, Channels, ActiveChannels>::type
{};

struct adjacent_difference_config_tag
{};

struct adjacent_difference_config_params
{
    kernel_config_params          adjacent_difference_kernel_config;
    ::rocprim::block_load_method  block_load_method;
    ::rocprim::block_store_method block_store_method;
};
} // namespace detail

/// \brief Configuration of device-level adjacent difference primitives.
///
/// \tparam BlockSize - number of threads in a block.
/// \tparam ItemsPerThread - number of items processed by each thread.
/// \tparam BlockLoadMethod - method for loading input values.
/// \tparam BlockStoreMethod - method for storing values.
/// \tparam SizeLimit - limit on the number of items for a single adjacent difference kernel launch.
template<unsigned int       BlockSize,
         unsigned int       ItemsPerThread,
         block_load_method  BlockLoadMethod  = block_load_method::block_load_transpose,
         block_store_method BlockStoreMethod = block_store_method::block_store_transpose,
         unsigned int       SizeLimit        = ROCPRIM_GRID_SIZE_LIMIT>
struct adjacent_difference_config : public detail::adjacent_difference_config_params
{
    /// \brief Identifies the algorithm associated to the config.
    using tag = detail::adjacent_difference_config_tag;
#ifndef DOXYGEN_SHOULD_SKIP_THIS
    static constexpr ::rocprim::block_load_method  block_load_method  = BlockLoadMethod;
    static constexpr ::rocprim::block_store_method block_store_method = BlockStoreMethod;
    static constexpr unsigned int                  block_size         = BlockSize;
    static constexpr unsigned int                  items_per_thread   = ItemsPerThread;
    static constexpr unsigned int                  size_limit         = SizeLimit;

    constexpr adjacent_difference_config()
        : detail::adjacent_difference_config_params{
            {BlockSize, ItemsPerThread, SizeLimit},
            BlockLoadMethod, BlockStoreMethod
    } {};
#endif
};

namespace detail
{

template<class Value>
struct default_adjacent_difference_config_base_helper
{
    static constexpr unsigned int item_scale
        = ::rocprim::detail::ceiling_div<unsigned int>(sizeof(Value), sizeof(int));

    using type = adjacent_difference_config<
        limit_block_size<256U, sizeof(Value), ROCPRIM_WARP_SIZE_64>::value,
        ::rocprim::max(1u, 16u / item_scale),
        ::rocprim::block_load_method::block_load_transpose,
        ::rocprim::block_store_method::block_store_transpose>;
};

template<class Value>
struct default_adjacent_difference_config_base
    : default_adjacent_difference_config_base_helper<Value>::type
{};

} // namespace detail

END_ROCPRIM_NAMESPACE

/// @}
// end of group primitivesmodule_deviceconfigs

#endif //ROCPRIM_DEVICE_DETAIL_CONFIG_HELPER_HPP_
