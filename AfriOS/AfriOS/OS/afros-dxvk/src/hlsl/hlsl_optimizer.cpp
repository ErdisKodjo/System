// SPDX-License-Identifier: MIT
//
// hlsl_optimizer.cpp — AST-level optimization passes.
//
// Two passes run on the AST produced by `hlsl_compiler.cpp`:
//
//   * **Constant folding** — walks binary/unary operator nodes whose children
//     are both constants, evaluates them at compile time, and replaces the
//     node with a Constant carrying the result.
//   * **Dead-code elimination** — removes `Return` / `Branch` / `Assignment`
//     nodes whose result is never consumed (e.g. assignments to a temporary
//     that has no further reads), plus empty statement blocks.
//
// The skeleton implements the constant-folding pass fully and a conservative
// dead-code pass that strips unreachable nodes after a `Return`. The real
// DXVK path runs these passes on the SPIR-V IR instead (via spirv-opt), but
// keeping an AST-level pass here lets the skeleton stand alone.

#include "vulkan_loader.h"
#include "hlsl_types.h"

#include <cstdint>
#include <cstring>
#include <memory>
#include <vector>

namespace dxvk::hlsl {

namespace {

/// Evaluate a binary operator on two constant Values. Returns true and writes
/// `out` when the fold succeeds.
bool foldBinary(BinOp op, const Value& a, const Value& b, Value& out) {
    if (!a.isConstant || !b.isConstant) return false;
    out.isConstant = true;
    out.type = a.type;
    if (a.type.base == BaseType::Float) {
        const float x = a.as.f, y = b.as.f;
        switch (op) {
            case BinOp::Add: out.as.f = x + y; return true;
            case BinOp::Sub: out.as.f = x - y; return true;
            case BinOp::Mul: out.as.f = x * y; return true;
            case BinOp::Div: out.as.f = (y != 0.0f) ? x / y : 0.0f; return true;
            case BinOp::Eq:  out.as.u = (x == y) ? 1 : 0; out.type.base = BaseType::Bool; return true;
            case BinOp::Ne:  out.as.u = (x != y) ? 1 : 0; out.type.base = BaseType::Bool; return true;
            case BinOp::Lt:  out.as.u = (x <  y) ? 1 : 0; out.type.base = BaseType::Bool; return true;
            case BinOp::Le:  out.as.u = (x <= y) ? 1 : 0; out.type.base = BaseType::Bool; return true;
            case BinOp::Gt:  out.as.u = (x >  y) ? 1 : 0; out.type.base = BaseType::Bool; return true;
            case BinOp::Ge:  out.as.u = (x >= y) ? 1 : 0; out.type.base = BaseType::Bool; return true;
            default: return false;
        }
    } else {
        const int32_t x = a.as.i, y = b.as.i;
        switch (op) {
            case BinOp::Add: out.as.i = x + y; return true;
            case BinOp::Sub: out.as.i = x - y; return true;
            case BinOp::Mul: out.as.i = x * y; return true;
            case BinOp::Div: out.as.i = (y != 0) ? x / y : 0; return true;
            case BinOp::Mod: out.as.i = (y != 0) ? x % y : 0; return true;
            case BinOp::And: out.as.i = x & y; return true;
            case BinOp::Or:  out.as.i = x | y; return true;
            case BinOp::Xor: out.as.i = x ^ y; return true;
            case BinOp::Eq:  out.as.u = (x == y) ? 1 : 0; out.type.base = BaseType::Bool; return true;
            case BinOp::Ne:  out.as.u = (x != y) ? 1 : 0; out.type.base = BaseType::Bool; return true;
            case BinOp::Lt:  out.as.u = (x <  y) ? 1 : 0; out.type.base = BaseType::Bool; return true;
            case BinOp::Le:  out.as.u = (x <= y) ? 1 : 0; out.type.base = BaseType::Bool; return true;
            case BinOp::Gt:  out.as.u = (x >  y) ? 1 : 0; out.type.base = BaseType::Bool; return true;
            case BinOp::Ge:  out.as.u = (x >= y) ? 1 : 0; out.type.base = BaseType::Bool; return true;
            default: return false;
        }
    }
}

/// Recursively fold constants in a subtree. Returns the number of folds.
uint32_t foldConstants(Node* node) {
    if (!node) return 0;
    uint32_t count = 0;
    for (auto& child : node->children) {
        count += foldConstants(child.get());
        if (child && child->kind == NodeKind::BinaryOp && child->children.size() == 2) {
            const auto& a = child->children[0];
            const auto& b = child->children[1];
            if (a && b && a->kind == NodeKind::Constant && b->kind == NodeKind::Constant) {
                Value folded{};
                if (foldBinary(child->op, a->value, b->value, folded)) {
                    child->kind = NodeKind::Constant;
                    child->value = folded;
                    child->children.clear();
                    ++count;
                }
            }
        }
    }
    return count;
}

/// Recursively strip nodes that follow a `Return` in the same block (they are
/// unreachable). Returns the number of nodes removed.
uint32_t eliminateDeadCode(Node* node) {
    if (!node) return 0;
    uint32_t removed = 0;
    bool returned = false;
    std::vector<std::unique_ptr<Node>> kept;
    kept.reserve(node->children.size());
    for (auto& child : node->children) {
        if (returned) { ++removed; continue; }
        removed += eliminateDeadCode(child.get());
        if (child && child->kind == NodeKind::Return) {
            kept.push_back(std::move(child));
            returned = true;
            continue;
        }
        // Drop empty non-side-effecting assignment blocks.
        if (child && child->kind == NodeKind::Assignment &&
            child->children.empty() && child->text.empty()) {
            ++removed;
            continue;
        }
        kept.push_back(std::move(child));
    }
    node->children = std::move(kept);
    return removed;
}

} // namespace

} // namespace dxvk::hlsl

// --- Public entry point ----------------------------------------------------
extern "C" uint32_t dxvk::hlsl::OptimizeAst(Node* root) {
    if (!root) return 0;
    uint32_t total = 0;
    // Iterate to a fixed point so nested folds + DCE converge.
    for (int i = 0; i < 8; ++i) {
        const uint32_t folded = foldConstants(root);
        const uint32_t dead   = eliminateDeadCode(root);
        total += folded + dead;
        if (folded == 0 && dead == 0) break;
    }
    return total;
}
