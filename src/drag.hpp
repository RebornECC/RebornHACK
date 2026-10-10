#pragma once
#include "popup.hpp"

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

