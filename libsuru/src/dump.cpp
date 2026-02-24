#include "suru/front/dump.hpp"

#include <sstream>
#include <string>
#include <vector>

namespace suru::front {
namespace {

std::string yaml_quote(std::string_view input) {
    std::string out;
    out.reserve(input.size() + 8U);
    out.push_back('\'');
    for (char ch : input) {
        if (ch == '\'') {
            out += "''";
        } else if (ch == '\n') {
            out += "\\n";
        } else if (ch == '\r') {
            out += "\\r";
        } else if (ch == '\t') {
            out += "\\t";
        } else if (static_cast<unsigned char>(ch) < 0x20U) {
            out += "?";
        } else {
            out += ch;
        }
    }
    out.push_back('\'');
    return out;
}

void write_indent(std::ostringstream& out, std::size_t depth) {
    out << std::string(depth * 2, ' ');
}

void write_block_node(std::ostringstream& out, const ParseNode& node, std::size_t depth);
void write_block_list(std::ostringstream& out, const std::vector<ParseNode>& list, std::size_t depth);

void write_node_fields(std::ostringstream& out, const ParseNode& node, std::size_t depth) {
    write_indent(out, depth);
    out << "kind: " << yaml_quote(node.kind) << "\n";
    write_indent(out, depth);
    out << "loc: { line: " << node.location.line << ", column: " << node.location.column << " }\n";

    for (const auto& [key, value] : node.attributes) {
        write_indent(out, depth);
        out << key << ": " << yaml_quote(value) << "\n";
    }

    for (const auto& [key, child] : node.nodes) {
        write_indent(out, depth);
        out << key << ":\n";
        write_block_node(out, child, depth + 1);
    }

    for (const auto& [key, list] : node.lists) {
        write_indent(out, depth);
        out << key << ":\n";
        write_block_list(out, list, depth + 1);
    }
}

void write_block_node(std::ostringstream& out, const ParseNode& node, std::size_t depth) {
    write_node_fields(out, node, depth);
}

void write_block_list(std::ostringstream& out, const std::vector<ParseNode>& list, std::size_t depth) {
    if (list.empty()) {
        write_indent(out, depth);
        out << "[]\n";
        return;
    }

    for (const ParseNode& item : list) {
        write_indent(out, depth);
        out << "- kind: " << yaml_quote(item.kind) << "\n";
        write_indent(out, depth + 1);
        out << "loc: { line: " << item.location.line << ", column: " << item.location.column << " }\n";

        for (const auto& [key, value] : item.attributes) {
            write_indent(out, depth + 1);
            out << key << ": " << yaml_quote(value) << "\n";
        }

        for (const auto& [key, child] : item.nodes) {
            write_indent(out, depth + 1);
            out << key << ":\n";
            write_block_node(out, child, depth + 2);
        }

        for (const auto& [key, nested] : item.lists) {
            write_indent(out, depth + 1);
            out << key << ":\n";
            write_block_list(out, nested, depth + 2);
        }
    }
}

} // namespace

std::string dump(const ParseTree& tree) {
    std::ostringstream out;
    write_node_fields(out, tree.root, 0);
    out << "\n";
    return out.str();
}

} // namespace suru::front
