#include "ScriptedController.hpp"

#include <script/ScriptBindings.hpp>

#include <util/UTF8.hpp>

namespace hyperion::v2 {

ScriptedController::ScriptedController(Handle<Script> &&script)
    : Controller("ScriptedController"),
      m_script(std::move(script))
{
}

void ScriptedController::OnAdded()
{
    if (!m_script) {
        return;
    }

    if (m_script->Compile()) {
        m_script->Bake();
        m_script->Decompile(&utf::cout);
        m_script->Run();

        Script::ObjectHandle engine_object_handle;
        if (!m_script->GetObjectHandle("_engine", engine_object_handle)) {
            DebugLog(
                LogType::Error,
                "Failed to get `_engine` object\n"
            );
            return;
        }

        vm::Value engine_handle_value(vm::Value::ValueType::USER_DATA, { .user_data = static_cast<void *>(GetEngine()) });

        if (!m_script->SetMember(engine_object_handle, "handle", engine_handle_value)) {
            DebugLog(
                LogType::Error,
                "Failed to set `_engine.handle` member\n"
            );

            return;
        }

        if (!m_script->GetObjectHandle("movement_controller", m_controller_object)) {
            DebugLog(
                LogType::Error,
                "Failed to get `controller` object\n"
            );

            return;
        }

        if (!m_script->GetMember(m_controller_object, "OnAdded", m_onadded)) {
            DebugLog(
                LogType::Error,
                "Failed to get `OnAdded` method\n"
            );

            return;
        }

        if (!m_script->GetMember(m_controller_object, "OnTick", m_ontick)) {
            DebugLog(
                LogType::Error,
                "Failed to get `OnTick` method\n"
            );

            return;
        }

        if (!m_script->GetMember(m_controller_object, "OnRemoved", m_onremoved)) {
            DebugLog(
                LogType::Error,
                "Failed to get `OnRemoved` method\n"
            );

            return;
        }

        m_script->CallFunction(m_onadded, m_controller_object, GetOwner());
    } else {
        DebugLog(LogType::Error, "Script compilation failed!\n");

        m_script->GetErrors().WriteOutput(std::cout);

        HYP_BREAKPOINT;
    }
}

void ScriptedController::OnRemoved()
{
    m_script->CallFunction(m_onremoved, m_controller_object, GetOwner());
}

void ScriptedController::OnUpdate(GameCounter::TickUnit delta)
{
    m_script->CallFunction(m_ontick, m_controller_object, delta);
}

} // namespace hyperion::v2