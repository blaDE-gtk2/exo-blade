#!/bin/sh
#
# Copyright (c) 2002-2019
#         The Xfce development team. All rights reserved.
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; version 2 of the License ONLY.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
# MA 02110-1301 USA
#
# Written for Xfce by Benedikt Meurer <benny@xfce.org>
#                 and Brian Tarricone <brian@tarricone.org>.
#

(type xdt-autogen) >/dev/null 2>&1 || {
  cat >&2 <<EOF
autogen.sh: You don't seem to have the Xfce development tools (at least
            version $XDT_REQURED_VERSION) installed on your system, which
            are required to build this software.
            Please install the xfce4-dev-tools package first; it is available
            from https://www.xfce.org/.
EOF
  exit 1
}

test -d m4 || mkdir m4

XDT_AUTOGEN_REQUIRED_VERSION="4.7.2" exec xdt-autogen $@

# vi:set ts=2 sw=2 et ai:
