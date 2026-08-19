#pragma once

#include "hui/types.h"
#include <string>
#include <string_view>
#include <vector>
#include <cstdint>

namespace hui {

// §6.5 List Data Sourcing

enum class ListItemVariant : uint8_t {
    Default,
    Track,
    Folder,
    Playlist
};

// Filled by the source on demand. Contains no owning storage: the string_views
// must remain valid for the duration of the draw() call that requested them,
// which is free if the application holds its data in a container that outlives
// the frame (the normal case).
struct RowData {
    std::string_view primary;
    std::string_view secondary;
    std::string_view rightMeta;
    ListItemVariant  variant  = ListItemVariant::Default;
    TextureHandle    icon     = 0;
    bool             playing  = false;
    bool             disabled = false;
};

class IListSource {
public:
    virtual ~IListSource() = default;
    virtual int  rowCount() const = 0;
    virtual void rowAt(int index, RowData& out) const = 0;
};

// Owns its strings. Use for lists of a handful of fixed entries.
class VectorListSource : public IListSource {
public:
    void add(std::string primary,
             std::string secondary = {},
             std::string rightMeta = {},
             ListItemVariant variant = ListItemVariant::Default,
             TextureHandle icon = 0,
             bool playing = false,
             bool disabled = false);

    void clear();

    int  rowCount() const override;
    void rowAt(int index, RowData& out) const override;

private:
    struct Entry {
        std::string primary;
        std::string secondary;
        std::string rightMeta;
        ListItemVariant variant = ListItemVariant::Default;
        TextureHandle icon = 0;
        bool playing = false;
        bool disabled = false;
    };

    std::vector<Entry> entries_;
};

} // namespace hui
