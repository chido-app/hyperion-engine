#include "Skeleton.hpp"
#include "Bone.hpp"
#include <Engine.hpp>
#include <rendering/backend/RendererResult.hpp>

namespace hyperion::v2 {

using renderer::Result;

#pragma region Render commmands

struct RENDER_COMMAND(UpdateSkeletonRenderData) : RenderCommand
{
    ID<Skeleton> id;
    SkeletonBoneData bone_data;

    RENDER_COMMAND(UpdateSkeletonRenderData)(ID<Skeleton> id, const SkeletonBoneData &bone_data)
        : id(id),
          bone_data(bone_data)
    {
    }

    virtual Result operator()() override
    {
        SkeletonShaderData shader_data;
        Memory::MemCpy(shader_data.bones, bone_data.matrices->Data(), sizeof(Matrix4) * MathUtil::Min(std::size(shader_data.bones), bone_data.matrices->Size()));

        g_engine->GetRenderData()->skeletons.Set(id.ToIndex(), shader_data);

        HYPERION_RETURN_OK;
    }
};

#pragma endregion

Skeleton::Skeleton()
    : EngineComponentBase(),
      m_root_bone(nullptr),
      m_shader_data_state(ShaderDataState::DIRTY)
{
}

Skeleton::Skeleton(const NodeProxy &root_bone)
    : EngineComponentBase()
{
    if (root_bone && root_bone.Get()->GetType() == Node::Type::BONE) {
        m_root_bone = root_bone;
        static_cast<Bone *>(m_root_bone.Get())->SetSkeleton(this);
    }
}

Skeleton::~Skeleton()
{
    SetReady(false);
}

void Skeleton::Init()
{
    if (IsInitCalled()) {
        return;
    }

    EngineComponentBase::Init();

    Update(0.0166f);

    SetReady(true);
}

void Skeleton::Update(GameCounter::TickUnit)
{
    if (!m_shader_data_state.IsDirty()) {
        return;
    }

    const auto num_bones = MathUtil::Min(SkeletonShaderData::max_bones, NumBones());

    if (num_bones != 0) {
        m_bone_data.SetMatrix(0, static_cast<Bone *>(m_root_bone.Get())->GetBoneMatrix());

        for (SizeType i = 1; i < num_bones; i++) {
            if (auto &descendent = m_root_bone.Get()->GetDescendents()[i - 1]) {
                if (!descendent) {
                    continue;
                }

                if (descendent.Get()->GetType() != Node::Type::BONE) {
                    continue;
                }

                m_bone_data.SetMatrix(i, static_cast<const Bone *>(descendent.Get())->GetBoneMatrix());
            }
        }

        PUSH_RENDER_COMMAND(UpdateSkeletonRenderData, GetID(), m_bone_data);
    }
    
    m_shader_data_state = ShaderDataState::CLEAN;
}

Bone *Skeleton::FindBone(const String &name)
{
    if (!m_root_bone) {
        return nullptr;
    }

    if (m_root_bone.GetName() == name) {
        return static_cast<Bone *>(m_root_bone.Get());
    }

    for (auto &node : m_root_bone.Get()->GetDescendents()) {
        if (!node) {
            continue;
        }

        if (node.Get()->GetType() != Node::Type::BONE) {
            continue;
        }

        Bone *bone = static_cast<Bone *>(node.Get());  // NOLINT(cppcoreguidelines-pro-type-static-cast-downcast)

        if (bone->GetName() == name) {
            return bone;
        }
    }

    return nullptr;
}

UInt Skeleton::FindBoneIndex(const String &name) const
{
    if (!m_root_bone) {
        return UInt(-1);
    }

    UInt index = 0;

    if (m_root_bone.GetName() == name) {
        return index;
    }

    for (auto &node : m_root_bone.Get()->GetDescendents()) {
        ++index;

        if (!node) {
            continue;
        }

        if (node.Get()->GetType() != Node::Type::BONE) {
            continue;
        }

        const Bone *bone = static_cast<const Bone *>(node.Get());  // NOLINT(cppcoreguidelines-pro-type-static-cast-downcast)

        if (bone->GetName() == name) {
            return index;
        }
    }

    return UInt(-1);
}

Bone *Skeleton::GetRootBone()
{
    return static_cast<Bone *>(m_root_bone.Get());
}

const Bone *Skeleton::GetRootBone() const
{
    return static_cast<const Bone *>(m_root_bone.Get());
}

void Skeleton::SetRootBone(const NodeProxy &root_bone)
{
    if (m_root_bone) {
        static_cast<Bone *>(m_root_bone.Get())->SetSkeleton(nullptr);

        m_root_bone = NodeProxy();
    }

    if (!root_bone || root_bone.Get()->GetType() != Node::Type::BONE) {
        return;
    }

    m_root_bone = root_bone;
    static_cast<Bone *>(m_root_bone.Get())->SetSkeleton(this);
}

SizeType Skeleton::NumBones() const
{
    if (!m_root_bone) {
        return 0;
    }

    return 1 + m_root_bone.Get()->GetDescendents().Size();
}

void Skeleton::AddAnimation(Animation &&animation)
{
    for (auto &track : animation.GetTracks()) {
        track.bone = nullptr;

        if (track.bone_name.Empty()) {
            continue;
        }

        track.bone = FindBone(track.bone_name);

        if (track.bone == nullptr) {
            DebugLog(
                LogType::Warn,
                "Skeleton could not find bone with name \"%s\"\n",
                track.bone_name.Data()
            );
        }
    }

    m_animations.PushBack(std::move(animation));
}

const Animation *Skeleton::FindAnimation(const String &name, UInt *out_index) const
{
    const auto it = m_animations.FindIf([&name](const auto &item) {
        return item.GetName() == name;
    });

    if (it == m_animations.End()) {
        return nullptr;
    }

    if (out_index != nullptr) {
        *out_index = m_animations.IndexOf(it);
    }

    return it;
}

} // namespace hyperion::v2
