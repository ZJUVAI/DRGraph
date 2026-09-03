#ifndef PROGRESS_H
#define PROGRESS_H

#include <cstdint>
#include <string>

class ProgressBar {
public:
    static void Begin(const std::string& label, std::uint64_t total);
    static void Update(std::uint64_t completed);
    static void Finish();
    static void Abort();
};

#endif
