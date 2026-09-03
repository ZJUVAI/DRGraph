#include "progress.h"

#include <algorithm>
#include <cstdio>
#include <mutex>

#ifdef __linux__
#include <unistd.h>
#endif

namespace {

struct ProgressState {
    bool active = false;
    bool interactive = false;
    std::uint64_t total = 0;
    std::uint64_t last_completed = 0;
    std::string label;
    std::mutex mutex;
};

ProgressState& State() {
    static ProgressState state;
    return state;
}

bool IsInteractiveTerminal() {
#ifdef __linux__
    return isatty(fileno(stderr)) != 0;
#else
    return false;
#endif
}

void Render(const ProgressState& state, std::uint64_t completed) {
    const std::uint64_t bounded = std::min(completed, state.total);
    const unsigned width = 24;
    const unsigned filled = state.total == 0 ? 0 : static_cast<unsigned>(bounded * width / state.total);
    std::fputs("\r", stderr);
    std::fputs(state.label.c_str(), stderr);
    std::fputs(" [", stderr);
    for (unsigned index = 0; index < width; ++index) std::fputc(index < filled ? '#' : '.', stderr);
    std::fprintf(stderr, "] %3llu%%", static_cast<unsigned long long>(state.total == 0 ? 0 : bounded * 100 / state.total));
    std::fflush(stderr);
}

}  // namespace

void ProgressBar::Begin(const std::string& label, std::uint64_t total) {
    ProgressState& state = State();
    std::lock_guard<std::mutex> lock(state.mutex);
    state.interactive = IsInteractiveTerminal();
    state.active = state.interactive && total > 0;
    state.total = total;
    state.last_completed = 0;
    state.label = label;
    if (state.active) Render(state, 0);
}

void ProgressBar::Update(std::uint64_t completed) {
    ProgressState& state = State();
    std::lock_guard<std::mutex> lock(state.mutex);
    if (!state.active || completed <= state.last_completed) return;
    state.last_completed = completed;
    Render(state, completed);
}

void ProgressBar::Finish() {
    ProgressState& state = State();
    std::lock_guard<std::mutex> lock(state.mutex);
    if (state.active) {
        if (state.last_completed < state.total) Render(state, state.total);
        std::fputc('\n', stderr);
        std::fflush(stderr);
    }
    state.active = false;
}

void ProgressBar::Abort() {
    ProgressState& state = State();
    std::lock_guard<std::mutex> lock(state.mutex);
    if (state.active) {
        std::fputc('\n', stderr);
        std::fflush(stderr);
    }
    state.active = false;
}
