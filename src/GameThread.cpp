#include "GameThread.hpp"
#include "Engine.hpp"
#include "Game.hpp"
#include "GameCounter.hpp"
#include <math/MathUtil.hpp>

#define HYP_GAME_THREAD_LOCKED 1

namespace hyperion::v2 {

static constexpr float game_thread_target_ticks_per_second = 120.0f;

GameThread::GameThread()
    : Thread(Threads::thread_ids.At(THREAD_GAME)),
      m_is_running { false }
{
}

void GameThread::operator()(Engine *engine, Game *game, SystemWindow *window)
{
#if HYP_GAME_THREAD_LOCKED
    LockstepGameCounter counter(1.0f / game_thread_target_ticks_per_second);
#else
    GameCounter counter;
#endif

    m_is_running.store(true);

    game->InitGame(engine);

    while (engine->m_running.load()) {
        if (auto num_enqueued = m_scheduler->NumEnqueued()) {
            m_scheduler->Flush([last_delta = counter.delta](auto &fn) {
                fn(last_delta);
            });
        }

#if HYP_GAME_THREAD_LOCKED
        if (counter.Waiting()) {
            continue;
        }
#endif

        counter.NextTick();
        
        game->Logic(engine, counter.delta);
    }

    // flush scheduler
    m_scheduler->Flush([](auto &fn) {
        fn(MathUtil::epsilon<GameCounter::TickUnit>);
    });

    game->Teardown(engine);

    m_is_running.store(false);
}

} // namespace hyperion::v2