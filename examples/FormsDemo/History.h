#pragma once

// An undo history: every version of a value, and a cursor to the current one.
// immer::vector is persistent, so each version is a cheap snapshot sharing structure with the last.

#include <cstddef>
#include <utility>

#include <immer/vector.hpp>

template <class T>
class History
{
    immer::vector<T> versions_{ T{} }; // oldest first; never empty
    std::size_t      cursor_ = 0;      // the current version; anything after it is what redo restores

public:
    const T& current() const { return versions_[cursor_]; }

    std::size_t version() const { return cursor_ + 1; } // 1-based
    std::size_t size() const { return versions_.size(); }

    bool canUndo() const { return cursor_ > 0; }
    bool canRedo() const { return cursor_ + 1 < versions_.size(); }

    // Append a new version, dropping any redo tail.
    void commit(T next)
    {
        versions_ = versions_.take(cursor_ + 1).push_back(std::move(next));
        cursor_ = versions_.size() - 1;
    }

    // Each returns whether the current version changed.
    bool undo()
    {
        if (!canUndo())
            return false;

        --cursor_;
        return true;
    }

    bool redo()
    {
        if (!canRedo())
            return false;

        ++cursor_;
        return true;
    }
};
