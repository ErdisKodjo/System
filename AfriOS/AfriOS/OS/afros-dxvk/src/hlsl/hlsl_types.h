// SPDX-License-Identifier: MIT
//
// hlsl_types.h — shared AST + SPIR-V module types used by the three HLSL
// front-end translation units (hlsl_compiler / hlsl_optimizer /
// spirv_generator). Kept self-contained so each .cpp compiles independently
// with `-I include`.

#pragma once

#include "vulkan_loader.h"

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace dxvk::hlsl {

/// Shader stage for HLSL compilation (mirrors glslang EShLanguage).
enum class Stage : int {
    Vertex        = 0,
    Hull          = 1,
    Domain        = 2,
    Geometry      = 3,
    Pixel         = 4,
    Compute       = 5,
    Unknown       = -1,
};

/// Scalar / vector / matrix base type.
enum class BaseType : uint8_t {
    Void, Bool, Int, Uint, Float, Double,
};

/// One AST type descriptor.
struct Type {
    BaseType base  = BaseType::Float;
    uint8_t  rows  = 1;     // 1 for scalar/vector, >1 for matrix
    uint8_t  cols  = 1;
    uint8_t  arraySize = 0; // 0 = not an array
};

/// One value (constant or variable reference) carried through optimization.
struct Value {
    Type        type{};
    bool        isConstant = false;
    union {
        int32_t  i;
        uint32_t u;
        float    f;
        double   d;
    } as = {0};
    std::string name; // non-empty when !isConstant
};

/// AST node kinds the optimizer / generator walk.
enum class NodeKind : uint8_t {
    Function,
    Variable,
    BinaryOp,
    UnaryOp,
    Call,
    Return,
    Branch,
    Loop,
    Assignment,
    Constant,
    Swizzle,
};

/// Binary operator kinds.
enum class BinOp : uint8_t {
    Add, Sub, Mul, Div, Mod,
    And, Or, Xor,
    Eq, Ne, Lt, Le, Gt, Ge,
};

/// Minimal AST node. The real front-end uses a richer hierarchy; this struct
/// is a tagged union that is enough for dead-code elimination + constant
/// folding + SPIR-V emission in the skeleton.
struct Node {
    NodeKind kind = NodeKind::Variable;
    Value    value{};
    BinOp    op = BinOp::Add;
    std::vector<std::unique_ptr<Node>> children;
    std::string text; // identifier / intrinsic name
};

/// A compiled SPIR-V module.
struct SpirvModule {
    std::vector<uint32_t> words;
    Stage                 stage = Stage::Unknown;
    std::string           entryPoint;
    std::string           disassembly; // optional, human-readable
};

// --- Cross-file entry points (declared in hlsl_compiler.cpp, etc.) --------
extern "C" bool CompileHlslToSpirv(const char* src, const char* entry,
                                   Stage stage, SpirvModule* out);

/// Run dead-code elimination + constant folding on an AST root.
/// Returns the number of nodes removed/folded.
extern "C" uint32_t OptimizeAst(Node* root);

/// Emit SPIR-V binary from an optimized AST into `out`.
extern "C" bool EmitSpirv(const Node* root, Stage stage,
                          const char* entryPoint, SpirvModule* out);

} // namespace dxvk::hlsl
