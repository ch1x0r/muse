//=========================================================
//  MusE
//  Linux Music Editor
//    $Id: filedialog.cpp,v 1.3.2.3 2005/06/19 06:32:07 lunar_shuttle Exp $
//  (C) Copyright 2000 Werner Schweer (ws@seh.de)
//=========================================================

#include <iostream>
#include <errno.h>
#include <qwidget.h>
#include <qurl.h>
#include <qpixmap.h>
#include <qmessagebox.h>
#include <qbuttongroup.h>
#include <qtoolbutton.h>
#include <qradiobutton.h>
#include <qstringlist.h>

#include "filedialog.h"
#include "fdialogbuttons.h"
#include "../globals.h"

MFileDialog::ViewType MFileDialog::lastViewUsed = GLOBAL_VIEW;
QString MFileDialog::lastUserDir = "";
QString MFileDialog::lastGlobalDir = "";

//---------------------------------------------------------
//   createDir
//    return true if dir could not created
//---------------------------------------------------------

static bool createDir(const QString& s)
      {
      QString sl("/");
      QStringList l = QStringList::split(sl, s);
      QString path(sl);
      QDir dir;
      for (QStringList::Iterator it = l.begin(); it != l.end(); ++it) {
            dir.setPath(path);
            if (!QDir(path + sl + *it).exists()) {
                  if (!dir.mkdir(*it)) {
                        printf("mkdir failed: %s %s\n",
                           path.latin1(), (*it).latin1());
                        return true;
                        }
                  }
            path += sl;
            path += *it;
            }
      return false;
      }

//---------------------------------------------------------
//   testDirCreate
//    return true if dir does not exist
//---------------------------------------------------------

static bool testDirCreate(QWidget* parent, const QString& path)
      {
      QDir dir(path);
      if (!dir.exists()) {
            int n = QMessageBox::information(parent,
               QWidget::tr("MusE: get file name"),
               QWidget::tr("the directory\n") + path
                  + QWidget::tr("\ndoes not exist\ncreate?"),
               QWidget::tr("&Create"),
               QWidget::tr("Cancel"),
               QString::null,  1, 1);
            if (n == 0) {
                  if (createDir(path)) {
                        QMessageBox::critical(parent,
                           QWidget::tr("MusE: create directory"),
                           QWidget::tr("creating dir failed")
                           );
                        return true;
                        }
                  return false;
                  }
            return true;
            }
      return false;
      }

//---------------------------------------------------------
//   globalToggled
//---------------------------------------------------------

void MFileDialog::globalToggled(bool flag)
      {
      if (flag) {
            buttons->userButton->setOn(!flag);
            buttons->projectButton->setOn(!flag);
            if (lastGlobalDir.isEmpty())
                  lastGlobalDir = museGlobalShare + QString("/") + baseDir; // Initialize if first time
            QString dir = lastGlobalDir;
            setDir(dir);
            lastViewUsed = GLOBAL_VIEW;
            }
      }

//---------------------------------------------------------
//   userToggled
//---------------------------------------------------------

void MFileDialog::userToggled(bool flag)
      {
      if (flag) {
            buttons->globalButton->setOn(!flag);
            buttons->projectButton->setOn(!flag);


            if (lastUserDir.isEmpty()) {
                  lastUserDir = museUser + QString("/") + baseDir; // Initialize if first time
                  }

            if (testDirCreate(this, lastUserDir))
                  setDir(museUser);
            else
                  setDir(lastUserDir);

            lastViewUsed = USER_VIEW;
            }
      }

//---------------------------------------------------------
//   projectToggled
//---------------------------------------------------------

void MFileDialog::projectToggled(bool flag)
      {
      if (flag) {
            buttons->globalButton->setOn(!flag);
            buttons->userButton->setOn(!flag);

            QString s;
            if (museProject == museProjectInitPath ) {
                  // if project path is uninitialized, meaning it is still set to museProjectInitPath.
                  // then project path is set to current pwd instead.
                  s = QString(getcwd(0,0)) + QString("/");
                  }
            else
                  s = museProject + QString("/"); // + baseDir;

            if (testDirCreate(this, s))
                  setDir(museProject);
            else
                  setDir(s);
            lastViewUsed = PROJECT_VIEW;
            }
      }


//---------------------------------------------------------
//   MFileDialog
//---------------------------------------------------------

MFileDialog::MFileDialog(const QString& dir,
   const QString& filter, QWidget* parent, bool writeFlag)
   : QFileDialog(QString("."), filter, parent, 0, true)
      {
      showButtons = false;
      if (dir[0] == '/') {
            buttons = 0;
            setDir(dir);
            }
      else {
            baseDir     = dir;
            showButtons = true;
            buttons     = new FileDialogButtons(this, "fdialogbuttons");
            addLeftWidget(buttons);
            connect(buttons->globalButton, SIGNAL(toggled(bool)), SLOT(globalToggled(bool)));
            connect(buttons->userButton, SIGNAL(toggled(bool)), SLOT(userToggled(bool)));
            connect(buttons->projectButton, SIGNAL(toggled(bool)), SLOT(projectToggled(bool)));
            connect(this, SIGNAL(dirEntered(const QString&)), SLOT(directoryChanged(const QString&)));
            if (writeFlag) {
                  buttons->globalButton->setEnabled(false);
                  switch (lastViewUsed) {
                           case GLOBAL_VIEW:
                           case PROJECT_VIEW:
                                 buttons->projectButton->setOn(true);
                                 break;

                           case USER_VIEW:
                                 buttons->userButton->setOn(true);
                                 break;
                        }
                  }
            else {
                  switch (lastViewUsed) {
                        case GLOBAL_VIEW:
                              buttons->globalButton->setOn(true);
                              break;

                        case PROJECT_VIEW:
                              buttons->projectButton->setOn(true);
                              break;

                        case USER_VIEW:
                              buttons->userButton->setOn(true);
                              break;
                        }

                  }
            buttons->loadAllGroup->setShown(false);
            }
      }

//---------------------------------------------------------
//   MFileDialog::directoryChanged
//---------------------------------------------------------
void MFileDialog::directoryChanged(const QString&)
      {
      ViewType currentView = GLOBAL_VIEW;
      const QDir* ndir = dir();
      QString newdir = ndir->absPath().latin1();
      delete ndir; // We're owners of this one so we should delete it
      if (buttons->projectButton->isOn())
            currentView = PROJECT_VIEW;
      else if (buttons->userButton->isOn())
            currentView = USER_VIEW;

      switch (currentView) {
            case GLOBAL_VIEW:
                  lastGlobalDir = newdir;
                  break;

            case USER_VIEW:
                  lastUserDir = newdir;
                  break;

            case PROJECT_VIEW: // Do nothing
            default:
                  break;
            }
      }

//---------------------------------------------------------
//   ContentsPreview
//---------------------------------------------------------

ContentsPreview::~ContentsPreview()
      {
      if (bg)
            delete bg;
      }

//---------------------------------------------------------
//   ContentsPreview::showPreview
//---------------------------------------------------------

void ContentsPreview::previewUrl(const QUrl& url)
      {
      if (!url.isLocalFile())
            return;
      if (url.path() == path)
            return;
      path = url.path();
      if (bg)
            delete bg;
      bg  = new QPixmap(path);
      if (bg)
            setBackgroundPixmap(*bg);
      }

//---------------------------------------------------------
//   getFilterExtension
//---------------------------------------------------------

QString getFilterExtension(const QString &filter)
{
  //
  // Return the first extension found. Must contain at least one * character.
  //
  
  int pos = filter.find('*');
  if(pos == -1)
    return QString(); 
  
  QString filt;
  int len = filter.length();
  ++pos;
  for( ; pos < len; ++pos)
  {
    QChar c = filter[pos];
    if((c == ')') || (c == ';') || (c == ',') || (c == ' '))
      break; 
    filt += filter[pos];
  }
  return filt;
}

//---------------------------------------------------------
//   getOpenFileName
//---------------------------------------------------------

QString getOpenFileName(const QString &startWith,
   //const char** filters, QWidget* parent, const QString& name, bool* all)
   const QStringList& filters, QWidget* parent, const QString& name, bool* all)
      {
      QString initialSelection;
      MFileDialog *dlg = new MFileDialog(startWith, QString::null, parent, false);
      dlg->setFilters(filters);
      dlg->setCaption(name);
      if (all)
            dlg->buttons->loadAllGroup->setShown(true);
      if (!initialSelection.isEmpty())
            dlg->setSelection(initialSelection);
      dlg->setMode(QFileDialog::ExistingFile);
      QString result;
      if (dlg->exec() == QDialog::Accepted) {
            result = dlg->selectedFile();
            if (all) {
                  *all = dlg->buttons->loadAllButton->isOn();
                  }
            }
      delete dlg;
      return result;
      }

//---------------------------------------------------------
//   getSaveFileName
//---------------------------------------------------------

QString getSaveFileName(const QString &startWith,
   //const char** filters, QWidget* parent, const QString& name)
   const QStringList& filters, QWidget* parent, const QString& name)
      {
      MFileDialog *dlg = new MFileDialog(startWith, QString::null, parent, true);
      dlg->setFilters(filters);
      dlg->setCaption(name);
      dlg->setMode(QFileDialog::AnyFile);
      QString result;
      if (dlg->exec() == QDialog::Accepted) 
        result = dlg->selectedFile();
            
      
      // Added by T356.
      if(!result.isEmpty())
      {
        QString filt = dlg->selectedFilter();
        filt = getFilterExtension(filt);
        // Do we have a valid extension?
        if(!filt.isEmpty())
        {
          // If the rightmost characters of the filename do not already contain
          //  the extension, add the extension to the filename.
          //if(result.right(filt.length()) != filt)
          if(!result.endsWith(filt))
            result += filt;
        }
        else
        {
          // No valid extension, or just * was given. Although it would be nice to allow no extension
          //  or any desired extension by commenting this section out, it's probably not a good idea to do so.
          //
          // NOTE: Most calls to this routine getSaveFileName() are followed by fileOpen(),
          //  which can tack on its own extension, but only if the *complete* extension is blank. 
          // So there is some overlap going on. Enabling this actually stops that action, 
          //  but only if there are no errors in the list of filters. fileOpen() will act as a 'catchall'.
          //
          // Force the filter list to the first one (the preferred one), and then get the filter.
          dlg->setSelectedFilter(0);
          filt = dlg->selectedFilter();
          filt = getFilterExtension(filt);
              
          // Do we have a valid extension?
          if(!filt.isEmpty())
          {
            // If the rightmost characters of the filename do not already contain
            //  the extension, add the extension to the filename.
            //if(result.right(filt.length()) != filt)
            if(!result.endsWith(filt))
              result += filt;
          }
        }
      }
      
      delete dlg;
      return result;
      }

//---------------------------------------------------------
//   getImageFileName
//---------------------------------------------------------

QString getImageFileName(const QString& startWith,
   //const char** filters, QWidget* parent, const QString& name)
   const QStringList& filters, QWidget* parent, const QString& name)
      {
      QString initialSelection;
	QString* workingDirectory = new QString(QDir::currentDirPath());
      if (!startWith.isEmpty() ) {
            QFileInfo fi(startWith);
            if (fi.exists() && fi.isDir()) {
                  *workingDirectory = startWith;
                  }
            else if (fi.exists() && fi.isFile()) {
                  *workingDirectory = fi.dirPath(TRUE);
                  initialSelection = fi.absFilePath();
                  }
            }
      MFileDialog *dlg = new MFileDialog(*workingDirectory, QString::null,
         parent);

      dlg->setContentsPreviewEnabled(true);
      ContentsPreview* preview = new ContentsPreview(dlg);
      dlg->setContentsPreview(preview, preview);
      dlg->setPreviewMode(QFileDialog::Contents);

      dlg->setCaption(name);
      dlg->setFilters(filters);
      dlg->setMode(QFileDialog::ExistingFile);
      QString result;
      if (!initialSelection.isEmpty())
            dlg->setSelection( initialSelection);
      if (dlg->exec() == QDialog::Accepted) {
            result = dlg->selectedFile();
            }
      delete dlg;
      return result;
      }

//---------------------------------------------------------
//   fileOpen
//    opens file "name" with extension "ext" in mode "mode"
//    handles "name.ext.bz2" and "name.ext.gz"
//
//    mode = "r" or "w"
//    popenFlag   set to true on return if file was opened
//                with popen() (and therefore must be closed
//                with pclose())
//    noError     show no error if file was not found in "r"
//                mode. Has no effect in "w" mode
//    overwriteWarning
//                warn in "w" mode, if file exists
//---------------------------------------------------------

FILE* fileOpen(QWidget* parent, QString name, const QString& ext,
   const char* mode, bool& popenFlag, bool noError,
   bool overwriteWarning)
      {
      QFileInfo info(name);
      QString zip;

      popenFlag = false;
      if (info.extension(true) == "") {
            name += ext;
            info.setFile(name);
            }
      else if (info.extension(false) == "gz") {
            popenFlag = true;
            zip = QString("gzip");
            }
      else if (info.extension(false) == "bz2") {
            popenFlag = true;
            zip = QString("bzip2");
            }

      if (strcmp(mode,"w") == 0 && overwriteWarning && info.exists()) {
            QString s(QWidget::tr("File\n") + name + QWidget::tr("\nexists"));
            int rv = QMessageBox::warning(parent,
               QWidget::tr("MusE: write"),
               s,
               QWidget::tr("Overwrite"),
               QWidget::tr("Quit"), QString::null, 0, 1);
            switch(rv) {
                  case 0:  // overwrite
                        break;
                  case 1:  // quit
                        return 0;
                  }
            }
      FILE* fp = 0;
      if (popenFlag) {
            if (strcmp(mode, "r") == 0)
                  zip += QString(" -d < ");
            else
                  zip += QString(" > ");
            zip += name;
            fp  = popen(zip.ascii(), mode);
            }
      else {
            fp = fopen(name.ascii(), mode);
            }
      if (fp == 0 && !noError) {
            QString s(QWidget::tr("Open File\n") + name + QWidget::tr("\nfailed: ")
               + QString(strerror(errno)));
            QMessageBox::critical(parent, QWidget::tr("MusE: Open File"), s);
            return 0;
            }
      return fp;
      }

//---------------------------------------------------------
//   MFile
//---------------------------------------------------------

MFile::MFile(const QString& _path, const QString& _ext)
   : path(_path), ext(_ext)
      {
      f = 0;
      isPopen = false;
      }

MFile::~MFile()
      {
      if (f) {
            if (isPopen)
                  pclose(f);
            else
                  fclose(f);
            }
      }

//---------------------------------------------------------
//   open
//---------------------------------------------------------

//FILE* MFile::open(const char* mode, const char** pattern,
FILE* MFile::open(const char* mode, const QStringList& pattern,
   QWidget* parent, bool noError, bool warnIfOverwrite, const QString& caption)
      {
      QString name;
      if (strcmp(mode, "r") == 0)
           name = getOpenFileName(path, pattern, parent, caption, 0);
      else
           name = getSaveFileName(path, pattern, parent, caption);
      if (name.isEmpty())
            return 0;
      f = fileOpen(parent, name, ext, mode, isPopen, noError,
         warnIfOverwrite);
      return f;
      }

