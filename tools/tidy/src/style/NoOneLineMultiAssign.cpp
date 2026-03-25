#include "ASTHelperVisitors.h"
#include "TidyDiags.h"
#include "fmt/color.h"
#include <iostream>

#include "slang/ast/ASTVisitor.h"
#include "slang/ast/statements/LoopStatements.h"
#include "slang/syntax/AllSyntax.h"

using namespace slang;
using namespace slang::ast;
using namespace slang::syntax;

namespace no_one_line_multi_assign {

struct MainVisitor : public TidyVisitor, ASTVisitor<MainVisitor, VisitFlags::AllGood> {
    explicit MainVisitor(Diagnostics& diagnostics) : TidyVisitor(diagnostics) {}

private:
    std::size_t prev_stmt_line{};
    StatementKind prev_stmt_kind{StatementKind::Invalid};

    const SyntaxNode* elseSyntax(const ConditionalStatement& stmt) {
        const SyntaxNode* syntax = stmt.syntax;
        for (int i{0}; i < syntax->getChildCount(); i++) {
            const SyntaxNode* child = syntax->childNode(i);
            if (child && child->kind == SyntaxKind::ElseClause) {
                return child;
            }
        }

        return nullptr;
    }

    void handleConditional(const ConditionalStatement& stmt) {
        const Statement& true_stmt = stmt.ifTrue;
        true_stmt.visit(*this);

        const Statement* false_stmt = stmt.ifFalse;
        if (false_stmt) {
            SourceLocation else_loc = elseSyntax(stmt)->sourceRange().start();
            std::size_t else_line = sourceManager->getLineNumber(else_loc);

            if (else_line == prev_stmt_line) {
                diags.add(diag::NoOneLineMultiAssign, else_loc);
            }

            prev_stmt_kind = StatementKind::Conditional;
            false_stmt->visit(*this);
        }
    }

    // Случай по умолчанию ищем на первых двух уровнях дерева
    const SyntaxNode* defaultCaseSyntax(const CaseStatement& stmt) {
        const SyntaxNode* syntax = stmt.syntax;
        for (int i{0}; i < syntax->getChildCount(); i++) {
            const SyntaxNode* child = syntax->childNode(i);
            if (child) {
                for (int i{0}; i < child->getChildCount(); i++) {
                    const SyntaxNode* grandchild = child->childNode(i);
                    if (grandchild && grandchild->kind == SyntaxKind::DefaultCaseItem) {
                        return grandchild;
                    }
                }
            }
        }

        return nullptr;
    }

    struct CaseLine {
        CaseLine(SourceLocation case_start, SourceLocation case_end, const Statement* stmt,
                 const SourceManager* sourceManager) :
            stmt{stmt}, case_start{case_start},
            case_start_line{sourceManager->getLineNumber(case_start)},
            case_end_line{sourceManager->getLineNumber(case_end)},
            case_start_column{sourceManager->getColumnNumber(case_start)},
            is_macro_case_start{sourceManager->isMacroLoc(case_start)} {}

        const Statement* stmt;

        SourceLocation case_start{};

        std::size_t case_start_line{};
        std::size_t case_end_line{};

        std::size_t case_start_column{};

        bool is_macro_case_start{};
    };

    void handleCase(const CaseStatement& stmt) {
        std::vector<CaseLine> cases;

        const Statement* default_stmt = stmt.defaultCase;
        if (default_stmt) {
            SourceLocation default_start = defaultCaseSyntax(stmt)->sourceRange().start();
            SourceLocation default_case_end = default_start;

            cases.push_back({default_start, default_case_end, default_stmt, sourceManager});
        }

        for (auto iter_items{stmt.items.begin()}; iter_items != stmt.items.end(); iter_items++) {
            const Statement& item_stmt{*iter_items->stmt.get()};
            SourceLocation start_loc{iter_items->expressions.front()->sourceRange.start()};
            SourceLocation case_end_loc{iter_items->expressions.back()->sourceRange.end()};

            cases.push_back({start_loc, case_end_loc, iter_items->stmt, sourceManager});
        }

        std::sort(cases.begin(), cases.end(), [this](const CaseLine& a, const CaseLine& b) {
            return !(
                a.case_start_line > b.case_start_line ||
                (a.case_start_line == b.case_start_line && a.case_start_line > b.case_start_line));
        });

        for (auto iter_cases = cases.begin(); iter_cases != cases.end(); iter_cases++) {
            if (!iter_cases->is_macro_case_start && iter_cases->case_start_line == prev_stmt_line) {
                diags.add(diag::NoOneLineMultiAssign, iter_cases->case_start);
            }
            else if (iter_cases->is_macro_case_start &&
                     iter_cases->case_start_line == prev_stmt_line) {
                diags.add(diag::NoOneLineMultiAssign,
                          sourceManager->getExpansionLoc(iter_cases->case_start));
            }

            prev_stmt_kind = stmt.kind;
            prev_stmt_line = 0;
            iter_cases->stmt->visit(*this);
        }

        prev_stmt_kind = stmt.kind;
    }

    template<std::derived_from<Statement> TStatement>
    void handleLoop(const TStatement& stmt) {
        stmt.body.visit(*this);
    }

public:
    template<std::derived_from<Statement> TStatement>
    void handle(const TStatement& stmt) {
        SourceLocation start{stmt.sourceRange.start()};
        SourceLocation end{stmt.sourceRange.end()};

        bool is_macro_start{sourceManager->isMacroLoc(start)};

        std::size_t start_line{sourceManager->getLineNumber(start)};
        std::size_t end_line{sourceManager->getLineNumber(end)};

        std::cout << stmt.kind << ' ' << start_line << '\n';
        std::size_t start_column{sourceManager->getColumnNumber(start)};
        std::size_t end_column{sourceManager->getColumnNumber(end)};

        if (std::is_same_v<TStatement, VariableDeclStatement> &&
            prev_stmt_kind == StatementKind::VariableDeclaration) {
            prev_stmt_line = start_line;
            return;
        }

        if constexpr (std::is_same_v<TStatement, StatementList>) {
            visitDefault(stmt);
            return;
        }

        if constexpr (std::is_same_v<TStatement, BlockStatement>) {
            switch (prev_stmt_kind) {
                case StatementKind::Case:
                case StatementKind::Conditional:
                case StatementKind::ForLoop:
                case StatementKind::ForeverLoop:
                case StatementKind::ForeachLoop:
                case StatementKind::WhileLoop:
                case StatementKind::DoWhileLoop:
                case StatementKind::RepeatLoop:
                case StatementKind::Timed:
                    prev_stmt_line = start_line;
                    prev_stmt_kind = stmt.kind;

                    visitDefault(stmt);

                    prev_stmt_line = end_line;
                    prev_stmt_kind = stmt.kind;
                    return;
                default:
                    break;
            }
        }

        if (!is_macro_start && prev_stmt_line == start_line) {
            diags.add(diag::NoOneLineMultiAssign, start);
        }
        else if (is_macro_start && prev_stmt_line == start_line) {
            diags.add(diag::NoOneLineMultiAssign, sourceManager->getExpansionLoc(start));
        }

        if constexpr (std::is_same_v<TStatement, ConditionalStatement>) {
            handleConditional(stmt);
            prev_stmt_line = end_line;
            prev_stmt_kind = stmt.kind;
            return;
        }
        else if constexpr (std::is_same_v<TStatement, ForeachLoopStatement> ||
                           std::is_same_v<TStatement, ForLoopStatement> ||
                           std::is_same_v<TStatement, ForeverLoopStatement> ||
                           std::is_same_v<TStatement, WhileLoopStatement> ||
                           std::is_same_v<TStatement, DoWhileLoopStatement> ||
                           std::is_same_v<TStatement, RepeatLoopStatement>) {
            handleLoop(stmt);
            prev_stmt_line = end_line;
            prev_stmt_kind = stmt.kind;
            return;
        }

        prev_stmt_line = start_line;
        prev_stmt_kind = stmt.kind;

        if constexpr (std::is_same_v<TStatement, CaseStatement>) {
            handleCase(stmt);
        }
        else if constexpr (!std::is_same_v<TStatement, ExpressionStatement>) {
            visitDefault(stmt);
        }

        prev_stmt_line = end_line;
        prev_stmt_kind = stmt.kind;
    }

    void handle(const AssignmentExpression& expr) {
        SourceLocation start{expr.sourceRange.start()};
        SourceLocation end{expr.sourceRange.end()};

        bool is_macro_start{sourceManager->isMacroLoc(start)};

        std::size_t start_line{sourceManager->getLineNumber(start)};
        std::size_t end_line{sourceManager->getLineNumber(end)};
        std::cout << expr.kind << ' ' << start_line << '\n';

        std::size_t start_column{sourceManager->getColumnNumber(start)};
        std::size_t end_column{sourceManager->getColumnNumber(end)};

        if (!is_macro_start && prev_stmt_line == start_line) {
            diags.add(diag::NoOneLineMultiAssign, start);
        }
        else if (is_macro_start && prev_stmt_line == start_line) {
            diags.add(diag::NoOneLineMultiAssign, sourceManager->getExpansionLoc(start));
        }

        prev_stmt_line = end_line;
    }
};
} // namespace no_one_line_multi_assign

using namespace no_one_line_multi_assign;
class NoOneLineMultiAssign : public TidyCheck {
public:
    [[maybe_unused]] explicit NoOneLineMultiAssign(
        TidyKind kind, std::optional<slang::DiagnosticSeverity> severity) :
        TidyCheck(kind, severity) {}

    bool check(const ast::RootSymbol& root, const slang::analysis::AnalysisManager&) override {
        MainVisitor visitor(diagnostics);
        root.visit(visitor);
        return diagnostics.empty();
    }

    DiagCode diagCode() const override { return diag::NoOneLineMultiAssign; }
    DiagnosticSeverity diagDefaultSeverity() const override { return DiagnosticSeverity::Warning; }
    std::string diagString() const override {
        return "multiple non hierarchically dependent statements described "
               "in one line";
    }
    std::string name() const override { return "NoOneLineMultiAssign"; }
    std::string description() const override { return shortDescription(); }
    std::string shortDescription() const override {
        return "Multiple statements are describedin the single line. "
               "Describe one statement per line to improve RTL "
               "description readability.";
    }
};

REGISTER(NoOneLineMultiAssign, NoOneLineMultiAssign, TidyKind::Style)
