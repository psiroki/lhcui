#include "hui/ListSource.h"

namespace hui {

void VectorListSource::add(std::string primary,
                           std::string secondary,
                           std::string rightMeta,
                           ListItemVariant variant,
                           TextureHandle icon,
                           bool playing,
                           bool disabled,
                           bool destructive) {
    entries_.push_back(Entry{
        std::move(primary),
        std::move(secondary),
        std::move(rightMeta),
        variant,
        icon,
        playing,
        disabled,
        destructive
    });
}

void VectorListSource::clear() {
    entries_.clear();
}

int VectorListSource::rowCount() const {
    return static_cast<int>(entries_.size());
}

void VectorListSource::rowAt(int index, RowData& out) const {
    if (index < 0 || index >= static_cast<int>(entries_.size())) {
        out = RowData{};
        return;
    }
    const auto& entry = entries_[index];
    out.primary = entry.primary;
    out.secondary = entry.secondary;
    out.rightMeta = entry.rightMeta;
    out.variant = entry.variant;
    out.icon = entry.icon;
    out.playing = entry.playing;
    out.disabled = entry.disabled;
    out.destructive = entry.destructive;
}

} // namespace hui
