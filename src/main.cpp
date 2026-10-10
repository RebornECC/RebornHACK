// RebornHack - gameplay hooks (see state.hpp, popup.hpp, drag.hpp)
#include "drag.hpp"

// ---------------------------------------------------------------- gameplay hooks
class $modify(HkPlayLayer, PlayLayer) {
    bool init(GJGameLevel* level, bool useReplay, bool dontCreateObjects) {
        if (!PlayLayer::init(level, useReplay, dontCreateObjects)) return false;
        hk::loadSettings();
        hk::cheated = false;
        hk::mode = hk::Mode::Idle;
        hk::cursor = 0;
        hk::hits = 0;
        hk::hitFrame = -1000;
        hk::acHeld = false;
        hk::levelKey = fmt::format("{}_{}", level->m_levelID.value(), std::string(level->m_levelName));
        hk::loadMacro();
        hk::applySpeed();
        hk::applyFps();
        hk::applyHitbox();

        auto win = CCDirector::get()->getWinSize();
        auto label = CCLabelBMFont::create("", "bigFont.fnt");
        label->setID("rebornhack-status"_spr);
        label->setScale(.45f);
        label->setAnchorPoint({0.f, 1.f});
        label->setPosition({12.f, win.height - 10.f});
        label->setVisible(false);
        this->addChild(label, 1000);

        auto info = CCLabelBMFont::create("", "chatFont.fnt");
        info->setID("rebornhack-info"_spr);
        info->setScale(.6f);
        info->setAnchorPoint({0.f, 0.f});
        info->setPosition({12.f, 8.f});
        info->setOpacity(190);
        info->setVisible(false);
        this->addChild(info, 1000);
        return true;
    }

    void postUpdate(float dt) {
        PlayLayer::postUpdate(dt);
        auto info = typeinfo_cast<CCLabelBMFont*>(this->getChildByID("rebornhack-info"_spr));
        if (!info) return;
        info->setVisible(hk::info);
        if (!hk::info) return;
        std::string t = fmt::format("Frame {}   Hits {}", m_gameState.m_currentProgress, hk::hits);
        if (hk::mode == hk::Mode::Play) t += fmt::format("   Macro {}/{}", hk::cursor, hk::macro.size());
        info->setString(t.c_str());
    }

    void resetLevel() {
        PlayLayer::resetLevel();
        hk::cursor = 0;
        hk::acHeld = false;
        hk::hitFrame = -1000;
        if (hk::mode == hk::Mode::Record) hk::macro.clear(); // each attempt is recorded from the start
        hk::updateIndicator();
    }

    void destroyPlayer(PlayerObject* player, GameObject* object) {
        if (hk::noclip && object != m_anticheatSpike) {
            hk::cheated = true;
            long long f = (long long)m_gameState.m_currentProgress;
            if (f - hk::hitFrame > 30) hk::hits++;
            hk::hitFrame = f;
            return;
        }
        PlayLayer::destroyPlayer(player, object);
    }

    void levelComplete() {
        if (hk::mode == hk::Mode::Record) {
            hk::mode = hk::Mode::Idle;
            hk::saveMacro();
            hk::updateIndicator();
        }
        if (hk::cheated || hk::noclip || hk::jump || hk::autoclick || hk::mode == hk::Mode::Play || std::abs(hk::speed - 1.f) > 0.001f) {
            m_isPracticeMode = true; // safe mode: never a real completion
            Notification::create("Safe mode: cheats used, completion not saved", NotificationIcon::Warning)->show();
        }
        PlayLayer::levelComplete();
    }

    void onQuit() {
        hk::mode = hk::Mode::Idle;
        CCDirector::get()->getScheduler()->setTimeScale(1.f);
        PlayLayer::onQuit();
    }
};

class $modify(HkPlayer, PlayerObject) {
    bool pushButton(PlayerButton btn) {
        if (hk::jump && PlayLayer::get() && btn == PlayerButton::Jump) {
            m_isOnGround = true; // jump hack: allow a jump at any moment
            hk::cheated = true;
        }
        return PlayerObject::pushButton(btn);
    }
};

class $modify(HkGameManager, GameManager) {
    bool isIconUnlocked(int id, IconType type) {
        if (!hk::loaded) { hk::loadSettings(); hk::loaded = true; }
        return hk::unlock || GameManager::isIconUnlocked(id, type);
    }
    bool isColorUnlocked(int key, UnlockType type) {
        if (!hk::loaded) { hk::loadSettings(); hk::loaded = true; }
        return hk::unlock || GameManager::isColorUnlocked(key, type);
    }
};

class $modify(HkBase, GJBaseGameLayer) {
    void handleButton(bool down, int button, bool isPlayer1) {
        if (hk::base() == this) {
            if (hk::mode == hk::Mode::Play && !hk::injecting) return; // ignore real input while a macro plays
            if (hk::mode == hk::Mode::Record && !hk::injecting) {
                hk::macro.push_back({m_gameState.m_currentProgress, down, button, isPlayer1});
            }
        }
        GJBaseGameLayer::handleButton(down, button, isPlayer1);
    }

    void processCommands(float dt, bool isHalfTick, bool isLastTick) {
        if (hk::base() == this && hk::mode == hk::Mode::Play) {
            unsigned frame = m_gameState.m_currentProgress;
            while (hk::cursor < hk::macro.size() && hk::macro[hk::cursor].frame <= frame) {
                auto const& e = hk::macro[hk::cursor++];
                hk::injecting = true;
                this->handleButton(e.down, e.button, e.p1);
                hk::injecting = false;
            }
        }
        if (hk::base() == this && hk::autoclick && hk::mode == hk::Mode::Idle) {
            bool want = (m_gameState.m_currentProgress % 24) < 12; // ~10 clicks per second
            if (want != hk::acHeld) {
                hk::acHeld = want;
                hk::cheated = true;
                hk::injecting = true;
                this->handleButton(want, 1, true);
                hk::injecting = false;
            }
        }
        GJBaseGameLayer::processCommands(dt, isHalfTick, isLastTick);
    }
};

