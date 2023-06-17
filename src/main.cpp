#include <atomic>
#include <fstream>
#include <future>
#include <indicators/dynamic_progress.hpp>
#include <indicators/progress_bar.hpp>
#include <iostream>
#include <mutex>
#include <set>
#include <thread>

#include "cli.hpp"
#include "download.hpp"
#include "error.hpp"
#include "file.hpp"
#include "manifest.hpp"

using namespace rman;
using namespace indicators;

struct Main {
    CLI cli = {};
    FileList manifest = {};
    std::optional<FileList> upgrade = {};
    DynamicProgress<ProgressBar> bars{};

    void parse_args(int argc, char** argv) { cli.parse(argc, argv); }

    void parse_manifest() {
        rman_trace("Manifest file: %s", cli.manifest.c_str());
        manifest = FileList::read(read_file(cli.manifest));
        manifest.filter_langs(cli.langs);
        manifest.filter_path(cli.path);
        manifest.sanitize();
    }

    void parse_upgrade() {
        if (!cli.upgrade.empty()) {
            rman_trace("Upgrade from manifest file: %s", cli.upgrade.c_str());
            upgrade = FileList::read(read_file(cli.upgrade));
            upgrade->filter_langs(cli.langs);
            upgrade->filter_path(cli.path);
            upgrade->sanitize();
            manifest.remove_uptodate(*upgrade);
        }
    }

    void process() {
        switch(cli.action) {
        case Action::List:
            action_list();
            break;
        case Action::ListBundles:
            action_list_bundles();
            break;
        case Action::ListChunks:
            action_list_chunks();
            break;
        case Action::Json:
            action_json();
            break;
        case Action::Download:
            action_download();
            break;
        case Action::Download2:
            action_download2();
            break;
        }
    }

    void action_list() noexcept {
        for(auto& file: manifest.files) {
            if (cli.exist && file.remove_exist(cli.output)) {
                continue;
            }
            if (cli.verify && file.remove_verified(cli.output)) {
                continue;
            }
            std::cout << file.to_csv() << std::endl;
        }
    }

    void action_list_bundles() noexcept {
        auto visited = std::set<BundleID>{};
        for (auto& file: manifest.files) {
            if (cli.exist && file.remove_exist(cli.output)) {
                continue;
            }
            if (cli.verify && file.remove_verified(cli.output)) {
                continue;
            }
            for (auto const& chunk: file.chunks) {
                auto const id = chunk.bundle_id;
                if (visited.find(id) != visited.end()) {
                    continue;
                }
                visited.insert(id);
                std::cout << cli.download.prefix << "/bundles/" << to_hex(chunk.bundle_id) << ".bundle" << std::endl;
            }
        }
        for (auto const& id: manifest.unreferenced) {
            if (visited.find(id) != visited.end()) {
                continue;
            }
            visited.insert(id);
            std::cout << cli.download.prefix << "/bundles/" << to_hex(id) << ".bundle" << std::endl;
        }
    }

    void action_list_chunks() noexcept {
        auto visited = std::set<std::pair<BundleID, ChunkID>>{};
        for (auto& file: manifest.files) {
            if (cli.exist && file.remove_exist(cli.output)) {
                continue;
            }
            if (cli.verify && file.remove_verified(cli.output)) {
                continue;
            }
            for (auto const& chunk: file.chunks) {
                auto const id = std::make_pair(chunk.bundle_id, chunk.id);
                if (visited.find(id) != visited.end()) {
                    continue;
                }
                visited.insert(id);
                std::cout << to_hex(chunk.bundle_id) << '\t'
                          << to_hex(chunk.id) << '\t'
                          << to_hex(chunk.compressed_offset, 8) << '\t'
                          << to_hex(chunk.compressed_size, 8) << '\t'
                          << to_hex(chunk.uncompressed_size, 8) << std::endl;;
            }
        }
    }

    void action_json() noexcept {
        std::cout << '[' << std::endl;
        bool need_separator = false;
        for(auto& file: manifest.files) {
            if (cli.exist && file.remove_exist(cli.output)) {
                continue;
            }
            if (cli.verify && file.remove_verified(cli.output)) {
                continue;
            }
            if (!need_separator) {
                need_separator = true;
                std::cout << file.to_json(2) << std::endl;
            } else {
                std::cout << ',' << file.to_json(2) << std::endl;
            }
        }
        std::cout << ']' << std::endl;
    }

    void action_download() {
        auto client = std::make_unique<HttpClient>(cli.download);
        for (auto& file: manifest.files) {
            ProgressBar bar{option::BarWidth{50},
                            option::ForegroundColor{Color::cyan},
                            option::ShowElapsedTime{true},
                            option::ShowRemainingTime{true},
                            option::PostfixText{"FILE: " + file.path},
                            option::PrefixText{"START!"}};
            bar.print_progress();

            if (cli.exist && file.remove_exist(cli.output)) {
                bar.set_option(option::PrefixText{"SKIP! "});
                bar.mark_as_completed();
                continue;
            }
            if (cli.verify && file.remove_verified(cli.output)) {
                bar.set_option(option::PrefixText{"OK!   "});
                bar.mark_as_completed();
                continue;
            }

            auto filedl = FileDownload::make(file, cli.download, cli.nowrite ? "" : cli.output);
            auto queued = std::move(filedl->bundles);
            auto failed = BundleDownloadList{};

            bar.set_option(option::MinProgress{0});
            bar.set_option(option::MaxProgress{queued.size()});
            filedl->update = [&](bool is_good, std::unique_ptr<BundleDownload> bundle) {
                if (is_good) {
                    bar.tick();
                    bundle.reset();
                } else {
                    failed.push_back(std::move(bundle));
                }
            };

            for (uint32_t tried = 0; !queued.empty() && tried <= cli.download.retry; tried++) {
                bar.set_option(option::PrefixText{"TRY #" + std::to_string(tried)});
                bar.print_progress();
                while (!queued.empty() || !client->finished()) {
                    client->push(queued);
                    client->perform();
                    client->poll(100);
                }
                queued.splice(queued.end(), failed);
            }

            bar.set_option(option::PrefixText{failed.empty() ? "OK!   " : "ERROR!"});
            bar.mark_as_completed();
        }
    }

    void action_download2() {
        bars.set_option(option::HideBarWhenComplete{false});
        auto client = std::make_unique<HttpClient>(cli.download);

        std::mutex mutex{};
        std::condition_variable cond{};
        BundleDownloadList queue_send{};
        enum class State {
            Produced,
            Consumed,
            Finished,
        } state = {};

        std::thread thread{[&, this] {
            BundleDownloadList queue{};
            for (bool running = true; running || !client->finished() || !queue.empty();) {
                if (running && queue.size() <= client->canpush()) {
                    std::unique_lock lock(mutex);
                    if (cond.wait_for(lock, std::chrono::milliseconds{10}, [&, this] { return state != State::Consumed; })) {
                        if (state == State::Finished) {
                            running = false;
                        }
                        queue.splice(queue.end(), queue_send);
                        state = State::Consumed;
                        lock.unlock();
                        cond.notify_one();
                    }
                }
                client->push(queue);
                client->perform();
                client->poll(1);
            }
        }};

        for (auto& file : manifest.files) {
            auto bar = new ProgressBar(option::BarWidth{50},
                                       option::ForegroundColor{Color::cyan},
                                       option::PostfixText{file.path},
                                       option::PrefixText{"START!"});
            auto b = bars.push_back(*bar);

            if (cli.exist && file.remove_exist(cli.output)) {
                bars[b].set_option(option::PrefixText{"SKIP! "});
                bars[b].mark_as_completed();
                continue;
            }
            if (cli.verify && file.remove_verified(cli.output)) {
                bars[b].set_option(option::PrefixText{"OK!   "});
                bars[b].mark_as_completed();
                continue;
            }
            auto filedl = FileDownload::make(file, cli.download, cli.nowrite ? "" : cli.output);
            auto  failed = std::make_shared<bool>(false);
            filedl->update = [this, b, failed](bool is_good, std::unique_ptr<BundleDownload> bundle) {
                if (is_good) {
                    bars[b].tick();
                } else {
                    *failed = true;
                }
                if (bundle->file.use_count() == 1) {
                    bars[b].set_option(option::PrefixText{!*failed ? "OK!   " : "ERROR!"});
                    bars[b].mark_as_completed();
                }
                bundle.reset();
            };
            bars[b].set_option(option::MinProgress{0});
            bars[b].set_option(option::MaxProgress{filedl->bundles.size()});
            bars[b].set_option(option::PrefixText{"DL    "});
            bars[b].print_progress();
            std::unique_lock lockk(mutex);
            cond.wait(lockk, [&, this] { return state == State::Consumed; });
            queue_send = std::move(filedl->bundles);
            state = State::Produced;
            lockk.unlock();
            cond.notify_one();
        }
        // Signal no more
        {
            std::unique_lock lock(mutex);
            cond.wait(lock, [&, this] { return state == State::Consumed; });
            state = State::Finished;
            lock.unlock();
            cond.notify_one();
        }
        thread.join();
    }

    static std::vector<char> read_file(std::string const& filename) {
        std::ifstream file(filename, std::ios::binary);
        rman_assert(file.good());
        auto start = file.tellg();
        file.seekg(0, std::ios::end);
        auto end = file.tellg();
        file.seekg(start, std::ios::beg);
        auto size = end - start;
        rman_assert(size > 0 && size <= INT32_MAX);
        std::vector<char> data;
        data.resize((size_t)size);
        file.read(data.data(), size);
        return data;
    }
};

int main(int argc, char ** argv) {
    auto main = Main{};
    try {
        main.parse_args(argc, argv);
        main.parse_manifest();
        main.parse_upgrade();
        main.process();
    } catch (std::exception const& e) {
        std::cerr << e.what() << std::endl;
        for(auto const& error: error_stack()) {
            std::cerr << error << std::endl;
        }
        error_stack().clear();
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
