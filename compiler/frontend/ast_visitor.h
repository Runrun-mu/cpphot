#pragma once
#include "hotvm/types.h"
#include "compiler/ir/ir_types.h"
#include <clang-c/Index.h>
#include <string>
#include <unordered_map>
#include <vector>

namespace hotvm {
namespace compiler {

// ── AST Visitor ──────────────────────────────────────────────
// Uses libclang C API to traverse a C++ source file's AST and
// produce an IR module (types + functions).

class ASTVisitor {
public:
    ASTVisitor();
    ~ASTVisitor();

    // Parse a source file and produce an IR module
    ir::IRModule Parse(const std::string& source_path,
                        const std::vector<std::string>& compiler_args = {});

private:
    // Visitor callbacks
    static CXChildVisitResult VisitTopLevel(CXCursor cursor, CXCursor parent, CXClientData data);
    static CXChildVisitResult VisitClassMembers(CXCursor cursor, CXCursor parent, CXClientData data);
    static CXChildVisitResult VisitFunctionBody(CXCursor cursor, CXCursor parent, CXClientData data);

    // Type extraction
    TypeInfo ExtractClassInfo(CXCursor cursor);
    ir::IRParam ExtractParam(CXCursor cursor);
    ir::IRType  MapClangType(CXType type);

    // Expression lowering to IR
    ir::IRValue LowerExpression(CXCursor expr, ir::IRFunction& func);
    ir::IRValue LowerBinaryOp(CXCursor expr, ir::IRFunction& func);
    ir::IRValue LowerUnaryOp(CXCursor expr, ir::IRFunction& func);
    ir::IRValue LowerCallExpr(CXCursor expr, ir::IRFunction& func);
    ir::IRValue LowerMemberExpr(CXCursor expr, ir::IRFunction& func);
    ir::IRValue LowerDeclRef(CXCursor expr, ir::IRFunction& func);
    ir::IRValue LowerIntegerLiteral(CXCursor expr, ir::IRFunction& func);
    ir::IRValue LowerFloatLiteral(CXCursor expr, ir::IRFunction& func);

    // Statement lowering
    void LowerStatement(CXCursor stmt, ir::IRFunction& func);
    void LowerIfStmt(CXCursor stmt, ir::IRFunction& func);
    void LowerWhileStmt(CXCursor stmt, ir::IRFunction& func);
    void LowerForStmt(CXCursor stmt, ir::IRFunction& func);
    void LowerReturnStmt(CXCursor stmt, ir::IRFunction& func);
    void LowerCompoundStmt(CXCursor stmt, ir::IRFunction& func);
    void LowerDeclStmt(CXCursor stmt, ir::IRFunction& func);

    // Function lowering
    void LowerFunction(CXCursor cursor);

    // State
    ir::IRModule module_;
    uint32_t next_func_id_ = 0;
    uint32_t next_type_id_ = 0;
    uint32_t next_label_ = 0;

    // Name → variable register mapping (per-function scope)
    std::unordered_map<std::string, ir::IRValue> var_map_;

    // Mangled name → func_id
    std::unordered_map<std::string, uint32_t> func_ids_;

    // Type name → type_id
    std::unordered_map<std::string, uint32_t> type_ids_;

    std::string NewLabel(const std::string& prefix = "L") {
        return prefix + std::to_string(next_label_++);
    }
};

}  // namespace compiler
}  // namespace hotvm
