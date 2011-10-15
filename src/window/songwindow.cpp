/*
   Vimpc
   Copyright (C) 2010 - 2011 Nathan Sweetman

   This program is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.

   songwindow.hpp - handling of the mpd library but with a playlist interface
   */

#include "songwindow.hpp"

#include <pcrecpp.h>

#include "buffers.hpp"
#include "callback.hpp"
#include "colour.hpp"
#include "mpdclient.hpp"
#include "settings.hpp"
#include "screen.hpp"
#include "buffer/library.hpp"
#include "mode/search.hpp"
#include "window/infowindow.hpp"

using namespace Ui;

SongWindow::SongWindow(Main::Settings const & settings, Ui::Screen & screen, Mpc::Client & client, Ui::Search const & search, std::string name) :
   SelectWindow     (screen, name),
   settings_        (settings),
   client_          (client),
   search_          (search),
   browse_          ()
{
}

SongWindow::~SongWindow()
{
}

void SongWindow::Add(Mpc::Song * song)
{
   if (song != NULL)
   {
      Buffer().Add(song);
   }
}

void SongWindow::AddToPlaylist(uint32_t position)
{
   if ((position < BufferSize()) && (Buffer().Get(position) != NULL))
   {
      Main::Playlist().Add(Buffer().Get(position));
      client_.Add(*(Buffer().Get(position)));
   }
}


void SongWindow::Print(uint32_t line) const
{
   uint32_t printLine = line + FirstLine();

   if (printLine < BufferSize())
   {
      Mpc::Song const * nextSong    = Buffer().Get(printLine);
      WINDOW          * window      = N_WINDOW();
      int32_t           colour      = DetermineSongColour(printLine, nextSong);

      if (settings_.ColourEnabled() == true)
      {
         wattron(window, COLOR_PAIR(colour));
      }

      if (printLine == CurrentLine())
      {
         wattron(window, A_REVERSE);
      }
      else if ((settings_.ColourEnabled() == true) && (colour != Colour::CurrentSong))
      {
         wattron(window, COLOR_PAIR(Colour::Song));
      }

      mvwhline(window,  line, 0, ' ', screen_.MaxColumns());
      mvwaddstr(window, line, 0, "[");

      if ((settings_.ColourEnabled() == true) && (colour != Colour::CurrentSong) && (printLine != CurrentLine()))
      {
         wattron(window, COLOR_PAIR(Colour::SongId));
      }

      wprintw(window, "%5d", FirstLine() + line + 1);

      if ((settings_.ColourEnabled() == true) && (colour != Colour::CurrentSong) && (printLine != CurrentLine()))
      {
         wattroff(window, COLOR_PAIR(Colour::SongId));
      }

      waddstr(window, "] ");

      if (settings_.ColourEnabled() == true)
      {
         wattron(window, COLOR_PAIR(colour));
      }

      std::string artist = nextSong->Artist().c_str();
      std::string title  = nextSong->Title().c_str();

      if (title == "Unknown")
      {
         title = nextSong->URI().c_str();
      }

      if (nextSong->Entry() == NULL)
      {
         artist = "";
         title = nextSong->URI().c_str();
      }

      if ((artist != "") && (artist != "Unknown"))
      {
         wprintw(window, "%s - %s", artist.c_str(), title.c_str());
      }
      else
      {
         wprintw(window, "%s", title.c_str());
      }

      if ((settings_.ColourEnabled() == true) && (colour != Colour::CurrentSong) && (printLine != CurrentLine()))
      {
         wattron(window, COLOR_PAIR(Colour::Song));
      }

      std::string const durationString(nextSong->DurationString());
      mvwprintw(window, line, (screen_.MaxColumns() - durationString.size() - 2), "[%s]", durationString.c_str());

      if (settings_.ColourEnabled() == true)
      {
         wattroff(window, COLOR_PAIR(colour));
      }

      wattroff(window, A_REVERSE);
   }
}

void SongWindow::Left(Ui::Player & player, uint32_t count)
{
   //player.SkipSong(Ui::Player::Previous, count);
}

void SongWindow::Right(Ui::Player & player, uint32_t count)
{
   //player.SkipSong(Ui::Player::Next, count);
}

void SongWindow::Confirm()
{
   if (Buffer().Size() > CurrentLine())
   {
      AddToPlaylist(CurrentLine());

      if (Buffer().Get(CurrentLine()) != NULL)
      {
         client_.Play(static_cast<uint32_t>(Main::Playlist().Size() - 1));
      }
   }
}

uint32_t SongWindow::Current() const
{
   uint32_t current       = CurrentLine();
   int32_t  currentSongId = client_.GetCurrentSong();

   if ((currentSongId >= 0) && (currentSongId < static_cast<int32_t>(Main::Playlist().Size())))
   {
      current = Buffer().Index(Main::Playlist().Get(currentSongId));
   }

   return current;
}

uint32_t SongWindow::Playlist(int count) const
{
   //! \todo not sure how i am going to do this
   // but it sure be used to navigate throw the browse window
   // skipping forward and backwards to songs that are in the playlist only
   //

   int32_t line = CurrentLine();

   if (count > 0)
   {
      for (int32_t i = CurrentLine() + 1; i <= ContentSize(); ++i)
      {
         if (Buffer().Get(i)->Reference() > 0)
         {
            --count;
            line = i;
         }

         if (count == 0)
         {
            break;
         }
      }
   }
   else
   {
      for (int32_t i = CurrentLine() - 1; i >= 0; --i)
      {
         if (Buffer().Get(i)->Reference() > 0)
         {
            ++count;
            line = i;
         }

         if (count == 0)
         {
            break;
         }
      }
   }

   return line;
}

void SongWindow::AddLine(uint32_t line, uint32_t count, bool scroll)
{
   if (count > 1)
   {
      client_.StartCommandList();
   }

   for (uint32_t i = 0; i < count; ++i)
   {
      AddToPlaylist(line + i);
   }

   if (count > 1)
   {
      client_.SendCommandList();
   }

   if (scroll == true)
   {
      Scroll(count);
   }
}

void SongWindow::AddAllLines()
{
   AddLine(0, BufferSize(), false);
}

void SongWindow::CropLine(uint32_t line, uint32_t count, bool scroll)
{
   DeleteLine(line, count, scroll);
}

void SongWindow::CropAllLines()
{
   DeleteLine(CurrentLine(), BufferSize() - CurrentLine(), false);
}

void SongWindow::DeleteLine(uint32_t line, uint32_t count, bool scroll)
{
   Main::PlaylistPasteBuffer().Clear();

   if (count > 1)
   {
      client_.StartCommandList();
   }

   for (uint32_t i = 0; i < count; ++i)
   {
      int32_t index = line;

      if (index + i < BufferSize())
      {
         index = Main::Playlist().Index(Buffer().Get(index + i));

         if (index >= 0)
         {
            client_.Delete(index);
            Main::Playlist().Remove(index, 1);
         }
      }
   }

   if (count > 1)
   {
      client_.SendCommandList();
   }

   if (scroll == true)
   {
      Scroll(count);
   }
}

void SongWindow::DeleteAllLines()
{
   DeleteLine(0, BufferSize(), false);
}

void SongWindow::Edit()
{
   if (CurrentLine() < BufferSize())
   {
      Mpc::Song * song(Buffer().Get(CurrentLine()));

      if (song != NULL)
      {
         if (screen_.GetWindowFromName("songinfo") != Ui::Screen::Unknown)
         {
            screen_.SetVisible(screen_.GetWindowFromName("songinfo"), false);
         }

         InfoWindow * window = screen_.CreateInfoWindow("songinfo", song);

         if (window->ContentSize() > -1)
         {
            screen_.SetActiveAndVisible(screen_.GetWindowFromName(window->Name()));
         }
      }
   }
}


void SongWindow::Save(std::string const & name)
{
   client_.StartCommandList();
   client_.CreatePlaylist(name);

   for (unsigned int i = 0; i < BufferSize(); ++i)
   {
      client_.AddToPlaylist(name, Buffer().Get(i));
   }

   client_.SendCommandList();
}

int32_t SongWindow::DetermineSongColour(uint32_t line, Mpc::Song const * const nextSong) const
{
   int32_t colour = Colour::Song;

   if ((nextSong->URI() == client_.GetCurrentSongURI()))
   {
      colour = Colour::CurrentSong;
   }
   else if (client_.SongIsInQueue(*nextSong))
   {
      colour = Colour::FullAdd;
   }
   else if ((search_.LastSearchString() != "") && (settings_.HightlightSearch() == true))
   {
      pcrecpp::RE expression(".*" + search_.LastSearchString() + ".*", search_.LastSearchOptions());

      if (expression.FullMatch(nextSong->PlaylistDescription()))
      {
         colour = Colour::SongMatch;
      }
   }

   return colour;
}

void SongWindow::Clear()
{
   ScrollTo(0);
   Buffer().Clear();
}
