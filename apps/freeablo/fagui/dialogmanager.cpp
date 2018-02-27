#include "dialogmanager.h"
#include "../faworld/actor.h"
#include "../faworld/equiptarget.h"
#include "../faworld/player.h"
#include "guimanager.h"
#include <misc/assert.h>
#include <utility>

namespace FAGui
{
    DialogLineData::DialogLineData(std::string text, TextColor color, bool alignCenter) : text(std::move(text)), alignCenter(alignCenter), color(color) {}

    DialogLineData::DialogLineData() {}

    DialogLineData& DialogLineData::setAction(std::function<void()> actionArg)
    {
        action = actionArg;
        return *this;
    }

    DialogLineData& DialogLineData::setNumber(int32_t number)
    {
        mNumber = number;
        return *this;
    }

    DialogLineData& DialogLineData::setYOffset(int32_t offset)
    {
        mYOffset = offset;
        return *this;
    }

    DialogLineData& DialogLineData::setXOffset(int offset)
    {
        mXOffset = offset;
        return *this;
    }

    DialogLineData& DialogData::textLines(const std::vector<std::string>& texts, TextColor color, bool alignCenter, std::vector<int> offsets)
    {
        auto index = mLines.size();
        for (int i = 0; i < static_cast<int>(texts.size()); ++i)
        {
            mLines.emplace_back(texts[i], color, alignCenter);
            if (i < static_cast<int>(offsets.size()))
                mLines.back().setXOffset(offsets[i]);
        }
        skip_line();
        return mLines[index];
    }

    void DialogData::skip_line(int32_t cnt) { mLines.insert(mLines.end(), cnt, {}); }

    void DialogData::separator()
    {
        mLines.emplace_back(DialogLineData::separator());
        skip_line();
    }

    DialogLineData& DialogData::footer(const std::string& text)
    {
        mFooter.emplace_back(DialogLineData::separator());
        mFooter.emplace_back(text, TextColor::white, true);
        mFooter.back().setYOffset(6);
        mFooter.emplace_back();
        return *std::prev(mFooter.end(), 2);
    }

    void DialogData::header(const std::vector<std::string>& text)
    {
        if (text.size() == 1)
        {
            mHeader.emplace_back();
            mHeader.emplace_back(text.front(), TextColor::golden, true);
            mHeader.emplace_back();
        }
        else
        {
            for (auto& line : text)
            {
                mHeader.emplace_back(line, TextColor::golden, true);
                mHeader.emplace_back();
            }
        }
        mHeader.emplace_back(DialogLineData::separator());
        mHeader.emplace_back();
    }

    int32_t DialogData::selectedLine()
    {
        if (mSelectedLine == -1)
        {
            auto it = std::find_if(mLines.begin(), mLines.end(), [](const DialogLineData& data) { return !!data.action; });
            if (it != mLines.end())
                return mSelectedLine = it - mLines.begin();
            return -1;
        }
        return mSelectedLine;
    }

    void DialogData::notify(Engine::KeyboardInputAction action, GuiManager& manager)
    {
        int32_t dir = 0;
        int32_t i = selectedLine();
        switch (action)
        {
            case Engine::KeyboardInputAction::nextOption:
                dir = 1;
                break;
            case Engine::KeyboardInputAction::prevOption:
                dir = -1;
                break;
            case Engine::KeyboardInputAction::reject:
                manager.popDialogData();
                return;
            case Engine::KeyboardInputAction::accept:
                for (auto& line : mFooter)
                    if (line.forceSelected)
                        return line.action();
                if (i < 0)
                    return;
                mLines[mSelectedLine].action();
                return;
            default:
                break;
        }

        if (dir != 0)
        {
            if (i == -1)
                return;

            do
            {
                i += dir;
                if (i < 0 || i >= static_cast<int32_t>(mLines.size()))
                {
                    if (isScrollbarShown())
                        return; // no looping if there's scroll bar
                    i = (i + mLines.size()) % mLines.size();
                }
            } while (!mLines[i].action);
            mSelectedLine = i;
            if (!isVisible(mSelectedLine))
            {
                if (dir == -1)
                {
                    mFirstVisible = mSelectedLine;
                }
                else
                {
                    int32_t firstInvisible = mSelectedLine;
                    ++firstInvisible;
                    if (firstInvisible >= static_cast<int32_t>(mLines.size()))
                        return;
                    while (firstInvisible < static_cast<int32_t>(mLines.size()) && !mLines[firstInvisible].action)
                    {
                        ++firstInvisible;
                    }
                    mFirstVisible = firstInvisible - visibleBodyLineCount();
                }
            }
        }
    }

    double DialogData::selectedLinePercent()
    {
        int32_t cnt = 0;
        int32_t selectedIndex = 0;
        for (int32_t i = 0; i < static_cast<int32_t>(mLines.size()); ++i)
        {
            if (i == selectedLine())
                selectedIndex = cnt;
            if (mLines[i].action)
                ++cnt;
        }
        return (selectedIndex + 0.0) / (cnt - 1);
    }

    void DialogData::clearLines() { mLines.clear(); }

    DialogManager::DialogManager(GuiManager& gui_manager, FAWorld::World& world) : mGuiManager(gui_manager), mWorld(world) {}

    void DialogManager::talkOgden(const FAWorld::Actor* npc)
    {
        DialogData d;
        auto& td = npc->getTalkData();
        d.header({td.at("introductionHeader1"), td.at("introductionHeader2")});
        d.skip_line(2);
        d.textLines({td.at("introduction")}, TextColor::golden);
        d.skip_line();
        d.textLines({td.at("talk")}, TextColor::blue).setAction([]() {});
        d.skip_line(4);
        d.textLines({td.at("quit")}).setAction([&]() { quitDialog(); });
        mGuiManager.pushDialogData(std::move(d));
    }

    void DialogManager::talkFarnham(const FAWorld::Actor* npc)
    {
        DialogData d;
        auto& td = npc->getTalkData();
        d.header({td.at("introductionHeader")});
        d.skip_line(2);
        d.textLines({td.at("introduction")}, TextColor::golden);
        d.skip_line();
        d.textLines({td.at("talk")}, TextColor::blue).setAction([]() {});
        d.skip_line(4);
        d.textLines({td.at("quit")}).setAction([&]() { quitDialog(); });
        mGuiManager.pushDialogData(std::move(d));
    }

    static bool adriaSellFilter(const FAWorld::Item& item)
    {
        // TODO: add check for quest items
        return item.getType() == FAWorld::ItemType::misc || item.getType() == FAWorld::ItemType::staff;
    }

    void DialogManager::talkAdria(const FAWorld::Actor* npc)
    {
        DialogData d;
        auto& td = npc->getTalkData();
        d.header({{td.at("introductionHeader")}});

        d.skip_line(2);
        d.textLines({td.at("introduction")}, TextColor::golden);
        d.skip_line();
        d.textLines({td.at("talk")}, TextColor::blue).setAction([]() {});
        d.textLines({td.at("buy")}).setAction([]() {});
        d.textLines({td.at("sell")}).setAction([this]() { sellDialog(adriaSellFilter); });
        d.textLines({td.at("recharge")}).setAction([]() {});
        d.textLines({td.at("quit")}).setAction([&]() { quitDialog(); });
        mGuiManager.pushDialogData(std::move(d));
    }

    void DialogManager::talkWirt(const FAWorld::Actor* npc)
    {
        DialogData d;
        auto& td = npc->getTalkData();
        d.header({td.at("introductionHeader")});

        d.skip_line();
        d.textLines({td.at("talk")}, TextColor::blue).setAction([]() {});
        d.skip_line(2);
        d.textLines({td.at("introduction1")}, TextColor::golden);
        d.textLines({td.at("introduction2")}, TextColor::golden);
        d.textLines({td.at("introduction3")}, TextColor::golden);
        d.textLines({td.at("look")}).setAction([&]() {});
        d.textLines({td.at("quit")}).setAction([&]() { quitDialog(); });
        mGuiManager.pushDialogData(std::move(d));
    }

    void DialogManager::talkPepin(const FAWorld::Actor* npc)
    {
        DialogData d;
        auto& td = npc->getTalkData();
        d.header({td.at("introductionHeader1"), td.at("introductionHeader2")});

        d.skip_line(2);
        d.textLines({td.at("introduction")}, TextColor::golden);
        d.skip_line();
        d.textLines({td.at("talk")}, TextColor::blue).setAction([]() {});
        d.textLines({td.at("heal")}).setAction([&]() { mWorld.getCurrentPlayer()->heal(); });
        d.textLines({td.at("buy")}).setAction([]() {});
        d.textLines({td.at("quit")}).setAction([&]() { quitDialog(); });
        mGuiManager.pushDialogData(std::move(d));
    }

    void DialogManager::talkCain(const FAWorld::Actor* npc)
    {
        DialogData d;
        auto& td = npc->getTalkData();
        d.header({td.at("introductionHeader")});
        d.skip_line(2);
        d.textLines({td.at("introduction")}, TextColor::golden);
        d.skip_line();
        d.textLines({td.at("talk")}, TextColor::blue).setAction([]() {});
        d.textLines({td.at("identify")}).setAction([]() {});
        d.skip_line(2);
        d.textLines({td.at("quit")}).setAction([&]() { quitDialog(); });
        mGuiManager.pushDialogData(std::move(d));
    }

    void DialogManager::talkGillian(const FAWorld::Actor* npc)
    {
        DialogData d;
        auto& td = npc->getTalkData();
        d.header({td.at("introductionHeader")});
        d.skip_line(2);
        d.textLines({td.at("introduction")}, TextColor::golden);
        d.skip_line();
        d.textLines({td.at("talk")}, TextColor::blue).setAction([]() {});
        d.skip_line(4);
        d.textLines({td.at("quit")}).setAction([&]() { quitDialog(); });
        mGuiManager.pushDialogData(std::move(d));
    }

    void DialogManager::talk(const FAWorld::Actor* npc)
    {
        auto npcId = npc->getNpcId();
        if (npcId == "NPCsmith")
            talkGriswold(npc);
        else if (npcId == "NPCtavern")
            talkOgden(npc);
        else if (npcId == "NPCdrunk")
            talkFarnham(npc);
        else if (npcId == "NPCmaid")
            talkGillian(npc);
        else if (npcId == "NPCboy")
            talkWirt(npc);
        else if (npcId == "NPChealer")
            talkPepin(npc);
        else if (npcId == "NPCwitch")
            talkAdria(npc);
        else if (npcId == "NPCstorytell")
            talkCain(npc);
    }

    void DialogManager::fillConfirmDialog(DialogData& data, const FAWorld::Item& item, int price, const char* question, std::function<void()> successAction)
    {
        data.showScrollBar(false);
        data.clearLines();
        data.skip_line(2);
        data.textLines(item.descriptionForMerchants(), TextColor::white, false, {20, 40, 40}).setNumber(price);
        data.skip_line(2);
        data.textLines({question});
        data.skip_line();
        data.textLines({"Yes"}).setAction([this, successAction]() {
            auto action = successAction;
            mGuiManager.popDialogData();
            action();
        });
        data.textLines({"No"}).setAction([this]() { mGuiManager.popDialogData(); });
    }

    template <typename FilterType> void DialogManager::sellDialog(FilterType filter)
    {
        DialogData d;
        d.widen();
        int32_t cnt = 0;
        auto& inventory = mWorld.getCurrentPlayer()->mInventory;
        auto positionFilter = [filter, &inventory](FAWorld::EquipTarget target) { return filter(inventory.getItemAt(target)); };
        auto positions = mWorld.getCurrentPlayer()->mInventory.getBeltAndInventoryItemPositions();
        d.header({(boost::format("%2%            Your gold : %1%") % mWorld.getCurrentPlayer()->getTotalGold() %
                   (std::count_if(positions.begin(), positions.end(), positionFilter) > 0 ? "Which item is for sale?" : "You have nothing I want."))
                      .str()});

        for (auto target : positions)
        {
            if (!positionFilter(target))
                continue;
            auto& item = inventory.getItemAt(target);
            auto sellPrice = item.getPrice() / 4;
            d.textLines(item.descriptionForMerchants(), TextColor::white, false, {20, 40, 40})
                .setAction([&inventory, target, this, sellPrice, d, item, filter]() {
                    auto d_copy = d;
                    fillConfirmDialog(d_copy, item, sellPrice, "Are you sure you want to sell this item?", [this, filter, &inventory, target, sellPrice]() {
                        inventory.takeOut(target);
                        inventory.placeGold(sellPrice, mWorld.getItemFactory());
                        // This looks insansenly unsafe. TODO: think of better way.
                        auto recreate = [this, filter] { sellDialog(filter); };
                        mGuiManager.popDialogData();
                        recreate();
                    });
                    mGuiManager.pushDialogData(std::move(d_copy));
                })
                .setNumber(sellPrice);
            ++cnt;
        }
        d.footer({"Back"}).setAction([&]() { mGuiManager.popDialogData(); }).setForceSelected(cnt == 0);
        d.showScrollBar();
        mGuiManager.pushDialogData(std::move(d));
    }

    static bool griswoldSellFilter(const FAWorld::Item& item)
    {
        // TODO: add check for quest items
        return item.getType() != FAWorld::ItemType::misc && item.getType() != FAWorld::ItemType::gold && item.getType() != FAWorld::ItemType::staff;
    }

    void DialogManager::talkGriswold(const FAWorld::Actor* npc)
    {
        DialogData d;
        auto& td = npc->getTalkData();
        d.header({td.at("introductionHeader1"), td.at("introductionHeader2")});
        d.textLines({td.at("introduction")}, TextColor::golden);
        d.skip_line();
        d.textLines({td.at("talk")}, TextColor::blue).setAction([]() {});
        d.textLines({td.at("buyBasic")}).setAction([]() {});
        d.textLines({td.at("buyPremium")}).setAction([]() {});
        d.textLines({td.at("sell")}).setAction([&] { sellDialog(griswoldSellFilter); });
        d.textLines({td.at("repair")}).setAction([]() {});
        d.textLines({td.at("quit")}).setAction([&]() { quitDialog(); });
        mGuiManager.pushDialogData(std::move(d));
    }

    void DialogManager::quitDialog() const { mGuiManager.popDialogData(); }
}