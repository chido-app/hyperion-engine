#include <script/compiler/ast/AstFunctionDefinition.hpp>
#include <script/compiler/AstVisitor.hpp>
#include <script/compiler/Module.hpp>
#include <script/compiler/Configuration.hpp>

#include <script/compiler/emit/BytecodeChunk.hpp>
#include <script/compiler/emit/BytecodeUtil.hpp>

#include <script/Instructions.hpp>
#include <system/debug.h>

namespace hyperion {
namespace compiler {

AstFunctionDefinition::AstFunctionDefinition(const std::string &name,
    const std::shared_ptr<AstFunctionExpression> &expr,
    const SourceLocation &location)
    : AstDeclaration(name, location),
      m_expr(expr)
{
}

void AstFunctionDefinition::Visit(AstVisitor *visitor, Module *mod)
{
    AssertThrow(m_expr != nullptr);
    m_expr->Visit(visitor, mod);

    AstDeclaration::Visit(visitor, mod);

    if (m_identifier) {
        // functions are implicitly const
        m_identifier->GetFlags() |= FLAG_CONST;
        m_identifier->SetSymbolType(m_expr->GetSymbolType());
        m_identifier->SetCurrentValue(m_expr);
    }
}

std::unique_ptr<Buildable> AstFunctionDefinition::Build(AstVisitor *visitor, Module *mod)
{
    std::unique_ptr<BytecodeChunk> chunk = BytecodeUtil::Make<BytecodeChunk>();

    if (!hyperion::compiler::Config::cull_unused_objects || m_identifier->GetUseCount() > 0) {
        // get current stack size
        int stack_location = visitor->GetCompilationUnit()->GetInstructionStream().GetStackSize();
        // set identifier stack location
        m_identifier->SetStackLocation(stack_location);

        // increment stack size before we build the expression
        visitor->GetCompilationUnit()->GetInstructionStream().IncStackSize();

        // build function expression
        chunk->Append(m_expr->Build(visitor, mod));

        // get active register
        uint8_t rp = visitor->GetCompilationUnit()->GetInstructionStream().GetCurrentRegister();
        
        // store on stack
        auto instr_push = BytecodeUtil::Make<RawOperation<>>();
        instr_push->opcode = PUSH;
        instr_push->Accept<uint8_t>(rp);
        chunk->Append(std::move(instr_push));
    }

    return std::move(chunk);
}

void AstFunctionDefinition::Optimize(AstVisitor *visitor, Module *mod)
{
    m_expr->Optimize(visitor, mod);
}

Pointer<AstStatement> AstFunctionDefinition::Clone() const
{
    return CloneImpl();
}

} // namespace compiler
} // namespace hyperion
