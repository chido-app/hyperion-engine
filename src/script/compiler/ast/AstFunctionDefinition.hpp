#ifndef AST_FUNCTION_DEFINITION_HPP
#define AST_FUNCTION_DEFINITION_HPP

#include <script/compiler/ast/AstDeclaration.hpp>
#include <script/compiler/ast/AstFunctionExpression.hpp>

#include <memory>
#include <vector>

namespace hyperion::compiler {

class AstFunctionDefinition : public AstDeclaration {
public:
    AstFunctionDefinition(const std::string &name,
        const RC<AstFunctionExpression> &expr,
        const SourceLocation &location);
    virtual ~AstFunctionDefinition() = default;

    const RC<AstFunctionExpression> &GetAssignment() const
        { return m_expr; }

    virtual void Visit(AstVisitor *visitor, Module *mod) override;
    virtual std::unique_ptr<Buildable> Build(AstVisitor *visitor, Module *mod) override;
    virtual void Optimize(AstVisitor *visitor, Module *mod) override;
    
    virtual RC<AstStatement> Clone() const override;

protected:
    RC<AstFunctionExpression> m_expr;

    RC<AstFunctionDefinition> CloneImpl() const
    {
        return RC<AstFunctionDefinition>(new AstFunctionDefinition(
            m_name,
            CloneAstNode(m_expr),
            m_location
        ));
    }
};

} // namespace hyperion::compiler

#endif
