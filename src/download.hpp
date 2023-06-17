#ifndef RMAN_DOWNLOAD_HPP
#define RMAN_DOWNLOAD_HPP
#include "file.hpp"
#include "download_opts.hpp"
#include <memory>
#include <filesystem>
#include <map>
#include <fstream>
#include <list>
#include <string_view>
#include <functional>

namespace rman {
    struct ChunkDownload : FileChunk {
        std::vector<uint32_t> offsets = {};
    };

    struct FileDownload;
    struct BundleDownload {
        BundleID id = {};
        std::vector<ChunkDownload> chunks = {};
        std::string range_multi = {};
        std::string range_one = {};
        std::string path = {};
        size_t total_size = {};
        size_t offset_count = {};
        size_t max_uncompressed = {};
        RangeMode range_mode = {};
        std::shared_ptr<FileDownload> file;

        bool can_simplify() const noexcept;

        bool max_range() const noexcept;
    };

    using BundleDownloadList = std::list<std::unique_ptr<BundleDownload>>;

    struct FileDownload {
        BundleDownloadList bundles{};
        std::function<void(bool good, std::unique_ptr<BundleDownload> bundle)> update;
        std::unique_ptr<std::ofstream> outfile;

        static std::shared_ptr<FileDownload> make(FileInfo const& info,
                                                  DownloadOpts const& opts,
                                                  std::string outfolder);
    };

    enum class HttpState {
        Done = 0,
        RecvData,
        RecvR0,
        RecvN0,
        RecvR1,
        RecvN1,
    };

    struct HttpConnection {
        HttpConnection(DownloadOpts const& opts);
        HttpConnection(HttpConnection const&) = delete;
        HttpConnection(HttpConnection&& other) = delete;
        HttpConnection& operator=(HttpConnection const&) = delete;
        HttpConnection& operator=(HttpConnection&& other) = delete;
        ~HttpConnection() noexcept;

        inline void* get_handle() const noexcept { return handle_; }

        void give_bundle(std::unique_ptr<BundleDownload> bundle);

        std::unique_ptr<BundleDownload> take_bundle();

        inline bool is_done() const noexcept {
            return state_ == HttpState::Done && bundle_ && bundle_->chunks.size() == chunk_;
        }
    private:
        void* handle_ = {};
        std::string prefix_ = {};
        std::string archive_ = {};
        std::vector<char> inbuffer_ = {};
        mutable std::vector<char> outbuffer_ = {};
        HttpState state_ = {};
        std::unique_ptr<BundleDownload> bundle_ = {};
        size_t chunk_ = {};
        std::size_t range_pos_ = {};
        RangeMode range_mode_ = {};
        std::unique_ptr<std::ofstream> archivefile_ = {};

        static size_t write_data(char const* p, size_t s, size_t n, HttpConnection *c) noexcept;
        bool write(char const* data, char const* end) noexcept;
        bool write_raw(char const* data, char const* end) noexcept;
        bool write_http(char const* data, char const* end) noexcept;

        char const* receive(char const* data, ptrdiff_t size) noexcept;
        bool decompress(char const* data) const noexcept;
    };

    struct HttpClient {
        HttpClient(DownloadOpts const& opts);
        HttpClient(HttpClient const&) = delete;
        HttpClient(HttpClient&& other) = delete;
        HttpClient& operator=(HttpClient const&) = delete;
        HttpClient& operator=(HttpClient&& other) = delete;
        ~HttpClient() noexcept;

        inline bool finished() const noexcept { return inprogress_.size() == 0; }

        std::size_t canpush() { return !free_.size(); }
        void push(BundleDownloadList& queued);
        void perform();
        void poll(int timeout);

    private:
        void* handle_ = {};
        std::vector<std::unique_ptr<HttpConnection>> connections_ = {};
        std::map<void*, HttpConnection*> inprogress_ = {};
        std::vector<HttpConnection*> free_ = {};
    };
}

#endif // RMAN_DOWNLOAD_HPP
