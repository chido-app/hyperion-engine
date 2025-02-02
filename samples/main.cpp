﻿
#include "system/SdlSystem.hpp"
#include "system/Debug.hpp"

#include <HyperionEngine.hpp>

#include <rendering/backend/RendererInstance.hpp>
#include <rendering/backend/RendererDescriptorSet.hpp>
#include <rendering/backend/RendererImage.hpp>
#include <rendering/backend/RendererRenderPass.hpp>
#include <rendering/backend/rt/RendererRaytracingPipeline.hpp>

#include <core/lib/Proc.hpp>

#include <Engine.hpp>
#include <scene/Node.hpp>
#include <rendering/Atomics.hpp>
#include <scene/animation/Bone.hpp>
#include <rendering/rt/AccelerationStructureBuilder.hpp>
#include <rendering/rt/ProbeSystem.hpp>
#include <rendering/post_fx/FXAA.hpp>
#include <scene/controllers/AudioController.hpp>
#include <scene/controllers/AnimationController.hpp>
#include <scene/controllers/AabbDebugController.hpp>
#include <scene/controllers/FollowCameraController.hpp>
#include <scene/controllers/paging/BasicPagingController.hpp>
#include <scene/controllers/ScriptedController.hpp>
#include <scene/controllers/physics/RigidBodyController.hpp>
#include <scene/controllers/LightController.hpp>
#include <scene/controllers/ShadowMapController.hpp>
#include <scene/controllers/EnvGridController.hpp>
#include <ui/controllers/UIButtonController.hpp>
#include <ui/controllers/UIContainerController.hpp>
#include <core/lib/FlatSet.hpp>
#include <core/lib/FlatMap.hpp>
#include <core/lib/Pair.hpp>
#include <core/lib/DynArray.hpp>

#include <util/json/JSON.hpp>

#include <core/net/Socket.hpp>

#include <GameThread.hpp>
#include <Game.hpp>

#include <rendering/rt/BlurRadiance.hpp>
#include <rendering/rt/RTRadianceRenderer.hpp>

#include <ui/UIText.hpp>

#include <asset/serialization/fbom/FBOM.hpp>
#include <asset/serialization/fbom/marshals/NodeMarshal.hpp>
#include <asset/serialization/fbom/marshals/SceneMarshal.hpp>

#include <scene/terrain/controllers/TerrainPagingController.hpp>
#include <scene/skydome/controllers/SkydomeController.hpp>

#include <rendering/vct/VoxelConeTracing.hpp>
#include <rendering/SparseVoxelOctree.hpp>

#include <util/fs/FsUtil.hpp>
#include <util/img/Bitmap.hpp>

#include <scene/NodeProxy.hpp>

#include <scene/camera/FirstPersonCamera.hpp>
#include <scene/camera/FollowCamera.hpp>
#include <scene/camera/CameraTrackController.hpp>

#include <util/MeshBuilder.hpp>

#include <asset/BufferedByteReader.hpp>
#include <asset/model_loaders/PLYModelLoader.hpp>

#include "util/Profile.hpp"

/* Standard library */
#include <cstdlib>
#include <ctime>
#include <iostream>
#include <unordered_map>
#include <string>
#include <cmath>
#include <thread>
#include <list>
#include <iomanip>

#include "rendering/RenderEnvironment.hpp"
#include "rendering/CubemapRenderer.hpp"
#include "rendering/PointShadowRenderer.hpp"
#include "rendering/UIRenderer.hpp"
#include <rendering/GaussianSplatting.hpp>

#include <rendering/ParticleSystem.hpp>

#include <script/ScriptBindings.hpp>

#include <util/UTF8.hpp>

#include <util/shader_compiler/ShaderCompiler.hpp>

#include "scene/controllers/AabbDebugController.hpp"

using namespace hyperion;
using namespace hyperion::v2;

// #define HYP_TEST_DISTANCE_FIELDS
    
#ifdef HYP_TEST_DISTANCE_FIELDS

class MyGame : public Game
{
public:
    MyGame(RC<Application> application)
        : Game(application)
    {   
    }

    virtual void InitGame() override
    {
        Game::InitGame();

        // Start loading a model in the background
        auto batch = g_asset_manager->CreateBatch();
        batch->Add<Node>("monkey", "models/monkey/monkey.obj");
        batch->Add<Node>("sponza", "models/sponza/sponza.obj");
        batch->LoadAsync();

        const Extent2D window_size = GetInputManager()->GetWindow()->GetExtent();

        // Create our main camera
        m_scene->SetCamera(CreateObject<Camera>(
            60.0f,
            window_size.width, window_size.height,
            0.01f, 500.0f
        ));

        m_scene->GetCamera()->SetCameraController(UniquePtr<FirstPersonCameraController>::Construct());

        // Create a sun
        const Vector3 sun_dir = Vector3(-0.5f, 1.0f, 0.65f).Normalize();

        auto sun_entity = CreateObject<Entity>();
        sun_entity->SetName(HYP_NAME(Sun));
        sun_entity->AddController<LightController>(CreateObject<Light>(DirectionalLight(
            sun_dir,
            Color(1.0f, 0.7f, 0.3f),
            1.0f
        )));
        sun_entity->SetTranslation(sun_dir);
        sun_entity->AddController<ShadowMapController>();
        GetScene()->AddEntity(sun_entity);

        // Create a skydome
        auto skydome_entity = CreateObject<Entity>();
        skydome_entity->SetName(HYP_NAME(Skydome));
        skydome_entity->AddController<SkydomeController>();
        GetScene()->AddEntity(skydome_entity);

        // Await our model to finish loading and add it to the scene
        auto batch_results = batch->AwaitResults();

        auto monkey_head = batch_results["monkey"].Get<Node>();
        GetScene()->GetRoot().AddChild(monkey_head);

        auto sponza = batch_results["sponza"].Get<Node>();
        sponza.Scale(0.015f);
        GetScene()->GetRoot().AddChild(sponza);

        // Load a script
        auto scripted_entity = CreateObject<Entity>();
        scripted_entity->SetName(HYP_NAME(MyScriptedObject));
        scripted_entity->AddController<ScriptedController>(g_asset_manager->Load<Script>("scripts/examples/load_assets.hypscript"));
        GetScene()->AddEntity(scripted_entity);

        // Enable UI rendering
        m_scene->GetEnvironment()->AddRenderComponent<UIRenderer>(HYP_NAME(UIRenderer0), GetUI().GetScene());

        // Add grid of environment probes to capture indirect lighting
        auto env_grid_entity = CreateObject<Entity>(HYP_NAME(EnvGridEntity));
        env_grid_entity->SetLocalAABB(BoundingBox(Vector3(-40.0f, -20.0f, -40.0f), Vector3(40.0f, 20.0f, 40.0f)));
        env_grid_entity->AddController<EnvGridController>();
        GetScene()->AddEntity(env_grid_entity);

        // Add a reflection probe
        m_scene->GetEnvironment()->AddRenderComponent<CubemapRenderer>(
            HYP_NAME(EnvProbe0),
            BoundingBox(BoundingBox(Vector3(-40.0f, -2.0f, -40.0f), Vector3(40.0f, 30.0f, 40.0f)))
        );
    }

    virtual void Logic(GameCounter::TickUnit delta) override
    {
        HandleCameraMovement(delta);
    }

    // Handle keyboard input to move the main camera
    void HandleCameraMovement(GameCounter::TickUnit delta)
    {
        Vector3 dir;

        if (m_input_manager->IsKeyDown(KEY_W)) {
            dir = GetScene()->GetCamera()->GetDirection();

            GetScene()->GetCamera()->GetCameraController()->PushCommand({
                CameraCommand::CAMERA_COMMAND_MOVEMENT,
                { .movement_data = { CameraCommand::CAMERA_MOVEMENT_FORWARD } }
            });
        }

        if (m_input_manager->IsKeyDown(KEY_S)) {
            dir = GetScene()->GetCamera()->GetDirection() * -1.0f;

            GetScene()->GetCamera()->GetCameraController()->PushCommand({
                CameraCommand::CAMERA_COMMAND_MOVEMENT,
                { .movement_data = { CameraCommand::CAMERA_MOVEMENT_BACKWARD } }
            });
        }

        if (m_input_manager->IsKeyDown(KEY_A)) {
            dir = GetScene()->GetCamera()->GetDirection().Cross(GetScene()->GetCamera()->GetUpVector()) * -1.0f;

            GetScene()->GetCamera()->GetCameraController()->PushCommand({
                CameraCommand::CAMERA_COMMAND_MOVEMENT,
                { .movement_data = { CameraCommand::CAMERA_MOVEMENT_LEFT } }
            });
        }

        if (m_input_manager->IsKeyDown(KEY_D)) {
            dir = GetScene()->GetCamera()->GetDirection().Cross(GetScene()->GetCamera()->GetUpVector());

            GetScene()->GetCamera()->GetCameraController()->PushCommand({
                CameraCommand::CAMERA_COMMAND_MOVEMENT,
                { .movement_data = { CameraCommand::CAMERA_MOVEMENT_RIGHT } }
            });
        }
    }
};

#else

class MyGame : public Game
{

public:
    Handle<Entity> m_sun;
    Array<Handle<Light>> m_point_lights;

    FilePath scene_export_filepath;

    MyGame(RC<Application> application)
        : Game(application),
          scene_export_filepath(g_asset_manager->GetBasePath() / "export.hypnode")
    {
    }

    virtual void InitGame() override
    {
        Game::InitGame();
    
        // g_engine->GetDeferredRenderer().GetPostProcessing().AddEffect<FXAAEffect>();

        const Extent2D window_size = GetInputManager()->GetWindow()->GetExtent();

        m_scene->SetCamera(CreateObject<Camera>(
            70.0f,
            window_size.width, window_size.height,
            0.01f, 30000.0f
        ));

        /*m_scene->GetCamera()->SetCameraController(UniquePtr<FollowCameraController>::Construct(
            Vector3(0.0f, 7.0f, 0.0f), Vector3(0.0f, 0.0f, 5.0f)
        ));*/
        m_scene->GetCamera()->SetCameraController(UniquePtr<FirstPersonCameraController>::Construct());

        RC<CameraTrack> camera_track = RC<CameraTrack>::Construct();
        camera_track->SetDuration(60.0);

        //m_scene->GetCamera()->SetCameraController(UniquePtr<CameraTrackController>::Construct(camera_track));

        {
            m_sun = CreateObject<Entity>();
            m_sun->SetName(HYP_NAME(Sun));
            m_sun->AddController<LightController>(CreateObject<Light>(DirectionalLight(
                Vector3(-0.105425f, 0.988823f, 0.105425f).Normalize(),
                Color(1.0f, 1.0f, 1.0f),
                5.0f
            )));
            m_sun->SetTranslation(Vector3(-0.105425f, 0.988823f, 0.105425f));
            m_sun->AddController<ShadowMapController>();
            GetScene()->AddEntity(m_sun);
        }
        
        if (false) { // adding lights to scene

            // m_scene->AddLight(m_sun);

            m_point_lights.PushBack(CreateObject<Light>(PointLight(
                Vector3(0.0f, 35.0f, 8.0f),
                Color(1.0f, 1.0f, 1.0f),
                40.0f,
                200.35f
            )));
            m_point_lights.PushBack(CreateObject<Light>(PointLight(
                Vector3(0.0f, 10.0f, 12.0f),
                Color(1.0f, 0.0f, 0.0f),
                15.0f,
                200.0f
            )));

            for (auto &light : m_point_lights) {
                auto point_light_entity = CreateObject<Entity>();
                point_light_entity->AddController<LightController>(light);
                GetScene()->AddEntity(std::move(point_light_entity));
            }
        }

        if (true) {
            auto btn_node = GetUI().GetScene()->GetRoot().AddChild();
            btn_node.SetEntity(CreateObject<Entity>());
            btn_node.GetEntity()->SetTranslation(Vector3(0.0f, 0.85f, 0.0f));
            btn_node.GetEntity()->AddController<UIButtonController>();

            if (UIButtonController *controller = btn_node.GetEntity()->GetController<UIButtonController>()) {
                controller->SetScript(g_asset_manager->Load<Script>("scripts/examples/ui_controller.hypscript"));
            }

            btn_node.Scale(0.01f);
        }

        { // allow ui rendering
            m_scene->GetEnvironment()->AddRenderComponent<UIRenderer>(HYP_NAME(UIRenderer0), GetUI().GetScene());
        }

        if (scene_export_filepath.Exists()) {
            // read file if it already exists.
            fbom::FBOMReader reader(fbom::FBOMConfig { });
            fbom::FBOMDeserializedObject deserialized;

            DebugLog(
                LogType::Info,
                "Attempting to load exported scene %s...\n",
                scene_export_filepath.Data()
            );

            if (auto err = reader.LoadFromFile(scene_export_filepath, deserialized)) {
                DebugLog(
                    LogType::Error,
                    "Failed to load scene export. Message was: %s\n",
                    err.message
                );

                HYP_BREAKPOINT;
            } else if (auto exported_node = deserialized.Get<Node>()) {
                Node *n = exported_node.Get();
                std::cout << " node name " << n->GetName() << std::endl;

                for (auto ch : n->GetChildren()) {
                    std::cout << "ch : " << ch.GetName() << std::endl;
                    std::cout << "entity : " << (ch.GetEntity() ? ch.GetEntity()->GetName().LookupString() : " <no entity>") << std::endl;

                    for (auto ch2 : ch.GetChildren()) {
                        std::cout << "\tch2 : " << ch2.GetName() << std::endl;

                        std::cout << "\tentity : " << (ch2.GetEntity() ? ch2.GetEntity()->GetName().LookupString() : " <no entity>") << std::endl;
                    }
                }

                m_scene->GetRoot().AddChild(exported_node);
                g_engine->GetWorld()->AddScene(m_scene);

                return;
            }
        }

        if (false) { // skydome
            if (auto skydome_node = m_scene->GetRoot().AddChild()) {
                skydome_node.SetEntity(CreateObject<Entity>());
                skydome_node.GetEntity()->AddController<SkydomeController>();
            }
        }
        
        if (g_engine->GetConfig().Get(CONFIG_VOXEL_GI)) { // voxel cone tracing for indirect light and reflections
            m_scene->GetEnvironment()->AddRenderComponent<VoxelConeTracing>(
                HYP_NAME(VCTRenderer0),
                VoxelConeTracing::Params { BoundingBox(-22.0f, 22.0f) }
            );
        } else if (g_engine->GetConfig().Get(CONFIG_VOXEL_GI_SVO)) {
            m_scene->GetEnvironment()->AddRenderComponent<SparseVoxelOctree>(HYP_NAME(VCT_SVO));
        }

        // m_scene->GetCamera()->SetCameraController(UniquePtr<FirstPersonCameraController>::Construct());
        
        m_scene->GetFogParams().start_distance = 5000.0f;
        m_scene->GetFogParams().end_distance = 40000.0f;
        
        auto batch = g_asset_manager->CreateBatch();
        batch->Add<Node>("zombie", "models/ogrexml/dragger_Body.mesh.xml");
        batch->Add<Node>("test_model", "models/pica_pica/pica_pica.obj");
        batch->Add<Node>("cube", "models/cube.obj");
        batch->Add<Node>("material", "models/material_sphere/material_sphere.obj");
        batch->Add<Node>("grass", "models/grass/grass.obj");

        //batch.Add<Node>("dude3", "models/dude3/Dude3_Body.mesh.xml");

        //batch.Add<Node>("monkey_fbx", "models/zombieSuit.fbx");
        batch->LoadAsync();

        auto obj_models = batch->AwaitResults();
        
#if 0
        auto zombie = obj_models["zombie"].Get<Node>();
        auto test_model = obj_models["test_model"].Get<Node>();//g_asset_manager->Load<Node>("../data/dump2/sponza.fbom");
        auto cube_obj = obj_models["cube"].Get<Node>();
        auto material_test_obj = obj_models["material"].Get<Node>();
        
        auto monkey_fbx = GetScene()->GetRoot().AddChild(obj_models["monkey_fbx"].Get<Node>());
        monkey_fbx.SetName("monkey_fbx");
        //monkey_fbx.Scale(0.2f);
        monkey_fbx.Rotate(Vector3(90, 0, 0));

        material_test_obj.Scale(2.0f);
        material_test_obj.Translate(Vector3(5.0f, 3.0f, 0.0f));
        GetScene()->GetRoot().AddChild(material_test_obj);

        if (auto dude = obj_models["dude3"].Get<Node>()) {
            dude.SetName("dude");
            for (auto &child : dude.GetChildren()) {
                if (auto entity = child.GetEntity()) {
                    if (auto *animation_controller = entity->GetController<AnimationController>()) {
                        animation_controller->Play(1.0f, LoopMode::REPEAT);
                    }
                }
            }
        
            GetScene()->GetRoot().AddChild(dude);
        }

        // test_model.Scale(1.825f);
        test_model.Scale(8.0f);
        // test_model.Scale(0.0225f);

        /*for (NodeProxy &item : test_model.GetChildren()) {
            DebugLog(LogType::Debug, "Item: %s\n", item.GetName().Data());

            if (item.GetName().Contains("Light")) {
                auto point_light_entity = CreateObject<Entity>();
                point_light_entity->AddController<LightController>(CreateObject<Light>(PointLight(
                    Vector3(0.0f, 0.0f, 0.0f),
                    Color(1.0f, 1.0f, 1.0f),
                    40.0f,
                    200.35f
                )));

                NodeProxy light_node = NodeProxy(new Node());
                light_node.SetEntity(point_light_entity);

                item.AddChild(std::move(light_node));
            }
        }*/

        if (g_engine->GetConfig().Get(CONFIG_ENV_GRID_GI)) {
            auto env_grid_entity = CreateObject<Entity>(HYP_NAME(EnvGridEntity));
            // Local aabb wil not be overwritten unless we add a Mesh to the Entity.
            // env_grid_entity->SetLocalAABB(BoundingBox(Vector3(-70.0f, -40.0f, -70.0f), Vector3(70.0f, 40.0f, 70.0f)));
            env_grid_entity->SetLocalAABB(test_model.GetWorldAABB() * 1.25f);
            env_grid_entity->AddController<EnvGridController>();
            GetScene()->AddEntity(std::move(env_grid_entity));
        }

        if (g_engine->GetConfig().Get(CONFIG_ENV_GRID_REFLECTIONS)) {
            m_scene->GetEnvironment()->AddRenderComponent<CubemapRenderer>(
                HYP_NAME(EnvProbe0),
                test_model.GetWorldAABB()
            );
        }

        /*m_scene->GetEnvironment()->AddRenderComponent<PointShadowRenderer>(
            HYP_NAME(PointShadowRenderer0),
            m_point_lights.Front(),
            Extent2D { 256, 256 }
        );*/

        if (false) {
            int i = 0;

            for (auto &child : test_model.GetChildren()) {
                if (!child) {
                    continue;
                }

                if (auto ent = child.GetEntity()) {
                    InitObject(ent);
                    // ent->CreateBLAS();

                    if (!ent->GetMesh()) {
                        continue;
                    }

                    Array<Vector3> vertices;
                    vertices.Reserve(ent->GetMesh()->GetVertices().size());

                    for (auto &vertex : ent->GetMesh()->GetVertices()) {
                        vertices.PushBack(vertex.GetPosition());
                    }

                    ent->AddController<RigidBodyController>(
                        RC<physics::ConvexHullPhysicsShape>::Construct(vertices),
                        physics::PhysicsMaterial { .mass = 0.0f }
                    );
                    ent->GetController<RigidBodyController>()->GetRigidBody()->SetIsKinematic(false);

                    i++;
                }
            }
        }

        if (false) { // hardware skinning
            auto zombie_entity = zombie[0].GetEntity();

            if (auto *animation_controller = zombie_entity->GetController<AnimationController>()) {
                animation_controller->Play(1.0f, LoopMode::REPEAT);
            }

            zombie_entity->GetMaterial()->SetParameter(Material::MaterialKey::MATERIAL_KEY_ALBEDO, Vector4(1.0f, 0.0f, 0.0f, 1.0f));
            zombie_entity->GetMaterial()->SetParameter(Material::MaterialKey::MATERIAL_KEY_ROUGHNESS, 0.05f);
            zombie_entity->GetMaterial()->SetParameter(Material::MaterialKey::MATERIAL_KEY_METALNESS, 1.0f);
            zombie_entity->RebuildRenderableAttributes();
            zombie_entity->SetTranslation(Vector3(0, 1, 0));
            zombie_entity->SetScale(Vector3(0.25f));

            InitObject(zombie_entity);
            zombie_entity->CreateBLAS();
            zombie.SetName("zombie");

            m_scene->GetRoot().AddChild(zombie);
        }
        
        for (auto &child : test_model.GetChildren()) {
            if (const Handle<Entity> &entity = child.GetEntity()) {
                Handle<Entity> ent = entity;

                if (InitObject(ent)) {
                    entity->CreateBLAS();
                }
            }
        }

        // add sponza model
        m_scene->GetRoot().AddChild(test_model);
        
        if (false) { // paged procedural terrain
            auto terrain_entity = CreateObject<Entity>();
            GetScene()->AddEntity(terrain_entity);
            terrain_entity->AddController<TerrainPagingController>(0xBEEF, Extent3D { 256 }, Vector3(0.5f), 1.0f);
        }

        if (false) { // physics
            for (int i = 0; i < 6; i++) {
                if (auto cube = g_asset_manager->Load<Node>("models/cube.obj")) {
                    cube.SetName("cube " + String::ToString(i));
                    auto cube_entity = cube[0].GetEntity();
                    cube_entity->SetFlags(Entity::InitInfo::ENTITY_FLAGS_RAY_TESTS_ENABLED, false);
                    cube_entity->GetMaterial()->SetParameter(Material::MATERIAL_KEY_ROUGHNESS, 0.3f);
                    cube_entity->GetMaterial()->SetParameter(Material::MATERIAL_KEY_METALNESS, 0.0f);
                    cube_entity->GetMaterial()->SetTexture(Material::MATERIAL_TEXTURE_METALNESS_MAP, Handle<Texture>());
                    cube_entity->GetMaterial()->SetTexture(Material::MATERIAL_TEXTURE_ROUGHNESS_MAP, Handle<Texture>());
                    cube_entity->GetMaterial()->SetTexture(Material::MATERIAL_TEXTURE_NORMAL_MAP, Handle<Texture>());
                    cube_entity->GetMaterial()->SetTexture(Material::MATERIAL_TEXTURE_ALBEDO_MAP, Handle<Texture>());
                    cube_entity->GetMaterial()->SetParameter(Material::MATERIAL_KEY_ALBEDO, Vector4(MathUtil::RandRange(0.0f, 1.0f), MathUtil::RandRange(0.0f, 1.0f), MathUtil::RandRange(0.0f, 1.0f), 1.0f));
                    cube_entity->RebuildRenderableAttributes();

                    cube_entity->SetScale(Vector3(3.0f));
                    cube_entity->SetTranslation(Vector3(0, i * 40 + 50, 0));

                    cube_entity->CreateBLAS();
                    InitObject(cube_entity);
                    m_scene->GetRoot().AddChild(cube);

                    cube_entity->AddController<RigidBodyController>(
                        RC<physics::BoxPhysicsShape>::Construct(cube_entity->GetWorldAABB()),
                        physics::PhysicsMaterial { .mass = 1.0f }
                    );
                }
            }
        }

        if (false) {
            if (auto monkey = g_asset_manager->Load<Node>("models/monkey/monkey.obj")) {
                monkey.SetName("monkey");
                auto monkey_entity = monkey[0].GetEntity();
                monkey_entity->SetShader(g_shader_manager->GetOrCreate(HYP_NAME(Forward),
                    ShaderProperties(renderer::static_mesh_vertex_attributes, {{ "FORWARD_LIGHTING" }})));
                monkey_entity->SetFlags(Entity::InitInfo::ENTITY_FLAGS_RAY_TESTS_ENABLED, false);
                monkey_entity->GetMaterial()->SetParameter(Material::MATERIAL_KEY_ROUGHNESS, 0.08f);
                monkey_entity->GetMaterial()->SetParameter(Material::MATERIAL_KEY_METALNESS, 0.0f);
                monkey_entity->GetMaterial()->SetParameter(Material::MATERIAL_KEY_TRANSMISSION, 0.95f);
                monkey_entity->GetMaterial()->SetBucket(Bucket::BUCKET_TRANSLUCENT);
                monkey_entity->GetMaterial()->SetIsAlphaBlended(true);
                monkey_entity->GetMaterial()->SetTexture(Material::MATERIAL_TEXTURE_METALNESS_MAP, Handle<Texture>());
                monkey_entity->GetMaterial()->SetTexture(Material::MATERIAL_TEXTURE_ROUGHNESS_MAP, Handle<Texture>());
                monkey_entity->GetMaterial()->SetTexture(Material::MATERIAL_TEXTURE_NORMAL_MAP, Handle<Texture>());
                monkey_entity->GetMaterial()->SetTexture(Material::MATERIAL_TEXTURE_ALBEDO_MAP, Handle<Texture>());
                monkey_entity->GetMaterial()->SetParameter(Material::MATERIAL_KEY_ALBEDO, Color(1.0f, 1.0f, 1.0f, 1.0f));
                monkey_entity->RebuildRenderableAttributes();
                monkey.SetLocalTranslation(Vector3(0.0f, 0.0f, 0.0f));
                monkey.Scale(1.2f);
                monkey.Rotate(Quaternion(Vector3::UnitY(), MathUtil::DegToRad(90.0f)));
                InitObject(monkey_entity);

                monkey_entity->AddController<ScriptedController>(
                    g_asset_manager->Load<Script>("scripts/examples/controller.hypscript")
                );

                monkey_entity->CreateBLAS();
                m_scene->GetRoot().AddChild(monkey);

                //monkey[0].GetEntity()->AddController<RigidBodyController>(
                //    UniquePtr<physics::BoxPhysicsShape>::Construct(monkey[0].GetWorldAABB()),
                //    physics::PhysicsMaterial { .mass = 1.0f }
            // );
            }
        }

        if (false) {
#if 0
            auto mh = g_asset_manager->Load<Node>("models/mh/mh1.obj");
            mh.SetName("mh_model");
            mh.Scale(0.35f);
            for (auto &mh_child : mh.GetChildren()) {
                // mh_child.SetEntity(Handle<Entity>::empty);

                if (auto entity = mh_child.GetEntity()) {
                    // entity->GetMaterial()->SetTexture(Material::MATERIAL_TEXTURE_ALBEDO_MAP, Handle<Texture>());
                    // entity->GetMaterial()->SetTexture(Material::MATERIAL_TEXTURE_NORMAL_MAP, Handle<Texture>());
                    // entity->GetMaterial()->SetParameter(Material::MATERIAL_KEY_ALBEDO, Vector4(1.0f, 0.0f, 0.0f, 1.0f));
                    // entity->GetMaterial()->SetParameter(Material::MATERIAL_KEY_ROUGHNESS, 0.05f);
                    // entity->GetMaterial()->SetParameter(Material::MATERIAL_KEY_METALNESS, 1.0f);
                }
            }
            GetScene()->GetRoot().AddChild(mh);
#endif

            NodeProxy tree = g_asset_manager->Load<Node>("models/conifer/Conifer_Low.obj");
            tree.SetName("tree");
            tree.Scale(0.175f);
            tree.SetLocalTranslation(Vector3(3, 1, 0));
            if (auto needles = tree.Select("Needles")) {
                if (needles.GetEntity() && needles.GetEntity()->GetMaterial()) {
                    needles.GetEntity()->SetShader(g_shader_manager->GetOrCreate(HYP_NAME(Forward),
                        ShaderProperties(renderer::static_mesh_vertex_attributes, {{ "FORWARD_LIGHTING" }})));
                    needles.GetEntity()->GetMaterial()->SetFaceCullMode(FaceCullMode::NONE);
                    needles.GetEntity()->GetMaterial()->SetParameter(Material::MATERIAL_KEY_ALPHA_THRESHOLD, 0.05f);
                    needles.GetEntity()->GetMaterial()->SetBucket(BUCKET_TRANSLUCENT);
                    needles.GetEntity()->GetMaterial()->SetBlendMode(BlendMode::NORMAL);
                    needles.GetEntity()->GetMaterial()->SetIsDepthWriteEnabled(false);
                    needles.GetEntity()->RebuildRenderableAttributes();
                }

                auto needles_copy = CreateObject<Entity>();
                needles_copy->SetMesh(needles.GetEntity()->GetMesh());
                auto needles_copy_material = CreateObject<Material>();
                needles_copy_material->SetTexture(Material::MATERIAL_TEXTURE_ALBEDO_MAP, needles.GetEntity()->GetMaterial()->GetTexture(Material::MATERIAL_TEXTURE_ALBEDO_MAP));
                needles_copy_material->SetTexture(Material::MATERIAL_TEXTURE_NORMAL_MAP, needles.GetEntity()->GetMaterial()->GetTexture(Material::MATERIAL_TEXTURE_NORMAL_MAP));
                needles_copy_material->SetParameter(Material::MATERIAL_KEY_ALPHA_THRESHOLD, 0.5f);
                needles_copy_material->SetFaceCullMode(FaceCullMode::NONE);
                needles_copy->SetMaterial(needles_copy_material);

                needles_copy->SetShader(g_shader_manager->GetOrCreate(HYP_NAME(Forward), ShaderProperties(renderer::static_mesh_vertex_attributes)));

                auto needles_copy_node = NodeProxy(new Node);
                needles_copy_node.SetEntity(needles_copy);
                needles.AddChild(needles_copy_node);
            }

            GetScene()->GetRoot().AddChild(tree);
        }
        
        if (false) {
            auto cube_model = g_asset_manager->Load<Node>("models/cube.obj");

            // add a plane physics shape
            auto plane = CreateObject<Entity>();
            plane->SetTranslation(Vector3(0, 0.1f, 0));
            plane->SetMesh(MeshBuilder::Quad());//Cube());
            plane->GetMesh()->SetVertexAttributes(renderer::static_mesh_vertex_attributes);
            plane->SetScale(Vector3(15.0f));
            plane->SetMaterial(CreateObject<Material>());
            plane->GetMaterial()->SetParameter(Material::MATERIAL_KEY_ALBEDO, Vector4(1.0f, 1.0f, 1.0f, 1.0f));
            plane->GetMaterial()->SetParameter(Material::MATERIAL_KEY_ROUGHNESS, 0.005f);
            plane->GetMaterial()->SetParameter(Material::MATERIAL_KEY_METALNESS, 1.0f);
            //plane->GetMaterial()->SetParameter(Material::MATERIAL_KEY_TRANSMISSION, 0.8f);
            plane->GetMaterial()->SetParameter(Material::MATERIAL_KEY_UV_SCALE, Vector2(8.0f));
            //plane->GetMaterial()->SetTexture(Material::TextureKey::MATERIAL_TEXTURE_NORMAL_MAP, g_asset_manager->Load<Texture>("textures/water.jpg"));
            // plane->GetMaterial()->SetTexture(Material::TextureKey::MATERIAL_TEXTURE_ALBEDO_MAP, g_asset_manager->Load<Texture>("textures/bamboo_wood/bamboo-wood-semigloss-albedo.png"));
            // plane->GetMaterial()->SetTexture(Material::TextureKey::MATERIAL_TEXTURE_NORMAL_MAP, g_asset_manager->Load<Texture>("textures/bamboo_wood/bamboo-wood-semigloss-normal.png"));
            // plane->GetMaterial()->SetTexture(Material::TextureKey::MATERIAL_TEXTURE_PARALLAX_MAP, g_asset_manager->Load<Texture>("textures/forest-floor-unity/forest_floor_Height.png"));
            // plane->GetMaterial()->SetTexture(Material::TextureKey::MATERIAL_TEXTURE_METALNESS_MAP, g_asset_manager->Load<Texture>("textures/bamboo_wood/bamboo-wood-semigloss-metal.png"));
            // plane->GetMaterial()->SetTexture(Material::TextureKey::MATERIAL_TEXTURE_AO_MAP, g_asset_manager->Load<Texture>("textures/bamboo_wood/bamboo-wood-semigloss-ao.png"));
            // plane->GetMaterial()->SetTexture(Material::TextureKey::MATERIAL_TEXTURE_ROUGHNESS_MAP, g_asset_manager->Load<Texture>("textures/bamboo_wood/bamboo-wood-semigloss-roughness.png"));
            plane->GetMaterial()->SetParameter(Material::MATERIAL_KEY_NORMAL_MAP_INTENSITY, 0.1f);
            //plane->GetMaterial()->SetBucket(Bucket::BUCKET_TRANSLUCENT);
            //plane->GetMaterial()->SetIsAlphaBlended(true);
            plane->SetShader(Handle<Shader>(g_shader_manager->GetOrCreate(HYP_NAME(Forward), ShaderProperties(plane->GetMesh()->GetVertexAttributes(), {}))));
            plane->RebuildRenderableAttributes();
            plane->CreateBLAS();
            if (NodeProxy plane_node_proxy = GetScene()->AddEntity(plane)) {
                plane_node_proxy.SetWorldRotation(Quaternion(Vector3::UnitX(), MathUtil::DegToRad(-90.0f)));
                // plane->AddController<RigidBodyController>(
                //     UniquePtr<physics::BoxPhysicsShape>::Construct(plane->GetWorldAABB()),
                //     physics::PhysicsMaterial { .mass = 0.0f }
                // );
                // plane->GetController<RigidBodyController>()->GetRigidBody()->SetIsKinematic(false);

                // plane->AddController<AABBDebugController>();
            }
        }

        if (false) { // particles test
            auto particle_spawner = CreateObject<ParticleSpawner>(ParticleSpawnerParams {
                .texture = g_asset_manager->Load<Texture>("textures/spark.png"),
                .max_particles = 1024u,
                .origin = Vector3(0.0f, 4.1f, 0.0f),
                .randomness = 1.0f,
                .lifespan = 4.0f,
                .has_physics = true
            });

            InitObject(particle_spawner);

            m_scene->GetEnvironment()->GetParticleSystem()->GetParticleSpawners().Add(std::move(particle_spawner));
        }
#endif
        
        // Test gaussian splatting
        if (true) {
            auto batch = g_asset_manager->CreateBatch();
            batch->Add<json::JSONValue>("cameras json", "models/gaussian_splatting/cameras.json");
            batch->Add<PLYModelLoader::PLYModel>("ply model", "models/gaussian_splatting/point_cloud.ply");

            batch->LoadAsync();

            auto loaded_assets = batch->AwaitResults();

            auto cameras_json = loaded_assets["cameras json"].Get<json::JSONValue>();
            AssertThrow(loaded_assets["cameras json"].result.status == LoaderResult::Status::OK);

            struct GaussianSplattingCameraDefinition
            {
                String id;
                String img_name;
                UInt32 width;
                UInt32 height;
                Vector3 position;
                Matrix3 rotation;
                Float fx;
                Float fy;
            };

            Array<GaussianSplattingCameraDefinition> camera_definitions;

            if (cameras_json && cameras_json->IsArray()) {
                camera_definitions.Reserve(cameras_json->AsArray().Size());

                for (const auto &item : cameras_json->AsArray()) {
                    GaussianSplattingCameraDefinition definition;
                    definition.id = (*item)["id"].ToString();
                    definition.img_name = (*item)["img_name"].ToString();
                    definition.width = MathUtil::Floor((*item)["width"].ToNumber());
                    definition.height = MathUtil::Floor((*item)["height"].ToNumber());
                    definition.fx = (*item)["fx"].ToNumber();
                    definition.fy = (*item)["fy"].ToNumber();

                    if ((*item)["position"].IsArray()) {
                        definition.position = Vector3(
                            (*item)["position"][0].ToNumber(),
                            (*item)["position"][1].ToNumber(),
                            (*item)["position"][2].ToNumber()
                        );
                    }

                    if ((*item)["rotation"].IsArray()) {
                        float v[9] = {
                            (*item)["rotation"][0][0].ToNumber(),
                            (*item)["rotation"][0][1].ToNumber(),
                            (*item)["rotation"][0][2].ToNumber(),
                            (*item)["rotation"][1][0].ToNumber(),
                            (*item)["rotation"][1][1].ToNumber(),
                            (*item)["rotation"][1][2].ToNumber(),
                            (*item)["rotation"][2][0].ToNumber(),
                            (*item)["rotation"][2][1].ToNumber(),
                            (*item)["rotation"][2][2].ToNumber()
                        };

                        definition.rotation = Matrix3(v);
                    }

                    camera_definitions.PushBack(definition);
                }
            }

            Quaternion camera_offset_rotation = Quaternion::Identity();
            Vector3 up_direction = Vector3::UnitY();

            Array<Vector3> all_up_directions;
            all_up_directions.Reserve(camera_definitions.Size());

            for (const auto &camera_definition : camera_definitions) {
                const Vector3 camera_up = Matrix4(camera_definition.rotation) * Vector3::UnitY();

                all_up_directions.PushBack(camera_up);
            }

            if (all_up_directions.Size() != 0) {
                up_direction = Vector3::Zero();

                for (const auto &camera_up_direction : all_up_directions) {
                    up_direction += camera_up_direction;
                }

                up_direction /= Float(all_up_directions.Size());
                up_direction.Normalize();

                const Vector3 axis = up_direction.Cross(Vector3::UnitY()).Normalize();

                const Float cos_theta = up_direction.Dot(Vector3::UnitY());
                const Float theta = MathUtil::Arccos(cos_theta);

                camera_offset_rotation = Quaternion(axis, theta).Invert();
            }

            DebugLog(LogType::Debug, "Up direction = %f, %f, %f\n", up_direction.x, up_direction.y, up_direction.z);

            //const auto camera_rotation = camera_definitions.Any() ? Quaternion(Matrix4(camera_definitions[0].rotation)).Invert() : Quaternion::identity;

            auto ply_model = loaded_assets["ply model"].Get<PLYModelLoader::PLYModel>();

            const SizeType num_points = ply_model->vertices.Size();

            RC<GaussianSplattingModelData> gaussian_splatting_model = RC<GaussianSplattingModelData>::Construct();
            gaussian_splatting_model->points.Resize(num_points);
            gaussian_splatting_model->transform.SetRotation(Quaternion(Vector3(1, 0, 0), M_PI));

            const bool has_rotations = ply_model->custom_data.Contains("rot_0")
                && ply_model->custom_data.Contains("rot_1")
                && ply_model->custom_data.Contains("rot_2")
                && ply_model->custom_data.Contains("rot_3");

            const bool has_scales = ply_model->custom_data.Contains("scale_0")
                && ply_model->custom_data.Contains("scale_1")
                && ply_model->custom_data.Contains("scale_2");

            const bool has_sh = ply_model->custom_data.Contains("f_dc_0")
                && ply_model->custom_data.Contains("f_dc_1")
                && ply_model->custom_data.Contains("f_dc_2");

            const bool has_opacity = ply_model->custom_data.Contains("opacity");

            for (SizeType index = 0; index < num_points; index++) {
                auto &out_point = gaussian_splatting_model->points[index];

                out_point.position = Vector4(ply_model->vertices[index].GetPosition(), 1.0f);

                if (has_rotations) {
                    Quaternion rotation;

                    ply_model->custom_data["rot_0"].Read(index * sizeof(Float), &rotation.w);
                    ply_model->custom_data["rot_1"].Read(index * sizeof(Float), &rotation.x);
                    ply_model->custom_data["rot_2"].Read(index * sizeof(Float), &rotation.y);
                    ply_model->custom_data["rot_3"].Read(index * sizeof(Float), &rotation.z);
                    
                    rotation.Normalize();

                    out_point.rotation = rotation;
                }

                if (has_scales) {
                    Vector3 scale = Vector3::one;

                    ply_model->custom_data["scale_0"].Read(index * sizeof(Float), &scale.x);
                    ply_model->custom_data["scale_1"].Read(index * sizeof(Float), &scale.y);
                    ply_model->custom_data["scale_2"].Read(index * sizeof(Float), &scale.z);

                    //scale = Vector3(MathUtil::Exp(scale.x), MathUtil::Exp(scale.y), MathUtil::Exp(scale.z));

                    out_point.scale = Vector4(scale, 1.0f);

                    // DebugLog(LogType::Debug, "Scale %u = %f, %f, %f\n", index, out_point.scale.x, out_point.scale.y, out_point.scale.z);
                }

                if (has_sh) {
                    Float f_dc_0 = 0.0f;
                    Float f_dc_1 = 0.0f;
                    Float f_dc_2 = 0.0f;
                    Float opacity = 1.0f;

                    static constexpr Float SH_C0 = 0.28209479177387814f;

                    ply_model->custom_data["f_dc_0"].Read(index * sizeof(Float), &f_dc_0);
                    ply_model->custom_data["f_dc_1"].Read(index * sizeof(Float), &f_dc_1);
                    ply_model->custom_data["f_dc_2"].Read(index * sizeof(Float), &f_dc_2);

                    if (has_opacity) {
                        ply_model->custom_data["opacity"].Read(index * sizeof(Float), &opacity);
                    }

                    out_point.color = Vector4(
                        0.5f + (SH_C0 * f_dc_0),
                        0.5f + (SH_C0 * f_dc_1),
                        0.5f + (SH_C0 * f_dc_2),
                        1.0f / (1.0f + MathUtil::Exp(-opacity))
                    );
                }
            }
            
            UInt camera_definition_index = 0;

            for (const auto &camera_definition : camera_definitions) {
                camera_track->AddPivot({
                    Double(camera_definition_index) / Double(camera_definitions.Size()),
                    gaussian_splatting_model->transform * Transform(camera_definition.position, Vector3(1.0f), Quaternion(Matrix4(camera_definition.rotation).Orthonormalized()))
                });

                camera_definition_index++;

                break;
            }

            auto gaussian_splatting_instance = CreateObject<GaussianSplattingInstance>(std::move(gaussian_splatting_model));
            InitObject(gaussian_splatting_instance);

            m_scene->GetEnvironment()->GetGaussianSplatting()->SetGaussianSplattingInstance(std::move(gaussian_splatting_instance));
        }
    }

    virtual void Teardown() override
    {
        Game::Teardown();
    }

    bool svo_ready_to_build = false;

    Handle<Entity> selected_entity;

    virtual void Logic(GameCounter::TickUnit delta) override
    {
        timer += delta;

        m_ui.Update(delta);

        HandleCameraMovement(delta);

        // if (const Handle<Entity> &env_grid_entity = GetScene()->FindEntityByName(HYP_NAME(EnvGridEntity))) {
        //     env_grid_entity->SetTranslation(GetScene()->GetCamera()->GetTranslation());
        // }

        if (auto fbx_node = GetScene()->GetRoot().Select("monkey_fbx")) {
            if (auto body = fbx_node.Select("Models:Body")) {
                if (const auto &entity = body.GetEntity()) {
                    if (const auto &skeleton = entity->GetSkeleton()) {
                        if (Bone *bone = skeleton->FindBone("thigh.L")) {
                            //bone->SetLocalRotation(Quaternion(Vector3(MathUtil::RadToDeg(MathUtil::Sin(delta * 2.0f)), 0.0f, 0.0f)));
                        }
                    }
                }
            }
        }

        //GetScene()->GetCamera()->SetTarget(GetScene()->GetRoot().Select("monkey")[0].GetWorldTranslation());

        /*for (auto &light : m_point_lights) {
            light->SetPosition(Vector3(
                MathUtil::Sin(light->GetID().Value() + timer) * 30.0f,
                30.0f,
                MathUtil::Cos(light->GetID().Value() + timer) * 30.0f
            ));
        }*/

        if (!m_point_lights.Empty()) {
            // m_point_lights.Front()->SetPosition(GetScene()->GetCamera()->GetTranslation() + GetScene()->GetCamera()->GetDirection() * 2.4f);
        }

        bool sun_position_changed = false;

        if (GetInputManager()->IsKeyDown(KEY_ARROW_LEFT)) {
            m_sun->SetTranslation((m_sun->GetTranslation() + Vector3(0.02f, 0.0f, 0.0f)).Normalize());
            sun_position_changed = true;
        } else if (GetInputManager()->IsKeyDown(KEY_ARROW_RIGHT)) {
            m_sun->SetTranslation((m_sun->GetTranslation() + Vector3(-0.02f, 0.0f, 0.0f)).Normalize());
            sun_position_changed = true;
        } else if (GetInputManager()->IsKeyDown(KEY_ARROW_UP)) {
            m_sun->SetTranslation((m_sun->GetTranslation() + Vector3(0.0f, 0.02f, 0.0f)).Normalize());
            sun_position_changed = true;
        } else if (GetInputManager()->IsKeyDown(KEY_ARROW_DOWN)) {
            m_sun->SetTranslation((m_sun->GetTranslation()+ Vector3(0.0f, -0.02f, 0.0f)).Normalize());
            sun_position_changed = true;
        }

        if (m_export_task == nullptr || m_export_task->IsCompleted()) {
            if (GetInputManager()->IsKeyStateChanged(KEY_C, &m_export_pressed) && m_export_pressed) {
                m_export_task.Reset(new TaskBatch);

                m_export_task->AddTask([export_path = scene_export_filepath, node = m_scene->GetRoot().Get()](...) {
                    DebugLog(LogType::Info, "Begin export task, exporting to path: %s\n", export_path.Data());

                    UniquePtr<fbom::FBOMWriter> writer(new fbom::FBOMWriter);
                    writer->Append(*node);

                    FileByteWriter byte_writer(export_path.Data());
                    auto err = writer->Emit(&byte_writer);
                    byte_writer.Close();

                    if (err.value != fbom::FBOMResult::FBOM_OK) {
                        DebugLog(LogType::Error, "Failed to export scene: %s\n", err.message);
                    } else {
                        DebugLog(LogType::Info, "Finished exporting!\n");
                    }
                });

                TaskSystem::GetInstance().EnqueueBatch(m_export_task.Get());
            }
        }

        // m_sun->SetPosition(Vector3(MathUtil::Sin(timer * 0.25f), MathUtil::Cos(timer * 0.25f), -MathUtil::Sin(timer * 0.25f)).Normalize());

        #if 1// bad performance on large meshes. need bvh
        if (GetInputManager()->IsButtonDown(MOUSE_BUTTON_LEFT) && ray_cast_timer > 1.0f) {
            ray_cast_timer = 0.0f;

            /*if (auto ray_hit = GetRayHitWorld()) {
                const ID<Entity> entity_id { ray_hit.Get().id };

                bool select_new_entity = false;

                if (selected_entity && selected_entity->GetID() != entity_id) {
                    selected_entity->RemoveController<AABBDebugController>();

                    select_new_entity = true;
                } else if (!selected_entity) {
                    select_new_entity = true;
                }

                if (select_new_entity) {
                    if (auto entity = Handle<Entity>(entity_id)) {
                        // entity->AddController<AABBDebugController>();

                        selected_entity = entity;
                    }
                }

                if (auto monkey_node = GetScene()->GetRoot().Select("monkey")) {
                    monkey_node.SetWorldTranslation(ray_hit.Get().hitpoint);
                    monkey_node.SetWorldRotation(Quaternion::LookAt(GetScene()->GetCamera()->GetTranslation() - ray_hit.Get().hitpoint, Vector3::UnitY()));
                }
            }*/

            const auto ray_hit_world = GetRayHitWorld();

            if (!ray_hit_world) {
                return;
            }

            auto point_light_entity = CreateObject<Entity>();
            point_light_entity->SetTranslation(ray_hit_world.Get().hitpoint - ray_hit_world.Get().normal * 0.1f);

            point_light_entity->AddController<LightController>(CreateObject<Light>(PointLight(
                Vector3(),
                Color(1.0f, 0.8f, 0.5f),
                50.0f,
                3.0f
            )));

            GetScene()->AddEntity(point_light_entity);

            DebugLog(LogType::Debug, "Add light %f, %f, %f\n", ray_hit_world.Get().hitpoint.x, ray_hit_world.Get().hitpoint.y, ray_hit_world.Get().hitpoint.z);
        } else {
            ray_cast_timer += delta;
        }
        #endif
    }

    Optional<RayHit> GetRayHitWorld(bool precise = false) const
    {
        const auto &mouse_position = GetInputManager()->GetMousePosition();

        const Int mouse_x = mouse_position.GetX();
        const Int mouse_y = mouse_position.GetY();

        const auto mouse_world = m_scene->GetCamera()->TransformScreenToWorld(
            Vector2(
                mouse_x / Float(GetInputManager()->GetWindow()->GetExtent().width),
                mouse_y / Float(GetInputManager()->GetWindow()->GetExtent().height)
            )
        );

        auto ray_direction = mouse_world.Normalized();

        // std::cout << "ray direction: " << ray_direction << "\n";

        Ray ray { m_scene->GetCamera()->GetTranslation(), ray_direction.GetXYZ() };
        RayTestResults results;

        if (g_engine->GetWorld()->GetOctree().TestRay(ray, results)) {

            if (precise) {
                RayTestResults triangle_mesh_results;

                for (const auto &hit : results) {
                    // now ray test each result as triangle mesh to find exact hit point
                    Handle<Entity> entity(ID<Entity> { hit.id });

                    if (entity) {
                        if (auto &mesh = entity->GetMesh()) {
                            ray.TestTriangleList(
                                mesh->GetVertices(),
                                mesh->GetIndices(),
                                entity->GetTransform(),
                                entity->GetID().value,
                                triangle_mesh_results
                            );
                        }
                    }
                }

                if (!triangle_mesh_results.Empty()) {
                    return triangle_mesh_results.Front();
                }
            } else {
                return results.Front();
            }
        }

        return { };
    }

    HashMap<ANSIString, Handle<Mesh>> cached_meshes;

    virtual void OnInputEvent(const SystemEvent &event) override
    {
        Game::OnInputEvent(event);
        DebugLog(LogType::Debug, "Input Event: %u\n", event.GetType());

        /*if (event.GetType() == SystemEventType::EVENT_MOUSEBUTTON_UP) {
            if (event.GetMouseButton() == MOUSE_BUTTON_LEFT) {
                const auto ray_hit_world = GetRayHitWorld();

                if (!ray_hit_world) {
                    return;
                }

                auto point_light_entity = CreateObject<Entity>();
                point_light_entity->AddController<LightController>(CreateObject<Light>(PointLight(
                    ray_hit_world.Get().hitpoint,
                    Color(1.0f, 1.0f, 1.0f),
                    1.0f,
                    5.0f
                )));

                NodeProxy light_node = NodeProxy(new Node());
                light_node.SetEntity(point_light_entity);

                GetScene()->GetRoot().AddChild(std::move(light_node));
            }
        }*/

        if (event.GetType() == SystemEventType::EVENT_KEYDOWN) {
            if (event.GetNormalizedKeyCode() == KEY_M) {
                Vector3 box_position = GetScene()->GetCamera()->GetTranslation() + GetScene()->GetCamera()->GetDirection() * 5.0f;

                if (auto hit_world = GetRayHitWorld()) {
                    box_position = hit_world.Get().hitpoint;
                }

                auto box_entity = CreateObject<Entity>();
                box_entity->SetFlags(Entity::InitInfo::ENTITY_FLAGS_INCLUDE_IN_INDIRECT_LIGHTING, false);

                auto box_mesh = cached_meshes["Cube"];

                if (!box_mesh) {
                    box_mesh = MeshBuilder::Cube();
                    cached_meshes["Cube"] = box_mesh;
                }

                Material::ParameterTable material_parameters = Material::DefaultParameters();
                material_parameters.Set(Material::MATERIAL_KEY_ROUGHNESS, 0.01f);
                material_parameters.Set(Material::MATERIAL_KEY_METALNESS, 0.01f);
                
                box_entity->SetMesh(box_mesh);
                box_entity->SetMaterial(g_material_system->GetOrCreate({ }, material_parameters));
                box_entity->SetShader(g_shader_manager->GetOrCreate(HYP_NAME(Forward), ShaderProperties(box_mesh->GetVertexAttributes())));
                box_entity->SetTranslation(box_position);

                GetScene()->AddEntity(box_entity);
            }
        }

        if (event.GetType() == SystemEventType::EVENT_FILE_DROP) {
            if (const FilePath *path = event.GetEventData().TryGet<FilePath>()) {
                Reader reader;

                if (path->Open(reader)) {

                    auto batch = g_asset_manager->CreateBatch();
                    batch->Add<Node>("dropped_object", *path);
                    batch->LoadAsync();

                    auto results = batch->AwaitResults();

                    if (results.Any()) {
                        for (auto &it : results) {
                            GetScene()->GetRoot().AddChild(it.second.Get<Node>());
                        }
                    }

                    reader.Close();
                }
            }
        }
    }

    // not overloading; just creating a method to handle camera movement
    void HandleCameraMovement(GameCounter::TickUnit delta)
    {
        bool moving = false;

        Vector3 dir;

        if (m_input_manager->IsKeyDown(KEY_W)) {
            dir = GetScene()->GetCamera()->GetDirection();

            moving = true;

            GetScene()->GetCamera()->GetCameraController()->PushCommand({
                CameraCommand::CAMERA_COMMAND_MOVEMENT,
                { .movement_data = { CameraCommand::CAMERA_MOVEMENT_FORWARD } }
            });
        }

        if (m_input_manager->IsKeyDown(KEY_S)) {
            dir = GetScene()->GetCamera()->GetDirection() * -1.0f;

            moving = true;

            GetScene()->GetCamera()->GetCameraController()->PushCommand({
                CameraCommand::CAMERA_COMMAND_MOVEMENT,
                { .movement_data = { CameraCommand::CAMERA_MOVEMENT_BACKWARD } }
            });
        }

        if (m_input_manager->IsKeyDown(KEY_A)) {
            dir = GetScene()->GetCamera()->GetDirection().Cross(GetScene()->GetCamera()->GetUpVector()) * -1.0f;

            moving = true;

            GetScene()->GetCamera()->GetCameraController()->PushCommand({
                CameraCommand::CAMERA_COMMAND_MOVEMENT,
                { .movement_data = { CameraCommand::CAMERA_MOVEMENT_LEFT } }
            });
        }

        if (m_input_manager->IsKeyDown(KEY_D)) {
            dir = GetScene()->GetCamera()->GetDirection().Cross(GetScene()->GetCamera()->GetUpVector());

            moving = true;

            GetScene()->GetCamera()->GetCameraController()->PushCommand({
                CameraCommand::CAMERA_COMMAND_MOVEMENT,
                { .movement_data = { CameraCommand::CAMERA_MOVEMENT_RIGHT } }
            });
        }

        if (auto character = GetScene()->GetRoot().Select("zombie")) {
            character.SetWorldRotation(Quaternion::LookAt(GetScene()->GetCamera()->GetDirection(), GetScene()->GetCamera()->GetUpVector()));

            if (const auto &entity = character[0].GetEntity()) {
                if (auto *controller = entity->GetController<RigidBodyController>()) {
                    controller->GetRigidBody()->ApplyForce(dir);
                }
            }
        }

#if 0
        if (auto character = GetScene()->GetRoot().Select("dude")) {
            character.SetWorldRotation(Quaternion::LookAt(GetScene()->GetCamera()->GetDirection() * Vector3(1.0f, 0.0f, 1.0f), GetScene()->GetCamera()->GetUpVector()));
            character.SetWorldTranslation(GetScene()->GetCamera()->GetTranslation() - (GetScene()->GetCamera()->GetUpVector() * 16.0f));

            if (auto *animation_controller = character[0].GetEntity()->GetController<AnimationController>()) {
                if (moving) {
                    animation_controller->Play("Sprint", 1.0f, LoopMode::REPEAT);
                } else {
                    animation_controller->Stop();
                }
            }
        }
#endif
    }

    GameCounter::TickUnit timer = -18.0f;
    GameCounter::TickUnit ray_cast_timer{};
    bool m_export_pressed = false;
    bool m_export_in_progress = false;
    UniquePtr<TaskBatch> m_export_task;

};

#endif

void HandleSignal(int signum)
{
    if (g_engine->m_stop_requested.Get(MemoryOrder::RELAXED)) {
        DebugLog(
            LogType::Warn,
            "Forcing stop\n"
        );

        fflush(stdout);

        exit(signum);

        return;
    }

    g_engine->RequestStop();

    while (g_engine->IsRenderLoopActive());

    exit(signum);
}

int main()
{
    signal(SIGINT, HandleSignal);
    
    RC<Application> application(new SDLApplication("My Application"));
    application->SetCurrentWindow(application->CreateSystemWindow({
        "Hyperion Engine",
        1024,
        1024,
        true
    }));
    
    hyperion::InitializeApplication(application);

    auto *my_game = new MyGame(application);
    g_engine->InitializeGame(my_game);

    UInt num_frames = 0;
    float delta_time_accum = 0.0f;
    GameCounter counter;

    SystemEvent event;

    SocketServer server;

    server.SetEventProc(
        HYP_NAME(OnError),
        [](Array<SocketProcArgument> &&args) {
            const String *error_message = args.Size() >= 1
                ? args[0].TryGet<String>()
                : nullptr;

            const Int32 *error_code = args.Size() >= 2
                ? args[1].TryGet<Int32>()
                : nullptr;

            if (error_message != nullptr) {
                DebugLog(
                    LogType::Error,
                    "Socket error! %s Code: %d\n",
                    error_message->Data(),
                    error_code ? *error_code : INT32_MIN
                );
            } else {
                DebugLog(
                    LogType::Error,
                    "Unknown socket error! Code: %d\n",
                    error_code ? *error_code : INT32_MIN
                );
            }
        }
    );

    server.SetEventProc(
        HYP_NAME(OnServerStarted),
        [](Array<SocketProcArgument> &&args) {
            DebugLog(LogType::Info, "Server started\n");
        }
    );

    // AssertThrow(server.Start());

     while (g_engine->IsRenderLoopActive()) {
        // input manager stuff
        while (application->PollEvent(event)) {
            my_game->HandleEvent(std::move(event));
        }

        counter.NextTick();
        delta_time_accum += counter.delta;
        num_frames++;

        if (num_frames >= 250) {
            DebugLog(
                LogType::Debug,
                "Render FPS: %f\n",
                1.0f / (delta_time_accum / Float(num_frames))
            );

            delta_time_accum = 0.0f;
            num_frames = 0;
        }
        
        g_engine->RenderNextFrame(my_game);
    }

    delete my_game;

    return 0;
}
