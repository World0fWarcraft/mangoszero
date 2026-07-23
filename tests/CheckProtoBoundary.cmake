# MaNGOS is a full featured server for World of Warcraft, supporting
# the following clients: 1.12.x, 2.4.3, 3.3.5a, 4.3.4a and 5.4.8
#
# Copyright (C) 2005-2025 MaNGOS <https://www.getmangos.eu>
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

if(NOT IS_DIRECTORY "${PROTO_DIR}")
  message(FATAL_ERROR "Protocol boundary missing: ${PROTO_DIR}")
endif()

file(GLOB PROTO_SOURCES
  "${PROTO_DIR}/*.h" "${PROTO_DIR}/*.hpp"
  "${PROTO_DIR}/*.cpp" "${PROTO_DIR}/*.cc")

set(FORBIDDEN_PATTERNS
  "#[ \t]*include[ \t]*[\"<](Database/|World\\.h|WorldSession\\.h|AddonHandler\\.h|LuaEngine\\.h|Warden)"
  "(^|[^A-Za-z0-9_])(WorldSession|sWorld|LoginDatabase|CharacterDatabase|WorldDatabase|sAddOnHandler|LuaEngine|Warden)([^A-Za-z0-9_]|$)")

foreach(FILE_PATH IN LISTS PROTO_SOURCES)
  file(READ "${FILE_PATH}" CONTENTS)
  foreach(PATTERN IN LISTS FORBIDDEN_PATTERNS)
    if(CONTENTS MATCHES "${PATTERN}")
      message(FATAL_ERROR "Forbidden protocol dependency in ${FILE_PATH}: ${PATTERN}")
    endif()
  endforeach()
endforeach()
