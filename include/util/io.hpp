#ifndef OSRM_INCLUDE_UTIL_IO_HPP_
#define OSRM_INCLUDE_UTIL_IO_HPP_

#include "util/log.hpp"

#include <boost/filesystem.hpp>
#include <boost/numeric/conversion/cast.hpp>

#include <cstddef>
#include <cstdint>

#include <bitset>
#include <fstream>
#include <stxxl/vector>
#include <vector>

#include "storage/io.hpp"
#include "util/fingerprint.hpp"

namespace osrm
{
namespace util
{

inline bool writeFingerprint(std::ostream &stream)
{
    const auto fingerprint = FingerPrint::GetValid();
    stream.write(reinterpret_cast<const char *>(&fingerprint), sizeof(fingerprint));
    return static_cast<bool>(stream);
}

template <typename simple_type>
bool serializeVector(std::ostream &stream, const std::vector<simple_type> &data)
{
    std::uint64_t count = data.size();
    stream.write(reinterpret_cast<const char *>(&count), sizeof(count));
    if (!data.empty())
        stream.write(reinterpret_cast<const char *>(&data[0]), sizeof(simple_type) * count);
    return static_cast<bool>(stream);
}

template <typename simple_type>
bool serializeVector(const std::string &filename, const std::vector<simple_type> &data)
{
    std::ofstream stream(filename, std::ios::binary);

    writeFingerprint(stream);

    return serializeVector(stream, data);
}

// serializes a vector of vectors into an adjacency array (creates a copy of the data internally)
template <typename simple_type>
bool serializeVectorIntoAdjacencyArray(const std::string &filename,
                                       const std::vector<std::vector<simple_type>> &data)
{
    storage::io::FileWriter file(filename, storage::io::FileWriter::HasNoFingerprint);

    std::vector<std::uint32_t> offsets;
    offsets.reserve(data.size() + 1);
    std::uint64_t current_offset = 0;
    offsets.push_back(current_offset);
    for (auto const &vec : data)
    {
        current_offset += vec.size();
        offsets.push_back(boost::numeric_cast<std::uint32_t>(current_offset));
    }

    std::vector<simple_type> all_data;
    all_data.reserve(offsets.back());
    for (auto const &vec : data)
        all_data.insert(all_data.end(), vec.begin(), vec.end());

    file.SerializeVector(offsets);
    file.SerializeVector(all_data);

    return true;
}

template <typename simple_type, std::size_t WRITE_BLOCK_BUFFER_SIZE = 1024>
bool serializeVector(std::ofstream &out_stream, const stxxl::vector<simple_type> &data)
{
    const std::uint64_t size = data.size();
    out_stream.write(reinterpret_cast<const char *>(&size), sizeof(size));

    simple_type write_buffer[WRITE_BLOCK_BUFFER_SIZE];
    std::size_t buffer_len = 0;

    for (const auto entry : data)
    {
        write_buffer[buffer_len++] = entry;

        if (buffer_len >= WRITE_BLOCK_BUFFER_SIZE)
        {
            out_stream.write(reinterpret_cast<const char *>(write_buffer),
                             WRITE_BLOCK_BUFFER_SIZE * sizeof(simple_type));
            buffer_len = 0;
        }
    }

    // write remaining entries
    if (buffer_len > 0)
        out_stream.write(reinterpret_cast<const char *>(write_buffer),
                         buffer_len * sizeof(simple_type));

    return static_cast<bool>(out_stream);
}

template <typename simple_type>
void deserializeAdjacencyArray(const std::string &filename,
                               std::vector<std::uint32_t> &offsets,
                               std::vector<simple_type> &data)
{
    storage::io::FileReader file(filename, storage::io::FileReader::HasNoFingerprint);

    file.DeserializeVector(offsets);
    file.DeserializeVector(data);

    // offsets have to match up with the size of the data
    if (offsets.empty() || (offsets.back() != boost::numeric_cast<std::uint32_t>(data.size())))
        throw util::exception(
            "Error in " + filename +
            (offsets.empty() ? "Offsets are empty" : "Offset and data size do not match") +
            SOURCE_REF);
}

inline bool serializeFlags(const boost::filesystem::path &path, const std::vector<bool> &flags)
{
    // TODO this should be replaced with a FILE-based write using error checking
    std::ofstream flag_stream(path.string(), std::ios::binary);

    writeFingerprint(flag_stream);

    std::uint32_t number_of_bits = flags.size();
    flag_stream.write(reinterpret_cast<const char *>(&number_of_bits), sizeof(number_of_bits));
    // putting bits in ints
    std::uint32_t chunk = 0;
    std::size_t chunk_count = 0;
    for (std::size_t bit_nr = 0; bit_nr < number_of_bits;)
    {
        std::bitset<32> chunk_bitset;
        for (std::size_t chunk_bit = 0; chunk_bit < 32 && bit_nr < number_of_bits;
             ++chunk_bit, ++bit_nr)
            chunk_bitset[chunk_bit] = flags[bit_nr];

        chunk = chunk_bitset.to_ulong();
        ++chunk_count;
        flag_stream.write(reinterpret_cast<const char *>(&chunk), sizeof(chunk));
    }
    Log() << "Wrote " << number_of_bits << " bits in " << chunk_count << " chunks (Flags).";
    return static_cast<bool>(flag_stream);
}

} // namespace util
} // namespace osrm

#endif // OSRM_INCLUDE_UTIL_IO_HPP_
