// LHCUI example application (§14) — a mock gamepad music player.
//
// Demonstrates the three Level 4 screens (DirectoryView, LibraryView,
// NowPlayingView) inside a Shell, driven by the global button map in
// DESIGN.md §9.5. All data is mock; nothing touches the filesystem or audio.

#include "hui/types.h"
#include "hui/IRenderer.h"
#include "hui/UISystem.h"
#include "hui/ListSource.h"
#include "hui/Shell.h"
#include "hui/DirectoryView.h"
#include "hui/LibraryView.h"
#include "hui/NowPlayingView.h"
#include "hui/GuideOverlayView.h"
#include "hui/TrackInfoPanelView.h"

#ifdef HUI_ENABLE_KEYBOARD_FALLBACK
#include "hui/sdl/KeyboardFallback.h"
#endif
#include "hui/sdl/SDLGamepadHelper.h"

#ifdef HUI_USE_SDL1
#include <SDL.h>
#include <SDL_ttf.h>
#include "../src/renderer/SDL1Renderer.h"
#else
#include <SDL.h>
#include <SDL_ttf.h>
#include "../src/renderer/SDL2Renderer.h"
#endif

#include <algorithm>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

namespace example {

// ---------------------------------------------------------------------------
// Mock library data
// ---------------------------------------------------------------------------

struct Track {
    std::string title;
    std::string artist;
    std::string album;
    std::string year;
    std::string format;
    std::string bitrate;
    float       seconds = 0.0f;
    std::string durationLabel;   // pre-rendered "m:ss"; RowData borrows it
    bool        unsupported = false;
};

// One entry in a mock directory: either a subfolder or a track.
struct DirEntry {
    std::string name;
    int         childDir = -1;   // index into Library::dirs when a folder
    int         track    = -1;   // index into Library::tracks when a file
};

struct Directory {
    std::string           path;
    int                   parent = -1;
    std::vector<DirEntry> entries;
};

struct Album {
    std::string title;
    std::string artist;
    std::string year;
};

std::string formatTime(float seconds) {
    int total = static_cast<int>(seconds + 0.5f);
    int mins = total / 60;
    int secs = total % 60;
    std::string s = std::to_string(mins) + ":";
    if (secs < 10) s += "0";
    s += std::to_string(secs);
    return s;
}

class Library {
public:
    Library() { build(); }

    std::vector<Track>     tracks;
    std::vector<Directory> dirs;
    std::vector<Album>     albums;

    int rootDir = 0;

    int addTrack(std::string title, std::string artist, std::string album,
                 std::string year, std::string format, std::string bitrate,
                 float seconds, bool unsupported = false) {
        Track t;
        t.title = std::move(title);
        t.artist = std::move(artist);
        t.album = std::move(album);
        t.year = std::move(year);
        t.format = std::move(format);
        t.bitrate = std::move(bitrate);
        t.seconds = seconds;
        t.durationLabel = formatTime(seconds);
        t.unsupported = unsupported;
        tracks.push_back(std::move(t));
        return static_cast<int>(tracks.size()) - 1;
    }

    int addDir(std::string path, int parent) {
        Directory d;
        d.path = std::move(path);
        d.parent = parent;
        dirs.push_back(std::move(d));
        return static_cast<int>(dirs.size()) - 1;
    }

    int seedAlbumDir = 0;   // queue is primed from this folder

private:
    void build() {
        rootDir = addDir("/music", -1);

        const int autechre = addDir("/music/Autechre", rootDir);
        const int triRepetae = addDir("/music/Autechre/Tri Repetae", autechre);
        seedAlbumDir = triRepetae;
        const int amber = addDir("/music/Autechre/Amber", autechre);
        const int boc = addDir("/music/Boards of Canada", rootDir);
        const int mhtrtc = addDir("/music/Boards of Canada/Music Has the Right to Children", boc);

        dirs[rootDir].entries.push_back({"Autechre", autechre, -1});
        dirs[rootDir].entries.push_back({"Boards of Canada", boc, -1});
        // A file the mock decoder cannot read — exercises the disabled row state.
        dirs[rootDir].entries.push_back(
            {"field_recording.wv", -1,
             addTrack("field_recording", "Unknown", "", "", "WavPack", "—", 214.0f, true)});

        dirs[autechre].entries.push_back({"Tri Repetae", triRepetae, -1});
        dirs[autechre].entries.push_back({"Amber", amber, -1});

        albums.push_back({"Tri Repetae", "Autechre", "1995"});
        albums.push_back({"Amber", "Autechre", "1994"});
        albums.push_back({"Music Has the Right to Children", "Boards of Canada", "1998"});

        struct Seed {
            int         dir;
            const char* title;
            const char* artist;
            const char* album;
            const char* year;
            float       seconds;
        };

        const Seed seeds[] = {
            {triRepetae, "Dael",             "Autechre",          "Tri Repetae", "1995", 336.0f},
            {triRepetae, "Clipper",          "Autechre",          "Tri Repetae", "1995", 425.0f},
            {triRepetae, "Leterel",          "Autechre",          "Tri Repetae", "1995", 322.0f},
            {triRepetae, "Rotar",            "Autechre",          "Tri Repetae", "1995", 359.0f},
            {triRepetae, "Stud",             "Autechre",          "Tri Repetae", "1995", 291.0f},
            {triRepetae, "Eutow",            "Autechre",          "Tri Repetae", "1995", 344.0f},
            {triRepetae, "C/Pach",           "Autechre",          "Tri Repetae", "1995", 411.0f},
            {triRepetae, "Overand",          "Autechre",          "Tri Repetae", "1995", 228.0f},
            {triRepetae, "Rsdio",            "Autechre",          "Tri Repetae", "1995", 314.0f},
            {amber,      "Foil",             "Autechre",          "Amber",       "1994", 383.0f},
            {amber,      "Montreal",         "Autechre",          "Amber",       "1994", 340.0f},
            {amber,      "Silverside",       "Autechre",          "Amber",       "1994", 305.0f},
            {amber,      "Slip",             "Autechre",          "Amber",       "1994", 366.0f},
            {mhtrtc,     "Wildlife Analysis","Boards of Canada",  "Music Has the Right to Children", "1998", 76.0f},
            {mhtrtc,     "An Eagle in Your Mind", "Boards of Canada", "Music Has the Right to Children", "1998", 384.0f},
            {mhtrtc,     "Telephasic Workshop",   "Boards of Canada", "Music Has the Right to Children", "1998", 386.0f},
            {mhtrtc,     "Roygbiv",          "Boards of Canada",  "Music Has the Right to Children", "1998", 151.0f},
            {mhtrtc,     "Aquarius",         "Boards of Canada",  "Music Has the Right to Children", "1998", 350.0f},
        };

        for (const Seed& s : seeds) {
            const int idx = addTrack(s.title, s.artist, s.album, s.year, "FLAC", "1006 kbps", s.seconds);
            std::string file = std::to_string(dirs[s.dir].entries.size() + 1);
            if (file.size() < 2) file = "0" + file;
            file += ". " + std::string(s.title) + ".flac";
            dirs[s.dir].entries.push_back({std::move(file), -1, idx});
        }

        dirs[boc].entries.push_back({"Music Has the Right to Children", mhtrtc, -1});
    }
};

// ---------------------------------------------------------------------------
// Player state
// ---------------------------------------------------------------------------

class Player {
public:
    std::vector<int> queue;        // track indices
    int   current  = -1;           // index into queue
    float elapsed  = 0.0f;
    bool  playing  = false;
    bool  shuffle  = false;
    hui::RepeatMode repeat = hui::RepeatMode::Off;

    int currentTrack() const {
        if (current < 0 || current >= static_cast<int>(queue.size())) return -1;
        return queue[current];
    }
};

// ---------------------------------------------------------------------------
// List sources over the application's own data (§6.5 — no row copies)
// ---------------------------------------------------------------------------

class DirectorySource : public hui::IListSource {
public:
    DirectorySource(const Library& lib, const Player& player) : lib_(lib), player_(player) {}

    void setDirectory(int dir) { dir_ = dir; }
    int  directory() const { return dir_; }

    const DirEntry* entryAt(int index) const {
        const auto& entries = lib_.dirs[dir_].entries;
        if (index < 0 || index >= static_cast<int>(entries.size())) return nullptr;
        return &entries[index];
    }

    int rowCount() const override {
        return static_cast<int>(lib_.dirs[dir_].entries.size());
    }

    void rowAt(int index, hui::RowData& out) const override {
        const DirEntry& e = lib_.dirs[dir_].entries[index];
        out.primary = e.name;
        if (e.childDir >= 0) {
            out.variant = hui::ListItemVariant::Folder;
            out.secondary = {};
            out.rightMeta = {};
            return;
        }
        const Track& t = lib_.tracks[e.track];
        out.variant = hui::ListItemVariant::Track;
        out.secondary = t.artist;
        out.rightMeta = t.durationLabel;
        out.disabled = t.unsupported;
        out.playing = (player_.currentTrack() == e.track);
    }

private:
    const Library& lib_;
    const Player&  player_;
    int dir_ = 0;
};

class AllTracksSource : public hui::IListSource {
public:
    AllTracksSource(const Library& lib, const Player& player) : lib_(lib), player_(player) {}

    int rowCount() const override { return static_cast<int>(lib_.tracks.size()); }

    void rowAt(int index, hui::RowData& out) const override {
        const Track& t = lib_.tracks[index];
        out.primary = t.title;
        out.secondary = t.artist;
        out.rightMeta = t.durationLabel;
        out.variant = hui::ListItemVariant::Track;
        out.disabled = t.unsupported;
        out.playing = (player_.currentTrack() == index);
    }

private:
    const Library& lib_;
    const Player&  player_;
};

class AlbumSource : public hui::IListSource {
public:
    explicit AlbumSource(const Library& lib) : lib_(lib) {}

    int rowCount() const override { return static_cast<int>(lib_.albums.size()); }

    void rowAt(int index, hui::RowData& out) const override {
        const Album& a = lib_.albums[index];
        out.primary = a.title;
        out.secondary = a.artist;
        out.rightMeta = a.year;
    }

private:
    const Library& lib_;
};

class QueueSource : public hui::IListSource {
public:
    QueueSource(const Library& lib, const Player& player) : lib_(lib), player_(player) {}

    int rowCount() const override { return static_cast<int>(player_.queue.size()); }

    void rowAt(int index, hui::RowData& out) const override {
        const Track& t = lib_.tracks[player_.queue[index]];
        out.primary = t.title;
        out.secondary = t.album;
        out.rightMeta = t.durationLabel;
        out.variant = hui::ListItemVariant::Track;
        out.playing = (index == player_.current);
    }

private:
    const Library& lib_;
    const Player&  player_;
};

// ---------------------------------------------------------------------------
// Application — owns the screens and wires them to the player
// ---------------------------------------------------------------------------

class App {
public:
    App(hui::UISystem& ui, hui::Shell& shell)
        : ui_(ui)
        , shell_(shell)
        , stack_(ui.viewStack())
        , dirSource_(library_, player_)
        , allTracks_(library_, player_)
        , albums_(library_)
        , queueSource_(library_, player_) {
        buildQueue();
    }

    void start() {
        dirSource_.setDirectory(library_.rootDir);

        // The root browser is pushed once and never popped: ViewStack::pop()
        // no-ops at depth 1, so this pointer stays valid for the whole session.
        auto view = std::make_unique<hui::DirectoryView>(stack_, &shell_);
        directoryView_ = view.get();
        wireDirectory(*directoryView_);
        directoryView_->setSource(&dirSource_);
        directoryView_->setHeaderPath(library_.dirs[library_.rootDir].path);
        directoryView_->setSortBadge("Name");
        stack_.push(std::move(view));

        // Stack mutations are deferred to the next update (§8.2). Realise this
        // one now so the first button of the session has a screen to land on.
        stack_.applyPendingMutations(ui_.focusManager());

        refreshChrome();
    }

    bool running() const { return running_; }

    // Global accelerators (§9.5) for buttons no screen consumed.
    bool onGlobalButton(hui::Button b) {
        switch (b) {
            case hui::Button::Start:  togglePlayPause();  return true;
            case hui::Button::Select: showNowPlaying();   return true;
            case hui::Button::Guide:  openGuide();        return true;
            case hui::Button::B:      goBack();           return true;
            default: return false;
        }
    }

    void update(float dt) {
        advancePlayback(dt);
        refreshChrome();
    }

    // --- Inspection, used by the scripted self-test ------------------------

    hui::ViewStack& stack() { return stack_; }
    int directoryFocusIndex() const { return directoryView_->list().getFocusIndex(); }
    int currentDirectory() const { return dirSource_.directory(); }
    const Player& player() const { return player_; }

    bool topIsNowPlaying() { return topAs<hui::NowPlayingView>() != nullptr; }
    bool topIsLibrary()    { return topAs<hui::LibraryView>() != nullptr; }
    bool topIsDirectory()  { return topAs<hui::DirectoryView>() != nullptr; }

private:
    // Views other than the root are owned by the ViewStack and destroyed on pop,
    // so they are located by type rather than held (RTTI-free, via View::isType).
    template <typename T>
    T* topAs() {
        hui::View* t = stack_.top();
        return (t && t->isType<T>()) ? static_cast<T*>(t) : nullptr;
    }

    // --- Screen wiring ---------------------------------------------------

    void wireDirectory(hui::DirectoryView& view) {
        view.setOnActivate([this](int index) { activateDirectoryRow(index); });

        view.setOnBuildContextMenu([this](int index, hui::VectorListSource& menu) {
            const DirEntry* e = dirSource_.entryAt(index);
            const bool isTrack = e && e->track >= 0;
            menu.add(isTrack ? "Play Now" : "Open");
            menu.add("Add to Queue", {}, {}, hui::ListItemVariant::Default, 0, false, !isTrack);
            menu.add("Track Info", {}, {}, hui::ListItemVariant::Default, 0, false, !isTrack);
            menu.add("Go to Library");
        });

        view.setOnContextAction([this](int menuIndex, int itemIndex) {
            const DirEntry* e = dirSource_.entryAt(itemIndex);
            switch (menuIndex) {
                case 0: activateDirectoryRow(itemIndex); break;
                case 1: enqueueDirectoryRow(itemIndex); break;
                case 2: openTrackInfo(e && e->track >= 0 ? e->track : -1); break;
                case 3: showLibrary(); break;
                default: break;
            }
        });
    }

    void wireLibrary(hui::LibraryView& view) {
        view.setListSource(&allTracks_);
        view.setGridSource(&albums_);

        view.setOnActivate([this](int index, int tab) {
            if (tab == 0) {
                playTrack(index);
                showNowPlaying();
            } else if (index >= 0 && index < static_cast<int>(library_.albums.size())) {
                shell_.showToast("Album: " + library_.albums[index].title, 2.0f);
            }
        });

        view.setOnBuildContextMenu([](int, int tab, hui::VectorListSource& menu) {
            menu.add(tab == 0 ? "Play Now" : "Play Album");
            menu.add("Add to Queue");
            menu.add("Track Info", {}, {}, hui::ListItemVariant::Default, 0, false, tab != 0);
        });

        view.setOnContextAction([this](int menuIndex, int itemIndex, int tab) {
            if (tab != 0) {
                shell_.showToast("Album action", 1.5f);
                return;
            }
            if (menuIndex == 0)      playTrack(itemIndex);
            else if (menuIndex == 1) enqueueTrack(itemIndex);
            else if (menuIndex == 2) openTrackInfo(itemIndex);
        });

        view.setOnLetterChanged([this](char c) {
            shell_.showToast(std::string("Jump to ") + c, 1.2f);
        });
    }

    void wireNowPlaying(hui::NowPlayingView& view) {
        view.setQueueSource(&queueSource_);
        view.setShuffle(player_.shuffle);
        view.setRepeatMode(player_.repeat);

        view.setOnSeek([this](int direction) { seek(direction * 10.0f); });

        view.setOnTransport([this](hui::TransportAction action) {
            switch (action) {
                case hui::TransportAction::PlayPause: togglePlayPause(); break;
                case hui::TransportAction::Previous:  skip(-1); break;
                case hui::TransportAction::Next:      skip(1); break;
                case hui::TransportAction::Shuffle:   toggleShuffle(); break;
                case hui::TransportAction::Repeat:    cycleRepeat(); break;
            }
        });

        view.setOnQueueActivate([this](int index) { playQueueIndex(index); });

        view.setOnBuildContextMenu([](int, hui::VectorListSource& menu) {
            menu.add("Track Info");
            menu.add("Remove from Queue", {}, {}, hui::ListItemVariant::Default, 0, false, false, true);
        });

        view.setOnContextAction([this](int menuIndex, int itemIndex) {
            if (itemIndex < 0 || itemIndex >= static_cast<int>(player_.queue.size())) return;
            if (menuIndex == 0)      openTrackInfo(player_.queue[itemIndex]);
            else if (menuIndex == 1) removeFromQueue(itemIndex);
        });
    }

    // --- Navigation ------------------------------------------------------

    void activateDirectoryRow(int index) {
        const DirEntry* e = dirSource_.entryAt(index);
        if (!e) return;

        if (e->childDir >= 0) {
            openDirectory(e->childDir);
            return;
        }
        if (library_.tracks[e->track].unsupported) {
            shell_.showToast("Unsupported format", 2.0f);
            return;
        }
        playTrack(e->track);
        showNowPlaying();
    }

    void openDirectory(int dir) {
        dirSource_.setDirectory(dir);
        directoryView_->setHeaderPath(library_.dirs[dir].path);
        directoryView_->list().resetFocus();
        directoryView_->list().notifyRowsChanged();
        directoryView_->setSource(&dirSource_);
    }

    void goBack() {
        // Overlays consume B themselves; this only ever sees screens (§9.5).
        if (stack_.depth() > 1) {
            stack_.pop();
            return;
        }
        const int parent = library_.dirs[dirSource_.directory()].parent;
        if (parent >= 0) {
            openDirectory(parent);
            return;
        }
        running_ = false;
    }

    void showLibrary() {
        if (topAs<hui::LibraryView>()) return;
        auto view = std::make_unique<hui::LibraryView>(stack_);
        wireLibrary(*view);
        stack_.push(std::move(view));
    }

    void showNowPlaying() {
        if (topAs<hui::NowPlayingView>()) return;
        if (player_.current < 0) {
            shell_.showToast("Nothing playing", 1.5f);
            return;
        }
        auto view = std::make_unique<hui::NowPlayingView>(stack_);
        wireNowPlaying(*view);
        stack_.push(std::move(view));
    }

    void openGuide() {
        auto guide = std::make_unique<hui::GuideOverlayView>(stack_, ui_.animationsEnabled());
        guide->masterVolumeSlider().setOnValueChanged([this](int v) {
            shell_.showToast("Volume " + std::to_string(v) + "%", 1.0f);
        });
        guide->setOnSettings([this] { shell_.showToast("Settings (not implemented)", 1.5f); });
        guide->setOnEqualizer([this] { shell_.showToast("Equalizer (not implemented)", 1.5f); });
        stack_.push(std::move(guide));
    }

    void openTrackInfo(int trackIndex) {
        if (trackIndex < 0) return;
        stack_.push(std::make_unique<hui::TrackInfoPanelView>(stack_, metadataFor(trackIndex)));
    }

    // --- Playback --------------------------------------------------------

    void buildQueue() {
        // Seed the queue with one album so Now Playing has content from the start.
        for (const DirEntry& e : library_.dirs[library_.seedAlbumDir].entries) {
            if (e.track >= 0) player_.queue.push_back(e.track);
        }
        if (!player_.queue.empty()) player_.current = 0;
    }

    void playTrack(int trackIndex) {
        if (trackIndex < 0 || trackIndex >= static_cast<int>(library_.tracks.size())) return;
        if (library_.tracks[trackIndex].unsupported) {
            shell_.showToast("Unsupported format", 2.0f);
            return;
        }

        auto it = std::find(player_.queue.begin(), player_.queue.end(), trackIndex);
        if (it == player_.queue.end()) {
            player_.queue.push_back(trackIndex);
            it = player_.queue.end() - 1;
        }
        player_.current = static_cast<int>(it - player_.queue.begin());
        player_.elapsed = 0.0f;
        player_.playing = true;
        queueChanged();
        shell_.showToast("Playing: " + library_.tracks[trackIndex].title, 2.0f);
    }

    void playQueueIndex(int index) {
        if (index < 0 || index >= static_cast<int>(player_.queue.size())) return;
        player_.current = index;
        player_.elapsed = 0.0f;
        player_.playing = true;
    }

    void enqueueTrack(int trackIndex) {
        if (trackIndex < 0 || trackIndex >= static_cast<int>(library_.tracks.size())) return;
        player_.queue.push_back(trackIndex);
        queueChanged();
        shell_.showToast("Added to queue", 1.5f);
    }

    void enqueueDirectoryRow(int index) {
        const DirEntry* e = dirSource_.entryAt(index);
        if (e && e->track >= 0) enqueueTrack(e->track);
    }

    void removeFromQueue(int index) {
        if (index < 0 || index >= static_cast<int>(player_.queue.size())) return;
        player_.queue.erase(player_.queue.begin() + index);
        if (player_.queue.empty()) {
            player_.current = -1;
            player_.playing = false;
        } else if (index < player_.current) {
            --player_.current;
        } else if (index == player_.current) {
            player_.current = std::min(player_.current, static_cast<int>(player_.queue.size()) - 1);
            player_.elapsed = 0.0f;
        }
        queueChanged();
        shell_.showToast("Removed from queue", 1.5f);
    }

    void queueChanged() {
        if (auto* np = topAs<hui::NowPlayingView>()) np->queue().notifyRowsChanged();
        if (directoryView_) directoryView_->list().notifyRowsChanged();
    }

    void togglePlayPause() {
        if (player_.current < 0) {
            shell_.showToast("Nothing to play", 1.5f);
            return;
        }
        player_.playing = !player_.playing;
        shell_.showToast(player_.playing ? "Play" : "Pause", 1.0f);
    }

    void skip(int direction) {
        if (player_.queue.empty()) return;
        const int count = static_cast<int>(player_.queue.size());
        player_.current = ((player_.current + direction) % count + count) % count;
        player_.elapsed = 0.0f;
        queueChanged();
    }

    void seek(float deltaSeconds) {
        const int track = player_.currentTrack();
        if (track < 0) return;
        player_.elapsed = std::clamp(player_.elapsed + deltaSeconds, 0.0f, library_.tracks[track].seconds);
        shell_.showToast(deltaSeconds < 0 ? "<< 10s" : ">> 10s", 1.0f);
    }

    void toggleShuffle() {
        player_.shuffle = !player_.shuffle;
        if (auto* np = topAs<hui::NowPlayingView>()) np->setShuffle(player_.shuffle);
        shell_.showToast(player_.shuffle ? "Shuffle on" : "Shuffle off", 1.2f);
    }

    void cycleRepeat() {
        switch (player_.repeat) {
            case hui::RepeatMode::Off: player_.repeat = hui::RepeatMode::All; break;
            case hui::RepeatMode::All: player_.repeat = hui::RepeatMode::One; break;
            case hui::RepeatMode::One: player_.repeat = hui::RepeatMode::Off; break;
        }
        if (auto* np = topAs<hui::NowPlayingView>()) np->setRepeatMode(player_.repeat);
        const char* label = player_.repeat == hui::RepeatMode::Off ? "Repeat off"
                          : player_.repeat == hui::RepeatMode::All ? "Repeat all" : "Repeat one";
        shell_.showToast(label, 1.2f);
    }

    void advancePlayback(float dt) {
        const int track = player_.currentTrack();
        if (!player_.playing || track < 0) return;

        // 8x so a demo session actually reaches the end of a track.
        player_.elapsed += dt * 8.0f;
        if (player_.elapsed < library_.tracks[track].seconds) return;

        player_.elapsed = 0.0f;
        if (player_.repeat == hui::RepeatMode::One) return;
        if (player_.current + 1 >= static_cast<int>(player_.queue.size())
            && player_.repeat == hui::RepeatMode::Off) {
            player_.playing = false;
            return;
        }
        skip(1);
    }

    // --- Chrome ----------------------------------------------------------

    void refreshChrome() {
        const int track = player_.currentTrack();

        auto* nowPlaying = topAs<hui::NowPlayingView>();
        if (nowPlaying)                       shell_.setViewMode("NOW PLAYING");
        else if (topAs<hui::LibraryView>())   shell_.setViewMode("LIBRARY");
        else                                  shell_.setViewMode("BROWSE");

        if (track >= 0) {
            shell_.setContextLabel(library_.tracks[track].title + " - " + library_.tracks[track].artist);
        } else {
            shell_.setContextLabel(library_.dirs[dirSource_.directory()].path);
        }
        shell_.setNowPlaying(player_.playing);

        if (!nowPlaying) return;

        nowPlaying->setPlaybackState(player_.playing ? hui::PlaybackState::Playing
                                                     : hui::PlaybackState::Paused);
        if (track >= 0) {
            const float total = library_.tracks[track].seconds;
            nowPlaying->setProgress(total > 0.0f ? player_.elapsed / total : 0.0f,
                                    player_.elapsed, total);
        } else {
            nowPlaying->setProgress(0.0f, 0.0f, 0.0f);
        }
    }

    hui::TrackMetadata metadataFor(int trackIndex) const {
        hui::TrackMetadata m;
        if (trackIndex < 0 || trackIndex >= static_cast<int>(library_.tracks.size())) return m;
        const Track& t = library_.tracks[trackIndex];
        m.title = t.title;
        m.artist = t.artist;
        m.album = t.album;
        m.year = t.year;
        m.genre = "Electronic";
        m.duration = t.durationLabel;
        m.format = t.format;
        m.bitrate = t.bitrate;
        return m;
    }

    hui::UISystem& ui_;
    hui::Shell&    shell_;
    hui::ViewStack& stack_;

    Library library_;
    Player  player_;

    DirectorySource dirSource_;
    AllTracksSource allTracks_;
    AlbumSource     albums_;
    QueueSource     queueSource_;

    hui::DirectoryView* directoryView_ = nullptr;   // owned by the stack, never popped

    bool running_ = true;
};

// ---------------------------------------------------------------------------
// Scripted self-test (--selftest)
//
// Drives the app through the Phase 14 QA navigation paths with no window and no
// user. Exists because those paths cannot be checked by hand in CI, and because
// running it under ASan catches lifetime bugs in the view push/pop cycle.
// ---------------------------------------------------------------------------

class SelfTest {
public:
    SelfTest(hui::UISystem& ui, App& app, hui::Shell& shell) : ui_(ui), app_(app), shell_(shell) {}

    int run() {
        press(hui::Button::Down);                       // move off row 0
        const int browsedRow = app_.directoryFocusIndex();
        check(browsedRow == 1, "Down moves directory focus to row 1");

        press(hui::Button::Up);
        press(hui::Button::A);                          // enter /music/Autechre
        check(app_.topIsDirectory(), "folder activation stays in DirectoryView");
        check(app_.currentDirectory() != 0, "folder activation changed directory");

        press(hui::Button::A);                          // enter Tri Repetae
        press(hui::Button::Down);
        press(hui::Button::Down);
        const int trackRow = app_.directoryFocusIndex();
        press(hui::Button::A);                          // play a track
        check(app_.topIsNowPlaying(), "activating a track opens NowPlayingView");
        check(app_.player().playing, "activating a track starts playback");

        // Transport row: Left/Right must reach all five segments, A must fire.
        press(hui::Button::Up);                         // queue -> transport
        press(hui::Button::Up);                         // transport -> seek bar
        press(hui::Button::Down);                       // back to transport
        const bool wasPlaying = app_.player().playing;
        press(hui::Button::A);                          // play/pause segment
        check(app_.player().playing != wasPlaying, "A on the transport row toggles play/pause");
        for (int i = 0; i < 5; ++i) press(hui::Button::Right);
        press(hui::Button::A);
        for (int i = 0; i < 5; ++i) press(hui::Button::Left);

        // Seek bar must respond to both Left/Right and L2/R2 (§9.5).
        press(hui::Button::Up);                         // transport -> seek bar
        const float before = app_.player().elapsed;
        press(hui::Button::Right);
        check(app_.player().elapsed > before, "Left/Right seeks while the seek bar is focused");
        const float afterDpad = app_.player().elapsed;
        press(hui::Button::L2);
        check(app_.player().elapsed < afterDpad, "L2 seeks backward");

        // Overlays: open and dismiss each, confirming the stack unwinds.
        const size_t depthBefore = app_.stack().depth();
        press(hui::Button::X);
        check(app_.stack().depth() == depthBefore + 1, "X opens the context menu");
        press(hui::Button::B);
        check(app_.stack().depth() == depthBefore, "B closes the context menu");

        press(hui::Button::Guide);
        check(app_.stack().depth() == depthBefore + 1, "Guide opens the guide overlay");
        press(hui::Button::B);
        check(app_.stack().depth() == depthBefore, "B closes the guide overlay");

        // Back to the browser; focus must land where it was left.
        press(hui::Button::B);
        check(app_.topIsDirectory(), "B returns from NowPlayingView to the browser");
        check(app_.directoryFocusIndex() == trackRow, "browser focus index survives the round trip");

        // Select jumps to Now Playing from anywhere, then B returns.
        press(hui::Button::Select);
        check(app_.topIsNowPlaying(), "Select jumps to NowPlayingView");
        press(hui::Button::B);
        check(app_.topIsDirectory(), "B returns to the browser");

        // Start toggles playback without leaving the browser.
        const bool playing = app_.player().playing;
        press(hui::Button::Start);
        check(app_.player().playing != playing, "Start toggles play/pause from the browser");
        check(app_.topIsDirectory(), "Start does not change screens");

        // Climb back to /music, where the unsupported file lives.
        press(hui::Button::B);
        press(hui::Button::B);
        const int rootDir = app_.currentDirectory();
        check(app_.topIsDirectory() && app_.running(), "B walks up the tree without quitting");

        // The disabled row must be reachable by focus but refuse activation.
        press(hui::Button::R2);                         // jump to last row
        press(hui::Button::A);
        check(app_.topIsDirectory() && app_.currentDirectory() == rootDir,
              "activating an unsupported track does not navigate");

        // Library: both tabs render, and L1/R1 switches between them.
        openLibrary();
        check(app_.topIsLibrary(), "the library is reachable from the browser");
        press(hui::Button::Down);
        press(hui::Button::Down);
        press(hui::Button::R1);                         // list tab -> grid tab
        press(hui::Button::Down);
        press(hui::Button::L1);                         // back to the list tab
        check(app_.topIsLibrary(), "L1/R1 switches library tabs without leaving the view");
        press(hui::Button::B);
        check(app_.topIsDirectory(), "B leaves the library");

        mash();
        check(app_.running(), "button mashing left the app running");

        std::cout << "\nself-test: " << passed_ << " passed, " << failed_ << " failed\n";
        return failed_ == 0 ? 0 : 1;
    }

private:
    void pump(int frames = 3) {
        for (int i = 0; i < frames; ++i) {
            ui_.update(kDt);
            app_.update(kDt);
            shell_.update(kDt);
        }
    }

    void press(hui::Button b) {
        ui_.onButtonDown(b);
        pump(1);
        ui_.onButtonUp(b);
        pump();
    }

    // The library has no shortcut by design (§9.5); it hangs off the browser's
    // context menu, which is the last entry.
    void openLibrary() {
        press(hui::Button::X);
        for (int i = 0; i < 3; ++i) press(hui::Button::Down);
        press(hui::Button::A);
    }

    // Holds several buttons at once and releases them out of order, which is
    // what actual mashing looks like to ChordDetector and KeyRepeatDriver.
    // B is excluded: at the root it quits by design, which would end the run
    // early and prove nothing about crash resistance.
    void mash() {
        const hui::Button all[] = {
            hui::Button::Up, hui::Button::Down, hui::Button::Left, hui::Button::Right,
            hui::Button::A, hui::Button::X, hui::Button::Y,
            hui::Button::L1, hui::Button::L2, hui::Button::R1, hui::Button::R2,
            hui::Button::Start, hui::Button::Select, hui::Button::Guide,
        };
        const int count = static_cast<int>(sizeof(all) / sizeof(all[0]));

        unsigned seed = 12345;
        auto next = [&seed] { seed = seed * 1103515245u + 12345u; return (seed >> 16) & 0x7fff; };

        for (int round = 0; round < 400 && app_.running(); ++round) {
            hui::Button held[4];
            const int n = 1 + static_cast<int>(next() % 4);
            for (int i = 0; i < n; ++i) {
                held[i] = all[next() % count];
                ui_.onButtonDown(held[i]);
            }
            pump(1);
            for (int i = n - 1; i >= 0; --i) {
                ui_.onButtonUp(held[i]);
            }
            pump(1);
        }
    }

    void check(bool ok, const char* what) {
        if (ok) {
            ++passed_;
        } else {
            ++failed_;
            std::cerr << "  FAIL: " << what << "\n";
        }
    }

    static constexpr float kDt = 1.0f / 60.0f;

    hui::UISystem& ui_;
    App&           app_;
    hui::Shell&    shell_;
    int passed_ = 0;
    int failed_ = 0;
};

hui::Theme makeTheme(hui::FontHandle body, hui::FontHandle small) {
    hui::Theme theme{};
    theme.background       = {18, 20, 26, 255};
    theme.surface          = {28, 32, 42, 255};
    theme.surfaceAlt       = {38, 44, 58, 255};
    theme.accent           = {70, 160, 245, 255};
    theme.textPrimary      = {245, 248, 255, 255};
    theme.textSecondary    = {150, 160, 180, 255};
    theme.textDisabled     = {90, 95, 110, 255};
    theme.warning          = {245, 80, 80, 255};
    theme.success          = {80, 220, 110, 255};
    theme.overlay          = {0, 0, 0, 175};
    theme.focusBorderColor = {90, 175, 255, 255};
    theme.focusBorderWidth = 2;
    theme.focusFillColor   = {35, 55, 85, 255};
    theme.fontBody         = body;
    theme.fontSmall        = small;
    theme.fontMono         = small;
    theme.fontBodySize     = 15;
    theme.fontSmallSize    = 12;
    return theme;
}

void printControls() {
    std::cout <<
        "\n=========================================================\n"
        "  LHCUI example — mock gamepad music player\n"
        "=========================================================\n"
        "  Button map (DESIGN.md 9.5)      Keyboard\n"
        "  ---------------------------------------------------\n"
        "  D-pad move focus                Arrow keys\n"
        "  A     activate                  Z\n"
        "  B     back / quit at root       X\n"
        "  X     context menu              A or C\n"
        "  Y     queue grab mode           S or V\n"
        "  Start play/pause                Enter\n"
        "  Sel.  jump to Now Playing       Tab\n"
        "  L1/R1 prev / next track         Q / E\n"
        "  L2/R2 seek -10s / +10s          W / R\n"
        "  Guide guide overlay             Esc, G or F12\n"
        "=========================================================\n\n" << std::flush;
}

} // namespace example

int main(int argc, char* argv[]) {
    bool selfTest = false;
    for (int i = 1; i < argc; ++i) {
        if (std::string(argv[i]) == "--selftest") selfTest = true;
    }
    if (selfTest) {
        // No window server needed, so this runs in CI and under sanitizers.
#ifdef HUI_USE_SDL1
        setenv("SDL_VIDEODRIVER", "dummy", 1);
#else
        SDL_SetHint(SDL_HINT_VIDEODRIVER, "dummy");
#endif
    }

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_JOYSTICK) < 0) {
        std::cerr << "SDL_Init failed: " << SDL_GetError() << "\n";
        return 1;
    }
    if (TTF_Init() < 0) {
        std::cerr << "TTF_Init failed: " << TTF_GetError() << "\n";
        SDL_Quit();
        return 1;
    }

    const int screenW = 640;
    const int screenH = 480;

    std::unique_ptr<hui::IRenderer> renderer;

#ifdef HUI_USE_SDL1
    SDL_Surface* screen = SDL_SetVideoMode(screenW, screenH, 32, SDL_SWSURFACE);
    if (!screen) {
        std::cerr << "SDL_SetVideoMode failed: " << SDL_GetError() << "\n";
        TTF_Quit();
        SDL_Quit();
        return 1;
    }
    SDL_WM_SetCaption("LHCUI Example", nullptr);
    renderer = std::make_unique<hui::SDL1Renderer>(screen);
#else
    SDL_Window* window = SDL_CreateWindow("LHCUI Example — Music Player",
                                          SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                                          screenW, screenH, SDL_WINDOW_SHOWN);
    if (!window) {
        std::cerr << "SDL_CreateWindow failed: " << SDL_GetError() << "\n";
        TTF_Quit();
        SDL_Quit();
        return 1;
    }
    SDL_Renderer* sdlRenderer = SDL_CreateRenderer(window, -1,
                                                   SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!sdlRenderer && selfTest) {
        // Dummy video driver has no GPU; fall back to software for headless self-test.
        sdlRenderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_SOFTWARE);
    }
    if (!sdlRenderer) {
        std::cerr << "SDL_CreateRenderer failed: " << SDL_GetError() << "\n";
        SDL_DestroyWindow(window);
        TTF_Quit();
        SDL_Quit();
        return 1;
    }
    renderer = std::make_unique<hui::SDL2Renderer>(sdlRenderer);
#endif

    TTF_Font* bodyFont = TTF_OpenFont("assets/Roboto-Regular.ttf", 15);
    TTF_Font* smallFont = TTF_OpenFont("assets/Roboto-Regular.ttf", 12);
    if (!bodyFont) {
        bodyFont = TTF_OpenFont("../assets/Roboto-Regular.ttf", 15);
        smallFont = TTF_OpenFont("../assets/Roboto-Regular.ttf", 12);
    }
    if (!bodyFont) {
        std::cerr << "Failed to load assets/Roboto-Regular.ttf — run from the repo root or build/\n";
    }

    hui::FontHandle fontBody = 0;
    hui::FontHandle fontSmall = 0;
    if (bodyFont) {
#ifdef HUI_USE_SDL1
        auto* concrete = static_cast<hui::SDL1Renderer*>(renderer.get());
#else
        auto* concrete = static_cast<hui::SDL2Renderer*>(renderer.get());
#endif
        fontBody = concrete->registerFont(bodyFont);
        fontSmall = smallFont ? concrete->registerFont(smallFont) : fontBody;
    }

    const hui::Theme theme = example::makeTheme(fontBody, fontSmall);

    hui::UISystem uiSystem(*renderer, theme);

    hui::Shell shell(uiSystem.viewStack());
    shell.layout({0, 0, screenW, screenH});
    shell.setClock("14:23");
    shell.setBatteryLevel(92);
    uiSystem.setShell(&shell);

    example::App app(uiSystem, shell);
    uiSystem.setGlobalAccelerator([&app](hui::Button b) { return app.onGlobalButton(b); });
    app.start();

    if (selfTest) {
        const int result = example::SelfTest(uiSystem, app, shell).run();
        renderer.reset();
#ifndef HUI_USE_SDL1
        SDL_DestroyRenderer(sdlRenderer);
        SDL_DestroyWindow(window);
#endif
        if (bodyFont) TTF_CloseFont(bodyFont);
        if (smallFont && smallFont != bodyFont) TTF_CloseFont(smallFont);
        TTF_Quit();
        SDL_Quit();
        return result;
    }

    hui::SDLGamepadHelper gamepad;
    if (gamepad.openController(0)) {
        std::cout << "Gamepad connected.\n";
    }

    example::printControls();

    uint32_t lastTime = SDL_GetTicks();

    while (app.running()) {
        const uint32_t now = SDL_GetTicks();
        const float dt = (now - lastTime) / 1000.0f;
        lastTime = now;

        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT) {
                return 0;
            }

            if (auto padEvent = gamepad.translate(e)) {
                if (padEvent->kind == hui::ButtonEventKind::Down) uiSystem.onButtonDown(padEvent->button);
                else                                              uiSystem.onButtonUp(padEvent->button);
                continue;
            }

#ifdef HUI_ENABLE_KEYBOARD_FALLBACK
            if (auto keyEvent = hui::KeyboardFallback::translate(e)) {
                if (keyEvent->kind == hui::ButtonEventKind::Down) uiSystem.onButtonDown(keyEvent->button);
                else                                             uiSystem.onButtonUp(keyEvent->button);
            }
#endif
        }

        app.update(dt);
        uiSystem.update(dt);
        shell.update(dt);

        renderer->beginFrame();
        uiSystem.draw();
        renderer->endFrame();

        SDL_Delay(16);
    }

    if (bodyFont) TTF_CloseFont(bodyFont);
    if (smallFont && smallFont != bodyFont) TTF_CloseFont(smallFont);

    renderer.reset();

#ifndef HUI_USE_SDL1
    SDL_DestroyRenderer(sdlRenderer);
    SDL_DestroyWindow(window);
#endif

    TTF_Quit();
    SDL_Quit();

    return 0;
}
