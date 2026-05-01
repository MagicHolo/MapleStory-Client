//////////////////////////////////////////////////////////////////////////////////
//	This file is part of the continued Journey MMORPG client					//
//	Copyright (C) 2015-2019  Daniel Allendorf, Ryan Payton						//
//																				//
//	This program is free software: you can redistribute it and/or modify		//
//	it under the terms of the GNU Affero General Public License as published by	//
//	the Free Software Foundation, either version 3 of the License, or			//
//	(at your option) any later version.											//
//																				//
//	This program is distributed in the hope that it will be useful,				//
//	but WITHOUT ANY WARRANTY; without even the implied warranty of				//
//	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the				//
//	GNU Affero General Public License for more details.							//
//																				//
//	You should have received a copy of the GNU Affero General Public License	//
//	along with this program.  If not, see <https://www.gnu.org/licenses/>.		//
//////////////////////////////////////////////////////////////////////////////////
#include "UILogin.h"

#include "UILoginNotice.h"
#include "UILoginWait.h"

#include "../UI.h"

#include "../Components/MapleButton.h"

#include "../../Audio/Audio.h"

#include "../../Net/Packets/LoginPackets.h"

#include <windows.h>

#ifdef USE_NX
#include <nlnx/nx.hpp>
#endif

namespace ms
{
	constexpr int16_t VWIDTH = 1024;
	constexpr int16_t VHEIGHT = 768;
	constexpr int16_t WOFFSET = VWIDTH / 2;
	constexpr int16_t HOFFSET = VHEIGHT / 2;

	UILogin::UILogin() : UIElement(Point<int16_t>(0, 0), Point<int16_t>(VWIDTH, VHEIGHT))
	{
		Music(Configuration::get().get_login_music()).play();

		std::string version_text = Configuration::get().get_version();
		version = Text(Text::Font::A12B, Text::Alignment::LEFT, Color::Name::LEMONGRASS, "Ver. " + version_text);

		nl::node Login = nl::nx::UI["Login.img"];

		nl::node version_node = Login["Common"]["version"]["pos"];
		version_pos = version_node ? Point<int16_t>(version_node) : Point<int16_t>(7, 5);

		nl::node Title = Login["Title"];
		if (!Title)
			Title = Login["Title_new"];

		nl::node check_src = Title["check"];
		check[false] = check_src["0"];
		check[true] = check_src["1"];

		// Background layers with tiling support
		nl::node login_back = nl::nx::Map["Back"]["login.img"]["back"];

		if (!login_back)
			login_back = nl::nx::Map001["Back"]["UI_login.img"]["back"];

		if (login_back)
		{
			for (int i = 0; i < 100; i++)
			{
				nl::node layer = login_back[std::to_string(i)];

				if (!layer)
					break;

				BackgroundLayer bg;
				bg.animation = Animation(layer);

				auto dim = bg.animation.get_dimensions();
				bg.cx = (dim.x() > 0) ? dim.x() : 1;
				bg.cy = (dim.y() > 0) ? dim.y() : 1;

				// Tile narrow gradient strips (e.g. 20x600) but not small particles (e.g. 10x7)
				bg.htile = (bg.cx <= 30 && bg.cy >= 200) ? (VWIDTH / bg.cx + 3) : 1;
				bg.vtile = (bg.cy <= 30 && bg.cx >= 200) ? (VHEIGHT / bg.cy + 3) : 1;

				login_backgrounds.push_back(bg);
			}
		}

		// v83 assets designed for 800x600, center on 1024x768
		constexpr int16_t OX = (VWIDTH - 800) / 2;
		constexpr int16_t OY = (VHEIGHT - 600) / 2;

		// MSTitle logo (397x219) — centered horizontally, near top
		if (Title["MSTitle"])
			sprites.emplace_back(Title["MSTitle"], Point<int16_t>(OX + 201, OY + 57));

		// Buttons — v83 positions offset for 1024x768
		buttons[Buttons::BtLogin] = std::make_unique<MapleButton>(Title["BtLogin"], Point<int16_t>(OX + 475, OY + 278));
		buttons[Buttons::BtEmailSave] = std::make_unique<MapleButton>(Title["BtEmailSave"], Point<int16_t>(OX + 374, OY + 330));
		buttons[Buttons::BtEmailLost] = std::make_unique<MapleButton>(Title["BtEmailLost"], Point<int16_t>(OX + 470, OY + 357));
		buttons[Buttons::BtPasswdLost] = std::make_unique<MapleButton>(Title["BtPasswdLost"], Point<int16_t>(OX + 394, OY + 357));
		buttons[Buttons::BtNew] = std::make_unique<MapleButton>(Title["BtNew"], Point<int16_t>(OX + 394, OY + 386));
		buttons[Buttons::BtHomePage] = std::make_unique<MapleButton>(Title["BtHomePage"], Point<int16_t>(OX + 494, OY + 386));
		buttons[Buttons::BtQuit] = std::make_unique<MapleButton>(Title["BtQuit"], Point<int16_t>(OX + 7, OY + 7));

		background = ColorBox(dimension.x(), dimension.y(), Color::Name::BLACK, 1.0f);

		Point<int16_t> textfield_pos = Point<int16_t>(OX + 374, OY + 278);
		Point<int16_t> textfield_dim = Point<int16_t>(96, 20);

		account = Textfield(Text::Font::A13M, Text::Alignment::LEFT, Color::Name::JAMBALAYA, Rectangle<int16_t>(textfield_pos, textfield_pos + textfield_dim), TEXTFIELD_LIMIT);

		account.set_key_callback
		(
			KeyAction::Id::TAB, [&]
			{
				account.set_state(Textfield::State::NORMAL);
				password.set_state(Textfield::State::FOCUSED);
			}
		);

		account.set_enter_callback
		(
			[&](std::string msg)
			{
				login();
			}
		);

		textfield_pos.shift_y(textfield_dim.y() + 2);

		password = Textfield(Text::Font::A13M, Text::Alignment::LEFT, Color::Name::JAMBALAYA, Rectangle<int16_t>(textfield_pos, textfield_pos + textfield_dim), TEXTFIELD_LIMIT);

		password.set_key_callback
		(
			KeyAction::Id::TAB, [&]
			{
				account.set_state(Textfield::State::FOCUSED);
				password.set_state(Textfield::State::NORMAL);
			}
		);

		password.set_enter_callback
		(
			[&](std::string msg)
			{
				login();
			}
		);

		password.set_cryptchar('*');

		saveid = Setting<SaveLogin>::get().load();

		if (saveid)
		{
			account.change_text(Setting<DefaultAccount>::get().load());
			password.set_state(Textfield::State::FOCUSED);
		}
		else
		{
			account.set_state(Textfield::State::FOCUSED);
		}

		if (Configuration::get().get_auto_login())
		{
			UI::get().emplace<UILoginWait>([]() {});

			auto loginwait = UI::get().get_element<UILoginWait>();

			if (loginwait && loginwait->is_active())
				LoginPacket(
					Configuration::get().get_auto_acc(),
					Configuration::get().get_auto_pass()
				).dispatch();
		}
	}

	void UILogin::draw(float alpha) const
	{
		background.draw(position);

		for (auto& bg : login_backgrounds)
		{
			int16_t ix = WOFFSET;
			int16_t iy = HOFFSET;

			if (bg.htile > 1)
			{
				while (ix > 0)
					ix -= bg.cx;

				while (ix < -bg.cx)
					ix += bg.cx;
			}

			if (bg.vtile > 1)
			{
				while (iy > 0)
					iy -= bg.cy;

				while (iy < -bg.cy)
					iy += bg.cy;
			}

			int16_t tw = bg.cx * bg.htile;
			int16_t th = bg.cy * bg.vtile;

			for (int16_t tx = 0; tx < tw; tx += bg.cx)
				for (int16_t ty = 0; ty < th; ty += bg.cy)
					bg.animation.draw(DrawArgument(Point<int16_t>(ix + tx, iy + ty)), alpha);
		}

		UIElement::draw(alpha);

		version.draw(position + version_pos - Point<int16_t>(0, 5));
		account.draw(position);
		password.draw(position);

		constexpr int16_t OX = (VWIDTH - 800) / 2;
		constexpr int16_t OY = (VHEIGHT - 600) / 2;
		check[saveid].draw(DrawArgument(Point<int16_t>(OX + 374, OY + 332)));
	}

	void UILogin::update()
	{
		UIElement::update();

		for (auto& bg : login_backgrounds)
			bg.animation.update();

		account.update();
		password.update();

		if (account.get_state() == Textfield::State::NORMAL &&
			password.get_state() == Textfield::State::NORMAL)
		{
			if (!account.empty())
				password.set_state(Textfield::State::FOCUSED);
			else
				account.set_state(Textfield::State::FOCUSED);
		}
	}

	void UILogin::login()
	{
		account.set_state(Textfield::State::DISABLED);
		password.set_state(Textfield::State::DISABLED);

		std::string account_text = account.get_text();
		std::string password_text = password.get_text();

		std::function<void()> okhandler = [&, password_text]()
		{
			account.set_state(Textfield::State::NORMAL);
			password.set_state(Textfield::State::NORMAL);

			if (!password_text.empty())
				password.set_state(Textfield::State::FOCUSED);
			else
				account.set_state(Textfield::State::FOCUSED);
		};

		if (account_text.empty())
		{
			UI::get().emplace<UILoginNotice>(UILoginNotice::Message::NOT_REGISTERED, okhandler);
			return;
		}

		if (password_text.length() <= 4)
		{
			UI::get().emplace<UILoginNotice>(UILoginNotice::Message::WRONG_PASSWORD, okhandler);
			return;
		}

		UI::get().emplace<UILoginWait>(okhandler);

		auto loginwait = UI::get().get_element<UILoginWait>();

		if (loginwait && loginwait->is_active())
			LoginPacket(account_text, password_text).dispatch();
	}

	void UILogin::open_url(uint16_t id)
	{
		std::string url;

		switch (id)
		{
			case Buttons::BtNew:
				url = Configuration::get().get_joinlink();
				break;
			case Buttons::BtHomePage:
				url = Configuration::get().get_website();
				break;
			case Buttons::BtPasswdLost:
				url = Configuration::get().get_findpass();
				break;
			case Buttons::BtEmailLost:
				url = Configuration::get().get_findid();
				break;
			default:
				return;
		}

		ShellExecuteA(NULL, "open", url.c_str(), NULL, NULL, SW_SHOWNORMAL);
	}

	Button::State UILogin::button_pressed(uint16_t id)
	{
		switch (id)
		{
			case Buttons::BtLogin:
			{
				login();

				return Button::State::NORMAL;
			}
			case Buttons::BtNew:
			case Buttons::BtHomePage:
			case Buttons::BtPasswdLost:
			case Buttons::BtEmailLost:
			{
				open_url(id);

				return Button::State::NORMAL;
			}
			case Buttons::BtEmailSave:
			{
				saveid = !saveid;

				Setting<SaveLogin>::get().save(saveid);

				return Button::State::MOUSEOVER;
			}
			case Buttons::BtQuit:
			{
				UI::get().quit();

				return Button::State::PRESSED;
			}
			default:
			{
				return Button::State::DISABLED;
			}
		}
	}

	Cursor::State UILogin::send_cursor(bool clicked, Point<int16_t> cursorpos)
	{
		if (Cursor::State new_state = account.send_cursor(cursorpos, clicked))
			return new_state;

		if (Cursor::State new_state = password.send_cursor(cursorpos, clicked))
			return new_state;

		return UIElement::send_cursor(clicked, cursorpos);
	}

	UIElement::Type UILogin::get_type() const
	{
		return TYPE;
	}
}
