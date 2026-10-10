// RebornHack - pause-menu hack panel: noclip, jump hack, autoclicker, speedhack, FPS, hitboxes,
// unlock icons, info overlay, macro recorder / replayer (3 slots per level).
// Safe mode is ALWAYS on: if any cheat or a macro was used in a run, finishing the level is not saved
// as a real completion (it is turned into a practice-style run).
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

// ---------------------------------------------------------------- hack panel
class HackPopup : public geode::Popup {
public:
    static inline int s_open = 0;
    HackPopup() { s_open++; }
    ~HackPopup() override { s_open--; }

protected:
    struct Tog { CCMenuItemToggler* t; bool* flag; };
    std::vector<Tog> m_togs;
    std::vector<CCNode*> m_pages;
    std::vector<ButtonSprite*> m_tabs;
    int m_page = 0;
    ButtonSprite* m_recSpr = nullptr;
    ButtonSprite* m_playSpr = nullptr;
    CCLabelBMFont* m_speedLabel = nullptr;
    CCLabelBMFont* m_fpsLabel = nullptr;
    CCLabelBMFont* m_statusLabel = nullptr;
    CCLabelBMFont* m_macroLabel = nullptr;
    CCLabelBMFont* m_slotLabel = nullptr;

    void refresh() {
        for (auto& t : m_togs) t.t->toggle(*t.flag);
        for (size_t i = 0; i < m_tabs.size(); i++) {
            m_tabs[i]->updateBGImage((int)i == m_page ? "GJ_button_01.png" : "GJ_button_04.png");
        }
        for (size_t i = 0; i < m_pages.size(); i++) m_pages[i]->setVisible((int)i == m_page);
        m_speedLabel->setString(fmt::format("{:.2f}x", hk::speed).c_str());
        m_fpsLabel->setString(fmt::format("{}", hk::fps).c_str());
        m_slotLabel->setString(fmt::format("{}", hk::slot).c_str());
        m_recSpr->setString(hk::mode == hk::Mode::Record ? "Stop" : "Record");
        m_playSpr->setString(hk::mode == hk::Mode::Play ? "Stop" : "Play");
        m_statusLabel->setString(hk::mode == hk::Mode::Record ? "REC" : hk::mode == hk::Mode::Play ? "PLAY" : "Idle");
        m_statusLabel->setColor(hk::mode == hk::Mode::Record ? ccColor3B{255, 100, 100}
                              : hk::mode == hk::Mode::Play ? ccColor3B{110, 230, 150} : ccColor3B{255, 255, 255});
        m_macroLabel->setString(fmt::format("Slot {} - {} inputs", hk::slot, hk::macro.size()).c_str());
    }

    CCLabelBMFont* label(CCNode* parent, char const* text, char const* font, float scale, CCPoint pos, CCPoint anchor = {.5f, .5f}) {
        auto l = CCLabelBMFont::create(text, font);
        l->setScale(scale);
        l->setAnchorPoint(anchor);
        l->setPosition(pos);
        parent->addChild(l);
        return l;
    }

    ButtonSprite* makeBtn(char const* text, char const* tex) {
        return ButtonSprite::create(text, "bigFont.fnt", tex, .7f);
    }

    CCMenuItemSpriteExtra* addBtn(CCMenu* menu, ButtonSprite* spr, float scale, float x, float y, std::function<void()> fn) {
        auto btn = CCMenuItemExt::createSpriteExtra(spr, [fn](auto) { fn(); });
        btn->setScale(scale);
        btn->setPosition({x, y});
        menu->addChild(btn);
        return btn;
    }

    // checkbox row: [x] Name / small description
    void addRow(CCNode* page, CCMenu* menu, char const* name, char const* desc, bool* flag, float y, std::function<void()> after = nullptr) {
        auto tog = CCMenuItemExt::createTogglerWithStandardSprites(.6f, [this, flag, after](CCMenuItemToggler* t) {
            *flag = !t->isToggled(); // the callback runs before the toggler flips itself
            hk::saveSettings();
            if (after) after();
        });
        tog->setPosition({38.f, y});
        menu->addChild(tog);
        m_togs.push_back({tog, flag});
        label(page, name, "bigFont.fnt", .4f, {62.f, y + 7.f}, {0.f, .5f});
        auto d = label(page, desc, "chatFont.fnt", .5f, {62.f, y - 8.f}, {0.f, .5f});
        d->setColor({150, 158, 178});
    }

    // "<  value  >" stepper
    void addStepper(CCNode* page, CCMenu* menu, char const* name, char const* desc, float y, CCLabelBMFont*& valueLabel,
                    std::function<void()> prev, std::function<void()> next) {
        float w = m_mainLayer->getContentSize().width;
        label(page, name, "bigFont.fnt", .4f, {38.f, y + 7.f}, {0.f, .5f});
        auto d = label(page, desc, "chatFont.fnt", .5f, {38.f, y - 8.f}, {0.f, .5f});
        d->setColor({150, 158, 178});
        addBtn(menu, makeBtn("<", "GJ_button_04.png"), .55f, w - 110.f, y, prev);
        valueLabel = label(page, "", "bigFont.fnt", .5f, {w - 78.f, y});
        addBtn(menu, makeBtn(">", "GJ_button_04.png"), .55f, w - 46.f, y, next);
    }

    CCNode* newPage(CCMenu*& menu) {
        auto page = CCNode::create();
        menu = CCMenu::create();
        menu->setPosition({0, 0});
        page->addChild(menu, 10);
        m_mainLayer->addChild(page);
        m_pages.push_back(page);
        return page;
    }

    void stopMacroMode() {
        if (hk::mode == hk::Mode::Record) hk::saveMacro();
        hk::mode = hk::Mode::Idle;
        hk::updateIndicator();
    }

    void setSpeed(float v) {
        hk::speed = std::clamp(std::round(v * 100.f) / 100.f, 0.1f, 5.f);
        hk::saveSettings();
        hk::applySpeed();
        this->refresh();
    }

    bool init() {
        bool dark = Mod::get()->getSettingValue<bool>("dark-theme");
        if (!Popup::init(360.f, 280.f, dark ? "GJ_square05.png" : "GJ_square01.png")) return false;
        this->setTitle("RebornHack");
        hk::loadSettings();
        auto size = m_mainLayer->getContentSize();
        float w = size.width, h = size.height, cx = w / 2;

        // ---- tabs
        auto tabMenu = CCMenu::create();
        tabMenu->setPosition({0, 0});
        m_mainLayer->addChild(tabMenu, 20);
        char const* names[] = {"Cheats", "Speed", "Macro", "Visual"};
        for (int i = 0; i < 4; i++) {
            auto spr = makeBtn(names[i], "GJ_button_04.png");
            m_tabs.push_back(spr);
            addBtn(tabMenu, spr, .5f, cx - 123.f + i * 82.f, h - 48.f, [this, i] {
                m_page = i;
                this->refresh();
            });
        }

        // ---- page 1: cheats
        CCMenu* m1;
        auto p1 = newPage(m1);
        addRow(p1, m1, "Noclip", "No deaths from hazards", &hk::noclip, h - 92.f);
        addRow(p1, m1, "Jump hack", "Jump at any moment", &hk::jump, h - 126.f);
        addRow(p1, m1, "Autoclicker", "About 10 clicks per second", &hk::autoclick, h - 160.f, [] { hk::acHeld = false; });
        addRow(p1, m1, "Unlock icons", "All icons and colors, local only", &hk::unlock, h - 194.f);
        addBtn(m1, makeBtn("Reset all", "GJ_button_06.png"), .5f, w - 70.f, 52.f, [this] {
            hk::noclip = hk::jump = hk::autoclick = hk::hitbox = hk::info = false;
            hk::speed = 1.f;
            hk::fps = 60;
            hk::acHeld = false;
            hk::saveSettings();
            hk::applySpeed();
            hk::applyFps();
            hk::applyHitbox();
            this->refresh();
        });

        // ---- page 2: speed
        CCMenu* m2;
        auto p2 = newPage(m2);
        m_speedLabel = label(p2, "1.00x", "bigFont.fnt", .9f, {cx, h - 100.f});
        label(p2, "Game speed", "chatFont.fnt", .55f, {cx, h - 124.f})->setColor({150, 158, 178});
        addBtn(m2, makeBtn("-", "GJ_button_06.png"), .55f, cx - 120.f, h - 154.f, [this] { setSpeed(hk::speed - 0.1f); });
        addBtn(m2, makeBtn("+", "GJ_button_02.png"), .55f, cx - 60.f, h - 154.f, [this] { setSpeed(hk::speed + 0.1f); });
        addBtn(m2, makeBtn("0.5x", "GJ_button_04.png"), .5f, cx, h - 154.f, [this] { setSpeed(0.5f); });
        addBtn(m2, makeBtn("1x", "GJ_button_04.png"), .5f, cx + 60.f, h - 154.f, [this] { setSpeed(1.f); });
        addBtn(m2, makeBtn("2x", "GJ_button_04.png"), .5f, cx + 120.f, h - 154.f, [this] { setSpeed(2.f); });
        addStepper(p2, m2, "FPS", "Frame rate cap", h - 202.f, m_fpsLabel,
            [this] { stepFps(-1); }, [this] { stepFps(1); });

        // ---- page 3: macro
        CCMenu* m3;
        auto p3 = newPage(m3);
        m_statusLabel = label(p3, "Idle", "bigFont.fnt", .6f, {cx, h - 98.f});
        m_macroLabel = label(p3, "", "chatFont.fnt", .55f, {cx, h - 120.f});
        m_macroLabel->setColor({150, 158, 178});
        m_recSpr = makeBtn("Record", "GJ_button_06.png");
        addBtn(m3, m_recSpr, .6f, cx - 100.f, h - 154.f, [this] {
            if (hk::mode == hk::Mode::Record) {
                stopMacroMode();
            } else {
                hk::macro.clear();
                hk::mode = hk::Mode::Record;
                hk::cursor = 0;
                hk::updateIndicator();
            }
            this->refresh();
        });
        m_playSpr = makeBtn("Play", "GJ_button_01.png");
        addBtn(m3, m_playSpr, .6f, cx, h - 154.f, [this] {
            if (hk::mode == hk::Mode::Play) {
                stopMacroMode();
            } else if (hk::macro.empty()) {
                Notification::create("No macro in this slot", NotificationIcon::Warning)->show();
            } else {
                hk::mode = hk::Mode::Play;
                hk::cursor = 0;
                hk::cheated = true;
                hk::updateIndicator();
                Notification::create("Restart the attempt to play the macro", NotificationIcon::Info)->show();
            }
            this->refresh();
        });
        addBtn(m3, makeBtn("Clear", "GJ_button_04.png"), .6f, cx + 100.f, h - 154.f, [this] {
            hk::macro.clear();
            hk::saveMacro();
            this->refresh();
        });
        addStepper(p3, m3, "Slot", "3 macros per level", h - 202.f, m_slotLabel,
            [this] { stepSlot(2); }, [this] { stepSlot(1); });

        // ---- page 4: visual
        CCMenu* m4;
        auto p4 = newPage(m4);
        addRow(p4, m4, "Hitboxes", "Show hitboxes after restart", &hk::hitbox, h - 92.f, [] { hk::applyHitbox(); });
        addRow(p4, m4, "Info overlay", "Frame, hits, macro progress", &hk::info, h - 126.f);

        // ---- footer
        label(m_mainLayer, "Safe mode on: cheated runs are never saved as real", "chatFont.fnt", .5f, {cx, 20.f})
            ->setColor({127, 217, 160});

        this->refresh();
        return true;
    }

    void stepFps(int d) {
        static int const opts[] = {60, 120, 240, 360};
        int idx = 0;
        for (int i = 0; i < 4; i++) if (hk::fps == opts[i]) idx = i;
        hk::fps = opts[(idx + d + 4) % 4];
        hk::saveSettings();
        hk::applyFps();
        this->refresh();
    }

    void stepSlot(int d) {
        stopMacroMode();
        hk::slot = (hk::slot - 1 + d) % 3 + 1;
        hk::saveSettings();
        hk::loadMacro();
        this->refresh();
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
