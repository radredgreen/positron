positron
==================

Positron is the main application powering <https://github.com/radredgreen/wyrecam>

It connects an ingenic t31 camera to homekit

Compilation
============

This project is typically built as part of wyrecam.

Status
======
See <https://github.com/radredgreen/wyrecam>

Return audio (controller->camera) and audio recording aren't working.  

There are quite a few TODOs in the code that need to be resolved.  The one that bothers me the most is that SELECT in the audio/video return thread that caused runaway cpu usage in the HomeKitADK library.


See further status at <https://github.com/radredgreen/wyrecam>

License
========

    This program is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version.

    This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.

    You should have received a copy of the GNU General Public License along with this program. If not, see <https://www.gnu.org/licenses/>. 

    Copyright (C) 2023 Radredgreen


Original inspiration from this partially completed project <https://github.com/joebelford/homekit_camera>

