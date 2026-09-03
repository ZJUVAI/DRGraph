#ifndef CLI_H
#define CLI_H

#include <map>
#include <string>

struct CliOptions {
    bool show_help = false;
    bool show_help_all = false;
    bool validate_only = false;
    std::map<std::string, std::string> values;

    bool Has(const std::string& name) const;
    std::string Get(const std::string& name, const std::string& fallback = "") const;
};

bool ParseCli(int argc, char** argv, CliOptions* options, std::string* error);
std::string CliHelp(bool include_advanced);

#endif
