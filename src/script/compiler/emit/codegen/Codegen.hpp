#ifndef CODEGEN_HPP
#define CODEGEN_HPP

#include <script/compiler/emit/codegen/InternalByteStream.hpp>
#include <script/compiler/emit/BuildableVisitor.hpp>

#include <system/debug.h>

#include <map>
#include <sstream>
#include <vector>
#include <cstdint>

namespace hyperion {
namespace compiler {

class CodeGenerator : public BuildableVisitor {
public:
    CodeGenerator(BuildParams &build_params);
    virtual ~CodeGenerator() = default;

    inline InternalByteStream &GetInternalByteStream() { return m_ibs; }
    inline const InternalByteStream &GetInternalByteStream() const { return m_ibs; }

    virtual void Visit(BytecodeChunk *);
    virtual void Visit(LabelMarker *);
    virtual void Visit(Jump *);
    virtual void Visit(Comparison *);
    virtual void Visit(FunctionCall *);
    virtual void Visit(Return *);
    virtual void Visit(StoreLocal *);
    virtual void Visit(PopLocal *);
    virtual void Visit(LoadRef *);
    virtual void Visit(LoadDeref *);
    virtual void Visit(ConstI32 *);
    virtual void Visit(ConstI64 *);
    virtual void Visit(ConstF32 *);
    virtual void Visit(ConstF64 *);
    virtual void Visit(ConstBool *);
    virtual void Visit(ConstNull *);
    virtual void Visit(BuildableTryCatch *);
    virtual void Visit(BuildableFunction *);
    virtual void Visit(BuildableType *);
    virtual void Visit(BuildableString *);
    virtual void Visit(StorageOperation *);
    virtual void Visit(RawOperation<> *);

private:
    BuildParams &build_params;
    InternalByteStream m_ibs;
};

} // namespace compiler
} // namespace hyperion

#endif
