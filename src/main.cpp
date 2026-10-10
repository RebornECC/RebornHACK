// RebornHack - pause-menu hack panel: noclip, speedhack, macro recorder / replayer.
// Safe mode is ALWAYS on: if any cheat or a macro was used in a run, finishing the level is not saved
// as a real completion (it is turned into a practice-style run).
#include <Geode/Geode.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <Geode/modify/PauseLayer.hpp>
#include <Geode/modify/GJBaseGameLayer.hpp>
#include <algorithm>
#include <cmath>
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

    GJBaseGameLayer* base() {
        return static_cast<GJBaseGameLayer*>(PlayLayer::get());
    }

    void applySpeed() {
        CCDirector::get()->getScheduler()->setTimeScale(speed);
    }

    void loadSettings() {
        noclip = Mod::get()->getSavedValue<bool>("noclip", false);
        speed = std::clamp((float)Mod::get()->getSavedValue<double>("speed", 1.0), 0.1f, 5.f);
    }

    void saveSettings() {
        Mod::get()->setSavedValue<bool>("noclip", noclip);
        Mod::get()->setSavedValue<double>("speed", (double)speed);
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
        if (!levelKey.empty()) Mod::get()->setSavedValue<std::string>("macro_" + levelKey, serialize());
    }

    void loadMacro() {
        if (!levelKey.empty()) deserialize(Mod::get()->getSavedValue<std::string>("macro_" + levelKey, ""));
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

// ---------------------------------------------------------------- hack panel
class HackPopup : public geode::Popup {
public:
    static inline int s_open = 0;
    HackPopup() { s_open++; }
    ~HackPopup() override { s_open--; }

protected:
    ButtonSprite* m_noclipSpr = nullptr;
    ButtonSprite* m_recSpr = nullptr;
    ButtonSprite* m_playSpr = nullptr;
    CCLabelBMFont* m_speedLabel = nullptr;
    CCLabelBMFont* m_macroLabel = nullptr;

    void refresh() {
        m_noclipSpr->setString(hk::noclip ? "Noclip  ON" : "Noclip  OFF");
        m_speedLabel->setString(fmt::format("{:.2f}x", hk::speed).c_str());
        m_recSpr->setString(hk::mode == hk::Mode::Record ? "Stop" : "Record");
        m_playSpr->setString(hk::mode == hk::Mode::Play ? "Stop" : "Play");
        m_macroLabel->setString(fmt::format("Macro: {} inputs", hk::macro.size()).c_str());
    }

    CCLabelBMFont* section(char const* text, float y, float width) {
        auto l = CCLabelBMFont::create(text, "goldFont.fnt");
        l->setScale(.5f);
        l->setPosition({width / 2, y});
        m_mainLayer->addChild(l);
        return l;
    }

    ButtonSprite* makeBtn(char const* text, char const* tex) {
        return ButtonSprite::create(text, "bigFont.fnt", tex, .7f);
    }

    bool init() {
        bool dark = Mod::get()->getSettingValue<bool>("dark-theme");
        if (!Popup::init(340.f, 270.f, dark ? "GJ_square05.png" : "GJ_square01.png")) return false;
        this->setTitle("RebornHack");
        hk::loadSettings();
        auto size = m_mainLayer->getContentSize();
        auto menu = CCMenu::create();
        menu->setPosition({0, 0});
        m_mainLayer->addChild(menu);

        // --- cheats
        section("CHEATS", size.height - 52.f, size.width);
        m_noclipSpr = makeBtn("Noclip  OFF", "GJ_button_01.png");
        auto noclipBtn = CCMenuItemExt::createSpriteExtra(m_noclipSpr, [this](auto) {
            hk::noclip = !hk::noclip;
            hk::saveSettings();
            this->refresh();
        });
        noclipBtn->setScale(.8f);
        noclipBtn->setPosition({size.width / 2, size.height - 80.f});
        menu->addChild(noclipBtn);

        // --- speed
        section("SPEED", size.height - 115.f, size.width);
        m_speedLabel = CCLabelBMFont::create("1.00x", "bigFont.fnt");
        m_speedLabel->setScale(.6f);
        m_speedLabel->setPosition({size.width / 2, size.height - 142.f});
        m_mainLayer->addChild(m_speedLabel);
        auto stepSpeed = [this](float d) {
            hk::speed = std::clamp(std::round((hk::speed + d) * 100.f) / 100.f, 0.1f, 5.f);
            hk::saveSettings();
            hk::applySpeed();
            this->refresh();
        };
        auto minus = CCMenuItemExt::createSpriteExtra(makeBtn("-", "GJ_button_06.png"), [stepSpeed](auto) { stepSpeed(-0.1f); });
        minus->setScale(.75f);
        minus->setPosition({size.width / 2 - 85.f, size.height - 142.f});
        menu->addChild(minus);
        auto plus = CCMenuItemExt::createSpriteExtra(makeBtn("+", "GJ_button_02.png"), [stepSpeed](auto) { stepSpeed(0.1f); });
        plus->setScale(.75f);
        plus->setPosition({size.width / 2 + 85.f, size.height - 142.f});
        menu->addChild(plus);
        auto one = CCMenuItemExt::createSpriteExtra(makeBtn("1x", "GJ_button_04.png"), [this](auto) {
            hk::speed = 1.f;
            hk::saveSettings();
            hk::applySpeed();
            this->refresh();
        });
        one->setScale(.6f);
        one->setPosition({size.width / 2 + 125.f, size.height - 142.f});
        menu->addChild(one);

        // --- macro
        section("MACRO", size.height - 180.f, size.width);
        m_macroLabel = CCLabelBMFont::create("Macro: 0 inputs", "chatFont.fnt");
        m_macroLabel->setScale(.6f);
        m_macroLabel->setPosition({size.width / 2, size.height - 200.f});
        m_mainLayer->addChild(m_macroLabel);

        m_recSpr = makeBtn("Record", "GJ_button_06.png");
        auto rec = CCMenuItemExt::createSpriteExtra(m_recSpr, [this](auto) {
            if (hk::mode == hk::Mode::Record) {
                hk::mode = hk::Mode::Idle;
                hk::saveMacro();
            } else {
                hk::macro.clear();
                hk::mode = hk::Mode::Record;
                hk::cursor = 0;
            }
            hk::updateIndicator();
            this->refresh();
        });
        rec->setScale(.7f);
        rec->setPosition({size.width / 2 - 100.f, 38.f});
        menu->addChild(rec);

        m_playSpr = makeBtn("Play", "GJ_button_01.png");
        auto play = CCMenuItemExt::createSpriteExtra(m_playSpr, [this](auto) {
            if (hk::mode == hk::Mode::Play) {
                hk::mode = hk::Mode::Idle;
            } else if (hk::macro.empty()) {
                Notification::create("No macro recorded for this level", NotificationIcon::Warning)->show();
            } else {
                hk::mode = hk::Mode::Play;
                hk::cursor = 0;
                hk::cheated = true;
                Notification::create("Restart the attempt to play the macro", NotificationIcon::Info)->show();
            }
            hk::updateIndicator();
            this->refresh();
        });
        play->setScale(.7f);
        play->setPosition({size.width / 2, 38.f});
        menu->addChild(play);

        auto clear = CCMenuItemExt::createSpriteExtra(makeBtn("Clear", "GJ_button_04.png"), [this](auto) {
            hk::macro.clear();
            hk::saveMacro();
            this->refresh();
        });
        clear->setScale(.7f);
        clear->setPosition({size.width / 2 + 100.f, 38.f});
        menu->addChild(clear);

        this->refresh();
        return true;
    }

public:
    static HackPopup* create() {
        auto ret = new HackPopup();
        if (ret->init()) {
            ret->autorelease();
            return ret;
        }
        delete ret;
        return nullptr;
    }
};

// ---------------------------------------------------------------- draggable icon (pause screen)
class HackDragButton : public CCLayer {
protected:
    CCNode* m_visual = nullptr;
    CCPoint m_start, m_origin;
    bool m_active = false, m_moved = false;

    bool init() {
        if (!CCLayer::init()) return false;
        auto spr = ButtonSprite::create("HACK", "bigFont.fnt", "GJ_button_06.png", .6f);
        spr->setPosition({0, 0});
        this->addChild(spr);
        m_visual = spr;
        this->setTouchEnabled(true);
        return true;
    }

    void registerWithTouchDispatcher() override {
        CCDirector::get()->getTouchDispatcher()->addTargetedDelegate(this, -600, true);
    }

    bool hit(CCTouch* t) {
        auto p = this->convertToNodeSpace(t->getLocation());
        auto r = m_visual->boundingBox();
        r.origin.x -= 8; r.origin.y -= 8; r.size.width += 16; r.size.height += 16;
        return r.containsPoint(p);
    }

public:
    bool ccTouchBegan(CCTouch* t, CCEvent*) override {
        if (HackPopup::s_open > 0 || !this->isVisible() || !hit(t)) return false;
        m_active = true;
        m_moved = false;
        m_start = t->getLocation();
        m_origin = this->getPosition();
        return true;
    }
    void ccTouchMoved(CCTouch* t, CCEvent*) override {
        if (!m_active) return;
        auto d = t->getLocation() - m_start;
        if (!m_moved && (std::abs(d.x) > 10.f || std::abs(d.y) > 10.f)) m_moved = true;
        if (m_moved) {
            auto win = CCDirector::get()->getWinSize();
            this->setPosition({
                std::clamp(m_origin.x + d.x, 30.f, win.width - 30.f),
                std::clamp(m_origin.y + d.y, 20.f, win.height - 20.f)
            });
        }
    }
    void ccTouchEnded(CCTouch*, CCEvent*) override {
        if (!m_active) return;
        m_active = false;
        if (m_moved) {
            auto p = this->getPosition();
            Mod::get()->setSavedValue<double>("btn-x", p.x);
            Mod::get()->setSavedValue<double>("btn-y", p.y);
        } else if (auto p = HackPopup::create()) {
            p->show();
        }
    }
    void ccTouchCancelled(CCTouch*, CCEvent*) override { m_active = false; }

    static HackDragButton* create() {
        auto ret = new HackDragButton();
        if (ret->init()) {
            ret->autorelease();
            return ret;
        }
        delete ret;
        return nullptr;
    }
};

class $modify(HkPauseLayer, PauseLayer) {
    void customSetup() {
        PauseLayer::customSetup();
        auto btn = HackDragButton::create();
        btn->setID("rebornhack-button"_spr);
        auto win = CCDirector::get()->getWinSize();
        float x = (float)Mod::get()->getSavedValue<double>("btn-x", win.width - 50.0);
        float y = (float)Mod::get()->getSavedValue<double>("btn-y", 30.0);
        btn->setPosition({std::clamp(x, 30.f, win.width - 30.f), std::clamp(y, 20.f, win.height - 20.f)});
        this->addChild(btn, 100);
    }
};

// ---------------------------------------------------------------- gameplay hooks
class $modify(HkPlayLayer, PlayLayer) {
    bool init(GJGameLevel* level, bool useReplay, bool dontCreateObjects) {
        if (!PlayLayer::init(level, useReplay, dontCreateObjects)) return false;
        hk::loadSettings();
        hk::cheated = false;
        hk::mode = hk::Mode::Idle;
        hk::cursor = 0;
        hk::levelKey = fmt::format("{}_{}", level->m_levelID.value(), std::string(level->m_levelName));
        hk::loadMacro();
        hk::applySpeed();

        auto label = CCLabelBMFont::create("", "bigFont.fnt");
        label->setID("rebornhack-status"_spr);
        label->setScale(.45f);
        label->setAnchorPoint({0.f, 1.f});
        auto win = CCDirector::get()->getWinSize();
        label->setPosition({12.f, win.height - 10.f});
        label->setVisible(false);
        this->addChild(label, 1000);
        return true;
    }

    void resetLevel() {
        PlayLayer::resetLevel();
        hk::cursor = 0;
        if (hk::mode == hk::Mode::Record) hk::macro.clear(); // each attempt is recorded from the start
        hk::updateIndicator();
    }

    void destroyPlayer(PlayerObject* player, GameObject* object) {
        if (hk::noclip && object != m_anticheatSpike) {
            hk::cheated = true;
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
        if (hk::cheated || hk::noclip || hk::mode == hk::Mode::Play || std::abs(hk::speed - 1.f) > 0.001f) {
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
        GJBaseGameLayer::processCommands(dt, isHalfTick, isLastTick);
    }
};
