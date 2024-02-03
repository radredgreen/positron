positron
==================

Positron is the main application powering <https://github.com/radredgreen/wyrecam>

It connects an ingenic t31 camera to homekit

Original inspiration from this partially completed project <https://github.com/joebelford/homekit_camera>

Compilation
============

This project is typically built as part of wyrecam.  See
<https://github.com/radredgreen/wyrecam>

Status
======
I'm pushing this because there's a lot working right now.  See <https://github.com/radredgreen/wyrecam>

But the code needs a lot of work. Compiler warnings need to be cleared and the code needs to be thoughly cleaned, linted and refactored.

Notably, return audio (controller->camera) isn't working properly.  See https://github.com/mstorsjo/fdk-aac/issues/164.  I'd prefer to stick with AAC-ELD for the audio as I believe that's required for HKSV, but we could use OPUS for SRTP and AAC for HKSV since there is no audio return channel through HKSV.  

There are quite a few TODOs in the code that need to be resolved.  The two that bother me the most are that SELECT in the audio/video return thread that caused runaway cpu usage in the HomeKitADK library and stream reconfiguration doesn't work because the TLV parser can't take a partial TLV (see the code for an example).

Please let me know if you can help with any of this!  Next directions are to either press on to HKSV (https://github.com/Supereg/secure-video-specification), fix these shortcomings or refactor.

Night time color is forced on, the image is not flipped (although all my testing has been flipped for ceiling mounts), and the IR LEDs are disabled by hard coded variables.  Need to decide a configuration method (probably a file, or the HK key-value-store) in the future.


See further status at <https://github.com/radredgreen/wyrecam>

License
========

    This program is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version.

    This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.

    You should have received a copy of the GNU General Public License along with this program. If not, see <https://www.gnu.org/licenses/>. 

    Copyright (C) 2023 Radredgreen

