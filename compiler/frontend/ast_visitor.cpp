#include "compiler/frontend/ast_visitor.h"
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <stdexcept>

namespace hotvm {
namespace compiler {

// ── Helpers ──────────────────────────────────────────────────

static std::string CXStringToStd(CXString cxs) {
    const char* cs = clang_getCString(cxs);
    std::string s = cs ? cs : "";
    clang_disposeString(cxs);
    return s;
}

static std::string GetCursorSpelling(CXCursor cursor) {
    return CXStringToStd(clang_getCursorSpelling(cursor));
}

static std::string GetCursorMangledName(CXCursor cursor) {
    // Try to get mangled name; fall back to spelling
    CXString mangled = clang_Cursor_getMangling(cursor);
    std::string name = CXStringToStd(mangled);
    if (name.empty()) {
        name = GetCursorSpelling(cursor);
    }
    return name;
}

static std::string GetTypeSpelling(CXType type) {
    return CXStringToStd(clang_getTypeSpelling(type));
}

// ── ASTVisitor ───────────────────────────────────────────────

ASTVisitor::ASTVisitor() = default;
ASTVisitor::~ASTVisitor() = default;

ir::IRModule ASTVisitor::Parse(const std::string& source_path,
                                const std::vector<std::string>& compiler_args) {
    module_ = ir::IRModule{};
    module_.source_file = source_path;

    // Convert args to C strings
    std::vector<const char*> c_args;
    c_args.push_back("-std=c++17");
    c_args.push_back("-fsyntax-only");
    for (auto& a : compiler_args) {
        c_args.push_back(a.c_str());
    }

    CXIndex index = clang_createIndex(0, 0);
    CXTranslationUnit tu = clang_parseTranslationUnit(
        index, source_path.c_str(),
        c_args.data(), static_cast<int>(c_args.size()),
        nullptr, 0,
        CXTranslationUnit_None);

    if (!tu) {
        clang_disposeIndex(index);
        throw std::runtime_error("Failed to parse: " + source_path);
    }

    // Check for errors
    unsigned num_diags = clang_getNumDiagnostics(tu);
    for (unsigned i = 0; i < num_diags; ++i) {
        CXDiagnostic diag = clang_getDiagnostic(tu, i);
        CXDiagnosticSeverity sev = clang_getDiagnosticSeverity(diag);
        if (sev >= CXDiagnostic_Error) {
            std::string msg = CXStringToStd(clang_formatDiagnostic(diag,
                clang_defaultDiagnosticDisplayOptions()));
            std::cerr << "Error: " << msg << "\n";
        }
        clang_disposeDiagnostic(diag);
    }

    // Visit top-level declarations
    CXCursor root = clang_getTranslationUnitCursor(tu);
    clang_visitChildren(root, VisitTopLevel, this);

    clang_disposeTranslationUnit(tu);
    clang_disposeIndex(index);

    return std::move(module_);
}

// ── Top-level visitor ────────────────────────────────────────

CXChildVisitResult ASTVisitor::VisitTopLevel(CXCursor cursor, CXCursor /*parent*/,
                                               CXClientData data) {
    auto* self = static_cast<ASTVisitor*>(data);
    CXCursorKind kind = clang_getCursorKind(cursor);

    // Skip system headers
    CXSourceLocation loc = clang_getCursorLocation(cursor);
    if (clang_Location_isInSystemHeader(loc)) {
        return CXChildVisit_Continue;
    }

    switch (kind) {
    case CXCursor_FunctionDecl:
        if (clang_isCursorDefinition(cursor)) {
            self->LowerFunction(cursor);
        }
        break;

    case CXCursor_ClassDecl:
    case CXCursor_StructDecl: {
        if (clang_isCursorDefinition(cursor)) {
            TypeInfo ti = self->ExtractClassInfo(cursor);
            self->type_ids_[ti.name] = ti.type_id;
            self->module_.types.push_back(std::move(ti));

            // Visit member functions
            clang_visitChildren(cursor, VisitClassMembers, data);
        }
        break;
    }

    case CXCursor_CXXMethod:
        if (clang_isCursorDefinition(cursor)) {
            self->LowerFunction(cursor);
        }
        break;

    default:
        break;
    }

    return CXChildVisit_Continue;
}

CXChildVisitResult ASTVisitor::VisitClassMembers(CXCursor cursor, CXCursor /*parent*/,
                                                    CXClientData data) {
    auto* self = static_cast<ASTVisitor*>(data);
    CXCursorKind kind = clang_getCursorKind(cursor);

    if (kind == CXCursor_CXXMethod || kind == CXCursor_Constructor ||
        kind == CXCursor_Destructor) {
        if (clang_isCursorDefinition(cursor)) {
            self->LowerFunction(cursor);
        }
    }

    return CXChildVisit_Continue;
}

// ── Type extraction ──────────────────────────────────────────

ir::IRType ASTVisitor::MapClangType(CXType type) {
    type = clang_getCanonicalType(type);

    switch (type.kind) {
    case CXType_Void:       return ir::IRType::kVoid;
    case CXType_Bool:       return ir::IRType::kBool;
    case CXType_Char_S:
    case CXType_SChar:
    case CXType_UChar:      return ir::IRType::kI8;
    case CXType_Short:
    case CXType_UShort:     return ir::IRType::kI16;
    case CXType_Int:
    case CXType_UInt:       return ir::IRType::kI32;
    case CXType_Long:
    case CXType_ULong:
    case CXType_LongLong:
    case CXType_ULongLong:  return ir::IRType::kI64;
    case CXType_Float:      return ir::IRType::kF32;
    case CXType_Double:     return ir::IRType::kF64;
    case CXType_Pointer:    return ir::IRType::kPtr;
    case CXType_Record:     return ir::IRType::kStruct;
    default:
        if (clang_Type_getSizeOf(type) <= 8) return ir::IRType::kI64;
        return ir::IRType::kPtr;
    }
}

TypeInfo ASTVisitor::ExtractClassInfo(CXCursor cursor) {
    TypeInfo ti;
    ti.type_id = next_type_id_++;
    ti.name = GetCursorSpelling(cursor);

    CXType type = clang_getCursorType(cursor);
    ti.size = static_cast<uint32_t>(clang_Type_getSizeOf(type));
    ti.align = static_cast<uint32_t>(clang_Type_getAlignOf(type));

    // Extract fields and virtual methods
    struct FieldExtractor {
        TypeInfo* ti;
        ASTVisitor* visitor;
        uint32_t vtable_slot;
    };
    FieldExtractor ctx{&ti, this, 0};

    clang_visitChildren(cursor,
        [](CXCursor c, CXCursor, CXClientData d) -> CXChildVisitResult {
            auto* ctx = static_cast<FieldExtractor*>(d);
            CXCursorKind k = clang_getCursorKind(c);

            if (k == CXCursor_FieldDecl) {
                FieldInfo fi;
                fi.name = GetCursorSpelling(c);
                fi.offset = static_cast<uint32_t>(
                    clang_Type_getOffsetOf(clang_getCursorType(
                        clang_getCursorSemanticParent(c)), fi.name.c_str()) / 8);
                fi.kind = static_cast<ArgKind>(
                    static_cast<uint8_t>(ctx->visitor->MapClangType(clang_getCursorType(c))));
                fi.type_id = kInvalidTypeId;
                ctx->ti->fields.push_back(std::move(fi));
            }
            else if (k == CXCursor_CXXMethod && clang_CXXMethod_isVirtual(c)) {
                VMethodInfo vm;
                vm.name = GetCursorSpelling(c);
                vm.vtable_slot = ctx->vtable_slot++;
                vm.func_id = kInvalidFuncId;  // Will be resolved later
                ctx->ti->vmethods.push_back(std::move(vm));
            }
            else if (k == CXCursor_CXXBaseSpecifier) {
                CXType base_type = clang_getCursorType(c);
                std::string base_name = GetTypeSpelling(base_type);
                auto it = ctx->visitor->type_ids_.find(base_name);
                if (it != ctx->visitor->type_ids_.end()) {
                    ctx->ti->base_type_id = it->second;
                }
            }

            return CXChildVisit_Continue;
        }, &ctx);

    return ti;
}

ir::IRParam ASTVisitor::ExtractParam(CXCursor cursor) {
    ir::IRParam p;
    p.name = GetCursorSpelling(cursor);
    p.type = MapClangType(clang_getCursorType(cursor));
    return p;
}

// ── Function lowering ────────────────────────────────────────

void ASTVisitor::LowerFunction(CXCursor cursor) {
    ir::IRFunction func;
    func.mangled_name = GetCursorMangledName(cursor);
    func.func_id = next_func_id_++;
    func.return_type = MapClangType(clang_getCursorResultType(cursor));

    func_ids_[func.mangled_name] = func.func_id;

    // Check if virtual
    if (clang_CXXMethod_isVirtual(cursor)) {
        func.is_virtual = true;
    }

    // Extract parameters
    int num_params = clang_Cursor_getNumArguments(cursor);

    // For member functions, add implicit 'this' parameter
    if (clang_getCursorKind(cursor) == CXCursor_CXXMethod ||
        clang_getCursorKind(cursor) == CXCursor_Constructor ||
        clang_getCursorKind(cursor) == CXCursor_Destructor) {
        ir::IRParam this_param;
        this_param.name = "this";
        this_param.type = ir::IRType::kPtr;
        func.params.push_back(this_param);

        // Map 'this' to register
        ir::IRValue this_reg = func.NewReg();
        var_map_["this"] = this_reg;
    }

    for (int i = 0; i < num_params; ++i) {
        CXCursor param_cursor = clang_Cursor_getArgument(cursor, i);
        ir::IRParam p = ExtractParam(param_cursor);
        func.params.push_back(p);

        // Map parameter to register
        ir::IRValue param_reg = func.NewReg();
        var_map_[p.name] = param_reg;
    }

    // Lower function body
    var_map_.clear();
    // Re-add params to var_map after clear
    for (uint32_t i = 0; i < func.params.size(); ++i) {
        var_map_[func.params[i].name] = ir::IRValue::Reg(i);
    }
    func.next_reg = static_cast<uint32_t>(func.params.size());

    // Visit the function body
    struct BodyVisitor {
        ASTVisitor* self;
        ir::IRFunction* func;
    };
    BodyVisitor bv{this, &func};

    clang_visitChildren(cursor,
        [](CXCursor c, CXCursor, CXClientData d) -> CXChildVisitResult {
            auto* bv = static_cast<BodyVisitor*>(d);
            CXCursorKind k = clang_getCursorKind(c);

            if (k == CXCursor_CompoundStmt) {
                bv->self->LowerCompoundStmt(c, *bv->func);
                return CXChildVisit_Break;
            }
            else if (k == CXCursor_ParmDecl) {
                // Skip - already handled
                return CXChildVisit_Continue;
            }

            return CXChildVisit_Continue;
        }, &bv);

    module_.functions.push_back(std::move(func));
}

// ── Statement lowering ───────────────────────────────────────

void ASTVisitor::LowerStatement(CXCursor stmt, ir::IRFunction& func) {
    CXCursorKind kind = clang_getCursorKind(stmt);

    switch (kind) {
    case CXCursor_CompoundStmt:
        LowerCompoundStmt(stmt, func);
        break;
    case CXCursor_ReturnStmt:
        LowerReturnStmt(stmt, func);
        break;
    case CXCursor_IfStmt:
        LowerIfStmt(stmt, func);
        break;
    case CXCursor_WhileStmt:
        LowerWhileStmt(stmt, func);
        break;
    case CXCursor_ForStmt:
        LowerForStmt(stmt, func);
        break;
    case CXCursor_DeclStmt:
        LowerDeclStmt(stmt, func);
        break;
    default:
        // Try as expression statement
        LowerExpression(stmt, func);
        break;
    }
}

void ASTVisitor::LowerCompoundStmt(CXCursor stmt, ir::IRFunction& func) {
    struct Ctx {
        ASTVisitor* self;
        ir::IRFunction* func;
    };
    Ctx ctx{this, &func};

    clang_visitChildren(stmt,
        [](CXCursor c, CXCursor, CXClientData d) -> CXChildVisitResult {
            auto* ctx = static_cast<Ctx*>(d);
            ctx->self->LowerStatement(c, *ctx->func);
            return CXChildVisit_Continue;
        }, &ctx);
}

void ASTVisitor::LowerReturnStmt(CXCursor stmt, ir::IRFunction& func) {
    struct Ctx {
        ASTVisitor* self;
        ir::IRFunction* func;
        bool has_expr;
    };
    Ctx ctx{this, &func, false};

    clang_visitChildren(stmt,
        [](CXCursor c, CXCursor, CXClientData d) -> CXChildVisitResult {
            auto* ctx = static_cast<Ctx*>(d);
            ir::IRValue val = ctx->self->LowerExpression(c, *ctx->func);

            ir::IRInst ret_inst(ir::IROpCode::kRet);
            ret_inst.operands[0] = val;
            ctx->func->body.push_back(ret_inst);
            ctx->has_expr = true;
            return CXChildVisit_Break;
        }, &ctx);

    if (!ctx.has_expr) {
        ir::IRInst ret_inst(ir::IROpCode::kRetVoid);
        func.body.push_back(ret_inst);
    }
}

void ASTVisitor::LowerIfStmt(CXCursor stmt, ir::IRFunction& func) {
    std::string else_label = NewLabel("else");
    std::string end_label = NewLabel("endif");

    struct Ctx {
        ASTVisitor* self;
        ir::IRFunction* func;
        std::string else_label;
        std::string end_label;
        int child_idx;
    };
    Ctx ctx{this, &func, else_label, end_label, 0};

    clang_visitChildren(stmt,
        [](CXCursor c, CXCursor, CXClientData d) -> CXChildVisitResult {
            auto* ctx = static_cast<Ctx*>(d);
            if (ctx->child_idx == 0) {
                // Condition
                ir::IRValue cond = ctx->self->LowerExpression(c, *ctx->func);
                ir::IRInst branch(ir::IROpCode::kBranchIfNot);
                branch.operands[0] = cond;
                branch.label = ctx->else_label;
                ctx->func->body.push_back(branch);
            } else if (ctx->child_idx == 1) {
                // Then body
                ctx->self->LowerStatement(c, *ctx->func);
                ir::IRInst jmp(ir::IROpCode::kJmp);
                jmp.label = ctx->end_label;
                ctx->func->body.push_back(jmp);

                // Else label
                ir::IRInst lbl(ir::IROpCode::kLabel);
                lbl.label = ctx->else_label;
                ctx->func->body.push_back(lbl);
            } else if (ctx->child_idx == 2) {
                // Else body
                ctx->self->LowerStatement(c, *ctx->func);
            }
            ctx->child_idx++;
            return CXChildVisit_Continue;
        }, &ctx);

    // End label
    ir::IRInst end_lbl(ir::IROpCode::kLabel);
    end_lbl.label = end_label;
    func.body.push_back(end_lbl);
}

void ASTVisitor::LowerWhileStmt(CXCursor stmt, ir::IRFunction& func) {
    std::string loop_label = NewLabel("while");
    std::string end_label = NewLabel("endwhile");

    // Loop header label
    ir::IRInst loop_lbl(ir::IROpCode::kLabel);
    loop_lbl.label = loop_label;
    func.body.push_back(loop_lbl);

    struct Ctx {
        ASTVisitor* self;
        ir::IRFunction* func;
        std::string loop_label;
        std::string end_label;
        int child_idx;
    };
    Ctx ctx{this, &func, loop_label, end_label, 0};

    clang_visitChildren(stmt,
        [](CXCursor c, CXCursor, CXClientData d) -> CXChildVisitResult {
            auto* ctx = static_cast<Ctx*>(d);
            if (ctx->child_idx == 0) {
                // Condition
                ir::IRValue cond = ctx->self->LowerExpression(c, *ctx->func);
                ir::IRInst branch(ir::IROpCode::kBranchIfNot);
                branch.operands[0] = cond;
                branch.label = ctx->end_label;
                ctx->func->body.push_back(branch);
            } else if (ctx->child_idx == 1) {
                // Body
                ctx->self->LowerStatement(c, *ctx->func);
                ir::IRInst jmp(ir::IROpCode::kJmp);
                jmp.label = ctx->loop_label;
                ctx->func->body.push_back(jmp);
            }
            ctx->child_idx++;
            return CXChildVisit_Continue;
        }, &ctx);

    ir::IRInst end_lbl(ir::IROpCode::kLabel);
    end_lbl.label = end_label;
    func.body.push_back(end_lbl);
}

void ASTVisitor::LowerForStmt(CXCursor stmt, ir::IRFunction& func) {
    std::string cond_label = NewLabel("for_cond");
    std::string body_label = NewLabel("for_body");
    std::string inc_label = NewLabel("for_inc");
    std::string end_label = NewLabel("for_end");

    struct Ctx {
        ASTVisitor* self;
        ir::IRFunction* func;
        std::string cond_label, body_label, inc_label, end_label;
        int child_idx;
    };
    Ctx ctx{this, &func, cond_label, body_label, inc_label, end_label, 0};

    clang_visitChildren(stmt,
        [](CXCursor c, CXCursor, CXClientData d) -> CXChildVisitResult {
            auto* ctx = static_cast<Ctx*>(d);

            switch (ctx->child_idx) {
            case 0: // Init
                ctx->self->LowerStatement(c, *ctx->func);
                {
                    ir::IRInst lbl(ir::IROpCode::kLabel);
                    lbl.label = ctx->cond_label;
                    ctx->func->body.push_back(lbl);
                }
                break;
            case 1: // Condition
                {
                    ir::IRValue cond = ctx->self->LowerExpression(c, *ctx->func);
                    ir::IRInst branch(ir::IROpCode::kBranchIfNot);
                    branch.operands[0] = cond;
                    branch.label = ctx->end_label;
                    ctx->func->body.push_back(branch);
                }
                break;
            case 2: // Increment
                // Will be emitted after body
                break;
            case 3: // Body
                ctx->self->LowerStatement(c, *ctx->func);
                break;
            }

            ctx->child_idx++;
            return CXChildVisit_Continue;
        }, &ctx);

    // Back-edge
    ir::IRInst jmp(ir::IROpCode::kJmp);
    jmp.label = cond_label;
    func.body.push_back(jmp);

    ir::IRInst end_lbl(ir::IROpCode::kLabel);
    end_lbl.label = end_label;
    func.body.push_back(end_lbl);
}

void ASTVisitor::LowerDeclStmt(CXCursor stmt, ir::IRFunction& func) {
    clang_visitChildren(stmt,
        [](CXCursor c, CXCursor, CXClientData d) -> CXChildVisitResult {
            auto* self_pair = static_cast<std::pair<ASTVisitor*, ir::IRFunction*>*>(d);
            if (clang_getCursorKind(c) == CXCursor_VarDecl) {
                std::string name = GetCursorSpelling(c);
                ir::IRValue reg = self_pair->second->NewReg();
                self_pair->first->var_map_[name] = reg;

                // Check for initializer
                struct InitCtx {
                    ASTVisitor* self;
                    ir::IRFunction* func;
                    ir::IRValue dest;
                };
                InitCtx ictx{self_pair->first, self_pair->second, reg};

                clang_visitChildren(c,
                    [](CXCursor ic, CXCursor, CXClientData id) -> CXChildVisitResult {
                        auto* ictx = static_cast<InitCtx*>(id);
                        ir::IRValue val = ictx->self->LowerExpression(ic, *ictx->func);
                        ir::IRInst mov(ir::IROpCode::kMov);
                        mov.dest = ictx->dest;
                        mov.operands[0] = val;
                        ictx->func->body.push_back(mov);
                        return CXChildVisit_Break;
                    }, &ictx);
            }
            return CXChildVisit_Continue;
        }, new std::pair<ASTVisitor*, ir::IRFunction*>(this, &func));
}

// ── Expression lowering ──────────────────────────────────────

ir::IRValue ASTVisitor::LowerExpression(CXCursor expr, ir::IRFunction& func) {
    CXCursorKind kind = clang_getCursorKind(expr);

    switch (kind) {
    case CXCursor_BinaryOperator:
        return LowerBinaryOp(expr, func);
    case CXCursor_UnaryOperator:
        return LowerUnaryOp(expr, func);
    case CXCursor_CallExpr:
        return LowerCallExpr(expr, func);
    case CXCursor_MemberRefExpr:
        return LowerMemberExpr(expr, func);
    case CXCursor_DeclRefExpr:
        return LowerDeclRef(expr, func);
    case CXCursor_IntegerLiteral:
        return LowerIntegerLiteral(expr, func);
    case CXCursor_FloatingLiteral:
        return LowerFloatLiteral(expr, func);
    case CXCursor_CXXThisExpr: {
        auto it = var_map_.find("this");
        if (it != var_map_.end()) return it->second;
        return ir::IRValue::Reg(0);  // 'this' is always reg 0 for methods
    }
    case CXCursor_ParenExpr: {
        // Unwrap parenthesized expression
        ir::IRValue result;
        clang_visitChildren(expr,
            [](CXCursor c, CXCursor, CXClientData d) -> CXChildVisitResult {
                auto* pair = static_cast<std::pair<ASTVisitor*, ir::IRFunction*>*>(d);
                // This is a simplified approach
                return CXChildVisit_Continue;
            }, nullptr);
        return result;
    }
    case CXCursor_CXXNewExpr: {
        // new T → alloc(sizeof(T))
        CXType type = clang_getCursorType(expr);
        CXType pointee = clang_getPointeeType(type);
        uint32_t size = static_cast<uint32_t>(clang_Type_getSizeOf(pointee));
        if (size == 0) size = 8;

        ir::IRValue dest = func.NewReg();
        ir::IRInst alloc(ir::IROpCode::kAlloc);
        alloc.dest = dest;
        alloc.operands[0] = ir::IRValue::Imm(size);
        func.body.push_back(alloc);
        return dest;
    }
    case CXCursor_CXXDeleteExpr: {
        ir::IRValue ptr;
        clang_visitChildren(expr,
            [](CXCursor c, CXCursor, CXClientData d) -> CXChildVisitResult {
                auto* pair = static_cast<std::pair<ASTVisitor*, ir::IRValue*>*>(d);
                // Would need to lower the child expression
                return CXChildVisit_Break;
            }, nullptr);
        ir::IRInst free_inst(ir::IROpCode::kFree);
        free_inst.operands[0] = ptr;
        func.body.push_back(free_inst);
        return ir::IRValue::None();
    }
    default:
        // Unknown expression - return a dummy register
        return func.NewReg();
    }
}

ir::IRValue ASTVisitor::LowerBinaryOp(CXCursor expr, ir::IRFunction& func) {
    // Collect two children (LHS and RHS)
    struct BinCtx {
        ASTVisitor* self;
        ir::IRFunction* func;
        ir::IRValue children[2];
        int idx;
    };
    BinCtx ctx{this, &func, {}, 0};

    clang_visitChildren(expr,
        [](CXCursor c, CXCursor, CXClientData d) -> CXChildVisitResult {
            auto* ctx = static_cast<BinCtx*>(d);
            if (ctx->idx < 2) {
                ctx->children[ctx->idx++] =
                    ctx->self->LowerExpression(c, *ctx->func);
            }
            return ctx->idx >= 2 ? CXChildVisit_Break : CXChildVisit_Continue;
        }, &ctx);

    // Determine operator from the cursor's tokens
    // (Simplified: we'd need to tokenize to get the actual operator)
    // For now, use a heuristic based on the expression type
    CXType result_type = clang_getCursorType(expr);
    bool is_float = (result_type.kind == CXType_Float || result_type.kind == CXType_Double);

    // Get operator by examining tokens
    CXSourceRange range = clang_getCursorExtent(expr);
    CXTranslationUnit tu = clang_Cursor_getTranslationUnit(expr);
    CXToken* tokens = nullptr;
    unsigned num_tokens = 0;
    clang_tokenize(tu, range, &tokens, &num_tokens);

    std::string op_str;
    for (unsigned i = 0; i < num_tokens; ++i) {
        CXTokenKind tk = clang_getTokenKind(tokens[i]);
        if (tk == CXToken_Punctuation) {
            std::string tok = CXStringToStd(clang_getTokenSpelling(tu, tokens[i]));
            if (tok == "+" || tok == "-" || tok == "*" || tok == "/" ||
                tok == "%" || tok == "==" || tok == "!=" || tok == "<" ||
                tok == "<=" || tok == ">" || tok == ">=" || tok == "=" ||
                tok == "&" || tok == "|" || tok == "^" || tok == "<<" ||
                tok == ">>") {
                op_str = tok;
                break;
            }
        }
    }
    clang_disposeTokens(tu, tokens, num_tokens);

    ir::IRValue dest = func.NewReg();
    ir::IRInst inst;

    if (op_str == "=") {
        // Assignment
        inst.op = ir::IROpCode::kMov;
        inst.dest = ctx.children[0];
        inst.operands[0] = ctx.children[1];
        func.body.push_back(inst);
        return ctx.children[0];
    }

    inst.dest = dest;
    inst.operands[0] = ctx.children[0];
    inst.operands[1] = ctx.children[1];

    if (op_str == "+")       inst.op = is_float ? ir::IROpCode::kFAdd : ir::IROpCode::kAdd;
    else if (op_str == "-")  inst.op = is_float ? ir::IROpCode::kFSub : ir::IROpCode::kSub;
    else if (op_str == "*")  inst.op = is_float ? ir::IROpCode::kFMul : ir::IROpCode::kMul;
    else if (op_str == "/")  inst.op = is_float ? ir::IROpCode::kFDiv : ir::IROpCode::kDiv;
    else if (op_str == "%")  inst.op = ir::IROpCode::kMod;
    else if (op_str == "==") inst.op = ir::IROpCode::kCmpEq;
    else if (op_str == "!=") inst.op = ir::IROpCode::kCmpNe;
    else if (op_str == "<")  inst.op = ir::IROpCode::kCmpLt;
    else if (op_str == "<=") inst.op = ir::IROpCode::kCmpLe;
    else if (op_str == ">")  inst.op = ir::IROpCode::kCmpGt;
    else if (op_str == ">=") inst.op = ir::IROpCode::kCmpGe;
    else if (op_str == "&")  inst.op = ir::IROpCode::kAnd;
    else if (op_str == "|")  inst.op = ir::IROpCode::kOr;
    else if (op_str == "^")  inst.op = ir::IROpCode::kXor;
    else if (op_str == "<<") inst.op = ir::IROpCode::kShl;
    else if (op_str == ">>") inst.op = ir::IROpCode::kShr;
    else inst.op = ir::IROpCode::kAdd;  // fallback

    func.body.push_back(inst);
    return dest;
}

ir::IRValue ASTVisitor::LowerUnaryOp(CXCursor expr, ir::IRFunction& func) {
    ir::IRValue child;
    clang_visitChildren(expr,
        [](CXCursor c, CXCursor, CXClientData d) -> CXChildVisitResult {
            auto* pair = static_cast<std::pair<ASTVisitor*, ir::IRFunction*>*>(d);
            // Simplified
            return CXChildVisit_Break;
        }, nullptr);

    ir::IRValue dest = func.NewReg();
    ir::IRInst inst(ir::IROpCode::kNeg);
    inst.dest = dest;
    inst.operands[0] = child;
    func.body.push_back(inst);
    return dest;
}

ir::IRValue ASTVisitor::LowerCallExpr(CXCursor expr, ir::IRFunction& func) {
    // Collect callee name and arguments
    struct CallCtx {
        ASTVisitor* self;
        ir::IRFunction* func;
        std::string callee;
        std::vector<ir::IRValue> args;
        int child_idx;
    };
    CallCtx ctx{this, &func, "", {}, 0};

    clang_visitChildren(expr,
        [](CXCursor c, CXCursor, CXClientData d) -> CXChildVisitResult {
            auto* ctx = static_cast<CallCtx*>(d);
            if (ctx->child_idx == 0) {
                // First child is the callee reference
                ctx->callee = GetCursorSpelling(c);
            }
            // All children after first are arguments
            if (ctx->child_idx > 0) {
                ctx->args.push_back(
                    ctx->self->LowerExpression(c, *ctx->func));
            }
            ctx->child_idx++;
            return CXChildVisit_Continue;
        }, &ctx);

    ir::IRValue dest = func.NewReg();
    ir::IRInst call(ir::IROpCode::kCall);
    call.dest = dest;
    call.func_name = ctx.callee;
    // Store args in operands (simplified: first 3 only)
    for (size_t i = 0; i < ctx.args.size() && i < 3; ++i) {
        call.operands[i] = ctx.args[i];
    }
    func.body.push_back(call);
    return dest;
}

ir::IRValue ASTVisitor::LowerMemberExpr(CXCursor expr, ir::IRFunction& func) {
    std::string member_name = GetCursorSpelling(expr);

    // Get the object (first child)
    struct MemberCtx {
        ASTVisitor* self;
        ir::IRFunction* func;
        ir::IRValue obj;
    };
    MemberCtx ctx{this, &func, {}};

    clang_visitChildren(expr,
        [](CXCursor c, CXCursor, CXClientData d) -> CXChildVisitResult {
            auto* ctx = static_cast<MemberCtx*>(d);
            ctx->obj = ctx->self->LowerExpression(c, *ctx->func);
            return CXChildVisit_Break;
        }, &ctx);

    ir::IRValue dest = func.NewReg();
    ir::IRInst load(ir::IROpCode::kGetField);
    load.dest = dest;
    load.operands[0] = ctx.obj;
    load.label = member_name;
    func.body.push_back(load);
    return dest;
}

ir::IRValue ASTVisitor::LowerDeclRef(CXCursor expr, ir::IRFunction& func) {
    std::string name = GetCursorSpelling(expr);
    auto it = var_map_.find(name);
    if (it != var_map_.end()) {
        return it->second;
    }
    // Unknown variable - create a new register
    ir::IRValue reg = func.NewReg();
    var_map_[name] = reg;
    return reg;
}

ir::IRValue ASTVisitor::LowerIntegerLiteral(CXCursor expr, ir::IRFunction& func) {
    // Get the literal value from tokens
    CXSourceRange range = clang_getCursorExtent(expr);
    CXTranslationUnit tu = clang_Cursor_getTranslationUnit(expr);
    CXToken* tokens = nullptr;
    unsigned num_tokens = 0;
    clang_tokenize(tu, range, &tokens, &num_tokens);

    int64_t value = 0;
    if (num_tokens > 0) {
        std::string tok = CXStringToStd(clang_getTokenSpelling(tu, tokens[0]));
        value = std::strtoll(tok.c_str(), nullptr, 0);
    }
    clang_disposeTokens(tu, tokens, num_tokens);

    ir::IRValue dest = func.NewReg();
    ir::IRInst inst(ir::IROpCode::kLoadImm);
    inst.dest = dest;
    inst.operands[0] = ir::IRValue::Imm(value);
    func.body.push_back(inst);
    return dest;
}

ir::IRValue ASTVisitor::LowerFloatLiteral(CXCursor expr, ir::IRFunction& func) {
    CXSourceRange range = clang_getCursorExtent(expr);
    CXTranslationUnit tu = clang_Cursor_getTranslationUnit(expr);
    CXToken* tokens = nullptr;
    unsigned num_tokens = 0;
    clang_tokenize(tu, range, &tokens, &num_tokens);

    double value = 0.0;
    if (num_tokens > 0) {
        std::string tok = CXStringToStd(clang_getTokenSpelling(tu, tokens[0]));
        value = std::strtod(tok.c_str(), nullptr);
    }
    clang_disposeTokens(tu, tokens, num_tokens);

    ir::IRValue dest = func.NewReg();
    ir::IRInst inst(ir::IROpCode::kLoadImmF);
    inst.dest = dest;
    inst.operands[0] = ir::IRValue::ImmF(value);
    func.body.push_back(inst);
    return dest;
}

}  // namespace compiler
}  // namespace hotvm
