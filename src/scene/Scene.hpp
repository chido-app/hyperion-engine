#ifndef HYPERION_V2_SCENE_H
#define HYPERION_V2_SCENE_H

#include "Node.hpp"
#include "Entity.hpp"
#include "Octree.hpp"
#include <rendering/Base.hpp>
#include <rendering/Texture.hpp>
#include <rendering/Shader.hpp>
#include <rendering/Light.hpp>
#include <core/Scheduler.hpp>
#include <core/lib/FlatSet.hpp>
#include <core/lib/FlatMap.hpp>
#include <camera/Camera.hpp>
#include <GameCounter.hpp>
#include <Types.hpp>
#include <unordered_map>

namespace hyperion::v2 {

class RenderEnvironment;
class World;

class Scene : public EngineComponentBase<STUB_CLASS(Scene)> {
    friend class Entity;
    friend class World;
public:
    static constexpr UInt32 max_environment_textures = SceneShaderData::max_environment_textures;

    Scene(Ref<Camera> &&camera);
    Scene(const Scene &other) = delete;
    Scene &operator=(const Scene &other) = delete;
    ~Scene();

    Ref<Camera> &GetCamera()                  { return m_camera; }
    const Ref<Camera> &GetCamera() const      { return m_camera; }
    void SetCamera(Ref<Camera> &&camera)      { m_camera = std::move(camera); }

    /*! Add an Entity to the queue. On Update(), it will be added to the scene. */
    bool AddEntity(Ref<Entity> &&entity);
    bool HasEntity(Entity::ID id) const;
    /*! Add an Remove to the from the Scene in an enqueued way. On Update(), it will be removed from the scene. */
    bool RemoveEntity(const Ref<Entity> &entity);

    /*! ONLY CALL FROM GAME THREAD!!! */
    auto &GetEntities()                       { return m_entities; }
    const auto &GetEntities() const           { return m_entities; }

    Node *GetRootNode() const                 { return m_root_node.get(); }

    RenderEnvironment *GetEnvironment() const { return m_environment; }

    World *GetWorld() const                   { return m_world; }
    void SetWorld(World *world);

    Scene::ID GetParentId() const             { return m_parent_id; }
    void SetParentId(Scene::ID id)            { m_parent_id = id; }
    
    void Init(Engine *engine);

    void ForceUpdate();

    BoundingBox m_aabb;

private:
    // World only calls
    void Update(
        Engine *engine,
        GameCounter::TickUnit delta,
        bool update_octree_visiblity = true
    );

    void EnqueueRenderUpdates();
    
    void AddPendingEntities();
    void RemovePendingEntities();

    void RequestRendererInstanceUpdate(Ref<Entity> &entity);
    void RemoveFromRendererInstance(Ref<Entity> &entity, RendererInstance *renderer_instance);
    void RemoveFromRendererInstances(Ref<Entity> &entity);

    Ref<Camera>                  m_camera;
    std::unique_ptr<Node>        m_root_node;
    RenderEnvironment           *m_environment;
    World                       *m_world;
    std::array<Ref<Texture>,     max_environment_textures> m_environment_textures;

    // entities live in GAME thread
    FlatMap<IDBase, Ref<Entity>> m_entities;
    // NOTE: not for thread safety, it's to defer updates so we don't
    // remove in the update loop.
    FlatSet<Entity::ID>          m_entities_pending_removal;
    FlatSet<Ref<Entity>>         m_entities_pending_addition;

    Matrix4                      m_last_view_projection_matrix;
                                 
    Scene::ID                    m_parent_id;
                                 
    mutable ShaderDataState      m_shader_data_state;
};

} // namespace hyperion::v2

#endif
