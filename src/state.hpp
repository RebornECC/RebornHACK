// RebornHack - pause-menu hack panel: noclip, jump hack, autoclicker, speedhack, FPS, hitboxes,
// unlock icons, info overlay, macro recorder / replayer (3 slots per level).
// Safe mode is ALWAYS on: if any cheat or a macro was used in a run, finishing the level is not saved
// as a real completion (it is turned into a practice-style run).
#pragma once
#include <Geode/Geode.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <Geode/modify/PauseLayer.hpp>
#include <Geode/modify/GJBaseGameLayer.hpp>
#include <Geode/modify/PlayerObject.hpp>
#include <Geode/modify/GameManager.hpp>
#include <functional>
#include <vector>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <string>
#include <sstream>

using namespace geode::prelude;

namespace hk {
    enum class Mode { Idle, Record, Play };
    struct Ev { unsigned frame; bool down; int button; bool p1; };

    inline bool noclip = false;
    inline float speed = 1.f;
    inline bool cheated = false;
    inline Mode mode = Mode::Idle;
    inline std::vector<Ev> macro;
    inline size_t cursor = 0;
    inline bool injecting = false;
    inline std::string levelKey;
    inline bool jump = false, autoclick = false, hitbox = false, unlock = false, info = false;
    inline int fps = 60, slot = 1;
    inline int hits = 0;
    inline long long hitFrame = -1000;
    inline bool acHeld = false;
    inline bool loaded = false;

    GJBaseGameLayer* base() {
        return static_cast<GJBaseGameLayer*>(PlayLayer::get());
    }

    void applySpeed() {
        CCDirector::get()->getScheduler()->setTimeScale(speed);
    }

    void loadSettings() {
        noclip = Mod::get()->getSavedValue<bool>("noclip", false);
        speed = std::clamp((float)Mod::get()->getSavedValue<double>("speed", 1.0), 0.1f, 5.f);
        jump = Mod::get()->getSavedValue<bool>("jump", false);
        autoclick = Mod::get()->getSavedValue<bool>("autoclick", false);
        hitbox = Mod::get()->getSavedValue<bool>("hitbox", false);
        unlock = Mod::get()->getSavedValue<bool>("unlock", false);
        info = Mod::get()->getSavedValue<bool>("info", false);
        fps = std::clamp((int)Mod::get()->getSavedValue<int64_t>("fps", 60), 30, 1000);
        slot = std::clamp((int)Mod::get()->getSavedValue<int64_t>("slot", 1), 1, 3);
    }

    void saveSettings() {
        Mod::get()->setSavedValue<bool>("noclip", noclip);
        Mod::get()->setSavedValue<double>("speed", (double)speed);
        Mod::get()->setSavedValue<bool>("jump", jump);
        Mod::get()->setSavedValue<bool>("autoclick", autoclick);
        Mod::get()->setSavedValue<bool>("hitbox", hitbox);
        Mod::get()->setSavedValue<bool>("unlock", unlock);
        Mod::get()->setSavedValue<bool>("info", info);
        Mod::get()->setSavedValue<int64_t>("fps", (int64_t)fps);
        Mod::get()->setSavedValue<int64_t>("slot", (int64_t)slot);
    }

    void applyFps() {
        CCDirector::get()->setAnimationInterval(1.0 / (double)std::max(fps, 30));
    }

    void applyHitbox() {
        GameManager::get()->setGameVariable("0166", hitbox);
    }

    std::string macroKey() {
        return slot <= 1 ? "macro_" + levelKey : fmt::format("macro_{}_s{}", levelKey, slot);
    }

    std::string serialize() {
        std::string s;
        for (auto const& e : macro) s += fmt::format("{},{},{},{};", e.frame, e.down ? 1 : 0, e.button, e.p1 ? 1 : 0);
        return s;
    }

    void deserialize(std::string const& s) {
        macro.clear();
        std::stringstream ss(s);
        std::string item;
        while (std::getline(ss, item, ';')) {
            unsigned f = 0; int d = 0, b = 0, p = 1;
            if (std::sscanf(item.c_str(), "%u,%d,%d,%d", &f, &d, &b, &p) == 4) macro.push_back({f, d != 0, b, p != 0});
        }
    }

    void saveMacro() {
        if (!levelKey.empty()) Mod::get()->setSavedValue<std::string>(macroKey(), serialize());
    }

    void loadMacro() {
        if (!levelKey.empty()) deserialize(Mod::get()->getSavedValue<std::string>(macroKey(), ""));
    }

    char const* modeName() {
        switch (mode) {
            case Mode::Record: return "REC";
            case Mode::Play: return "PLAY";
            default: return "";
        }
    }

    void updateIndicator() {
        auto pl = PlayLayer::get();
        if (!pl) return;
        if (auto l = typeinfo_cast<CCLabelBMFont*>(pl->getChildByID("rebornhack-status"_spr))) {
            l->setString(modeName());
            l->setColor(mode == Mode::Record ? ccColor3B{255, 70, 70} : ccColor3B{80, 255, 120});
            l->setVisible(mode != Mode::Idle);
        }
    }
}

