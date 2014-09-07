# DeaDBeeF Library Browser

Music library plugin for DeadDBeeF based off of the [Filebrowser plugin](http://sourceforge.net/projects/deadbeef-fb/) by [Jan D. Behrens](mailto:zykure@web.de).

Thanks to [Alexey Yakovenko](https://github.com/Alexey-Yakovenko) for the awesome music player that is [DeaDBeeF](http://deadbeef.sourceforge.net/).

![Library Plugin](http://i.imgur.com/54KbDQa.png)

# Compiling & Installing

To build the plugin from source, you'll need the [DeaDBeeF](http://deadbeef.sourceforge.net/) and GTK2 headers
  (deadbeef-dev, libgtk2.0-dev). Clone this repository and make sure the DeaDBeeF headers are in your library path.

Then just simply run
``` bash
./autogen.sh
./configure [--enable-gtk3]
make

./userinstall.sh #Installs the plugin to $HOME/.local/lib/deadbeef/
```

# License

    DeaDBeeF Library Browser
    Copyright (C) 2011  Jan D. Behrens <zykure@web.de>
    Copyright (C) 2014  Jesse Farebrother <jessefarebro@gmail.com>

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
