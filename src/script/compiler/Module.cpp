#include <script/compiler/Module.hpp>

#include <system/Debug.hpp>

namespace hyperion::compiler {

Module::Module(
    const String &name,
    const SourceLocation &location
) : m_name(name),
    m_location(location),
    m_tree_link(nullptr)
{
}

FlatSet<String> Module::GenerateAllScanPaths() const
{
    FlatSet<String> all_scan_paths = m_scan_paths;

    TreeNode<Module*> *top = m_tree_link;
    
    while (top != nullptr) {
        AssertThrow(top->Get() != nullptr);

        for (const auto &other_scan_path : top->Get()->GetScanPaths()) {
            all_scan_paths.Insert(other_scan_path);
        }

        top = top->m_parent;
    }

    return all_scan_paths;
}

String Module::GenerateFullModuleName() const
{
    TreeNode<Module*> *top = m_tree_link;
    
    if (top != nullptr) {
        Array<String> parts;
        SizeType size = 0;

        do {
            AssertThrow(top->Get() != nullptr);

            parts.PushBack(top->Get()->GetName());
            size += top->Get()->GetName().Size();

            top = top->m_parent;
        } while (top != nullptr);

        String res;
        res.Reserve(size + (2 * parts.Size()));

        for (Int i = Int(parts.Size()) - 1; i >= 0; i--) {
            res += parts[i];

            if (i != 0) {
                res += "::";
            }
        }

        return res;
    }

    return m_name;
}

bool Module::IsInGlobalScope() const
{
    return m_scopes.TopNode()->m_parent == nullptr;
}

bool Module::IsInScopeOfType(ScopeType scope_type) const
{
    const TreeNode<Scope> *top = m_scopes.TopNode();

    while (top != nullptr) {
        if (top->Get().GetScopeType() == scope_type) {
            return true;
        }
        
        top = top->m_parent;
    }

    return false;
}

bool Module::IsInScopeOfType(ScopeType scope_type, UInt scope_flags) const
{
    const TreeNode<Scope> *top = m_scopes.TopNode();

    while (top != nullptr) {
        if (top->Get().GetScopeType() == scope_type && bool(UInt(top->Get().GetScopeFlags()) & scope_flags)) {
            return true;
        }
        
        top = top->m_parent;
    }

    return false;
}

Module *Module::LookupNestedModule(const String &name)
{
    AssertThrow(m_tree_link != nullptr);

    // search siblings of the current module,
    // rather than global lookup.
    for (auto *sibling : m_tree_link->m_siblings) {
        AssertThrow(sibling != nullptr);
        AssertThrow(sibling->Get() != nullptr);
        
        if (sibling->Get()->GetName() == name) {
            return sibling->Get();
        }
    }

    return nullptr;
}

Array<Module *> Module::CollectNestedModules() const
{
    AssertThrow(m_tree_link != nullptr);

    Array<Module *> nested_modules;

    // search siblings of the current module,
    // rather than global lookup.
    for (auto *sibling : m_tree_link->m_siblings) {
        AssertThrow(sibling != nullptr);
        AssertThrow(sibling->Get() != nullptr);

        nested_modules.PushBack(sibling->Get());
    }

    return nested_modules;
}

RC<Identifier> Module::LookUpIdentifier(const String &name, bool this_scope_only, bool outside_modules)
{
    TreeNode<Scope> *top = m_scopes.TopNode();

    while (top != nullptr) {
        if (RC<Identifier> result = top->Get().GetIdentifierTable().LookUpIdentifier(name)) {
            // a result was found
            return result;
        }

        if (this_scope_only) {
            return nullptr;
        }

        top = top->m_parent;
    }

    if (outside_modules) {
        if (m_tree_link != nullptr && m_tree_link->m_parent != nullptr) {
            if (Module *other = m_tree_link->m_parent->Get()) {
                if (other->GetLocation().GetFileName() == m_location.GetFileName()) {
                    return other->LookUpIdentifier(name, false);
                } else {
                    // we are outside of file scope, so loop until root/global module found
                    const TreeNode<Module*> *mod_link = m_tree_link->m_parent;

                    while (mod_link->m_parent != nullptr) {
                        mod_link = mod_link->m_parent;
                    }

                    AssertThrow(mod_link->Get() != nullptr);
                    AssertThrow(mod_link->Get()->GetName() == Config::global_module_name);

                    return mod_link->Get()->LookUpIdentifier(name, false);
                }
            }
        }
    }

    return nullptr;
}

RC<Identifier> Module::LookUpIdentifierDepth(const String &name, int depth_level)
{
    TreeNode<Scope> *top = m_scopes.TopNode();

    for (int i = 0; top != nullptr && i < depth_level; i++) {
        if (RC<Identifier> result = top->Get().GetIdentifierTable().LookUpIdentifier(name)) {
            return result;
        }

        top = top->m_parent;
    }

    return nullptr;
}

SymbolTypePtr_t Module::LookupSymbolType(const String &name)
{
    return PerformLookup(
        [&name](TreeNode<Scope> *top) {
            return top->Get().GetIdentifierTable().LookupSymbolType(name);
        },
        [&name](Module *mod) {
            return mod->LookupSymbolType(name);
        }
    );
}

SymbolTypePtr_t Module::LookupGenericInstance(
    const SymbolTypePtr_t &base,
    const Array<GenericInstanceTypeInfo::Arg> &params)
{
    return PerformLookup(
        [&base, &params](TreeNode<Scope> *top) {
            return top->Get().GetIdentifierTable().LookupGenericInstance(base, params);
        },
        [&base, &params](Module *mod) {
            return mod->LookupGenericInstance(base, params);
        }
    );
}

SymbolTypePtr_t Module::PerformLookup(
    std::function<SymbolTypePtr_t(TreeNode<Scope>*)> pred1,
    std::function<SymbolTypePtr_t(Module *mod)> pred2
)
{
    TreeNode<Scope> *top = m_scopes.TopNode();

    while (top) {
        if (SymbolTypePtr_t result = pred1(top)) {
            // a result was found
            return result;
        }
        top = top->m_parent;
    }

    if (m_tree_link && m_tree_link->m_parent) {
        if (Module *other = m_tree_link->m_parent->Get()) {
            if (other->GetLocation().GetFileName() == m_location.GetFileName()) {
                return pred2(other);
            } else {
                // we are outside of file scope, so loop until root/global module found
                auto *link = m_tree_link->m_parent;

                while (link->m_parent) {
                    link = link->m_parent;
                }

                AssertThrow(link->Get() != nullptr);
                AssertThrow(link->Get()->GetName() == Config::global_module_name);

                return pred2(link->Get());
            }
        }
    }

    return nullptr;
}

} // namespace hyperion::compiler
