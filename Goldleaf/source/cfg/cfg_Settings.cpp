
/*

    Goldleaf - Multipurpose homebrew tool for Nintendo Switch
    Copyright (C) 2018-2023 XorTroll

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <https://www.gnu.org/licenses/>.

*/

#include <cfg/cfg_Settings.hpp>
#include <fs/fs_FileSystem.hpp>
#include <ui/ui_Utils.hpp>

namespace cfg {

    namespace {

        inline std::string ColorToHex(const pu::ui::Color clr) {
            char str[0x20] = {};
            snprintf(str, sizeof(str), "#%02X%02X%02X%02X", clr.r, clr.g, clr.b, clr.a);
            return str;
        }

        Language g_DefaultLanguage = Language::English;
        bool g_DefaultLanguageLoaded = false;

        void EnsureDefaultLanguage() {
            if(!g_DefaultLanguageLoaded) {
                u64 tmp_lang_code = 0;
                auto sys_lang = SetLanguage_ENUS;
                GLEAF_RC_ASSERT(setGetSystemLanguage(&tmp_lang_code));
                GLEAF_RC_ASSERT(setMakeLanguage(tmp_lang_code, &sys_lang));
                g_DefaultLanguage = GetLanguageBySystemLanguage(sys_lang);
                g_DefaultLanguageLoaded = true;
            }

            GLEAF_ASSERT_TRUE(g_DefaultLanguageLoaded);
        }

    }

    void Settings::Save() {
        auto json = JSON::object();

        if(this->has_custom_lang) {
            json["general"]["customLanguage"] = GetLanguageCode(this->custom_lang);
        }
        if(this->has_external_romfs) {
            json["general"]["externalRomFs"] = this->external_romfs;
        }
        json["general"]["use12hTime"] = this->use_12h_time;
        json["general"]["ignoreHiddenFiles"] = this->ignore_hidden_files;
        
        if(this->has_custom_scheme) {
            json["ui"]["background"] = ColorToHex(this->custom_scheme.bg);
            json["ui"]["base"] = ColorToHex(this->custom_scheme.base);
            json["ui"]["baseFocus"] = ColorToHex(this->custom_scheme.base_focus);
            json["ui"]["text"] = ColorToHex(this->custom_scheme.text);
        }
        json["ui"]["menuItemSize"] = this->menu_item_size;
        if(this->has_scrollbar_color) {
            json["ui"]["scrollBar"] = ColorToHex(this->scrollbar_color);
        }
        if(this->has_progressbar_color) {
            json["ui"]["progressBar"] = ColorToHex(this->progressbar_color);
        }

        json["installs"]["ignoreRequiredFwVersion"] = this->ignore_required_fw_ver;
        json["installs"]["showDeletionPromptAfterInstall"] = this->show_deletion_prompt_after_install;
        json["installs"]["copyBufferMaxSize"] = this->copy_buffer_max_size;

        json["export"]["decryptBufferMaxSize"] = this->decrypt_buffer_max_size;
        
        for(u32 i = 0; i < this->bookmarks.size(); i++) {
            const auto &bmk = this->bookmarks[i];
            json["web"]["bookmarks"][i]["name"] = bmk.name;
            json["web"]["bookmarks"][i]["url"] = bmk.url;
        }

        auto sd_exp = fs::GetSdCardExplorer();
        sd_exp->DeleteFile(GLEAF_PATH_SETTINGS_FILE);
        sd_exp->WriteJSON(GLEAF_PATH_SETTINGS_FILE, json);
    }

    std::string Settings::PathForResource(const std::string &res_path) {
        auto romfs_exp = fs::GetRomFsExplorer();
        
        if(this->has_external_romfs) {
            const auto &ext_path = this->external_romfs + "/" + res_path;
            auto sd_exp = fs::GetSdCardExplorer();
            if(sd_exp->IsFile(ext_path)) {
                return ext_path;
            }
        }
        return romfs_exp->MakeAbsolute(res_path);
    }

    JSON Settings::ReadJSONResource(const std::string &res_path) {
        auto sd_exp = fs::GetSdCardExplorer();
        auto romfs_exp = fs::GetRomFsExplorer();

        if(this->has_external_romfs) {
            const auto &ext_path = this->external_romfs + "/" + res_path;
            if(sd_exp->IsFile(ext_path)) {
                return sd_exp->ReadJSON(ext_path);
            }
        }
        return romfs_exp->ReadJSON(romfs_exp->MakeAbsolute(res_path));
    }

    void Settings::ApplyScrollBarColor(pu::ui::elm::Menu::Ref &menu) {
        if(this->has_scrollbar_color) {
            menu->SetScrollbarColor(this->scrollbar_color);
        }
    }

    void Settings::ApplyProgressBarColor(pu::ui::elm::ProgressBar::Ref &p_bar) {
        if(this->has_progressbar_color) {
            p_bar->SetProgressColor(this->progressbar_color);
        }
    }

    Settings ProcessSettings() {
        Settings settings = {};

        settings.has_custom_lang = false;
        settings.has_external_romfs = false;
        settings.use_12h_time = false;
        settings.ignore_hidden_files = false;

        settings.has_custom_scheme = false;
        settings.menu_item_size = 80;
        settings.has_scrollbar_color = false;
        settings.has_progressbar_color = false;

        settings.ignore_required_fw_ver = true;
        settings.show_deletion_prompt_after_install = false;
        settings.copy_buffer_max_size = 4_MB;

        settings.decrypt_buffer_max_size = 16_MB;

        settings.custom_scheme = ui::GenerateRandomScheme();

        auto sd_exp = fs::GetSdCardExplorer();
        const auto settings_json = sd_exp->ReadJSON(GLEAF_PATH_SETTINGS_FILE);
        if(settings_json.count("general")) {
            const auto &lang = settings_json["general"].value("customLanguage", "");
            if(!lang.empty()) {
                const auto custom_lang = GetLanguageByCode(lang);
                settings.has_custom_lang = true;
                settings.custom_lang = custom_lang;
            }

            const auto &ext_romfs = settings_json["general"].value("externalRomFs", "");
            if(!ext_romfs.empty()) {
                settings.has_external_romfs = true;
                if(ext_romfs.substr(0, 6) == "sdmc:/") {
                    settings.external_romfs = ext_romfs;
                }
                else {
                    settings.external_romfs = "sdmc:";
                    if(ext_romfs[0] != '/') {
                        settings.external_romfs += "/";
                    }
                    settings.external_romfs += ext_romfs;
                }
            }

            settings.use_12h_time = settings_json["general"].value("use12hTime", settings.use_12h_time);
            settings.ignore_hidden_files = settings_json["general"].value("ignoreHiddenFiles", settings.ignore_hidden_files);
        }

        if(settings_json.count("ui")) {
            const auto &background_clr = settings_json["ui"].value("background", "");
            if(!background_clr.empty()) {
                settings.has_custom_scheme = true;
                settings.custom_scheme.bg = pu::ui::Color::FromHex(background_clr);
            }
            const auto &base_clr = settings_json["ui"].value("base", "");
            if(!base_clr.empty()) {
                settings.has_custom_scheme = true;
                settings.custom_scheme.base = pu::ui::Color::FromHex(base_clr);
            }
            const auto &base_focus_clr = settings_json["ui"].value("baseFocus", "");
            if(!base_focus_clr.empty()) {
                settings.has_custom_scheme = true;
                settings.custom_scheme.base_focus = pu::ui::Color::FromHex(base_focus_clr);
            }
            const auto &text_clr = settings_json["ui"].value("text", "");
            if(!text_clr.empty()) {
                settings.has_custom_scheme = true;
                settings.custom_scheme.text = pu::ui::Color::FromHex(text_clr);
            }
            settings.menu_item_size = settings_json["ui"].value("menuItemSize", settings.menu_item_size);
            const auto &scrollbar_clr = settings_json["ui"].value("scrollBar", "");
            if(!scrollbar_clr.empty()) {
                settings.has_scrollbar_color = true;
                settings.scrollbar_color = pu::ui::Color::FromHex(scrollbar_clr);
            }
            const auto &pbar_clr = settings_json["ui"].value("progressBar", "");
            if(!pbar_clr.empty()) {
                settings.has_progressbar_color = true;
                settings.progressbar_color = pu::ui::Color::FromHex(pbar_clr);
            }
        }
        if(settings_json.count("installs")) {
            settings.ignore_required_fw_ver = settings_json["installs"].value("ignoreRequiredFwVersion", settings.ignore_required_fw_ver);
            settings.show_deletion_prompt_after_install = settings_json["installs"].value("showDeletionPromptAfterInstall", settings.show_deletion_prompt_after_install);
            settings.copy_buffer_max_size = settings_json["installs"].value("copyBufferMaxSize", settings.copy_buffer_max_size);
        }
        if(settings_json.count("export")) {
            settings.decrypt_buffer_max_size = settings_json["installs"].value("decryptBufferMaxSize", settings.decrypt_buffer_max_size);
        }
        if(settings_json.count("web")) {
            if(settings_json["web"].count("bookmarks")) {
                for(u32 i = 0; i < settings_json["web"]["bookmarks"].size(); i++) {
                    const WebBookmark bmk = {
                        .name = settings_json["web"]["bookmarks"][i].value("name", ""),
                        .url = settings_json["web"]["bookmarks"][i].value("url", "")
                    };
                    if(!bmk.url.empty() && !bmk.name.empty()) {
                        settings.bookmarks.push_back(bmk);
                    }
                }
            }
        }
        return settings;
    }

    Language Settings::GetLanguage() {
        if(this->has_custom_lang) {
            return this->custom_lang;
        }
        else {
            EnsureDefaultLanguage();
            return g_DefaultLanguage;
        }
    }

}
