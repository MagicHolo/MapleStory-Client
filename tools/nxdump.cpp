#include <iostream>
#include <string>
#include <nlnx/nx.hpp>
#include <nlnx/node.hpp>
#include <nlnx/file.hpp>
#include <nlnx/bitmap.hpp>

void dump(nl::node n, int depth, int maxdepth) {
    if (depth > maxdepth) return;
    for (auto child : n) {
        for (int i = 0; i < depth; i++) std::cout << "  ";
        std::cout << child.name();
        if (child.data_type() == nl::node::type::bitmap) {
            nl::bitmap bmp = child.get_bitmap();
            std::cout << " [bitmap " << bmp.width() << "x" << bmp.height() << "]";
        } else if (child.data_type() == nl::node::type::integer) {
            std::cout << " = " << child.get_integer();
        } else if (child.data_type() == nl::node::type::string) {
            std::cout << " = \"" << child.get_string() << "\"";
        } else if (child.data_type() == nl::node::type::vector) {
            std::cout << " = (" << child.x() << ", " << child.y() << ")";
        } else if (child.data_type() == nl::node::type::real) {
            std::cout << " = " << child.get_real();
        }
        std::cout << " (" << child.size() << " children)" << std::endl;
        if (depth < maxdepth) dump(child, depth + 1, maxdepth);
    }
}

int main(int argc, char** argv) {
    if (argc < 2) { std::cerr << "Usage: nxdump <file.nx> [path] [depth]" << std::endl; return 1; }
    std::string filename = argv[1];
    std::string path = argc > 2 ? argv[2] : "";
    int maxdepth = argc > 3 ? std::atoi(argv[3]) : 1;

    nl::file f(filename);
    nl::node root = f;

    nl::node target = root;
    if (!path.empty()) {
        target = root.resolve(path);
        if (!target) { std::cerr << "Path not found: " << path << std::endl; return 1; }
    }

    std::cout << "Root: " << (path.empty() ? "/" : path) << " (" << target.size() << " children)" << std::endl;
    dump(target, 0, maxdepth);
    return 0;
}
