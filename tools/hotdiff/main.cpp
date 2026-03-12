#include "tools/hotdiff/diff.h"
#include <cstring>
#include <iostream>
#include <string>

using namespace hotvm;
using namespace hotvm::tools;

static void PrintUsage(const char* prog) {
    std::cerr << "Usage: " << prog
              << " <old.hotmod> <new.hotmod> -o <patch.hotpatch>\n"
              << "\nCompares two .hotmod modules and produces an incremental "
              << ".hotpatch file.\n";
}

int main(int argc, char* argv[]) {
    if (argc < 4) {
        PrintUsage(argv[0]);
        return 1;
    }

    std::string old_path, new_path, output_path;

    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "-o") == 0 && i + 1 < argc) {
            output_path = argv[++i];
        } else if (old_path.empty()) {
            old_path = argv[i];
        } else if (new_path.empty()) {
            new_path = argv[i];
        }
    }

    if (old_path.empty() || new_path.empty()) {
        std::cerr << "Error: two input .hotmod files required\n";
        PrintUsage(argv[0]);
        return 1;
    }
    if (output_path.empty()) {
        output_path = "patch.hotpatch";
    }

    try {
        std::cout << "Diffing:\n"
                  << "  Old: " << old_path << "\n"
                  << "  New: " << new_path << "\n";

        PatchManifest manifest = ModuleDiff::DiffFiles(old_path, new_path);

        int modified = 0, added = 0, removed = 0;
        for (auto& e : manifest.entries) {
            switch (e.action) {
            case PatchAction::kModified: modified++; break;
            case PatchAction::kAdded:    added++; break;
            case PatchAction::kRemoved:  removed++; break;
            }
        }

        std::cout << "\nPatch summary:\n"
                  << "  Modified: " << modified << "\n"
                  << "  Added:    " << added << "\n"
                  << "  Removed:  " << removed << "\n"
                  << "  Total:    " << manifest.entries.size() << "\n";

        if (manifest.entries.empty()) {
            std::cout << "\nNo changes detected. No patch file written.\n";
            return 0;
        }

        std::cout << "\nWriting " << output_path << "...\n";
        if (!WritePatchManifest(output_path, manifest)) {
            std::cerr << "Error: failed to write patch\n";
            return 1;
        }

        std::cout << "Done.\n";
        return 0;

    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }
}
