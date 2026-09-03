#include "cli.h"
#include "pipeline.h"

#include <iostream>
#include <string>

int main(int argc, char** argv) {
    CliOptions options;
    std::string error;
    if (!ParseCli(argc, argv, &options, &error)) {
        std::cerr << "Argument error: " << error << std::endl;
        return 2;
    }
    if (options.show_help || options.show_help_all) {
        std::cout << CliHelp(options.show_help_all);
        return 0;
    }
    return RunPipeline(options);
}
