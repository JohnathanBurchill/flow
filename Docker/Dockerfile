# flow: Dockerfile

# Copyright (C) 2023  Johnathan K Burchill

# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.

# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.

# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <https://www.gnu.org/licenses/>.

FROM johnathanburchill/utils:cdev AS dev
USER root
RUN apt -y update
RUN apt -y upgrade
RUN apt -y install libavutil-dev
RUN apt -y install libavcodec-dev
RUN apt -y install libswscale-dev
RUN apt -y install libavformat-dev
RUN apt -y install libavfilter-dev
RUN apt -y install libswresample-dev
RUN apt -y install libx264-dev
RUN apt -y install libsdl2-2.0-0
RUN apt -y install libsdl2-dev
RUN apt -y install libsdl2-doc
RUN apt -y install libsdl2-gfx-1.0-0
RUN apt -y install libsdl2-gfx-dev
RUN apt -y install libsdl2-gfx-doc
RUN apt -y install libsdl2-image-2.0-0
RUN apt -y install libsdl2-image-dev
RUN apt -y install libsdl2-mixer-2.0-0
RUN apt -y install libsdl2-mixer-dev
RUN apt -y install libsdl2-net-2.0-0
RUN apt -y install libsdl2-net-dev
RUN apt -y install libsdl2-ttf-2.0-0
RUN apt -y install libsdl2-ttf-dev
RUN apt -y install pkg-config

RUN chown science.science /home/science
USER science
WORKDIR /home/science/src
VOLUME ["/home/science/src"]
ENTRYPOINT ["/bin/bash"]



