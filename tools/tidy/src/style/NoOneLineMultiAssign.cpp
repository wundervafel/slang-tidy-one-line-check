#include "ASTHelperVisitors.h"
#include "TidyDiags.h"
#include "fmt/color.h"
#include <algorithm>
#include <compare>
#include <map>

#include "slang/ast/ASTVisitor.h"
#include "slang/syntax/AllSyntax.h"

using namespace slang;
using namespace slang::ast;
using namespace slang::syntax;
using namespace slang::parsing;

namespace no_one_line_multi_assign {

struct MainVisitor : public TidyVisitor, ASTVisitor<MainVisitor, VisitFlags::AllGood> {
    explicit MainVisitor(Diagnostics& diagnostics) : TidyVisitor(diagnostics) {}

    void inspectLines() {
        auto code_iter{loc_map.begin()};
        while (code_iter != loc_map.end()) {
            std::multiset<ExpandedLoc> line_loc;
            std::size_t line{code_iter->first};

            auto lower_bound{loc_map.lower_bound(line)};
            auto upper_bound{loc_map.upper_bound(line)};
            std::for_each(lower_bound, upper_bound,
                          [&line_loc](const auto& loc) { line_loc.insert(loc.second); });

            code_iter = upper_bound;

            auto line_iter{line_loc.begin()};
            ExpandedLoc prev_loc{*line_iter};
            for (line_iter++; line_iter != line_loc.end(); prev_loc = *line_iter, line_iter++) {
                if (*line_iter == prev_loc) {
                    continue;
                }

                if (line_iter->kind == ExpandedLoc::BlockName ||
                    line_iter->kind == ExpandedLoc::DoWhile ||
                    line_iter->kind == ExpandedLoc::DoWhileEnd) {
                    continue;
                }

                if (prev_loc.kind == prev_loc.If || prev_loc.kind == prev_loc.Else ||
                    prev_loc.kind == prev_loc.DoWhileStart || prev_loc.kind == prev_loc.Type ||
                    prev_loc.kind == prev_loc.CaseCond) {
                    continue;
                }

                if (prev_loc.kind == prev_loc.Statement) {
                    StatementKind stmt_kind{prev_loc.source.stmt->kind};
                    if (stmt_kind == StatementKind::RepeatLoop ||
                        stmt_kind == StatementKind::WhileLoop ||
                        stmt_kind == StatementKind::ForeverLoop ||
                        stmt_kind == StatementKind::ForeachLoop ||
                        stmt_kind == StatementKind::ForLoop) {
                        continue;
                    }

                    if (stmt_kind == StatementKind::VariableDeclaration &&
                        line_iter->kind == line_iter->Statement &&
                        line_iter->source.stmt->kind == StatementKind::VariableDeclaration) {
                        continue;
                    }
                }

                if (prev_loc.kind == prev_loc.Symbol) {
                    SymbolKind symb_kind{prev_loc.source.symb->kind};
                    if (symb_kind == SymbolKind::ProceduralBlock) {
                        continue;
                    }

                    if (symb_kind == SymbolKind::Variable) {
                        if (line_iter->kind == ExpandedLoc::Symbol &&
                            line_iter->source.symb->kind == SymbolKind::Variable) {
                            continue;
                        }
                    }
                }

                line_iter->addDiag(diags, sourceManager);
            }
        }
    }

    Token findIf(const ConditionalStatement& stmt) {
        const auto& syntax{stmt.syntax->as<ConditionalStatementSyntax>()};
        return syntax.ifKeyword;
    }
    Token findElse(const ConditionalStatement& stmt) {
        const auto& syntax{stmt.syntax->as<ConditionalStatementSyntax>()};
        const ElseClauseSyntax* else_syntax = syntax.elseClause;
        if (!else_syntax) {
            return Token{};
        }

        return else_syntax->elseKeyword;
    }

    struct ExpandedLoc {
        bool is_macro{false};
        SourceLocation loc;
        SourceLocation orig_loc;

        std::size_t line{};
        std::size_t column{};
        std::size_t offset{};

        Token token;

        enum LocKind {
            Symbol,
            Statement,
            Expression,
            CaseCond,
            Endcase,
            If,
            Else,
            BlockStart,
            BlockEnd,
            BlockName,
            DoWhileStart,
            DoWhileEnd,
            DoWhile,
            Type,
        } kind{};

        union {
            const ast::Symbol* symb;
            const ast::Statement* stmt;
            const ast::Expression* expr;
        } source{};

        ExpandedLoc() noexcept = default;
        ExpandedLoc(const ExpandedLoc&) = default;

        ExpandedLoc(LocKind kind, const Token& token, const SourceLocation& orig_loc,
                    const SourceManager* sourceManager) :
            is_macro{sourceManager->isMacroLoc(orig_loc)},
            loc{is_macro ? sourceManager->getFullyExpandedLoc(orig_loc) : orig_loc},
            orig_loc{orig_loc}, line{sourceManager->getLineNumber(loc)},
            column{sourceManager->getColumnNumber(loc)}, offset{loc.offset()}, token{token},
            kind{kind} {}

        template<typename TSource>
        ExpandedLoc(TSource& source, LocKind kind, const Token& token,
                    const SourceLocation& orig_loc, const SourceManager* sourceManager) :
            ExpandedLoc(kind, token, orig_loc, sourceManager) {
            if constexpr (std::is_base_of_v<ast::Symbol, TSource>) {
                ExpandedLoc::source.symb = &source;
            }
            else if constexpr (std::is_base_of_v<ast::Statement, TSource>) {
                ExpandedLoc::source.stmt = &source;
            }
            else if constexpr (std::is_base_of_v<ast::Expression, TSource>) {
                ExpandedLoc::source.expr = &source;
            }
        }

        inline bool isDifferentMacro(const ExpandedLoc& other) const noexcept {
            return is_macro && other.is_macro && offset != other.offset;
        }

        void addDiag(Diagnostics& diags, const SourceManager* sourceManager) const {
            SourceLocation final_loc{!is_macro ? loc : sourceManager->getExpansionLoc(orig_loc)};
            diags.add(diag::NoOneLineMultiAssign, final_loc);
        }

        friend std::strong_ordering operator<=>(const ExpandedLoc& a,
                                                const ExpandedLoc& b) noexcept {
            if (a.offset < b.offset) {
                return std::strong_ordering::less;
            }
            if (a.offset > b.offset) {
                return std::strong_ordering::greater;
            }
            return std::strong_ordering::equivalent;
        }

        friend bool operator==(const ExpandedLoc& a, const ExpandedLoc& b) noexcept {
            return (a <=> b) == 0;
        }
    };

    std::multimap<std::size_t, ExpandedLoc> loc_map;

    template<std::derived_from<Symbol> TSymbol>
    void handle(const TSymbol& symb) {
        visitDefault(symb);
    }

    void handle(const StatementBlockSymbol&) {};

    void handle(const VariableSymbol& symb) {
        const auto& var_syntax{symb.getSyntax()->as<DataDeclarationSyntax>()};
        const DataTypeSyntax& type_syntax{*symb.getDeclaredType()->getTypeSyntax()};

        Token first_token{var_syntax.getFirstToken()};
        Token last_token{var_syntax.getLastToken()};

        ExpandedLoc start_loc{symb, start_loc.Symbol, first_token, first_token.location(),
                              sourceManager};
        ExpandedLoc end_loc{symb, end_loc.Symbol, last_token, last_token.location(), sourceManager};

        loc_map.emplace(start_loc.line, start_loc);
        if (start_loc.line != end_loc.line) {
            loc_map.emplace(end_loc.line, end_loc);
        }

        Token type_token{type_syntax.getFirstToken()};
        ExpandedLoc type_loc{type_loc.Type, type_token, type_token.location(), sourceManager};
        loc_map.emplace(type_loc.line, type_loc);
    }

    void handle(const ProceduralBlockSymbol& symb) {
        ExpandedLoc loc{symb, loc.Symbol, symb.getSyntax()->getFirstToken(), symb.location,
                        sourceManager};
        loc_map.emplace(loc.line, loc);

        const Statement* body = &symb.getBody();
        if (body->kind == StatementKind::Timed) {
            body = &body->as<TimedStatement>().stmt;
        }
        body->visit(*this);
    }

    void handle(const PrimitiveInstanceSymbol& symb) {
        Token first_token{symb.getSyntax()->getFirstToken()};
        Token last_token{symb.getSyntax()->getLastToken()};

        ExpandedLoc start_loc{symb, start_loc.Symbol, first_token, first_token.location(),
                              sourceManager};
        ExpandedLoc end_loc{symb, end_loc.Symbol, last_token, last_token.location(), sourceManager};

        loc_map.emplace(start_loc.line, start_loc);
        if (start_loc.line != end_loc.line) {
            loc_map.emplace(end_loc.line, end_loc);
        }
    }

    void handle(const ContinuousAssignSymbol& symb) {
        const auto& syntax{symb.getSyntax()->as<ContinuousAssignSyntax>()};

        Token assign{syntax.assign};
        ExpandedLoc assign_loc{symb, assign_loc.Symbol, assign, assign.location(), sourceManager};
        loc_map.emplace(assign_loc.line, assign_loc);

        symb.getAssignment().visit(*this);
    }

    template<std::derived_from<Statement> TStatement>
    void handle(const TStatement& stmt) {
        if (!stmt.syntax) {
            visitDefault(stmt);
            return;
        }

        Token first_token{stmt.syntax->getFirstToken()};
        Token last_token{stmt.syntax->getLastToken()};

        ExpandedLoc start_loc{stmt, start_loc.Statement, first_token, first_token.location(),
                              sourceManager};
        ExpandedLoc end_loc{stmt, end_loc.Statement, last_token, last_token.location(),
                            sourceManager};
        loc_map.emplace(start_loc.line, start_loc);
        if (start_loc.line != end_loc.line) {
            loc_map.emplace(end_loc.line, end_loc);
        }
    }

    void handle(const ConditionalStatement& stmt) {
        Token if_token{findIf(stmt)};

        ExpandedLoc if_loc{stmt, if_loc.If, if_token, if_token.location(), sourceManager};
        loc_map.emplace(if_loc.line, if_loc);

        stmt.ifTrue.visit(*this);

        if (stmt.ifFalse) {
            Token else_token{findElse(stmt)};
            ExpandedLoc else_loc{stmt, else_loc.Else, else_token, else_token.location(),
                                 sourceManager};

            loc_map.emplace(else_loc.line, else_loc);

            stmt.ifFalse->visit(*this);
        }
    }

    void handle(const VariableDeclStatement& stmt) { stmt.symbol.visit(*this); }

    void handle(const CaseStatement& stmt) {
        Token first_token{stmt.syntax->getFirstToken()};
        Token last_token{stmt.syntax->getLastToken()};

        ExpandedLoc start_loc{stmt, start_loc.Statement, first_token, first_token.location(),
                              sourceManager};
        ExpandedLoc end_loc{end_loc.Endcase, last_token, last_token.location(), sourceManager};

        loc_map.emplace(start_loc.line, start_loc);
        loc_map.emplace(end_loc.line, end_loc);

        const auto& syntax{stmt.syntax->as<CaseStatementSyntax>()};
        for (const auto& item : syntax.items) {
            Token case_cond;
            Token colon;

            if (item->kind == SyntaxKind::DefaultCaseItem) {
                const auto& condition{item->as<DefaultCaseItemSyntax>()};
                case_cond = condition.defaultKeyword;
                colon = condition.colon;
            }
            else if (item->kind == SyntaxKind::StandardCaseItem) {
                const auto& condition{item->as<StandardCaseItemSyntax>()};
                case_cond = condition.getFirstToken();
                colon = condition.colon;
            }
            else {
                const auto& condition{item->as<PatternCaseItemSyntax>()};
                case_cond = condition.getFirstToken();
                colon = condition.colon;
            }

            ExpandedLoc case_loc{case_loc.CaseCond, case_cond, case_cond.location(), sourceManager};
            ExpandedLoc colon_loc{colon_loc.CaseCond, colon, colon.location(), sourceManager};

            loc_map.emplace(case_loc.line, case_loc);
            if (case_loc.line != colon_loc.line) {
                loc_map.emplace(colon_loc.line, colon_loc);
            }
        }

        stmt.visitStmts(*this);
    }

    void handle(const StatementList& stmt) { visitDefault(stmt); }

    void handle(const BlockStatement& stmt) {
        Token first_token = stmt.syntax->getFirstToken();
        if (first_token.kind == TokenKind::BeginKeyword ||
            first_token.kind == TokenKind::ForkKeyword) {

            Token last_token = stmt.syntax->getLastToken();

            ExpandedLoc start_loc{start_loc.BlockStart, first_token, first_token.location(),
                                  sourceManager};
            ExpandedLoc end_loc{end_loc.BlockEnd, last_token, last_token.location(), sourceManager};

            loc_map.emplace(start_loc.line, start_loc);
            if (start_loc.line != end_loc.line) {
                loc_map.emplace(end_loc.line, end_loc);
            }

            const auto* name = stmt.syntax->as<BlockStatementSyntax>().blockName;
            if (name) {
                Token name_token = name->name;
                ExpandedLoc name_loc{name_loc.BlockName, name_token, name_token.location(),
                                     sourceManager};
                if (start_loc.line != name_loc.line) {
                    loc_map.emplace(name_loc.line, name_loc);
                }
            }

            visitDefault(stmt);
        }
        else if (first_token.kind == TokenKind::ForKeyword) {
            // Ищем ForLoopStatement в списке
            const auto& list{stmt.body.as<StatementList>().list};
            for (const auto& entry : list) {
                if (entry->kind == StatementKind::ForLoop) {
                    entry->visit(*this);
                }
            }
        }
    }

    void handle(const ForLoopStatement& stmt) {
        const auto& syntax{stmt.syntax->as<ForLoopStatementSyntax>()};
        Token start_token{syntax.forKeyword};
        Token end_token{syntax.closeParen};

        ExpandedLoc start_loc(stmt, start_loc.Statement, start_token, start_token.location(),
                              sourceManager);
        ExpandedLoc end_loc(stmt, end_loc.Statement, end_token, end_token.location(),
                            sourceManager);

        loc_map.emplace(start_loc.line, start_loc);
        if (start_loc.line != end_loc.line) {
            loc_map.emplace(end_loc.line, end_loc);
        }

        stmt.visitStmts(*this);
    }

    void handle(const ForeachLoopStatement& stmt) {
        const auto& syntax{stmt.syntax->as<ForeachLoopStatementSyntax>()};
        Token start_token{syntax.keyword};
        Token end_token{syntax.loopList->closeParen};

        ExpandedLoc start_loc(stmt, start_loc.Statement, start_token, start_token.location(),
                              sourceManager);
        ExpandedLoc end_loc(stmt, end_loc.Statement, end_token, end_token.location(),
                            sourceManager);

        loc_map.emplace(start_loc.line, start_loc);
        if (start_loc.line != end_loc.line) {
            loc_map.emplace(end_loc.line, end_loc);
        }

        stmt.visitStmts(*this);
    }

    void handle(const ForeverLoopStatement& stmt) {
        Token start_token{stmt.syntax->getFirstToken()};
        ExpandedLoc start_loc(stmt, start_loc.Statement, start_token, start_token.location(),
                              sourceManager);
        loc_map.emplace(start_loc.line, start_loc);
        stmt.visitStmts(*this);
    }

    void handle(const RepeatLoopStatement& stmt) {
        const auto& syntax{stmt.syntax->as<LoopStatementSyntax>()};
        Token start_token{syntax.repeatOrWhile};
        Token end_token{syntax.closeParen};

        ExpandedLoc start_loc(stmt, start_loc.Statement, start_token, start_token.location(),
                              sourceManager);
        ExpandedLoc end_loc(stmt, end_loc.Statement, end_token, end_token.location(),
                            sourceManager);

        loc_map.emplace(start_loc.line, start_loc);
        if (start_loc.line != end_loc.line) {
            loc_map.emplace(end_loc.line, end_loc);
        }

        stmt.visitStmts(*this);
    }

    void handle(const WhileLoopStatement& stmt) {
        const auto& syntax{stmt.syntax->as<LoopStatementSyntax>()};
        Token start_token{syntax.repeatOrWhile};
        Token end_token{syntax.closeParen};

        ExpandedLoc start_loc(stmt, start_loc.Statement, start_token, start_token.location(),
                              sourceManager);
        ExpandedLoc end_loc(stmt, end_loc.Statement, end_token, end_token.location(),
                            sourceManager);

        loc_map.emplace(start_loc.line, start_loc);
        if (start_loc.line != end_loc.line) {
            loc_map.emplace(end_loc.line, end_loc);
        }

        stmt.visitStmts(*this);
    }

    void handle(const DoWhileLoopStatement& stmt) {
        const auto& syntax{stmt.syntax->as<DoWhileStatementSyntax>()};
        Token start_token{syntax.doKeyword};
        Token while_token{syntax.whileKeyword};
        Token end_token{syntax.getLastToken()};

        ExpandedLoc start_loc(start_loc.DoWhileStart, start_token, start_token.location(),
                              sourceManager);
        ExpandedLoc while_loc(while_loc.DoWhile, while_token, while_token.location(),
                              sourceManager);
        ExpandedLoc end_loc(end_loc.DoWhileEnd, end_token, end_token.location(), sourceManager);

        loc_map.emplace(start_loc.line, start_loc);
        loc_map.emplace(while_loc.line, while_loc);
        if (while_loc.line != end_loc.line) {
            loc_map.emplace(end_loc.line, end_loc);
        }

        stmt.visitStmts(*this);
    }

    void handle(const AssignmentExpression& expr) {
        const SyntaxNode* syntax{expr.syntax};
        if (syntax) {
            Token first_token{expr.syntax->getFirstToken()};
            Token last_token{expr.syntax->getLastToken()};

            ExpandedLoc start_loc{expr, start_loc.Expression, first_token, first_token.location(),
                                  sourceManager};
            ExpandedLoc end_loc{expr, end_loc.Expression, last_token, last_token.location(),
                                sourceManager};
            loc_map.emplace(start_loc.line, start_loc);
            if (start_loc.line != end_loc.line) {
                loc_map.emplace(end_loc.line, end_loc);
            }
        }
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
        visitor.inspectLines();
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
