#ifndef RMAN_FILE_HPP
#define RMAN_FILE_HPP
#include "manifest.hpp"
#include <filesystem>
#include <fstream>
#include <list>
#include <optional>
#include <regex>
#include <set>
#include <string>
#include <unordered_set>
#include <vector>

namespace rman {
    template <typename T>
    inline std::string to_hex (T id, std::size_t s = 16) noexcept {
        static constexpr char table[] = "0123456789ABCDEF";
        char result[] = "0000000000000000";
        auto num = static_cast<uint64_t>(id);
        auto output = result + (s - 1);
        while (num) {
            *(output--) = table[num & 0xF];
            num >>= 4;
        }
        return std::string(result, s);
    };

    struct FileChunk : RMANChunk {
        BundleID bundle_id;
        uint32_t compressed_offset;
        uint32_t uncompressed_offset;

        bool verify(std::vector<uint8_t> const& buffer, HashType type) const noexcept;
    };

    struct FileInfo {
        FileID id;
        uint32_t size;
        std::string path;
        std::string link;
        std::unordered_set<std::string> langs;
        std::vector<FileChunk> chunks;
        RMANParams params;
        uint8_t permissions;
        uint8_t unk5;
        uint8_t unk6;
        uint8_t unk8;
        uint8_t unk10;

        std::string to_csv() const noexcept;
        std::string to_json(int indent) const noexcept;
        std::ofstream create_file(std::string const& folder_name) const;
        bool remove_exist(std::string const& folder_name) noexcept;
        bool remove_verified(std::string const& folder_name) noexcept;
        bool remove_cached(std::ofstream* outfile, std::filesystem::path const& cache_folder) noexcept;
        bool is_uptodate(FileInfo const& old) const noexcept;
    };

    struct FileList {
        std::list<FileInfo> files;
        std::set<BundleID> unreferenced;

        static FileList from_manifest(RManifest const& manifest);
        static FileList read(char const* data, size_t size);
        inline static FileList read(std::vector<char> const& data) {
            return read(data.data(), data.size());
        }
        void filter_path(std::optional<std::regex> const& pat) noexcept;
        void filter_langs(std::vector<std::string> const& langs) noexcept;
        void remove_uptodate(FileList const& old) noexcept;
        void sanitize() const;
    };
}

#endif // RMAN_FILE_HPP
