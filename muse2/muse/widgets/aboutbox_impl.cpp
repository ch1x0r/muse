//=========================================================
//  MusE
//  Linux Music Editor
//  $Id: ./muse/widgets/aboutbox_impl.cpp $
//
//  Copyright (C) 1999-2011 by Werner Schweer and others
//
//  This program is free software; you can redistribute it and/or
//  modify it under the terms of the GNU General Public License
//  as published by the Free Software Foundation; version 2 of
//  the License, or (at your option) any later version.
//
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//  You should have received a copy of the GNU General Public License
//  along with this program; if not, write to the Free Software
//  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
//
//=========================================================
#include "aboutbox_impl.h"
#include "config.h"
#include "icons.h"

namespace MusEGui {

AboutBoxImpl::AboutBoxImpl()
{
  setupUi(this);
  imageLabel->setPixmap(*aboutMuseImage);
  QString version(VERSION);
  QString gitstring(GITSTRING);
  versionLabel->setText("Version: " + version + (gitstring == QString() ? "" : "\n("+ gitstring + ")"));
  QString systemInfo="";

#ifdef LV2_SUPPORT
  systemInfo.append("LV2 support enabled.\n");
#endif
#ifdef DSSI_SUPPORT
  systemInfo.append("DSSI support enabled.\n");
#endif
#ifdef VST_NATIVE_SUPPORT
  #ifdef VST_VESTIGE_SUPPORT
    systemInfo.append("Native VST support enabled using VESTIGE compatibility header.\n");
  #else
    systemInfo.append("Native VST support enabled using Steinberg VSTSDK.\n");
  #endif
#endif
    systemInformationLabel->setText(systemInfo);
}

}
