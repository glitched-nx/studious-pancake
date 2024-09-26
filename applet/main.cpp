/*
 * Copyright (c) 2020-2023 Studious Pancake
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#include <payload.hpp>
#include <util.hpp>

#include <cstdio>
#include <cstdlib>
#include <switch.h>
#include <vector>

namespace {

    typedef void (*TuiCallback)(void const *const user);

    struct TuiItem {
        std::string const text;
        TuiCallback const cb;
        void const *const user;
        bool const selectable;
    };

    void BootConfigCallback(void const *const user) {
        auto const config = reinterpret_cast<Payload::HekateConfig const *>(user);

        Payload::RebootToHekateConfig(*config, false);
    }

    void IniConfigCallback(void const *const user) {
        auto const config = reinterpret_cast<Payload::HekateConfig const *>(user);

        Payload::RebootToHekateConfig(*config, true);
    }

    void UmsCallback(void const *const user) {
        Payload::RebootToHekateUMS(Payload::UmsTarget_Sd);

        (void)user;
    }

    void PayloadCallback(void const *const user) {
        auto const config = reinterpret_cast<Payload::PayloadConfig const *>(user);

        Payload::RebootToPayload(*config);
    }
}

extern "C" void userAppInit(void) {
    spsmInitialize();
    splInitialize();
    i2cInitialize();
}

extern "C" void userAppExit(void) {
    i2cExit();
    splExit();
    spsmExit();
}

int main(int const argc, char const *argv[]) {
    if (!util::IsErista() && !util::SupportsMarikoRebootToConfig()) {
        consoleInit(nullptr);

        printf("min. Atmosphère 1.6.1\n");

        consoleUpdate(nullptr);

        /* Configure input */
        padConfigureInput(8, HidNpadStyleSet_NpadStandard);

        /* Initialize pad */
        PadState pad;
        padInitializeAny(&pad);

        while (appletMainLoop()) {
            /* Update padstate */
            padUpdate(&pad);

            if (padGetButtonsDown(&pad))
                break;
        }

        consoleExit(nullptr);
        return 0;
    }

    std::vector<TuiItem> items;

    /* Load available boot configs */
    auto const boot_config_list = Payload::LoadHekateConfigList();

    /* Load available ini configs */
    auto const ini_config_list = Payload::LoadIniConfigList();

    /* Load available payloads */
    auto const payload_config_list = Payload::LoadPayloadList();

    /* Build menu item list */
    items.reserve(2 + boot_config_list.empty() ? 0 : 1 + boot_config_list.size()
                    + ini_config_list.empty()  ? 0 : 1 + ini_config_list.size()
                    + payload_config_list.empty()  ? 0 : 1 + payload_config_list.size());

    if (!boot_config_list.empty()) {
        items.emplace_back("quickReBoot to OS ->", nullptr, nullptr, false);
        for (auto const &entry : boot_config_list)
            items.emplace_back(entry.name, BootConfigCallback, &entry, true);
    }

    if (!ini_config_list.empty()) {
        items.emplace_back("quickReBoot to INI ->", nullptr, nullptr, false);
        for (auto const &entry : ini_config_list)
            items.emplace_back(entry.name, IniConfigCallback, &entry, true);
    }

    items.emplace_back("quickMount ->", nullptr, nullptr, false);
    items.emplace_back("SD-Card -> UMS", UmsCallback, nullptr, true);

    if (util::IsErista() && !payload_config_list.empty()) {
        items.emplace_back("quickReBoot to Payload ->", nullptr, nullptr, false);
        for (auto const &entry : payload_config_list)
            items.emplace_back(entry.name, PayloadCallback, &entry, true);
    }

    std::size_t index = 0;

    for (auto const &item : items) {
        if (item.selectable)
            break;

        index++;
    }

    PrintConsole *const console = consoleInit(nullptr);

    /* Configure input */
    padConfigureInput(8, HidNpadStyleSet_NpadStandard);

    /* Initialize pad */
    PadState pad;
    padInitializeAny(&pad);

    /* Deinit sm to free up our only service slot */
    smExit();

    bool repaint = true;

    while (appletMainLoop()) {
        {
            /* Update padstate */
            padUpdate(&pad);

            u64 const kDown = padGetButtonsDown(&pad);

            if ((kDown & (HidNpadButton_Plus | HidNpadButton_B | HidNpadButton_L)))
                break;

            if ((kDown & HidNpadButton_A)) {
                auto &item = items[index];

                if (item.selectable && item.cb)
                    item.cb(item.user);
            }

            if ((kDown & HidNpadButton_Minus)) {
                Payload::RebootToHekate();
            }

            if ((kDown & HidNpadButton_AnyDown) && (index + 1) < items.size()) {
                for (std::size_t i = index; i < items.size(); i++) {
                    if (!items[i + 1].selectable)
                        continue;

                    index = i + 1;
                    break;
                }
                repaint = true;
            }

            if ((kDown & HidNpadButton_AnyUp) && index > 0) {
                for (std::size_t i = index; i > 0; i--) {
                    if (!items[i - 1].selectable)
                        continue;

                    index = i - 1;
                    break;
                }
                repaint = true;
            }
        }

        if (repaint) {
            consoleClear();

            std::printf("  quickReBoot\n -------------\n");

            for (std::size_t i = 0; i < items.size(); i++) {
                auto const &item    = items[i];
                bool const selected = (i == index);

                if (!item.selectable)
                    console->flags |= CONSOLE_COLOR_FAINT;

                std::printf("%s %s\n", selected ? "->" : "  ", item.text.c_str());

                if (!item.selectable)
                    console->flags &= ~CONSOLE_COLOR_FAINT;
            }
            repaint = false;
        }

        consoleUpdate(nullptr);
    }

    consoleExit(nullptr);

    (void)argc;
    (void)argv;

    return EXIT_SUCCESS;
}
