#pragma once
#include "state.hpp"

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

