//=========================================================
//  MusE
//  Linux Music Editor
//  scoreedit.cpp
//  (C) Copyright 2011 Florian Jung (florian.a.jung@web.de)
//=========================================================


#include <QLayout>
#include <QSizeGrip>
#include <QLabel>
#include <QScrollBar>
#include <QPushButton>
#include <QToolButton>
#include <QToolTip>
#include <QMenu>
#include <QSignalMapper>
#include <QMenuBar>
#include <QApplication>
#include <QClipboard>
#include <QDir>
#include <QAction>
#include <QActionGroup>
#include <QKeySequence>
#include <QKeyEvent>
#include <QGridLayout>
#include <QResizeEvent>
#include <QCloseEvent>
#include <QMimeData>
#include <QScrollArea>
#include <QSettings>
#include <QImage>
#include <QInputDialog>
#include <QMessageBox>

#include <stdio.h>
#include <math.h>

#include <iostream>
#include <sstream>
using namespace std;

#include "app.h"
#include "xml.h"
#include "mtscale.h"
#include "prcanvas.h"
#include "scoreedit.h"
#include "scrollscale.h"
#include "piano.h"
#include "../ctrl/ctrledit.h"
#include "splitter.h"
#include "ttoolbar.h"
#include "tb1.h"
#include "utils.h"
#include "globals.h"
#include "gconfig.h"
#include "icons.h"
#include "audio.h"

#include "cmd.h"
#include "quantconfig.h"
#include "shortcuts.h"

#include "mtrackinfo.h"

#include "sig.h"


string IntToStr(int i);
QString IntToQStr(int i);


#define SPLIT_NOTE 60
//TODO: let the user specify that somehow?





//PIXELS_PER_NOTEPOS must be greater or equal to 3*NOTE_XLEN + 2*NOTE_SHIFT
//because if tick 0 is at x=0: the notes can be shifted by NOTE_SHIFT.
//additionally, they can be moved by NOTE_XLEN (collision avoiding)
//then, they have their own width, which is NOTE_XLEN/2 into the x>0-area
//the same thing applies to the x<0-area

//  OOO
//   |
//   ^actual calculated x, without shifting or moving
//  ^ and
//    ^   : moved note (by XLEN)
// additionally, a shift is possible
// total_width = shift + move + note_xlen + move + shift, where move==note_xlen
// total_width = 2*shift + 3*note_xlen
// if total_width is greater than px_per_notepos, there will be collisions!





//do NOT put parentheses around this!
#define PAGESTEP 3/4


#define SCROLL_MARGIN 10
#define SCROLL_SPEED 5
//SCROLL_SPEED is in (scroll_pixels per second) per mouse-move-pixel
#define SCROLL_SPEED_MAX 500
//SCROLL_SPEED_MAX is in scroll_pixels_per_second



#define STAFF_DISTANCE (10*YLEN)
#define GRANDSTAFF_DISTANCE (8*YLEN)

QString create_random_string(int len=8)
{
	string result;
	
	for (int i=0;i<len;i++)
		result+=char((rand() % 26) + 'A');
	
	return QString(result.c_str());
}






QPixmap *pix_whole, *pix_half, *pix_quarter; // arrays [NUM_MYCOLORS]
QPixmap *pix_dot, *pix_b, *pix_sharp, *pix_noacc; // arrays [NUM_MYCOLORS]
QPixmap *pix_r1, *pix_r2, *pix_r4, *pix_r8, *pix_r16; // pointers
QPixmap *pix_flag_up, *pix_flag_down; // arrays [4]
QPixmap *pix_num; // array [10]
QPixmap *pix_clef_violin, *pix_clef_bass; //pointers
bool pixmaps_initalized=false;
QColor* mycolors; // array [NUM_MYCOLORS]





int ScoreEdit::serial=1;
set<QString> ScoreEdit::names;

//---------------------------------------------------------
//   ScoreEdit
//---------------------------------------------------------

ScoreEdit::ScoreEdit(QWidget* parent, const char* name, unsigned initPos)
   : TopWin(parent, name)
{
	setAttribute(Qt::WA_DeleteOnClose);

	mainw    = new QWidget(this);

	mainGrid = new QGridLayout();
	mainw->setLayout(mainGrid);

	mainGrid->setContentsMargins(0, 0, 0, 0);
	mainGrid->setSpacing(0);  
	setCentralWidget(mainw);




	
	score_canvas=new ScoreCanvas(this, mainw, 1, 1);
	xscroll = new QScrollBar(Qt::Horizontal, mainw);
	yscroll = new QScrollBar(Qt::Vertical, mainw);

	connect(xscroll, SIGNAL(valueChanged(int)), score_canvas,   SLOT(x_scroll_event(int)));
	connect(score_canvas, SIGNAL(xscroll_changed(int)), xscroll,   SLOT(setValue(int)));

	connect(yscroll, SIGNAL(valueChanged(int)), score_canvas,   SLOT(y_scroll_event(int)));
	connect(score_canvas, SIGNAL(yscroll_changed(int)), yscroll,   SLOT(setValue(int)));

	connect(score_canvas, SIGNAL(canvas_width_changed(int)), SLOT(canvas_width_changed(int)));
	connect(score_canvas, SIGNAL(viewport_width_changed(int)), SLOT(viewport_width_changed(int)));
	connect(score_canvas, SIGNAL(canvas_height_changed(int)), SLOT(canvas_height_changed(int)));
	connect(score_canvas, SIGNAL(viewport_height_changed(int)), SLOT(viewport_height_changed(int)));

	connect(song, SIGNAL(songChanged(int)), score_canvas, SLOT(song_changed(int)));

	mainGrid->addWidget(score_canvas, 0, 0);
	mainGrid->addWidget(xscroll,1,0);
	mainGrid->addWidget(yscroll,0,1);

	xscroll->setMinimum(0);
	yscroll->setMinimum(0);

	menu_mapper=new QSignalMapper(this);
	connect(menu_mapper, SIGNAL(mapped(int)), SLOT(menu_command(int)));



	// Toolbars ---------------------------------------------------------
	QToolBar* undo_tools=addToolBar(tr("Undo/Redo tools"));
	undo_tools->setObjectName("Undo/Redo tools");
	undo_tools->addActions(undoRedo->actions());
	addToolBar(undo_tools);

	EditToolBar* edit_tools = new EditToolBar(this, PointerTool | PencilTool | RubberTool);
	addToolBar(edit_tools);
	edit_tools->set(PointerTool);
	score_canvas->set_tool(PointerTool);
	connect(edit_tools, SIGNAL(toolChanged(int)), score_canvas,   SLOT(set_tool(int)));

	QToolBar* panic_toolbar = addToolBar(tr("panic"));         
	panic_toolbar->setObjectName("panic");
	panic_toolbar->addAction(panicAction);

	QToolBar* transport_toolbar = addToolBar(tr("transport"));
	transport_toolbar->setObjectName("transport");
	transport_toolbar->addActions(transportAction->actions());

	QToolBar* newnote_toolbar = addToolBar(tr("New note settings"));
	newnote_toolbar->setObjectName("New note settings");
	newnote_toolbar->addWidget(new QLabel(tr("Note length:"), newnote_toolbar));
	QActionGroup* len_actions=new QActionGroup(this);
	QAction* n1_action = newnote_toolbar->addAction("1", menu_mapper, SLOT(map()));
	QAction* n2_action = newnote_toolbar->addAction("2", menu_mapper, SLOT(map()));
	QAction* n4_action = newnote_toolbar->addAction("4", menu_mapper, SLOT(map()));
	QAction* n8_action = newnote_toolbar->addAction("8", menu_mapper, SLOT(map()));
	QAction* n16_action = newnote_toolbar->addAction("16", menu_mapper, SLOT(map()));
	QAction* n32_action = newnote_toolbar->addAction("32", menu_mapper, SLOT(map()));
	QAction* nlast_action = newnote_toolbar->addAction(tr("last"), menu_mapper, SLOT(map()));
	menu_mapper->setMapping(n1_action, CMD_NOTELEN_1);
	menu_mapper->setMapping(n2_action, CMD_NOTELEN_2);
	menu_mapper->setMapping(n4_action, CMD_NOTELEN_4);
	menu_mapper->setMapping(n8_action, CMD_NOTELEN_8);
	menu_mapper->setMapping(n16_action, CMD_NOTELEN_16);
	menu_mapper->setMapping(n32_action, CMD_NOTELEN_32);
	menu_mapper->setMapping(nlast_action, CMD_NOTELEN_LAST);
	n1_action->setCheckable(true);
	n2_action->setCheckable(true);
	n4_action->setCheckable(true);
	n8_action->setCheckable(true);
	n16_action->setCheckable(true);
	n32_action->setCheckable(true);
	nlast_action->setCheckable(true);
	len_actions->addAction(n1_action);
	len_actions->addAction(n2_action);
	len_actions->addAction(n4_action);
	len_actions->addAction(n8_action);
	len_actions->addAction(n16_action);
	len_actions->addAction(n32_action);
	len_actions->addAction(nlast_action);
	
	nlast_action->setChecked(true);
	menu_command(CMD_NOTELEN_LAST);
	
	
	QToolBar* quant_toolbar = addToolBar(tr("Quantisation settings"));
	newnote_toolbar->setObjectName("Quantisation settings");
	quant_toolbar->addWidget(new QLabel(tr("Quantisation:"), quant_toolbar));
	QComboBox* quant_combobox = new QComboBox(this);
	quant_combobox->addItem("2"); // if you add or remove items from
	quant_combobox->addItem("4"); // here, also change quant_mapper[]
	quant_combobox->addItem("8"); // in ScoreCanvas::set_quant()!
	quant_combobox->addItem("16");
	quant_combobox->addItem("32");
	connect(quant_combobox, SIGNAL(currentIndexChanged(int)), score_canvas, SLOT(set_quant(int)));
	quant_toolbar->addWidget(quant_combobox);
	quant_combobox->setCurrentIndex(2);
	
	quant_toolbar->addSeparator();

	quant_toolbar->addWidget(new QLabel(tr("Pixels per whole:"), quant_toolbar));
	QSpinBox* px_per_whole_spinbox = new QSpinBox(this);
	px_per_whole_spinbox->setRange(10, 1000);
	px_per_whole_spinbox->setSingleStep(50);
	connect(px_per_whole_spinbox, SIGNAL(valueChanged(int)), score_canvas, SLOT(set_pixels_per_whole(int)));
	connect(score_canvas, SIGNAL(pixels_per_whole_changed(int)), px_per_whole_spinbox, SLOT(setValue(int)));
	quant_toolbar->addWidget(px_per_whole_spinbox);
	px_per_whole_spinbox->setValue(300);

	QMenu* settings_menu = menuBar()->addMenu(tr("&Settings"));      

		QMenu* color_menu = settings_menu->addMenu(tr("Note head &colors"));
			QActionGroup* color_actions = new QActionGroup(this);
			QAction* color_black_action = color_menu->addAction(tr("&Black"), menu_mapper, SLOT(map()));
			QAction* color_velo_action =  color_menu->addAction(tr("&Velocity"), menu_mapper, SLOT(map()));
			QAction* color_part_action =  color_menu->addAction(tr("&Part"), menu_mapper, SLOT(map()));
			color_black_action->setCheckable(true);
			color_velo_action->setCheckable(true);
			color_part_action->setCheckable(true);
			color_actions->addAction(color_black_action);
			color_actions->addAction(color_velo_action);
			color_actions->addAction(color_part_action);
			menu_mapper->setMapping(color_black_action, CMD_COLOR_BLACK);
			menu_mapper->setMapping(color_velo_action, CMD_COLOR_VELO);
			menu_mapper->setMapping(color_part_action, CMD_COLOR_PART);
			
			color_black_action->setChecked(true);
			menu_command(CMD_COLOR_BLACK);
  
		QMenu* preamble_menu = settings_menu->addMenu(tr("Set up &preamble"));
			QAction* preamble_keysig_action = preamble_menu->addAction(tr("Display &key signature"));
			QAction* preamble_timesig_action =  preamble_menu->addAction(tr("Display &time signature"));
			connect(preamble_keysig_action, SIGNAL(toggled(bool)), score_canvas, SLOT(preamble_keysig_slot(bool)));
			connect(preamble_timesig_action, SIGNAL(toggled(bool)), score_canvas, SLOT(preamble_timesig_slot(bool)));

			preamble_keysig_action->setCheckable(true);
			preamble_timesig_action->setCheckable(true);
			
			preamble_keysig_action->setChecked(true);
			preamble_timesig_action->setChecked(true);
  
		QAction* set_name_action = settings_menu->addAction(tr("Set Score &name"), menu_mapper, SLOT(map()));
		menu_mapper->setMapping(set_name_action, CMD_SET_NAME);
  
  


/* FINDMICHJETZT
	addToolBarBreak();
	info    = new NoteInfo(this);
	addToolBar(info);
*/




	score_canvas->song_changed(SC_EVENT_INSERTED);
	score_canvas->goto_tick(initPos,true);
	
	if (name!=NULL)
		set_name(name, false, true);
	else
		set_name("Score "+IntToQStr(serial++), false, true);
}


void ScoreEdit::add_parts(PartList* pl, bool all_in_one)
{
	score_canvas->add_staves(pl, all_in_one);
}

bool ScoreEdit::set_name(QString newname, bool emit_signal, bool emergency_name)
{
	if (names.find(newname)==names.end())
	{
		names.erase(name);
		names.insert(newname);

		name=newname;
		
		if (emit_signal)
			emit name_changed();
		
		return true;
	}
	else
	{
		if (emergency_name)
		{
			while (set_name(create_random_string(), emit_signal, false) == false);
			return true;
		}
		else
			return false;
	}
}

//---------------------------------------------------------
//   ~ScoreEdit
//---------------------------------------------------------

ScoreEdit::~ScoreEdit()
{
	
}


void ScoreEdit::canvas_width_changed(int width)
{
	xscroll->setMaximum(width);
}
void ScoreEdit::viewport_width_changed(int width)
{
	xscroll->setPageStep(width * PAGESTEP);
}

void ScoreEdit::canvas_height_changed(int height)
{
	int val=height - score_canvas->viewport_height();
	if (val<=0) val=0;
	
	yscroll->setMaximum(val);
	
	if (val==0)
		yscroll->hide();
	else
		yscroll->show();
}
void ScoreEdit::viewport_height_changed(int height)
{
	int val=score_canvas->canvas_height() - height;
	if (val<0) val=0;
	yscroll->setPageStep(height * PAGESTEP);
	yscroll->setMaximum(val);

	if (val==0)
		yscroll->hide();
	else
		yscroll->show();
}

void ScoreEdit::closeEvent(QCloseEvent* e)
{
	QSettings settings("MusE", "MusE-qt");
	//settings.setValue("ScoreEdit/geometry", saveGeometry());
	settings.setValue("ScoreEdit/windowState", saveState());

	emit deleted((unsigned long)this);
	e->accept();
}

void ScoreEdit::menu_command(int cmd)
{
	switch (cmd)
	{
		case CMD_SET_NAME:
		{
			bool ok;
			QString newname = QInputDialog::getText(this, tr("Enter the new score title"),
                                          tr("Enter the new score title"), QLineEdit::Normal,
                                          name, &ok);
			if (ok)
			{
				if (!set_name(newname))
					QMessageBox::warning(this, tr("Error"), tr("Changing score title failed:\nthe selected title is not unique"));
			}
		}
		
		default: 
			score_canvas->menu_command(cmd);
	}
}



void ScoreCanvas::add_staves(PartList* pl, bool all_in_one)
{
	staff_t staff(this);

	if (all_in_one)
	{
		staff.parts.clear();
		for (ciPart part_it=pl->begin(); part_it!=pl->end(); part_it++)
			staff.parts.insert(part_it->second);
		staff.cleanup_parts();

		staff.split_note=SPLIT_NOTE;

		staff.type=GRAND_TOP; //FINDMICH
		staff.clef=VIOLIN;
		staves.push_back(staff);

		staff.type=GRAND_BOTTOM;
		staff.clef=BASS;
		staves.push_back(staff);		
	}
	else
	{
		set<Track*> tracks;
		
		for (ciPart it=pl->begin(); it!=pl->end(); it++)
			tracks.insert(it->second->track());
		
		for (set<Track*>::iterator it=tracks.begin(); it!=tracks.end(); it++)
		{
			staff.parts.clear();
			for (ciPart part_it=pl->begin(); part_it!=pl->end(); part_it++)
				if (part_it->second->track() == *it)
					staff.parts.insert(part_it->second);
			staff.cleanup_parts();
			
			staff.split_note=SPLIT_NOTE;

			staff.type=GRAND_TOP; //FINDMICH
			staff.clef=VIOLIN;
			staves.push_back(staff);

			staff.type=GRAND_BOTTOM;
			staff.clef=BASS;
			staves.push_back(staff);
		}
	}
	
	cleanup_staves();
	recalc_staff_pos();
	song_changed(SC_EVENT_INSERTED);
}


ScoreCanvas::ScoreCanvas(ScoreEdit* pr, QWidget* parent_widget,
   int sx, int sy) : View(parent_widget, sx, sy)
{
	parent      = pr;
	setFocusPolicy(Qt::StrongFocus);
	setBg(Qt::white);
	
	setSizePolicy(QSizePolicy::Expanding,QSizePolicy::Expanding);
	
	init_pixmaps();
	
	x_pos=0;
	x_left=0;
	y_pos=0;
	dragging=false;
	mouse_erases_notes=false;
	mouse_inserts_notes=true;

	selected_part=NULL;
	
	last_len=384;
	new_len=-1;
	
	set_quant(2); //this is actually unneccessary, as while
	              //initalizing the quant_combobox, this gets
	              //called again. but for safety...
	set_pixels_per_whole(300); //same as above. but safety rocks

	dragging_staff=false;
	
	
	coloring_mode=COLOR_MODE_BLACK;
	preamble_contains_keysig=true;
	preamble_contains_timesig=true;
	
	
	x_scroll_speed=0;
	x_scroll_pos=0;	
	y_scroll_speed=0;
	y_scroll_pos=0;	
	connect (heartBeatTimer, SIGNAL(timeout()), SLOT(heartbeat_timer_event()));
	
	connect(song, SIGNAL(posChanged(int, unsigned, bool)), SLOT(pos_changed(int,unsigned,bool)));
	connect(song, SIGNAL(playChanged(bool)), SLOT(play_changed(bool)));
	connect(muse, SIGNAL(configChanged()), SLOT(config_changed()));
	
	
	staff_menu=new QMenu(this);

	staffmode_treble_action = staff_menu->addAction(tr("Treble"));
	connect(staffmode_treble_action, SIGNAL(triggered()), SLOT(staffmode_treble_slot()));

	staffmode_bass_action = staff_menu->addAction(tr("Bass"));
	connect(staffmode_bass_action, SIGNAL(triggered()), SLOT(staffmode_bass_slot()));

	staffmode_both_action = staff_menu->addAction(tr("Grand Staff"));
	connect(staffmode_both_action, SIGNAL(triggered()), SLOT(staffmode_both_slot()));

	remove_staff_action = staff_menu->addAction(tr("Remove staff"));
	connect(remove_staff_action, SIGNAL(triggered()), SLOT(remove_staff_slot()));

}

void ScoreCanvas::staffmode_treble_slot()
{
	set_staffmode(current_staff, MODE_TREBLE);
}

void ScoreCanvas::staffmode_bass_slot()
{
	set_staffmode(current_staff, MODE_BASS);
}

void ScoreCanvas::staffmode_both_slot()
{
	set_staffmode(current_staff, MODE_BOTH);
}

void ScoreCanvas::remove_staff_slot()
{
	remove_staff(current_staff);
}

void ScoreCanvas::set_staffmode(list<staff_t>::iterator it, staff_mode_t mode)
{
	if (it->type == GRAND_BOTTOM)
	{
		it--;
		if (it->type!=GRAND_TOP)
			cout << "THIS SHOULD NEVER HAPPEN: grand_bottom without top!"<<endl;
	}
	
	if (it->type==GRAND_TOP)
	{
		list<staff_t>::iterator tmp=it;
		tmp++;
		if (tmp->type!=GRAND_BOTTOM)
			cout << "THIS SHOULD NEVER HAPPEN: grand_top without bottom!"<<endl;
		staves.erase(tmp);
	}
	
	switch (mode)
	{
		case MODE_TREBLE:
			it->type=NORMAL;
			it->clef=VIOLIN;
			break;
			
		case MODE_BASS:
			it->type=NORMAL;
			it->clef=BASS;
			break;
		
		case MODE_BOTH:
			it->type=GRAND_BOTTOM;
			it->clef=BASS;
			it->split_note=SPLIT_NOTE;
			
			staves.insert(it, staff_t(this, GRAND_TOP, VIOLIN, it->parts, it->split_note));
			break;
		
		default:
			cout << "ILLEGAL FUNCTION CALL: invalid mode in set_staffmode" << endl;
	}
	
	recalc_staff_pos();
	song_changed(SC_EVENT_INSERTED);
}

void ScoreCanvas::remove_staff(list<staff_t>::iterator it)
{
	if (it->type == GRAND_BOTTOM)
	{
		it--;
		if (it->type!=GRAND_TOP)
			cout << "THIS SHOULD NEVER HAPPEN: grand_bottom without top!"<<endl;
	}
	
	if (it->type == NORMAL)
	{
		staves.erase(it);
	}
	else if (it->type == GRAND_TOP)
	{
		staves.erase(it++);
		if (it->type!=GRAND_BOTTOM)
			cout << "THIS SHOULD NEVER HAPPEN: grand_top without bottom!"<<endl;
		staves.erase(it);
	}		
	
	maybe_close_if_empty();
	recalc_staff_pos();
	song_changed(SC_EVENT_INSERTED);
}

void ScoreCanvas::merge_staves(list<staff_t>::iterator dest, list<staff_t>::iterator src)
{
	if (dest->type == GRAND_BOTTOM)
	{
		dest--;
		if (dest->type!=GRAND_TOP)
			cout << "THIS SHOULD NEVER HAPPEN: grand_bottom without top!"<<endl;
	}

	if (src->type == GRAND_BOTTOM)
	{
		src--;
		if (src->type!=GRAND_TOP)
			cout << "THIS SHOULD NEVER HAPPEN: grand_bottom without top!"<<endl;
	}
	
	if (dest==src) //dragged to itself?
		return;


	dest->parts.insert(src->parts.begin(), src->parts.end());
	
	if (dest->type == GRAND_TOP)
	{
		dest++;
		if (dest->type != GRAND_BOTTOM)
			cout << "THIS SHOULD NEVER HAPPEN: grand_top without bottom!"<<endl;
		dest->parts.insert(src->parts.begin(), src->parts.end());
	}
	
	remove_staff(src);

	recalc_staff_pos();
	song_changed(SC_EVENT_INSERTED);
}


void ScoreCanvas::song_changed(int flags)
{
	if (flags & (SC_PART_MODIFIED |
	             SC_EVENT_INSERTED | SC_EVENT_MODIFIED | SC_EVENT_REMOVED |
	             SC_SIG  | SC_KEY) )
	{
		calc_pos_add_list();
		
		for (list<staff_t>::iterator it=staves.begin(); it!=staves.end(); it++)
			it->recalculate();
		
		redraw();
		emit canvas_width_changed(canvas_width());
	}
	
	if (flags & SC_PART_REMOVED)
	{
		bool something_changed=false;
		
		for (list<staff_t>::iterator it=staves.begin(); it!=staves.end(); it++)
		{
			if (it->cleanup_parts())
				something_changed=true;
		}
		
		cleanup_staves();
		recalc_staff_pos();

		for (list<staff_t>::iterator it=staves.begin(); it!=staves.end(); it++)
			it->recalculate();

		redraw();
	}
}

int ScoreCanvas::canvas_width()
{
	//return tick_to_x(staves.begin()->itemlist.rbegin()->first); 
	return tick_to_x(SONG_LENGTH);
}

int ScoreCanvas::canvas_height()
{
	return staves.rbegin()->y_bottom;
}

int ScoreCanvas::viewport_width()
{
	return (width() - x_left);
}

int ScoreCanvas::viewport_height()
{
	return height();
}

string IntToStr(int i)
{
	ostringstream s;
	s<<i;
	return s.str();
}

QString IntToQStr(int i)
{
	return QString(IntToStr(i).c_str());
}

void color_image(QImage& img, const QColor& color)
{
	uchar* ptr=img.bits();
	int bytes=img.byteCount();
	int r,g,b;
	color.getRgb(&r,&g,&b);
	
	for (int i=0; i<bytes/4; i++)
	{
		QRgb* rgb=((QRgb*)ptr);
		(*rgb) = qRgba(r,g,b,qAlpha(*rgb));
		
		ptr+=4;
	}
}

void load_colored_pixmaps(QString file, QPixmap* array)
{
	QImage img(file);
		
	for (int color_index=0;color_index<NUM_MYCOLORS; color_index++)
	{
		color_image(img, mycolors[color_index]);
		array[color_index]=QPixmap::fromImage(img);
	}
}



void ScoreCanvas::init_pixmaps()
{
	if (!pixmaps_initalized)
	{
		mycolors=new QColor[NUM_MYCOLORS];
		
		mycolors[0]=Qt::black;
		for (int i=1;i<NUM_PARTCOLORS;i++)
			mycolors[i]=config.partColors[i];
		mycolors[BLACK_PIXMAP]=Qt::black;
		mycolors[HIGHLIGHTED_PIXMAP]=Qt::red;
		
		cout << "loading pixmaps..." << endl;
		
		pix_whole=new QPixmap[NUM_MYCOLORS];
		pix_half=new QPixmap[NUM_MYCOLORS];
		pix_quarter=new QPixmap[NUM_MYCOLORS];
		pix_dot=new QPixmap[NUM_MYCOLORS];
		pix_b=new QPixmap[NUM_MYCOLORS];
		pix_sharp=new QPixmap[NUM_MYCOLORS];
		pix_noacc=new QPixmap[NUM_MYCOLORS];
		pix_num=new QPixmap[10];
		
		pix_r1=new QPixmap;
		pix_r2=new QPixmap;
		pix_r4=new QPixmap;
		pix_r8=new QPixmap;
		pix_r16=new QPixmap;
		
		pix_clef_violin=new QPixmap;
		pix_clef_bass=new QPixmap;
		
		pix_flag_up=new QPixmap[4];
		pix_flag_down=new QPixmap[4];
		
		
		
		
		load_colored_pixmaps(museGlobalShare + "/scoreglyphs/whole.png", pix_whole);
		load_colored_pixmaps(museGlobalShare + "/scoreglyphs/half.png", pix_half);
		load_colored_pixmaps(museGlobalShare + "/scoreglyphs/quarter.png", pix_quarter);
		load_colored_pixmaps(museGlobalShare + "/scoreglyphs/dot.png", pix_dot);
		load_colored_pixmaps(museGlobalShare + "/scoreglyphs/acc_none.png", pix_noacc);
		load_colored_pixmaps(museGlobalShare + "/scoreglyphs/acc_sharp.png", pix_sharp);
		load_colored_pixmaps(museGlobalShare + "/scoreglyphs/acc_b.png", pix_b);

		pix_r1->load(museGlobalShare + "/scoreglyphs/rest1.png");
		pix_r2->load(museGlobalShare + "/scoreglyphs/rest2.png");
		pix_r4->load(museGlobalShare + "/scoreglyphs/rest4.png");
		pix_r8->load(museGlobalShare + "/scoreglyphs/rest8.png");
		pix_r16->load(museGlobalShare + "/scoreglyphs/rest16.png");
		pix_flag_up[0].load(museGlobalShare + "/scoreglyphs/flags8u.png");
		pix_flag_up[1].load(museGlobalShare + "/scoreglyphs/flags16u.png");
		pix_flag_up[2].load(museGlobalShare + "/scoreglyphs/flags32u.png");
		pix_flag_up[3].load(museGlobalShare + "/scoreglyphs/flags64u.png");
		pix_flag_down[0].load(museGlobalShare + "/scoreglyphs/flags8d.png");
		pix_flag_down[1].load(museGlobalShare + "/scoreglyphs/flags16d.png");
		pix_flag_down[2].load(museGlobalShare + "/scoreglyphs/flags32d.png");
		pix_flag_down[3].load(museGlobalShare + "/scoreglyphs/flags64d.png");
		
		pix_clef_violin->load(museGlobalShare + "/scoreglyphs/clef_violin_big.png");
		pix_clef_bass->load(museGlobalShare + "/scoreglyphs/clef_bass_big.png");
		
		for (int i=0;i<10;i++)
			pix_num[i].load(museGlobalShare + "/scoreglyphs/"+IntToQStr(i)+".png");
		
		pixmaps_initalized=true;
	}
}



int modulo(int a, int b) // similar to a % b
{
	return (((a%b)+b)%b);
}

int divide_floor(int a, int b) // similar to a / b
{ //TODO can be done better :/
	return int(floor(float(a)/float(b)));
}

#define DEFAULT_REST_HEIGHT 6 // TODO


bool operator< (const note_pos_t& a, const note_pos_t& b)
{
	if (a.height<b.height) return true;
	if (a.height>b.height) return false;
	return a.vorzeichen<b.vorzeichen;
}


 
int flo_quantize(int tick, int quant_ticks)
{
	//TODO quantizing must be done with the proper functions!
	return int(nearbyint((float)tick / quant_ticks))*quant_ticks;
}
 
int flo_quantize_floor(int tick, int quant_ticks)
{
	//TODO quantizing must be done with the proper functions, see above
	return int(tick / quant_ticks) * quant_ticks;
}

 
/* builds the event list used by the score editor.
 * that list contains only note-on and -off, time-sig- and
 * key-change events.
 * it stores them sorted by their time (quantized); if more
 * events with the same time occur, the NOTE-OFFs are
 * put before the NOTE-ONs
 * it only operates on parts which were selected in the
 * arranger when the score viewer was started
 * 
 * this abstracts the rest of the renderer from muse's internal
 * data structures, making this easy to port to another application
 */
void staff_t::create_appropriate_eventlist()
{
	using AL::sigmap;
	using AL::iSigEvent;

	eventlist.clear();

	// phase one: fill the list -----------------------------------------
	
	//insert note on events
	for (set<Part*>::const_iterator part_it=parts.begin(); part_it!=parts.end(); part_it++)
	{
		Part* part=*part_it;
		EventList* el=part->events();
		
		for (iEvent it=el->begin(); it!=el->end(); it++)
		{
			Event& event=it->second;
			
			if ( (event.isNote() && !event.isNoteOff()) &&
			     ( ((type==GRAND_TOP) && (event.pitch() >= split_note)) ||
			       ((type==GRAND_BOTTOM) && (event.pitch() < split_note)) ||
			       (type==NORMAL) )                          )
			{
				unsigned begin, end;
				begin=flo_quantize(event.tick()+part->tick(), parent->quant_ticks());
				end=flo_quantize(event.endTick()+part->tick(), parent->quant_ticks());
				cout <<"inserting note on at "<<begin<<" with pitch="<<event.pitch()<<" and len="<<end-begin<<endl;
				eventlist.insert(pair<unsigned, FloEvent>(begin, FloEvent(begin,event.pitch(), event.velo(),end-begin,FloEvent::NOTE_ON,part,&it->second)));
			}
			//else ignore it
		}
	}

	//insert bars and time signatures
	for (iSigEvent it=sigmap.begin(); it!=sigmap.end(); it++)
	{
		unsigned from=it->second->tick;
		unsigned to=it->first;
		unsigned ticks_per_measure=sigmap.ticksMeasure(it->second->tick);
		
		if (to > unsigned(SONG_LENGTH))
		{
			cout << "time signature's end-of-validness is outside of our song, limiting it." << endl;
			to=SONG_LENGTH;
		}
		
		cout << "new signature from tick "<<from<<" to " << to << ": "<<it->second->sig.z<<"/"<<it->second->sig.n<<"; ticks per measure = "<<ticks_per_measure<<endl;
		eventlist.insert(pair<unsigned, FloEvent>(from,  FloEvent(from, FloEvent::TIME_SIG, it->second->sig.z, it->second->sig.n) ) );
		for (unsigned t=from; t<to; t+=ticks_per_measure)
			eventlist.insert(pair<unsigned, FloEvent>(t,  FloEvent(t,0,0,ticks_per_measure,FloEvent::BAR) ) );
	}

	//insert key changes
	for (iKeyEvent it=keymap.begin(); it!=keymap.end(); it++)
		eventlist.insert(pair<unsigned, FloEvent>(it->second.tick,  FloEvent(it->second.tick,FloEvent::KEY_CHANGE, it->second.key ) ) );
	
	
	// phase two: deal with overlapping notes ---------------------------
	ScoreEventList::iterator it, it2;
	
	//iterate through all note_on - events
	for (it=eventlist.begin(); it!=eventlist.end(); it++)
		if (it->second.type==FloEvent::NOTE_ON)
		{
			unsigned end_tick=it->first + it->second.len;
			
			//iterate though all (relevant) later note_ons which are
			//at the same pitch. if there's a collision, shorten it's len
			for (it2=it, it2++; it2!=eventlist.end() && it2->first < end_tick; it2++)
				if ((it2->second.type==FloEvent::NOTE_ON) && (it2->second.pitch == it->second.pitch))
					it->second.len=it2->first - it->first;
		}
		
		
		// phase three: eliminate zero-length-notes -------------------------
		for (it=eventlist.begin(); it!=eventlist.end();)
			if ((it->second.type==FloEvent::NOTE_ON) && (it->second.len<=0))
				eventlist.erase(it++);
			else
				it++;
}


bool is_sharp_key(key_enum t)
{
	return ((t>=KEY_SHARP_BEGIN) && (t<=KEY_SHARP_END));
}
bool is_b_key(key_enum t)
{
	return ((t>=KEY_B_BEGIN) && (t<=KEY_B_END));
}

int n_accidentials(key_enum t)
{
	if (is_sharp_key(t))
		return t-KEY_SHARP_BEGIN-1;
	else
		return t-KEY_B_BEGIN-1;
}


//note needs to be 0..11
//always assumes violin clef
//only for internal use
note_pos_t note_pos_(int note, key_enum key)
{
	note_pos_t result;
	           //C CIS D DIS E F FIS G GIS A AIS H
	int foo[12]={0,-1, 1,-1, 2,3,-1, 4,-1, 5, -1,6};
	
	if ((note<0) || (note>=12))
		cout << "WARNING: ILLEGAL FUNCTION CALL (note_pos, note out of range)" << endl;
	
	if (foo[note]!=-1)
	{
		result.height=foo[note];
		result.vorzeichen=NONE;
	}
	else
	{
		if (is_sharp_key(key))
		{
			result.height=foo[note-1];
			result.vorzeichen=SHARP;
		}
		else // if is_b_key
		{
			result.height=foo[note+1];
			result.vorzeichen=B;			
		}
	}
	
	// Special cases for GES / FIS keys
	if (key==KEY_GES)
	{
		// convert a H to a Ces
		if (note==11)
		{
			result.height=12; 
			result.vorzeichen=B;
		}
	}
	else if (key==KEY_FIS)
	{
		// convert a F to an Eis
		if (note==5)
		{
			result.height=2;
			result.vorzeichen=SHARP;
		}
	}

	return result;
}


//  V   --------------------------  <-- height=10
//  I C --------------------------  <-- height=8
//  O L --------------------------  <-- height=6
//  L E --------------------------  <-- height=4
//  I F --------------------------  <-- height=2
//  N    --o--                      <-- this is C4. height=0

// the "spaces" in between the lines have odd numbers.
// that is, the space between line 2 and 4 is numbered 3.

// these numbers do not change when clef changes. line 2
// is always the "bottom line" of the system.
// in violin clef, line 2 is E4
// in bass clef, line 2 is G2

note_pos_t note_pos (unsigned note, key_enum key, clef_t clef)
{
	int octave=(note/12)-1; //integer division. note is unsigned
	note=note%12;
	
	//now octave contains the octave the note is in
	//(A4 is the 440Hz tone. C4 is the "low C" in the violin clef
	//and the "high C" in the bass clef.
	//note contains 0 for C, 1 for Cis, ..., 11 for H (or B if you're not german)
	
	note_pos_t pos=note_pos_(note,key);
	
	switch (clef) //CLEF_MARKER
	{
		case VIOLIN:
			pos.height=pos.height + (octave-4)*7;
			break;
			
		case BASS:
			pos.height=pos.height + (octave-3)*7 + 5;
			break;
	}
	
	return pos;
}


int calc_len(int l, int d)
{
	// l=0,1,2 -> whole, half, quarter (think of 2^0, 2^1, 2^2)
	// d=number of dots
	
	int tmp=0;
	for (int i=0;i<=d;i++)
		tmp+=TICKS_PER_WHOLE / (1 << (l+i));
	
	return tmp;
}

bool operator< (const note_len_t& a,const note_len_t& b) //TODO sane sorting order
{
	if (a.len<b.len) return true;
	else if (a.dots<b.dots) return true;
	else return false;
}



int calc_measure_len(const list<int>& nums, int denom)
{
	int sum=0;
	
	for (list<int>::const_iterator it=nums.begin(); it!=nums.end(); it++)
		sum+=*it;
	
	return 64* sum/denom;
}

vector<int> create_emphasize_list(const list<int>& nums, int denom)
{
	cout << "creating emphasize list for ";
	for (list<int>::const_iterator it=nums.begin(); it!=nums.end(); it++)
		cout << *it << " ";
	cout << "/ "<<denom;
	
	//        |----- 8th -----|
	int foo[]={4,7,6,7,5,7,6,7}; //if 64 changes, this also must change
	int pos=0;
	int len=calc_measure_len(nums, denom);

	vector<int> result(len);
	
	for (int i=0;i<len;i++)
		result[i]=foo[i%8];
	
	for (list<int>::const_iterator it=nums.begin(); it!=nums.end(); it++)
	{
		result[pos]=1;
		for (int i=1;i<*it;i++)
			result[pos + i*64/denom]=2;
		pos+= *it * 64 / denom;
	}
	
	result[0]=0;
	
	for (int i=0;i<len;i++)
	{
		if (i%8==0)
			cout << endl<<i<<":\t";
		cout << result[i]<<" ";
	}
	cout << endl;
	
	return result;
}

vector<int> create_emphasize_list(int num, int denom)
{
	list<int> nums;
	
	if (num%3 ==0)
	{
		for (int i=0;i<num/3;i++)
			nums.push_back(3);
	}
	else if (num%2 ==0)
	{
		for (int i=0;i<num/2;i++)
			nums.push_back(2);
	}
	else // num is odd
	{
		for (int i=0;i<(num-3)/2;i++)
			nums.push_back(2);
		
		nums.push_back(3);
	}
	
	return create_emphasize_list(nums, denom);
}

//quant_power2 must be in log(len), that is
//whole, half, quarter, eighth = 0,1,2,3
//NOT:  1,2,4,8! (think of 2^foo)
//len is in ticks
list<note_len_t> parse_note_len(int len_ticks, int begin_tick, vector<int>& foo, int quant_power2, bool allow_dots, bool allow_normal)
{
	list<note_len_t> retval;
	
	if (len_ticks<0)
		cout << "WARNING: ILLEGAL FUNCTION CALL in parse_note_len: len_ticks < 0" << endl;
	if (begin_tick<0)
		cout << "WARNING: ILLEGAL FUNCTION CALL in parse_note_len: begin_tick < 0" << endl;
	
	if (allow_normal)
	{
		int dot_max = allow_dots ? quant_power2 : 0;
		
		for (int i=0;i<=quant_power2;i++)
			for (int j=0;j<=dot_max-i;j++)
				if (calc_len(i,j) == len_ticks)
				{
					retval.push_back(note_len_t (i,j));
					return retval;
				}
	}
	
	//if !allow_normal or if the above failed
	
	int begin=begin_tick * 64 / TICKS_PER_WHOLE;
	int len=len_ticks * 64 / TICKS_PER_WHOLE;
	
	unsigned pos=begin;
	int len_done=0;
	
	while (len_done<len)
	{
		int len_now=0;
		int last_number=foo[pos];
		
		do {pos++;len_done++;len_now++;} while (! ((foo[pos]<=last_number) || (len_done==len) || (pos==foo.size())) );

		len_now=len_now*TICKS_PER_WHOLE/64;

		cout << "add " << len_now << " ticks" << endl;
		if (allow_dots)
		{
			for (int i=0;i<=quant_power2;i++)
				for (int j=0;j<=quant_power2-i;j++)
					if (calc_len(i,j) == len_now)
					{
						retval.push_back(note_len_t (i,j));
						len_now=0;
					}
		}
			
		if (len_now) //the above failed or allow_dots=false
		{
			for (int i=0; i<=quant_power2; i++)
			{
				int tmp=calc_len(i,0);
				if (tmp <= len_now)
				{
					retval.push_back(note_len_t(i));
					len_now-=tmp;
					if (len_now==0) break;
				}
			}
		}
		
		if (len_now!=0)
			cout << "WARNING: THIS SHOULD NEVER HAPPEN. wasn't able to split note len properly; len_now="<<len_now << endl;

		if (pos==foo.size()) //we cross measure boundaries?
			pos=0;
	}

	
	return retval;
}


#define YLEN 10
#define NOTE_SHIFT 3

#define REST_AUSWEICH_X 10
#define DOT_XDIST 6
#define DOT_XBEGIN 10
#define DOT_XBEGIN_REST 10

#define NUMBER_HEIGHT (pix_num[0].height())

//kann 0 oder 1 sein:
//bei notenkollisionen mit ungerader anzahl von kollidierenden
//wird immer so ausgewichen, dass möglichst wenige ausweichen müssen
//wenn die anzahl aber gerade ist, gibt es keine "bessere" lösung
//in dem fall werden immer die geraden (0) bzw. ungeraden (1)
//ausweichen.
#define AUSWEICHEN_BEVORZUGT 0

#define STEM_LEN 30

#define DOTTED_RESTS true
#define UNSPLIT_RESTS false

#define AUX_LINE_LEN 1.5

#define ACCIDENTIAL_DIST 11
#define KEYCHANGE_ACC_DIST 9
#define KEYCHANGE_ACC_LEFTDIST 9
#define KEYCHANGE_ACC_RIGHTDIST 0


#define no_notepos note_pos_t()

#define TIE_DIST 5
#define TIE_HEIGHT 6
#define TIE_THICKNESS 3

void ScoreCanvas::draw_tie (QPainter& p, int x1, int x4, int yo, bool up, QColor color)
{
	QPainterPath path;

	int y1, y2, y3;

	if (up)
	{
		y1 = yo - TIE_DIST;
		y2 = y1 - TIE_HEIGHT;
		y3=y2-TIE_THICKNESS;
	}
	else 
	{
		y1 = yo + TIE_DIST;
		y2 = y1 + TIE_HEIGHT;
		y3=y2+TIE_THICKNESS;
	}
	
	int x2 = x1 + (x4-x1)/4;
	int x3 = x4 - (x4-x1)/4;
	
	path.moveTo(x1,y1);
	path.cubicTo( x2,y2  ,  x3,y2  ,  x4,y1 );
	path.cubicTo( x3,y3  ,  x2,y3  ,  x1,y1 );
	
	p.setPen(color);
 	p.setBrush(color);

	p.drawPath(path);
}

void ScoreCanvas::draw_accidentials(QPainter& p, int x, int y_offset, const list<int>& acc_list, const QPixmap& pix)
{
	int n_acc_drawn=0;
	
	for (list<int>::const_iterator acc_it=acc_list.begin(); acc_it!=acc_list.end(); acc_it++)
	{
		int y_coord=2*YLEN  -  ( *acc_it -2)*YLEN/2;
		draw_pixmap(p,x + n_acc_drawn*KEYCHANGE_ACC_DIST,y_offset + y_coord,pix);
		n_acc_drawn++;
	}
}

void staff_t::create_itemlist()
{
	key_enum tmp_key=KEY_C;
	int lastevent=0;
	int next_measure=-1;
	int last_measure=-1;
	vector<int> emphasize_list=create_emphasize_list(4,4); //actually unneccessary, for safety

	itemlist.clear();

	for (ScoreEventList::iterator it=eventlist.begin(); it!=eventlist.end(); it++)
	{
		int t, pitch, len, velo, actual_tick;
		FloEvent::typeEnum type;
		t=it->first;
		pitch=it->second.pitch;
		velo=it->second.vel;
		len=it->second.len;
		type=it->second.type;
		actual_tick=it->second.tick;
		if (actual_tick==-1) actual_tick=t;
		
		note_pos_t notepos=note_pos(pitch,tmp_key,clef);
		
		printf("FLO: t=%i\ttype=%i\tpitch=%i\tvel=%i\tlen=%i\n",it->first, it->second.type, it->second.pitch, it->second.vel, it->second.len);
		cout << "\tline="<<notepos.height<<"\tvorzeichen="<<notepos.vorzeichen << endl;
		
				
		if (type==FloEvent::BAR)
		{
			if (last_measure!=-1) //i.e.: "this is NOT the first bar"
			{
				if (lastevent==last_measure) //there was no note?
				{
					unsigned tmppos=(last_measure+t-parent->quant_ticks())/2;
					cout << "\tend-of-measure: this was an empty measure. inserting rest in between at t="<<tmppos << endl;
					itemlist[tmppos].insert( FloItem(FloItem::REST,notepos,0,0) );
					itemlist[t].insert( FloItem(FloItem::REST_END,notepos,0,0) );
				}
				else
				{
					// if neccessary, insert rest at between last note and end-of-measure
					int rest=t-lastevent;
					if (rest)
					{
						printf("\tend-of-measure: set rest at %i with len %i\n",lastevent,rest);
						
						list<note_len_t> lens=parse_note_len(rest,lastevent-last_measure,emphasize_list,parent->quant_power2(),DOTTED_RESTS,UNSPLIT_RESTS);
						unsigned tmppos=lastevent;
						for (list<note_len_t>::iterator x=lens.begin(); x!=lens.end(); x++)
						{
							cout << "\t\tpartial rest with len="<<x->len<<", dots="<<x->dots<<endl;
							itemlist[tmppos].insert( FloItem(FloItem::REST,notepos,x->len,x->dots) );
							tmppos+=calc_len(x->len,x->dots);
							itemlist[tmppos].insert( FloItem(FloItem::REST_END,notepos,0,0) );
						}
					}
				}
			}
						
			lastevent=t;
			last_measure=t;
			next_measure=t+len;
			
			itemlist[t].insert( FloItem(FloItem::BAR,no_notepos,0,0) );
		}
		else if (type==FloEvent::NOTE_ON)
		{
			int rest=t-lastevent;
			if (rest)
			{
				printf("\tset rest at %i with len %i\n",lastevent,rest);
				// no need to check if the rest crosses measure boundaries;
				// it can't.
				
				list<note_len_t> lens=parse_note_len(rest,lastevent-last_measure,emphasize_list,parent->quant_power2(),DOTTED_RESTS,UNSPLIT_RESTS);
				unsigned tmppos=lastevent;
				for (list<note_len_t>::iterator x=lens.begin(); x!=lens.end(); x++)
				{
					cout << "\t\tpartial rest with len="<<x->len<<", dots="<<x->dots<<endl;
					itemlist[tmppos].insert( FloItem(FloItem::REST,notepos,x->len,x->dots) );
					tmppos+=calc_len(x->len,x->dots);
					itemlist[tmppos].insert( FloItem(FloItem::REST_END,notepos,0,0) );
				}
			}
			
			
			
			printf("\tset note at %i with len=%i\n", t, len);

			int tmplen;
			bool tied_note;

			// if the note exceeds the current measure, split it.
			if (t+len>next_measure)
			{
				tmplen=next_measure-t;
				tied_note=true;
				
				//append the "remainder" of the note to our EventList, so that
				//it gets processed again when entering the new measure
				int newlen=len-tmplen;
				eventlist.insert(pair<unsigned, FloEvent>(next_measure, FloEvent(actual_tick,pitch, velo,0,FloEvent::NOTE_OFF, it->second.source_part, it->second.source_event)));
				eventlist.insert(pair<unsigned, FloEvent>(next_measure, FloEvent(actual_tick,pitch, velo,newlen,FloEvent::NOTE_ON, it->second.source_part, it->second.source_event)));

				cout << "\t\tnote was split to length "<<tmplen<<" + " << newlen<<endl;
			}
			else
			{
				tmplen=len;
				tied_note=false;
				
				cout << "\t\tinserting NOTE OFF at "<<t+len<<endl;
				eventlist.insert(pair<unsigned, FloEvent>(t+len,   FloEvent(t+len,pitch, velo,0,FloEvent::NOTE_OFF,it->second.source_part, it->second.source_event)));
			}
							
			list<note_len_t> lens=parse_note_len(tmplen,t-last_measure,emphasize_list,parent->quant_power2(),true,true);
			unsigned tmppos=t;
			int n_lens=lens.size();
			int count=0;			
			for (list<note_len_t>::iterator x=lens.begin(); x!=lens.end(); x++)
			{
				cout << "\t\tpartial note with len="<<x->len<<", dots="<<x->dots<<endl;
				count++;
				
				bool tie;
				
				if (count<n_lens)
					tie=true;      // all notes except the last are always tied
				else
					tie=tied_note; // only the last respects tied_note
				
				itemlist[tmppos].insert( FloItem(FloItem::NOTE,notepos,x->len,x->dots, tie, actual_tick, it->second.source_part, it->second.source_event) );
				tmppos+=calc_len(x->len,x->dots);
				itemlist[tmppos].insert( FloItem(FloItem::NOTE_END,notepos,0,0) );
			}
		}
		else if (type==FloEvent::NOTE_OFF)
		{
			lastevent=t;
		}
		else if (type==FloEvent::TIME_SIG)
		{
			cout << "inserting TIME SIGNATURE "<<it->second.num<<"/"<<it->second.denom<<" at "<<t<<endl;
			itemlist[t].insert( FloItem(FloItem::TIME_SIG, it->second.num, it->second.denom) );
			
			emphasize_list=create_emphasize_list(it->second.num, it->second.denom);
		}
		else if (type==FloEvent::KEY_CHANGE)
		{
			cout << "inserting KEY CHANGE ("<<it->second.key<<") at "<<t<<endl;
			itemlist[t].insert( FloItem(FloItem::KEY_CHANGE, it->second.key) );
			tmp_key=it->second.key;
		}
	}	
}

void staff_t::process_itemlist()
{
	map<int,int> occupied;
	int last_measure=0;
	vector<int> emphasize_list=create_emphasize_list(4,4); //unneccessary, only for safety

	//iterate through all times with items
	for (ScoreItemList::iterator it2=itemlist.begin(); it2!=itemlist.end(); it2++)
	{
		set<FloItem, floComp>& curr_items=it2->second;
		
		cout << "at t="<<it2->first<<endl;
		
		// phase 0: keep track of active notes, rests -------------------
		//          (and occupied lines) and the last measure
		//          and the current time signature
		for (set<FloItem, floComp>::iterator it=curr_items.begin(); it!=curr_items.end(); it++)
		{
			if ((it->type==FloItem::NOTE) || (it->type==FloItem::REST))
				occupied[it->pos.height]++;
			else if ((it->type==FloItem::NOTE_END) || (it->type==FloItem::REST_END))
				occupied[it->pos.height]--;
			else if (it->type==FloItem::BAR)
				last_measure=it2->first;
			else if (it->type==FloItem::TIME_SIG)
				emphasize_list=create_emphasize_list(it->num, it->denom);
		}
		
		cout << "occupied: ";
		for (map<int,int>::iterator i=occupied.begin(); i!=occupied.end(); i++)
			if (i->second) cout << i->first << "("<<i->second<<")   ";
		cout << endl;
		
		
		
		
		
		// phase 1: group rests together -----------------------------------
		int n_groups=0;
		bool dont_group=false;
		
		//iterate through all rests R at that time
		//  iterate through all rests X at that time below R
		//    if something is between X and R ("barrier"), stop
		//    else: group them together
		for (set<FloItem, floComp>::iterator it=curr_items.begin(); it!=curr_items.end();)
		{
			//only operate on rests; ignore rests which are created by this code
			//(can be seen on already_grouped)
			if ((it->type==FloItem::REST) && (it->already_grouped==false))
			{
				cout << "trying to group" << endl;
				
				int lastheight;
				int height_cumulative=0;
				int counter=0;
				
				lastheight=it->pos.height;
				
				set<FloItem, floComp>::iterator tmp;
				for (tmp=it; tmp!=curr_items.end();)
				{
					cout << "checking if we can proceed with an item at height="<<tmp->pos.height<<endl;
					
					for (int i=lastheight+1; i<=tmp->pos.height-1; i++)
						if (occupied[i]!=0)
						{
							cout << "we can NOT, because occ["<<i<<"] != 0" << endl;
							//stop grouping that rest
							goto get_out_here;
						}
					
					lastheight=tmp->pos.height;
					
					// the current item is a rest with equal len? cool!
					if (tmp->type==FloItem::REST && tmp->len==it->len && tmp->dots==it->dots)
					{
						// füge diese pause zur gruppe dazu und entferne sie von diesem set hier
						// entfernen aber nur, wenn sie nicht it, also die erste pause ist, die brauchen wir noch!
						cout << "\tgrouping rest at height="<<tmp->pos.height<<endl;
						height_cumulative+=tmp->pos.height;
						counter++;
						if (tmp!=it)
							curr_items.erase(tmp++);
						else
							tmp++;
					}
					else //it's something else? well, we can stop grouping that rest then
					{
						cout << "we can NOT, because that item is not a rest" << endl;
						//stop grouping that rest
						goto get_out_here;
					}
				}
				cout << "no items to proceed on left, continuing" << endl;
				get_out_here:
				
				n_groups++;
				
				// entferne it vom set und
				// füge eine pause mit dem "mittelwert" ein.
				// occupied und die "_END"-events bleiben unberührt

				FloItem temp=*it;
				temp.already_grouped=true;
				
				// have we grouped all available rests into one single?
				if ( (n_groups==1) && (tmp==curr_items.end()) && !dont_group)
				{
					cout << "wow, we were able to group all rests into one single" << endl;
					if (temp.len==0) //the whole rest is shifted one line (one space and one line)
						temp.pos.height=DEFAULT_REST_HEIGHT+2;
					else
						temp.pos.height=DEFAULT_REST_HEIGHT;
				}
				else
				{
					cout << "creating group #"<<n_groups<<endl;
					temp.pos.height=nearbyint((float)height_cumulative/counter);
				}
				
				// do NOT first insert, then erase, because if temp.height ==
				// it->height, the set considers temp and it equal (it doesn't
				// take already_grouped into account)
				// the result of this: insert does nothing, and erase erases
				// the item. effect: you don't have the rest at all
				curr_items.erase(it++);
				
				cout << "replacing all grouped rests with a rest at height="<<temp.pos.height<<endl;
				
				curr_items.insert(temp);
			}
			else
			{
				if (it->type==FloItem::NOTE)
					dont_group=true;
				
				it++;
			}
		}
	
		
		
		
		
		// phase 2: avoid collisions of items ------------------------------
		set<FloItem, floComp>::iterator lastit, groupbegin, invalid;
		invalid=curr_items.end();
		lastit=invalid;
		groupbegin=invalid;
		int count;
		
		//TODO FINDMICH MARKER: is "grouping" notes and rests together okay?
		// or is it better to ignore rests when grouping?
		for (set<FloItem, floComp>::iterator it=curr_items.begin(); it!=curr_items.end(); it++)
			if ( (it->type==FloItem::NOTE) || (it->type==FloItem::REST) )
			{
				if (lastit != invalid)
				{
					if (it->pos.height == lastit->pos.height+1) // they would collide?
					{
						if (groupbegin==invalid) // we have no group atm?
						{
							groupbegin=lastit;     // start a new group
							count=1; // because lastit has to be taken into account.
							         // for "it", there's a count++ later
						}

						// the following will work even on start-new-group, 
						// because lastit will be "untouched", and that's why 
						// still be initalized to "false"
						it->ausweich=!lastit->ausweich;

						count++;
					}
					else
					{
						if (groupbegin!=invalid) //this is the first item which 
						{												 //doesn't belong to the previous group any more
							if (count%2 == 0) //count is even?
								if (modulo(groupbegin->pos.height, 2) == AUSWEICHEN_BEVORZUGT)
									for (set<FloItem, floComp>::iterator tmp=groupbegin; tmp!=it; tmp++)
										tmp->ausweich=!tmp->ausweich;
							
							groupbegin=invalid;
						}
						// else: everything is ok :)
					}
				}
				
				lastit=it;
			}
			
			// this could be the case if the last processed item before end()
			// still belonged to a group. finalize this last group as well:
			if (groupbegin!=invalid) 
			{
				if (count%2 == 0) //count is even?
					if (modulo(groupbegin->pos.height, 2) == AUSWEICHEN_BEVORZUGT)
						for (set<FloItem, floComp>::iterator tmp=groupbegin; tmp!=curr_items.end(); tmp++)
							tmp->ausweich=!tmp->ausweich;
			}
			// else: everything is ok :)
	
	
	
	
	
		// phase 3: group notes by their length and ------------------------
		//          find out appropriate stem directions
group_them_again:
		map<int, cumulative_t> lengths;
		bool has_whole=false;
		
		// find out which note lengths are present at that time
		for (set<FloItem, floComp>::iterator it=curr_items.begin(); it!=curr_items.end(); it++)
			if (it->type==FloItem::NOTE)
				lengths[it->len].add(it->pos.height);
		
		cout << "note lengths at that time are:";
		for (map<int, cumulative_t>::iterator it=lengths.begin(); it!=lengths.end(); it++)
			cout << it->first << "("<< it->second.mean() <<")  ";
		cout << endl;
		
		if (lengths.erase(0)) // in case "0" is in the set, erase it		
			has_whole=true;     // but remember there were whole notes
		
		if (lengths.size()==0)
		{
			cout << "no notes other than wholes, or no notes at all. we can relax" << endl;
		}
		else if (lengths.size()==1)
		{
			pair<const int, cumulative_t>& group=*(lengths.begin());
			stem_t stem;
			int shift=0;
			cout << "only one non-whole note group (len="<<group.first<<") at height="<<group.second.mean()<< endl;
			
			if (group.second.mean()>=6)
			{
				stem=DOWNWARDS;
				if (has_whole)
					shift=-1;
			}
			else
			{
				stem=UPWARDS;
				if (has_whole)
					shift=1;
			}
			
			// for each note in that group
			for (set<FloItem, floComp>::iterator it=curr_items.begin(); it!=curr_items.end(); it++)
				if ( (it->type==FloItem::NOTE) && (it->len==group.first) )
				{
					it->stem=stem;
					it->shift=shift;
				}
		}
		else if (lengths.size()==2)
		{
			map<int, cumulative_t>::iterator it=lengths.begin();
			pair<const int, cumulative_t>& group1=*it;
			it++;
			pair<const int, cumulative_t>& group2=*it;
			stem_t stem1, stem2;
			int shift1=0, shift2=0;
			cout << "two non-whole note group: len="<<group1.first<<" at height="<<group1.second.mean()<<"  and len="<<group2.first<<" at height="<<group2.second.mean()<< endl;
			
			if (group1.second.mean()<group2.second.mean())
			{
				stem1=DOWNWARDS;
				stem2=UPWARDS;
				shift1=-1;
				if (has_whole)
					shift2=1;
			}
			else
			{
				stem1=UPWARDS;
				stem2=DOWNWARDS;
				shift2=-1;
				if (has_whole)
					shift1=1;
			}
			
			// for each note in group1
			for (set<FloItem, floComp>::iterator it=curr_items.begin(); it!=curr_items.end(); it++)
				if ( (it->type==FloItem::NOTE) && (it->len==group1.first) )
				{
					it->stem=stem1;
					it->shift=shift1;
				}

			// for each note in group2
			for (set<FloItem, floComp>::iterator it=curr_items.begin(); it!=curr_items.end(); it++)
				if ( (it->type==FloItem::NOTE) && (it->len==group2.first) )
				{
					it->stem=stem2;
					it->shift=shift2;
				}
		}
		else //more than 2 groups
		{
			//at this time, there are no iterators pointing to curr_items.
			//this means, we can erase and insert safely into curr_items here.
			
			//group1 contains the longer notes, group2 the shorter
			
			int group1_n=lengths.size()/2; //round down
			int group2_n=lengths.size()-group1_n;
			
			int group1_len, group2_len;
			int group1_len_ticks, group2_len_ticks;
			

			map<int, cumulative_t>::iterator lit=lengths.begin();
			for (int i=0;i<group1_n-1;i++) lit++; //go to the group1_n-th entry
			group1_len=lit->first;
			for (int i=0;i<group2_n;i++) lit++;  //go to the (group1_n+group2_n)-th entry (i.e., the last before end() )
			group2_len=lit->first;
			
			group1_len_ticks=calc_len(group1_len,0);
			group2_len_ticks=calc_len(group2_len,0);
			
			cout << "we have "<<lengths.size()<<" groups. putting the "<<group1_n<<" longest and the "<<group2_n<<"shortest groups together"<<endl;
			cout << "\tgroup1 will have len="<<group1_len<<" ("<<group1_len_ticks<<" ticks), group2 will have len="<<group2_len<<" ("<<group2_len_ticks<<" ticks)"<<endl;
			
			for (set<FloItem, floComp>::iterator it=curr_items.begin(); it!=curr_items.end();)
				if (it->type==FloItem::NOTE)
				{
					//if *it belongs to group1 and has not already its destination length
					cout << "\tprocessing note-item with len="<<it->len<<endl;
					if (it->len<group1_len)
					{
						cout << "\t\thas to be changed to fit into group 1" << endl;
						FloItem tmp=*it;
						curr_items.erase(it++);

						int len_ticks_remaining=calc_len(tmp.len, tmp.dots)-group1_len_ticks;
						bool tied_note=tmp.tied;
						
						
						//shorten the current item to it's group's length
						tmp.len=group1_len;
						tmp.dots=0;
						tmp.tied=true;
						curr_items.insert(tmp);
						
						//create items for the remaining lengths (and a note_END for the just created shortened note)
						int t=it2->first+group1_len_ticks;

						itemlist[t].insert( FloItem(FloItem::NOTE_END,tmp.pos,0,0) );
						
						list<note_len_t> lens=parse_note_len(len_ticks_remaining,t-last_measure,emphasize_list,parent->quant_power2(),true,true);
						unsigned tmppos=t;
						int n_lens=lens.size();
						int count=0;			
						for (list<note_len_t>::iterator x=lens.begin(); x!=lens.end(); x++)
						{
							cout << "\t\twhile regrouping: partial note with len="<<x->len<<", dots="<<x->dots<<endl;
							count++;
							
							bool tie;
							
							if (count<n_lens)
								tie=true;      // all notes except the last are always tied
							else
								tie=tied_note; // only the last respects tied_note
							
							itemlist[tmppos].insert( FloItem(FloItem::NOTE, tmp.pos,x->len,x->dots, tie, tmp.begin_tick, tmp.source_part, tmp.source_event) );
							tmppos+=calc_len(x->len,x->dots);
							itemlist[tmppos].insert( FloItem(FloItem::NOTE_END, tmp.pos,0,0) );
						}

					}
					//else if *it belongs to group2 and has not already its destination length
					else if ((it->len<group2_len) && (it->len>group1_len))
					{
						cout << "\t\thas to be changed to fit into group 2" << endl;
						
						FloItem tmp=*it;
						curr_items.erase(it++);

						int len_ticks_remaining=calc_len(tmp.len, tmp.dots)-group2_len_ticks;
						bool tied_note=tmp.tied;
						
						
						//shorten the current item to it's group's length
						tmp.len=group2_len;
						tmp.dots=0;
						tmp.tied=true;
						curr_items.insert(tmp);
						
						//create items for the remaining lengths (and a note_END for the just created shortened note)
						int t=it2->first+group2_len_ticks;

						itemlist[t].insert( FloItem(FloItem::NOTE_END,tmp.pos,0,0) );
						
						list<note_len_t> lens=parse_note_len(len_ticks_remaining,t-last_measure,emphasize_list,parent->quant_power2(),true,true);
						unsigned tmppos=t;
						int n_lens=lens.size();
						int count=0;			
						for (list<note_len_t>::iterator x=lens.begin(); x!=lens.end(); x++)
						{
							cout << "\t\twhile regrouping: partial note with len="<<x->len<<", dots="<<x->dots<<endl;
							count++;
							
							bool tie;
							
							if (count<n_lens)
								tie=true;      // all notes except the last are always tied
							else
								tie=tied_note; // only the last respects tied_note
							
							itemlist[tmppos].insert( FloItem(FloItem::NOTE,tmp.pos,x->len,x->dots, tie, tmp.begin_tick, tmp.source_part, tmp.source_event) );
							tmppos+=calc_len(x->len,x->dots);
							itemlist[tmppos].insert( FloItem(FloItem::NOTE_END,tmp.pos,0,0) );
						}

					}
					else //nothing to do?
					{
						cout << "\t\tnothing to do" << endl;
						it++;
					}
				}
				else
					it++;
			
			goto group_them_again; //do it again
		}

	}
}

//draw a pixmap centered
void ScoreCanvas::draw_pixmap(QPainter& p, int x, int y, const QPixmap& pm)
{
	cout << "drawing pixmap width size="<<pm.width()<<"/"<<pm.height()<<" at "<<x<<"/"<<y<<endl;
	p.drawPixmap(x-pm.width()/2,y-pm.height()/2,pm);
}

QRect bbox_center(int x, int y, const QSize& size)
{
	//why x-foo/2+foo? because due to integer divisions,
	// x-foo/2+foo can be smaller than x+foo/2!
	return QRect(x-size.width()/2,y-size.height()/2,size.width(),size.height());
}

QRect FloItem::bbox() const
{
	return bbox_center(x,y,pix->size());
}

void ScoreCanvas::draw_note_lines(QPainter& p, int y)
{
	int xend=width();
	
	p.setPen(Qt::black);
	
	for (int i=0;i<5;i++)
		p.drawLine(0,y + i*YLEN - 2*YLEN,xend,y + i*YLEN - 2*YLEN);
}


void staff_t::calc_item_pos()
{
	key_enum curr_key=KEY_C; //this has to be KEY_C or KEY_C_B and nothing else,
	                         //because only with these two keys the next (initial)
	                         //key signature is properly drawn.
	int pos_add=0;
	
	for (ScoreItemList::iterator it2=itemlist.begin(); it2!=itemlist.end(); it2++)
	{
		for (set<FloItem, floComp>::iterator it=it2->second.begin(); it!=it2->second.end();it++)
		{
			it->x=it2->first * parent->pixels_per_whole()/TICKS_PER_WHOLE  +pos_add;
			//if this changes, also change the line(s) with YLEN (but not all). don't change it.
			it->y=2*YLEN  -  (it->pos.height-2)*YLEN/2;
			
			if (it->type==FloItem::NOTE)
			{
				it->x+=parent->note_x_indent() + it->shift*NOTE_SHIFT;
				
				switch (it->len)
				{
					case 0: it->pix=pix_whole; break;
					case 1: it->pix=pix_half; break;
					default: it->pix=pix_quarter; break;
				}
				
				it->stem_x=it->x;
				
				if (it->ausweich)
				{
					if ((it->stem==UPWARDS) || (it->len==0))
						it->x += it->pix->width()-1; //AUSWEICH_X
					else
						it->x -= it->pix->width()-1; //AUSWEICH_X
				}
				
				//if there's a tie, try to find the tie's destination and set is_tie_dest
				if (it->tied)
				{
					set<FloItem, floComp>::iterator dest;
					set<FloItem, floComp>& desttime = itemlist[it2->first+calc_len(it->len,it->dots)];
					for (dest=desttime.begin(); dest!=desttime.end();dest++)
						if ((dest->type==FloItem::NOTE) && (dest->pos==it->pos))
						{
							dest->is_tie_dest=true;
							dest->tie_from_x=it->x;
							break;
						}
					
					if (dest==desttime.end())
						cout << "THIS SHOULD NEVER HAPPEN: did not find destination note for tie!" << endl;		
				}
			}
			else if (it->type==FloItem::REST)
			{
				switch (it->len)
				{
					case 0: it->pix=pix_r1; break;
					case 1: it->pix=pix_r2; break;
					case 2: it->pix=pix_r4; break;
					case 3: it->pix=pix_r8; break;
					case 4: it->pix=pix_r16; break;
				}
				
				it->x+=parent->note_x_indent() + (it->ausweich ? REST_AUSWEICH_X : 0); //AUSWEICH_X
			}
			else if (it->type==FloItem::BAR)
			{
				//nothing to do :)
			}
			else if (it->type==FloItem::TIME_SIG)
			{
				int add=calc_timesig_width(it->num, it->denom);
				pos_add+=add;
			}
			else if (it->type==FloItem::KEY_CHANGE)
			{
				key_enum new_key=it->key;
				
				list<int> aufloes_list=calc_accidentials(curr_key, clef, new_key);
				list<int> new_acc_list=calc_accidentials(new_key, clef);

				int n_acc_drawn=aufloes_list.size() + new_acc_list.size();
				pos_add+=n_acc_drawn*KEYCHANGE_ACC_DIST+ KEYCHANGE_ACC_LEFTDIST+ KEYCHANGE_ACC_RIGHTDIST;
				
				curr_key=new_key;
			}
		}
	}		
}

void ScoreCanvas::calc_pos_add_list()
{
	using AL::sigmap;
	using AL::iSigEvent;

	
	pos_add_list.clear();
	
	//process time signatures
	for (iSigEvent it=sigmap.begin(); it!=sigmap.end(); it++)
		pos_add_list[it->second->tick]+=calc_timesig_width(it->second->sig.z, it->second->sig.n);
	
	
	//process key changes
	key_enum curr_key=KEY_C; //this has to be KEY_C or KEY_C_B and nothing else,
	                         //because only with these two keys the next (initial)
	                         //key signature is properly calculated.
	for (iKeyEvent it=keymap.begin(); it!=keymap.end(); it++)
	{
		key_enum new_key=it->second.key;
		list<int> aufloes_list=calc_accidentials(curr_key, VIOLIN, new_key); //clef argument is unneccessary
		list<int> new_acc_list=calc_accidentials(new_key, VIOLIN);           //in this case
		int n_acc_drawn=aufloes_list.size() + new_acc_list.size();
		pos_add_list[it->second.tick]+=n_acc_drawn*KEYCHANGE_ACC_DIST+ KEYCHANGE_ACC_LEFTDIST+ KEYCHANGE_ACC_RIGHTDIST;
		
		curr_key=new_key;
	}
}

void ScoreCanvas::draw_items(QPainter& p, int y, staff_t& staff, int x1, int x2)
{
	int from_tick, to_tick;
	ScoreItemList::iterator from_it, to_it;

	//drawing too much isn't bad. drawing too few is.

	from_tick=x_to_tick(x1);
	from_it=staff.itemlist.lower_bound(from_tick);
	//from_it now contains the first time which is fully drawn
	//however, the previous beat could still be relevant, when it's
	//partly drawn. so we decrement from_it
	if (from_it!=staff.itemlist.begin()) from_it--;

	//decrement until we're at a time with a bar
	//otherwise, drawing accidentials will be broken
	while (from_it!=staff.itemlist.begin() && from_it->second.find(FloItem(FloItem::BAR))==from_it->second.end())
		from_it--;
	
	
	to_tick=x_to_tick(x2);
	to_it=staff.itemlist.upper_bound(to_tick);
	//to_it now contains the first time which is not drawn at all any more
	//however, a tie from 1:04 to 2:01 is stored in 2:01, not in 1:04,
	//so for drawing ties, we need to increment to_it, so that the
	//"first time not drawn at all any more" is the last which gets
	//actually drawn.
	if (to_it!=staff.itemlist.end()) to_it++; //do one tick more than neccessary. this will draw ties

	draw_items(p,y, staff, from_it, to_it);	
}

void ScoreCanvas::draw_items(QPainter& p, int y, staff_t& staff)
{
	draw_items(p,y, staff,x_pos,x_pos+width()-x_left);
}

void ScoreCanvas::draw_items(QPainter& p, int y_offset, staff_t& staff, ScoreItemList::iterator from_it, ScoreItemList::iterator to_it)
{
	// init accidentials properly
	vorzeichen_t curr_accidential[7];
	vorzeichen_t default_accidential[7];
	key_enum curr_key;

	curr_key=key_at_tick(from_it->first);
	list<int> new_acc_list=calc_accidentials(curr_key, staff.clef);
	vorzeichen_t new_accidential = is_sharp_key(curr_key) ? SHARP : B;

	for (int i=0;i<7;i++)
		curr_accidential[i]=default_accidential[i]=NONE;

	for (list<int>::iterator acc_it=new_acc_list.begin(); acc_it!=new_acc_list.end(); acc_it++)
		default_accidential[*acc_it % 7]=curr_accidential[*acc_it % 7]=new_accidential;



	for (ScoreItemList::iterator it2=from_it; it2!=to_it; it2++)
	{
		cout << "at t="<<it2->first << endl;
		
		int upstem_y1 = -1, upstem_y2=-1, upstem_x=-1, upflag=-1;
		int downstem_y1 = -1, downstem_y2=-1, downstem_x=-1, downflag=-1;
		
		for (set<FloItem, floComp>::iterator it=it2->second.begin(); it!=it2->second.end();it++)
		{
			if (it->type==FloItem::NOTE)
			{
				cout << "\tNOTE at line"<<it->pos.height<<" with acc.="<<it->pos.vorzeichen<<", len="<<pow(2,it->len);
				for (int i=0;i<it->dots;i++) cout << ".";
				cout << " , stem=";
				if (it->stem==UPWARDS)
					cout << "UPWARDS";
				else
					cout << "DOWNWARDS";
				
				cout << " , shift="<<it->shift<<", ausweich="<<it->ausweich<<", ";
				if (!it->tied)	cout << "un";
				cout << "tied, is_tie_dest="<<it->is_tie_dest<<endl;

				if (it->len!=0) //only for non-whole notes the stems are relevant!
				{
					if (it->stem==UPWARDS)
					{
						if (upstem_y1 == -1)
							upstem_y1=it->y;
						
						upstem_y2=it->y;
						
						
						if ((upflag!=-1) && (upflag!=it->len))
							cout << "WARNING: THIS SHOULD NEVER HAPPEN: upflag != this->flag" << endl;
						upflag=it->len;
						
						if ((upstem_x!=-1) && (upstem_x!=it->stem_x ))
							cout << "WARNING: THIS SHOULD NEVER HAPPEN: upstem_x != x_result" << endl;
						upstem_x=it->stem_x;
					}
					else
					{
						if (downstem_y1 == -1)
							downstem_y1=it->y;
						
						downstem_y2=it->y;
						

						if ((downflag!=-1) && (downflag!=it->len))
							cout << "WARNING: THIS SHOULD NEVER HAPPEN: downflag != this->flag" << endl;
						downflag=it->len;
						
						if ((downstem_x!=-1) && (downstem_x!=it->stem_x))
							cout << "WARNING: THIS SHOULD NEVER HAPPEN: downstem_x != x_result" << endl;
						downstem_x=it->stem_x; //important: before the below calculation!
					}
				}					

		
				if (it->pos.height <= 0) //we need auxiliary lines on the bottom?
				{
					p.setPen(Qt::black);
					for (int i=0; i>=it->pos.height; i-=2)
						p.drawLine(it->x-it->pix->width()*AUX_LINE_LEN/2 -x_pos+x_left,y_offset + 2*YLEN  -  (i-2)*YLEN/2,it->x+it->pix->width()*AUX_LINE_LEN/2-x_pos+x_left,y_offset + 2*YLEN  -  (i-2)*YLEN/2);
				}
				else if (it->pos.height >= 12) //we need auxiliary lines on the top?
				{
					p.setPen(Qt::black);
					for (int i=12; i<=it->pos.height; i+=2)
						p.drawLine(it->x-it->pix->width()*AUX_LINE_LEN/2 -x_pos+x_left,y_offset + 2*YLEN  -  (i-2)*YLEN/2,it->x+it->pix->width()*AUX_LINE_LEN/2-x_pos+x_left,y_offset + 2*YLEN  -  (i-2)*YLEN/2);
				}
								
				it->is_active= ( (song->cpos() >= it->source_event->tick() + it->source_part->tick()) &&
				     			       (song->cpos() < it->source_event->endTick() + it->source_part->tick()) );


				int color_index;
				switch (coloring_mode)
				{
					case COLOR_MODE_BLACK:
						color_index=BLACK_PIXMAP;
						break;
						
					case COLOR_MODE_PART:
						color_index=it->source_part->colorIndex();
						break;
						
					case COLOR_MODE_PITCH: //TODO
						break;
				}
				if (audio->isPlaying() && it->is_active)
					color_index=HIGHLIGHTED_PIXMAP;
				
				draw_pixmap(p,it->x -x_pos+x_left,y_offset + it->y,it->pix[color_index]);
				//TODO FINDMICH maybe draw a margin around bright colors?
				//maybe draw the default color in black?
				
				//draw dots
				
				int x_dot=DOT_XBEGIN;
				int y_dot;
				if (modulo(it->pos.height, 2) == 0) //note is on a line?
					y_dot=YLEN * 0.33; // actually 0.5, but that would be _exactly_ in the space
				else //note is between two lines?
					y_dot=YLEN * 0.1;
				
				if (it->stem==DOWNWARDS)
					y_dot=-y_dot;
				//else y_dot=y_dot;
				
				for (int i=0;i<it->dots;i++)
				{
					draw_pixmap(p,it->x+x_dot -x_pos+x_left,y_offset + it->y+y_dot,pix_dot[color_index]);
					x_dot+=DOT_XDIST;
				}


				
				//draw accidentials
				if (it->pos.vorzeichen != curr_accidential[modulo(it->pos.height,7)])
				{
					QPixmap* acc_pix;
					switch (it->pos.vorzeichen)
					{
						case NONE: acc_pix=pix_noacc; break;
						case SHARP: acc_pix=pix_sharp; break;
						case B: acc_pix=pix_b; break;
					}
					draw_pixmap(p,it->x-ACCIDENTIAL_DIST -x_pos+x_left,y_offset + it->y, acc_pix[color_index]);

					curr_accidential[modulo(it->pos.height,7)]=it->pos.vorzeichen;
				}

				
				//if needed, draw tie
				if (it->is_tie_dest)
				{
					cout << "drawing tie" << endl;
					draw_tie(p,it->tie_from_x-x_pos+x_left,it->x -x_pos+x_left,y_offset + it->y, (it->len==0) ? true : (it->stem==DOWNWARDS) , mycolors[color_index]);
					// in english: "if it's a whole note, tie is upwards (true). if not, tie is upwards if
					//              stem is downwards and vice versa"
				}
			}
			else if (it->type==FloItem::REST)
			{
				cout << "\tREST at line"<<it->pos.height<<" with len="<<pow(2,it->len);
				for (int i=0;i<it->dots;i++) cout << ".";
				cout << " , ausweich="<<it->ausweich<<endl;
				
				draw_pixmap(p,it->x -x_pos+x_left,y_offset + it->y,*it->pix);
				

				//draw dots
				
				int x_dot=DOT_XBEGIN_REST;
				int y_dot;
				if (modulo(it->pos.height, 2) == 0) //rest is on a line?
					y_dot=YLEN * 0.33; // actually 0.5, but that would be _exactly_ in the space
				else //note is between two lines?
					y_dot=YLEN * 0.1;
				
				if (it->len!=0) // all rests except the whole are treated as 
					y_dot=-y_dot; // if they had a downwards stem
				
				for (int i=0;i<it->dots;i++)
				{
					draw_pixmap(p,it->x+x_dot -x_pos+x_left,y_offset + it->y+y_dot,pix_dot[BLACK_PIXMAP]);
					x_dot+=DOT_XDIST;
				}
			}
			else if (it->type==FloItem::BAR)
			{
				cout << "\tBAR" << endl;
				
				p.setPen(Qt::black);
				p.drawLine(it->x -x_pos+x_left,y_offset  -2*YLEN,it->x -x_pos+x_left,y_offset +2*YLEN);
				
				for (int i=0;i<7;i++)
					curr_accidential[i]=default_accidential[i];
			}
			else if (it->type==FloItem::TIME_SIG)
			{
				cout << "\tTIME SIGNATURE: "<<it->num<<"/"<<it->denom<<endl;

				draw_timesig(p,  it->x - x_pos+x_left, y_offset, it->num, it->denom);
			}
			else if (it->type==FloItem::KEY_CHANGE)
			{
				key_enum new_key=it->key;
				cout << "\tKEY CHANGE: from "<<curr_key<<" to "<<new_key<<endl;
								
				list<int> aufloes_list=calc_accidentials(curr_key, staff.clef, new_key);
				list<int> new_acc_list=calc_accidentials(new_key, staff.clef);
				
				// vorzeichen aus curr_key auflösen
				draw_accidentials(p, it->x + KEYCHANGE_ACC_LEFTDIST - x_pos+x_left, y_offset, aufloes_list, pix_noacc[BLACK_PIXMAP]);
								
				// alle vorzeichen aus new_key zeichnen
				QPixmap* pix = is_sharp_key(new_key) ? &pix_sharp[BLACK_PIXMAP] : &pix_b[BLACK_PIXMAP];
				vorzeichen_t new_accidential = is_sharp_key(new_key) ? SHARP : B;

				draw_accidentials(p, it->x + aufloes_list.size()*KEYCHANGE_ACC_DIST + KEYCHANGE_ACC_LEFTDIST - x_pos+x_left, y_offset, new_acc_list, *pix);

				for (int i=0;i<7;i++)
					curr_accidential[i]=default_accidential[i]=NONE;
				
				for (list<int>::iterator acc_it=new_acc_list.begin(); acc_it!=new_acc_list.end(); acc_it++)
					default_accidential[*acc_it % 7]=curr_accidential[*acc_it % 7]=new_accidential;
				
				curr_key=new_key;
			}
		}
		
		p.setPen(Qt::black);
		//note: y1 is bottom, y2 is top!
		if (upstem_x!=-1)
		{
			upstem_x=upstem_x-pix_quarter[0].width()/2 +pix_quarter[0].width() -1;
			p.drawLine(upstem_x -x_pos+x_left, y_offset + upstem_y1, upstem_x -x_pos+x_left, y_offset + upstem_y2-STEM_LEN);
			
			if (upflag>=3) //if the note needs a flag
				p.drawPixmap(upstem_x -x_pos+x_left,y_offset + upstem_y2-STEM_LEN,pix_flag_up[upflag-3]);
		}
		if (downstem_x!=-1)
		{
			downstem_x=downstem_x-pix_quarter[0].width()/2;
			p.drawLine(downstem_x -x_pos+x_left, y_offset + downstem_y1+STEM_LEN, downstem_x -x_pos+x_left, y_offset + downstem_y2);

			if (downflag>=3) //if the note needs a flag
				p.drawPixmap(downstem_x -x_pos+x_left,y_offset + downstem_y1+STEM_LEN-pix_flag_down[downflag-3].height(),pix_flag_down[downflag-3]);
		}
	}		
}

bool ScoreCanvas::need_redraw_for_hilighting()
{
	for (list<staff_t>::iterator it=staves.begin(); it!=staves.end(); it++)
		if (need_redraw_for_hilighting(it->itemlist)) return true;
	
	return false;
}

bool ScoreCanvas::need_redraw_for_hilighting(ScoreItemList& itemlist)
{
	return need_redraw_for_hilighting(itemlist, x_pos,x_pos+width()-x_left);
}

bool ScoreCanvas::need_redraw_for_hilighting(ScoreItemList& itemlist, int x1, int x2)
{
	int from_tick, to_tick;
	ScoreItemList::iterator from_it, to_it;

	from_tick=x_to_tick(x1);
	from_it=itemlist.lower_bound(from_tick);
	//from_it now contains the first time which is fully drawn
	//however, the previous beat could still be relevant, when it's
	//partly drawn. so we decrement from_it
	if (from_it!=itemlist.begin()) from_it--;	
	
	to_tick=x_to_tick(x2);
	to_it=itemlist.upper_bound(to_tick);
	//to_it now contains the first time which is not drawn at all any more

	return need_redraw_for_hilighting(from_it, to_it);	
}

bool ScoreCanvas::need_redraw_for_hilighting(ScoreItemList::iterator from_it, ScoreItemList::iterator to_it)
{
	//if we aren't playing, there will never be a need for redrawing due to highlighting things
	if (audio->isPlaying()==false)
		return false;
	
	for (ScoreItemList::iterator it2=from_it; it2!=to_it; it2++)
		for (set<FloItem, floComp>::iterator it=it2->second.begin(); it!=it2->second.end();it++)
			if (it->type==FloItem::NOTE)
			{
				bool is_active= ( (song->cpos() >= it->source_event->tick() + it->source_part->tick()) &&
				                  (song->cpos() < it->source_event->endTick() + it->source_part->tick()) );
				if (it->is_active != is_active)
					return true;
			}
			
	return false;
}

int clef_height(clef_t clef)
{
	switch (clef) //CLEF_MARKER
	{
		case VIOLIN: return 4;
		case BASS: return 8;
		default:
			cout << "WARNING: ILLEGAL FUNCTION CALL in clef_height()" << endl;
			return 6;
	}
}

#define TIMESIG_LEFTMARGIN 5
#define TIMESIG_RIGHTMARGIN 5
#define DIGIT_YDIST 9
#define DIGIT_WIDTH 12

#define CLEF_LEFTMARGIN 5
#define CLEF_RIGHTMARGIN 5

void ScoreCanvas::draw_preamble(QPainter& p, int y_offset, clef_t clef)
{
	int x_left_old=x_left;
	int tick=x_to_tick(x_pos);

	// draw clef --------------------------------------------------------
	QPixmap* pix_clef= (clef==BASS) ? pix_clef_bass : pix_clef_violin;
	int y_coord=2*YLEN  -  ( clef_height(clef) -2)*YLEN/2;
	
	draw_pixmap(p,CLEF_LEFTMARGIN + pix_clef->width()/2,y_offset + y_coord,*pix_clef);
	
	x_left= CLEF_LEFTMARGIN + pix_clef->width() + CLEF_RIGHTMARGIN;
	

	// draw accidentials ------------------------------------------------
	if (preamble_contains_keysig)
	{
		x_left+=KEYCHANGE_ACC_LEFTDIST;
		
		key_enum key=key_at_tick(tick);
		QPixmap* pix_acc=is_sharp_key(key) ? &pix_sharp[BLACK_PIXMAP] : &pix_b[BLACK_PIXMAP];
		list<int> acclist=calc_accidentials(key,clef);
		
		draw_accidentials(p,x_left, y_offset, acclist ,*pix_acc);
		
		x_left+=acclist.size()*KEYCHANGE_ACC_DIST + KEYCHANGE_ACC_RIGHTDIST;
	}


	// draw time signature ----------------------------------------------
	if (preamble_contains_timesig)
	{
		x_left+=TIMESIG_LEFTMARGIN;
		
		timesig_t timesig=timesig_at_tick(tick);

		draw_timesig(p, x_left, y_offset, timesig.num, timesig.denom);

		x_left+=calc_timesig_width(timesig.num, timesig.denom)+TIMESIG_RIGHTMARGIN;
	}
	
	// draw bar ---------------------------------------------------------
	p.setPen(Qt::black);
	p.drawLine(x_left,y_offset  -2*YLEN,x_left,y_offset +2*YLEN);


	if (x_left_old!=x_left)
		emit viewport_width_changed(viewport_width());
}


void ScoreCanvas::draw_timesig(QPainter& p, int x, int y_offset, int num, int denom)
{
	int num_width=calc_number_width(num);
	int denom_width=calc_number_width(denom);
	int width=((num_width > denom_width) ? num_width : denom_width);
	int num_indent=(width-num_width)/2 + TIMESIG_LEFTMARGIN;
	int denom_indent=(width-denom_width)/2 + TIMESIG_LEFTMARGIN;
	
	draw_number(p, x+num_indent, y_offset -DIGIT_YDIST, num);
	draw_number(p, x+denom_indent, y_offset +DIGIT_YDIST, denom);
}

int calc_timesig_width(int num, int denom)
{
	int num_width=calc_number_width(num);
	int denom_width=calc_number_width(denom);
	int width=((num_width > denom_width) ? num_width : denom_width);
	return width+TIMESIG_LEFTMARGIN+TIMESIG_RIGHTMARGIN;
}

int calc_number_width(int n)
{
	string str=IntToStr(n);
	return (str.length()*DIGIT_WIDTH);
}

void ScoreCanvas::draw_number(QPainter& p, int x, int y, int n)
{
	string str=IntToStr(n);
	int curr_x=x+DIGIT_WIDTH/2;
	
	for (size_t i=0;i<str.length(); i++)
	{
		draw_pixmap(p, curr_x, y, pix_num[str[i]-'0']);
		curr_x+=DIGIT_WIDTH;
	}
}


void ScoreCanvas::draw(QPainter& p, const QRect&)
{
	cout <<"now in ScoreCanvas::draw"<<endl;

	

	p.setPen(Qt::black);
	
	for (list<staff_t>::iterator it=staves.begin(); it!=staves.end(); it++)
	{
		//TODO: maybe only draw visible staves?
		draw_note_lines(p,it->y_draw - y_pos);
		draw_preamble(p,it->y_draw - y_pos, it->clef);
		p.setClipRect(x_left+1,0,p.device()->width(),p.device()->height());
		draw_items(p,it->y_draw - y_pos, *it);
		p.setClipping(false);
	}
}


list<int> calc_accidentials(key_enum key, clef_t clef, key_enum next_key)
{
	list<int> result;
	
	int violin_sharp_pos[]={10,7,11,8,5,9,6}; //CLEF_MARKER
	int violin_b_pos[]={6,9,5,8,4,7,3};
	int bass_sharp_pos[]={8,5,9,6,3,7,4};
	int bass_b_pos[]={4,7,3,6,2,5,1};
	
	int* accidential_pos;
	
	switch (clef)
	{
		case VIOLIN: accidential_pos = is_sharp_key(key) ? violin_sharp_pos : violin_b_pos; break;
		case BASS: accidential_pos = is_sharp_key(key) ? bass_sharp_pos : bass_b_pos; break;
	}

	int begin=0;
	
	if (is_sharp_key(key)==is_sharp_key(next_key)) //same kind of key (both b or both #)?
		begin=n_accidentials(next_key);
	else
		begin=0;
	
	
	int end=n_accidentials(key);
	
	for (int i=begin; i<end; i++)
		result.push_back(accidential_pos[i]);
	
	return result;
}




int ScoreCanvas::tick_to_x(int t)
{
	int x=t*pixels_per_whole()/TICKS_PER_WHOLE;
	
	for (std::map<int,int>::iterator it=pos_add_list.begin(); it!=pos_add_list.end() && it->first<=t; it++)
		x+=it->second;
	
	return x;
}

int ScoreCanvas::calc_posadd(int t)
{
	int result=0;

	for (std::map<int,int>::iterator it=pos_add_list.begin(); it!=pos_add_list.end() && it->first<t; it++)
		result+=it->second;	

	return result;
}

//doesn't round mathematically correct, but i don't think this
//will be a problem, because a tick is pretty small
int ScoreCanvas::x_to_tick(int x)
{
	int t=TICKS_PER_WHOLE * x/pixels_per_whole();
	int min_t=0;
	
	cout << "t="<<t<<endl;
	
	for (std::map<int,int>::iterator it=pos_add_list.begin(); it!=pos_add_list.end() && it->first<t; it++)
	{
		cout << "at pos_add event at t="<<it->first<<", add="<<it->second<<endl;
		min_t=it->first;
		x-=it->second;
		t=TICKS_PER_WHOLE * x/pixels_per_whole();
	}
	
	return t > min_t ? t : min_t;
}

key_enum ScoreCanvas::key_at_tick(int t_)
{
	unsigned int t= (t_>=0) ? t_ : 0;
	
	return keymap.keyAtTick(t);
}

timesig_t ScoreCanvas::timesig_at_tick(int t_)
{
	timesig_t tmp;
	unsigned int t= (t_>=0) ? t_ : 0;

	AL::sigmap.timesig(t, tmp.num, tmp.denom);

	return tmp;
}

int ScoreCanvas::height_to_pitch(int h, clef_t clef)
{
	int foo[]={0,2,4,5,7,9,11};
	
	switch(clef) //CLEF_MARKER
	{
		case VIOLIN:	return foo[modulo(h,7)] + ( divide_floor(h,7)*12 ) + 60;
		case BASS:		return foo[modulo((h-5),7)] + ( divide_floor(h-5,7)*12 ) + 48;
		default:
			cout << "WARNING: THIS SHOULD NEVER HAPPEN: unknown clef in height_to_pitch" << endl;
			return 60;
	}
}

int ScoreCanvas::height_to_pitch(int h, clef_t clef, key_enum key)
{
	int add=0;
	
	list<int> accs=calc_accidentials(key,clef);
	
	for (list<int>::iterator it=accs.begin(); it!=accs.end(); it++)
	{
		if (modulo(*it,7) == modulo(h,7))
		{
			add=is_sharp_key(key) ? 1 : -1;
			break;
		}
	}
	
	return height_to_pitch(h,clef)+add;
}

int ScoreCanvas::y_to_height(int y)
{
	return int(nearbyint(float(2*YLEN  - y)*2.0/YLEN))+2 ;
}

int ScoreCanvas::y_to_pitch(int y, int t, clef_t clef)
{
	return height_to_pitch(y_to_height(y), clef, key_at_tick(t));
}


#define DRAG_INIT_DISTANCE 5

void ScoreCanvas::mousePressEvent (QMouseEvent* event)
{
	// den errechneten tick immer ABrunden!
	// denn der "bereich" eines schlags geht von schlag_begin bis nächsterschlag_begin-1
	// noten werden aber genau in die mitte dieses bereiches gezeichnet

	list<staff_t>::iterator staff_it=staff_at_y(event->y() + y_pos);

	int y=event->y() + y_pos - staff_it->y_draw;
	int x=event->x()+x_pos-x_left;
	int tick=flo_quantize_floor(x_to_tick(x), quant_ticks());
						//TODO quantizing must (maybe?) be done with the proper functions

	if (staff_it!=staves.end())
	{
		if (event->x() <= x_left) //clicked in the preamble?
		{
			if (event->button() == Qt::RightButton) //right-click?
			{
				current_staff=staff_it;
				staff_menu->popup(event->globalPos());
			}
			else if (event->button() == Qt::MidButton) //middle click?
			{
				remove_staff(staff_it);
			}
			else if (event->button() == Qt::LeftButton) //left click?
			{
				current_staff=staff_it;
				dragging_staff=true;
			}
		}
		else
		{
			ScoreItemList& itemlist=staff_it->itemlist;

			cout << "mousePressEvent at "<<x<<"/"<<y<<"; tick="<<tick<<endl;
			set<FloItem, floComp>::iterator set_it;
			for (set_it=itemlist[tick].begin(); set_it!=itemlist[tick].end(); set_it++)
				if (set_it->type==FloItem::NOTE)
					if (set_it->bbox().contains(x,y))
						break;
			
			if (set_it!=itemlist[tick].end()) //we found something?
			{
				mouse_down_pos=event->pos();
				mouse_operation=NO_OP;
				
				int t=tick;
				set<FloItem, floComp>::iterator found;
				
				do
				{
					found=itemlist[t].find(FloItem(FloItem::NOTE, set_it->pos));
					if (found == itemlist[t].end())
					{
						cout << "FATAL: THIS SHOULD NEVER HAPPEN: could not find the note's tie-destination" << endl;
						break;
					}
					else
					{
						t+=calc_len(found->len, found->dots);
					}
				} while (found->tied);
				
				int total_begin=set_it->begin_tick;
				int total_end=t;
				
				int this_begin=tick;
				int this_end=this_begin+calc_len(set_it->len, set_it->dots);
				
				selected_part=set_it->source_part;
				
				//that's the only note corresponding to the event?
				if (this_begin==total_begin && this_end==total_end)
				{
					if (x < set_it->x)
						mouse_x_drag_operation=BEGIN;
					else
						mouse_x_drag_operation=LENGTH;
				}
				//that's NOT the only note?
				else
				{
					if (this_begin==total_begin)
						mouse_x_drag_operation=BEGIN;
					else if (this_end==total_end)
						mouse_x_drag_operation=LENGTH;
					else
						mouse_x_drag_operation=NO_OP;
				}
				
				cout << "you clicked at a note with begin at "<<set_it->begin_tick<<" and end at "<<t<<endl;
				cout << "x-drag-operation will be "<<mouse_x_drag_operation<<endl;
				cout << "pointer to part is "<<set_it->source_part;
				if (!set_it->source_part) cout << " (WARNING! THIS SHOULD NEVER HAPPEN!)";
				cout << endl;
				
				dragged_event=*set_it->source_event;
				dragged_event_part=set_it->source_part;
				dragged_event_original_pitch=dragged_event.pitch();
				
				if ((mouse_erases_notes) || (event->button()==Qt::MidButton)) //erase?
				{
					audio->msgDeleteEvent(dragged_event, dragged_event_part, true, false, false);
				}
				else if (event->button()==Qt::LeftButton) //edit?
				{
					setMouseTracking(true);		
					dragging=true;
					song->startUndo();
				}
			}
			else //we found nothing?
			{
				if ((event->button()==Qt::LeftButton) && (mouse_inserts_notes))
				{
					Part* curr_part = NULL;
					set<Part*> possible_dests=staff_it->parts_at_tick(tick);

					if (!possible_dests.empty())
					{
						if (possible_dests.size()==1)
							curr_part=*possible_dests.begin();
						else
						{
							if (possible_dests.find(selected_part)!=possible_dests.end())
								curr_part=selected_part;
							else
								QMessageBox::information(this, tr("Ambiguous part"), tr("There are two or more possible parts you could add the note to, but none matches the selected part. Please select the destination part by clicking on any note belonging to it and try again, or add a new stave containing only the destination part."));
						}
					}
					else
						QMessageBox::information(this, tr("No part"), tr("There are no parts you could add the note to."));
					
					if (curr_part!=NULL)
					{
						signed int relative_tick=(signed) tick - curr_part->tick();
						if (relative_tick<0)
							cout << "THIS SHOULD NEVER HAPPEN: relative_tick is negative!" << endl;
						song->startUndo();
						//stopping undo at the end of this function is unneccessary
						//because we'll begin a drag right after it. finishing
						//this drag will stop undo as well (in mouseReleaseEvent)
						
						Event newevent(Note);
						newevent.setPitch(y_to_pitch(y,tick, staff_it->clef));
						newevent.setVelo(64); //TODO
						newevent.setVeloOff(64); //TODO
						newevent.setTick(relative_tick);
						newevent.setLenTick((new_len>0)?new_len:last_len);
						
						if (newevent.endTick() > curr_part->lenTick())
						{
							cout << "DEBUG: clipping inserted note from len="<<newevent.endTick()<<" to len="<<(curr_part->lenTick() - newevent.tick())<<endl;
							newevent.setLenTick(curr_part->lenTick() - newevent.tick());
						}
						
						audio->msgAddEvent(newevent, curr_part, false, false, false);
						
						dragged_event_part=curr_part;
						dragged_event=newevent;
						dragged_event_original_pitch=newevent.pitch();

						mouse_down_pos=event->pos();
						mouse_operation=NO_OP;
						mouse_x_drag_operation=LENGTH;

						song_changed(SC_EVENT_INSERTED);

						setMouseTracking(true);	
						dragging=true;
						//song->startUndo(); unneccessary because we have started it already above
					}
				}
			}
		}
	}
}

void ScoreCanvas::mouseReleaseEvent (QMouseEvent* event)
{
	if (dragging)
	{
		if (event->button()==Qt::LeftButton)
		{
			if ((mouse_operation==LENGTH) || (mouse_operation==BEGIN)) //also BEGIN can change the len by clipping
			{
				if (flo_quantize(dragged_event.lenTick(), quant_ticks()) <= 0)
				{
					cout << "DEBUG: new length <= 0, erasing item" << endl;
					audio->msgDeleteEvent(dragged_event, dragged_event_part, false, false, false);
				}
				else
				{
					last_len=flo_quantize(dragged_event.lenTick(), quant_ticks());
				}
			}
			
			song->endUndo(SC_EVENT_MODIFIED);
			setMouseTracking(false);
			dragging=false;
			
			x_scroll_speed=0; x_scroll_pos=0;
		}
	}
	
	if (dragging_staff)
	{
		merge_staves(staff_at_y(event->y()+y_pos), current_staff);
		dragging_staff=false;
		
		y_scroll_speed=0; y_scroll_pos=0;
	}
}

#define PITCH_DELTA 5


void ScoreCanvas::mouseMoveEvent (QMouseEvent* event)
{
	if (dragging)
	{
		int dx=event->x()-mouse_down_pos.x();
		int dy=event->y()-mouse_down_pos.y();

		int x=event->x()+x_pos-x_left;
		
		int tick=flo_quantize_floor(x_to_tick(x), quant_ticks());

		if (mouse_operation==NO_OP)
		{		
			if ((abs(dx)>DRAG_INIT_DISTANCE) && (mouse_x_drag_operation!=NO_OP))
			{
				cout << "mouse-operation is now "<<mouse_x_drag_operation<<endl;
				mouse_operation=mouse_x_drag_operation;
			}
			else if (abs(dy)>DRAG_INIT_DISTANCE)
			{
				cout << "mouse-operation is now PITCH" << endl;
				mouse_operation=PITCH;
			}
		}

		int new_pitch;
		
		switch (mouse_operation)
		{
			case NONE:
				break;
				
			case PITCH:
				cout << "changing pitch, delta="<<nearbyint((float)dy/PITCH_DELTA)<<endl;
				new_pitch=dragged_event_original_pitch - nearbyint((float)dy/PITCH_DELTA);

				if (dragged_event.pitch()!=new_pitch)
				{
					Event tmp=dragged_event.clone();
					tmp.setPitch(new_pitch);
					
					audio->msgChangeEvent(dragged_event, tmp, dragged_event_part, false, false, false);
					dragged_event=tmp;
					
					song_changed(SC_EVENT_INSERTED);	
				}
				
				break;
			
			case BEGIN:
				if (dragged_event.tick()+dragged_event_part->tick() != unsigned(tick)) //TODO FINDMICHJETZT tick kann unsigned werden
				{
					Event tmp=dragged_event.clone();
					signed relative_tick=tick-signed(dragged_event_part->tick());
					
					if (relative_tick >= 0)
						tmp.setTick(relative_tick);
					else
					{
						tmp.setTick(0);
						cout << "DEBUG: not moving note before begin of part; setting it directly to the begin" << endl;
					}

					if (tmp.endTick() > dragged_event_part->lenTick())
					{
						signed new_len=dragged_event_part->lenTick() - tmp.tick();
						if (new_len>=0)
						{
							tmp.setLenTick(dragged_event_part->lenTick() - tmp.tick());
							cout << "DEBUG: moved note would exceed its part; clipping length to " << tmp.lenTick() << endl;
						}
						else
						{
							tmp.setLenTick(0);
							cout << "DEBUG: moved note would exceed its part; clipping length to 0 (actually negative)" << endl;
						}
					}
					
					audio->msgChangeEvent(dragged_event, tmp, dragged_event_part, false, false, false);
					dragged_event=tmp;
					
					song_changed(SC_EVENT_INSERTED);	
				}
				
				break;

			case LENGTH:
				tick+=quant_ticks();
				if (dragged_event.tick()+dragged_event.lenTick() + dragged_event_part->tick() != unsigned(tick))
				{
					Event tmp=dragged_event.clone();
					signed relative_tick=tick-signed(dragged_event_part->tick());
					signed new_len=relative_tick-dragged_event.tick();
					
					if (new_len>=0)
						tmp.setLenTick(new_len);
					else
					{
						tmp.setLenTick(0);
						cout << "DEBUG: not setting len to a negative value. using 0 instead" << endl;
					}
					
					if (tmp.endTick() > dragged_event_part->lenTick())
					{
						tmp.setLenTick(dragged_event_part->lenTick() - tmp.tick());
						cout << "DEBUG: resized note would exceed its part; limiting length to " << tmp.lenTick() << endl;
					}
					
					audio->msgChangeEvent(dragged_event, tmp, dragged_event_part, false, false, false);
					dragged_event=tmp;
					
					song_changed(SC_EVENT_INSERTED);	
				}
				
				break;
		}	
	

		if ((mouse_operation==LENGTH) || (mouse_operation==BEGIN)) //x-scrolling enabled?
		{
			int win_x=event->x();
			
			if (win_x < x_left + SCROLL_MARGIN)
			{
				x_scroll_speed=(win_x - (x_left + SCROLL_MARGIN)) * SCROLL_SPEED;
				if (x_scroll_speed < -SCROLL_SPEED_MAX) x_scroll_speed=-SCROLL_SPEED_MAX;
			}
			else if (win_x > width() - SCROLL_MARGIN)
			{
				x_scroll_speed=(win_x - (width() - SCROLL_MARGIN)) * SCROLL_SPEED;
				if (x_scroll_speed > SCROLL_SPEED_MAX) x_scroll_speed=SCROLL_SPEED_MAX;
			}
			else
				x_scroll_speed=0;
		}
		else
		{
			x_scroll_speed=0;
		}
	}
	
	if (dragging_staff) //y-scrolling enabled?
	{
		int win_y=event->y();
		
		if (win_y < SCROLL_MARGIN)
		{
			y_scroll_speed=(win_y - SCROLL_MARGIN) * SCROLL_SPEED;
			if (y_scroll_speed < -SCROLL_SPEED_MAX) y_scroll_speed=-SCROLL_SPEED_MAX;
		}
		else if (win_y > height() - SCROLL_MARGIN)
		{
			y_scroll_speed=(win_y - (height() - SCROLL_MARGIN)) * SCROLL_SPEED;
			if (y_scroll_speed > SCROLL_SPEED_MAX) y_scroll_speed=SCROLL_SPEED_MAX;
		}
		else
			y_scroll_speed=0;
	}
	else
	{
		y_scroll_speed=0;
	}
}

void ScoreCanvas::heartbeat_timer_event()
{
	if (x_scroll_speed)
	{
		int old_xpos=x_pos;
		
		x_scroll_pos+=x_scroll_speed*heartBeatTimer->interval()/1000.0;
		int tmp=int(x_scroll_pos);
		if (tmp!=0)
		x_pos+=tmp;
		x_scroll_pos-=tmp;
		
		if (x_pos<0) x_pos=0;
		if (x_pos>canvas_width()) x_pos=canvas_width();

		if (old_xpos!=x_pos) emit xscroll_changed(x_pos);
	}
	
	if (y_scroll_speed)
	{
		int old_ypos=y_pos;
		
		y_scroll_pos+=y_scroll_speed*heartBeatTimer->interval()/1000.0;
		int tmp=int(y_scroll_pos);
		if (tmp!=0)
		y_pos+=tmp;
		y_scroll_pos-=tmp;
		
		if (y_pos<0) y_pos=0;
		if (y_pos>canvas_height()) y_pos=canvas_height();

		if (old_ypos!=y_pos) emit yscroll_changed(y_pos);
	}
}

void ScoreCanvas::x_scroll_event(int x)
{
	cout << "SCROLL EVENT: x="<<x<<endl;
	x_pos=x;
	redraw();
}

void ScoreCanvas::y_scroll_event(int y)
{
	cout << "SCROLL EVENT: y="<<y<<endl;
	y_pos=y;
	redraw();
}


//if force is true, it will always happen something
//if force is false, it will only happen something, if
//tick isn't visible at all. if it's visible, but not at
//the position goto would set it to, nothing happens
void ScoreCanvas::goto_tick(int tick, bool force)
{
	if (!force)
	{
		if (tick < x_to_tick(x_pos))
		{
			x_pos=tick_to_x(tick) - x_left;
			if (x_pos<0) x_pos=0;
			if (x_pos>canvas_width()) x_pos=canvas_width();
			
			emit xscroll_changed(x_pos);
		}
		else if (tick > x_to_tick(x_pos+viewport_width()*PAGESTEP))
		{
			x_pos=tick_to_x(tick);
			if (x_pos<0) x_pos=0;
			if (x_pos>canvas_width()) x_pos=canvas_width();
			
			emit xscroll_changed(x_pos);
		}
	}
	else
	{
		x_pos=tick_to_x(tick)-viewport_width()/2;
		if (x_pos<0) x_pos=0;
		if (x_pos>canvas_width()) x_pos=canvas_width();
		
		emit xscroll_changed(x_pos);
	}
}

//---------------------------------------------------------
//   resizeEvent
//---------------------------------------------------------

void ScoreCanvas::resizeEvent(QResizeEvent* ev)
{
	QWidget::resizeEvent(ev); //TODO is this really neccessary?

	emit viewport_width_changed( viewport_width()  );
	emit viewport_height_changed( viewport_height()  );
}

void ScoreCanvas::pos_changed(int index, unsigned tick, bool scroll)
{
	if (index==0)
	{
		if (scroll) //potential need to scroll?
		{
			switch (song->follow())
			{
				case  Song::NO: break;
				case Song::JUMP:  goto_tick(tick,false);  break;
				case Song::CONTINUOUS:  goto_tick(tick,true);  break;
			}
		}
		
		if (need_redraw_for_hilighting())
			redraw();
	}
}


void ScoreCanvas::recalc_staff_pos()
{
	int y=0;
	
	for (list<staff_t>::iterator it=staves.begin(); it!=staves.end(); it++)
	{
		it->y_top=y;
		switch (it->type)
		{
			case NORMAL:
				it->y_draw = it->y_top + STAFF_DISTANCE/2;
				it->y_bottom = it->y_draw + STAFF_DISTANCE/2;
				break;
			case GRAND_TOP:
				it->y_draw = it->y_top + STAFF_DISTANCE/2;
				it->y_bottom = it->y_draw + GRANDSTAFF_DISTANCE/2;
				break;
			case GRAND_BOTTOM:
				it->y_draw = it->y_top + GRANDSTAFF_DISTANCE/2;
				it->y_bottom = it->y_draw + STAFF_DISTANCE/2;
				break;
			default:
				cout << "THIS SHOULD NEVER HAPPEN: invalid staff type!" << endl;
		}
		y=it->y_bottom;
	}
	
	emit canvas_height_changed( canvas_height() );
}

list<staff_t>::iterator ScoreCanvas::staff_at_y(int y)
{
	for (list<staff_t>::iterator it=staves.begin(); it!=staves.end(); it++)
		if ((y >= it->y_top) && (y < it->y_bottom))
			return it;

	return staves.end();
}

void ScoreCanvas::play_changed(bool)
{
	redraw();
}

void ScoreCanvas::config_changed()
{
	redraw();
}

void ScoreCanvas::set_tool(int tool)
{
	switch (tool)
	{
		case PointerTool: mouse_erases_notes=false; mouse_inserts_notes=false; break;
		case RubberTool:  mouse_erases_notes=true;  mouse_inserts_notes=false; break;
		case PencilTool:  mouse_erases_notes=false; mouse_inserts_notes=true;  break;
		default:
			cout << "THIS SHOULD NEVER HAPPEN: set_tool called with unknown tool ("<<tool<<")"<<endl;
	}
}

void ScoreCanvas::menu_command(int cmd)
{
	switch (cmd)
	{
		case CMD_COLOR_BLACK:  coloring_mode=COLOR_MODE_BLACK; redraw(); break;
		case CMD_COLOR_PART:   coloring_mode=COLOR_MODE_PART;  redraw(); break;
		case CMD_NOTELEN_1:    new_len=TICKS_PER_WHOLE/ 1; break;
		case CMD_NOTELEN_2:    new_len=TICKS_PER_WHOLE/ 2; break;
		case CMD_NOTELEN_4:    new_len=TICKS_PER_WHOLE/ 4; break;
		case CMD_NOTELEN_8:    new_len=TICKS_PER_WHOLE/ 8; break;
		case CMD_NOTELEN_16:   new_len=TICKS_PER_WHOLE/16; break;
		case CMD_NOTELEN_32:   new_len=TICKS_PER_WHOLE/32; break;
		case CMD_NOTELEN_LAST: new_len=-1; break;
		default: 
			cout << "ILLEGAL FUNCTION CALL: ScoreCanvas::menu_command called with unknown command ("<<cmd<<")"<<endl;
	}
}

void ScoreCanvas::preamble_keysig_slot(bool state)
{
	preamble_contains_keysig=state;
	redraw();
}
void ScoreCanvas::preamble_timesig_slot(bool state)
{
	preamble_contains_timesig=state;
	redraw();
}

void ScoreCanvas::set_quant(int val)
{
	int quant_mapper[]={1,2,3,4,5};

	if ((val>=0) && (val<signed(sizeof(quant_mapper)/sizeof(*quant_mapper))))
	{
		int old_len=quant_len();

		_quant_power2=quant_mapper[val];
		
		set_pixels_per_whole(pixels_per_whole() * quant_len() / old_len );

		song_changed(SC_EVENT_INSERTED);
	}
	else
	{
		cout << "ILLEGAL FUNCTION CALL: set_quant called with invalid value of "<<val<<endl;
	}
}

void ScoreCanvas::set_pixels_per_whole(int val)
{
	cout << "DEBUG: setting px per whole to " << val << endl;
	_pixels_per_whole=val;
	
	for (list<staff_t>::iterator it=staves.begin(); it!=staves.end(); it++)
		it->calc_item_pos();
	
	emit pixels_per_whole_changed(val);
	
	redraw();
}

void ScoreCanvas::cleanup_staves()
{
	for (list<staff_t>::iterator it=staves.begin(); it!=staves.end();)
	{
		if (it->parts.empty())
			staves.erase(it++);
		else
			it++;
	}
	
	maybe_close_if_empty();
}

void ScoreCanvas::maybe_close_if_empty()
{
	if (staves.empty())
	{
		if (!parent->close())
			cout << "THIS SHOULD NEVER HAPPEN: tried to close, but event hasn't been accepted!" << endl;
	}
}

bool staff_t::cleanup_parts()
{
	bool did_something=false;
	
	for (set<Part*>::iterator it=parts.begin(); it!=parts.end();)
	{
		bool valid=false;
		
		for (iTrack track=song->tracks()->begin(); track!=song->tracks()->end(); track++)
			if ((*track)->type() == Track::MIDI)
			{
				PartList* pl=(*track)->parts();
				for (iPart part=pl->begin(); part!=pl->end(); part++)
					if (*it == part->second)
					{
						valid=true;
						goto get_out_here2;
					}
			}
		
		get_out_here2:
		if (!valid)
		{
			parts.erase(it++);
			
			did_something=true;
		}
		else
			it++;
	}
	
	return did_something;
}

set<Part*> staff_t::parts_at_tick(unsigned tick)
{
	set<Part*> result;
	
	for (set<Part*>::iterator it=parts.begin(); it!=parts.end(); it++)
		if ((tick >= (*it)->tick()) && (tick<=(*it)->endTick()))
			result.insert(*it);
	
	return result;
}

//the following assertions are made:
//  pix_quarter.width() == pix_half.width()


// pix->width()-1 + 1/2*pix->width() + SHIFT + ADD_SPACE
// 10-1+5+3+3=20 <- um so viel wird der taktstrich verschoben
// um das doppelte (20*2=40) werden die kleinsten schläge gegeneinander versetzt





//hint: recalculating event- and itemlists "from zero"
//      could happen in realtime, as it is pretty fast.
//      however, this adds unneccessary cpu usage.
//      it is NO problem to recalc the stuff "from zero"
//      every time something changes.


/* BUGS and potential bugs
 *   o when the keymap is not used, this will probably lead to a bug
 * 
 * CURRENT TODO
 *   x nothing atm
 * 
 * IMPORTANT TODO
 *   o invalidate len-buttons for lens which are outside of the quantisation setting
 *   o add toolbar for new note's velocities
 *   o implement color=velocity
 *
 * less important stuff
 *   o support selections
 *   o more fine-grained redrawing in song_changed: sometimes,
 *     only a redraw and not a recalc is needed
 *   o do all the song_changed(SC_EVENT_INSERTED) properly
 *   o emit a "song-changed" signal instead of calling our
 *     internal song_changed() function
 *   o support different keys in different tracks at the same time
 *       calc_pos_add_list and calc_item_pos will be affected by this
 *       calc_pos_add_list must be called before calc_item_pos then,
 *       and calc_item_pos must respect the pos_add_list instead of
 *       keeping its own pos_add variable (which is only an optimisation)
 *   o draw measure numbers
 *   o tied notes don't work properly when there's a key-change in
 *     between, for example, when a cis is tied to a des
 *   o use timesig_t in all timesig-stuff
 *   o draw a margin around notes which are in a bright color
 *   o use bars instead of flags over groups of 8ths / 16ths etc
 *   o deal with expanding parts or clip (expanding is better)
 *   o refuse to resize so that width gets smaller or equal than x_left
 *   o add tracks in correct order to score
 *
 * stuff for the other muse developers
 *   o process accurate timesignatures from muse's list (has to be implemented first in muse)
 *      ( (2+2+3)/4 or (3+2+2)/4 instead of 7/4 )
 *   o maybe do expanding parts inside the msgChangeEvent or
 *     msgNewEvent functions (see my e-mail)
 *
 * GUI stuff
 *   o velocity/release-velo for newly inserted notes [->toolbar]
 *   o velocity/release-velo for already existing notes
 *     	  - do this by right-click -> some dialog shows up?
 *     	  - or by selecting the note and changing the values in the same widget which also is used for new notes?
 */


/* how to use the score editor with multiple tracks
 * ================================================
 * 
 * select parts, right-click, "display in new score window" or "display per-track in new score window"
 * or "display in existing window -> 1,2,3,4" or "display per-track in existing..."
 * 
 * ScoreCanvas has a list of note systems, consisting of the following:
 *   * all parts included in that view
 *   * eventlist, itemlist
 *   * used clef, transposing/octave settings
 *   * enum { NOT_GROUPED, I_AM_TOP, I_AM_BOTTOM } group_state
 *       NOT_GROUPED means "single note system"
 *       I_AM_TOP and I_AM_BOTTOM mean that the two systems belong
 *       together
 * 
 * when redrawing, we iterate through all systems.
 * we add a distance according to group_state
 * then we draw the system. if group_state is I_AM_BOTTOM, we
 * draw our beams longer/higher, and we draw a bracket
 * 
 * when clicking around, we first determine which system has been clicked in
 * (the systems have enough space in between, so there won't be notes
 *  from sys1 in sys2. if there are, they're ignored for simplicity)
 * then we proceed as usual (adding, removing, changing notes)
 * 
 * 
 * pos_add_list stays the same for each staff, so we only need one
 */

/* R O A D M A P
 * =============
 * 
 *   1. finish the score editor, without transposing instruments and
 *      with only a global keymap
 * 
 * 			REASON: a score editor with few functions is better than
 *              no score editor at all
 * 
 * 
 *   2. support transposing by octave-steps
 * 
 *      REASON: the main problem with transposing is, that the
 *              editor needs different key signatures and needs
 *              to align them against each other. this problem
 *              doesn't exist when only transposing by octaves
 * 
 * 
 *   3. support transposing instruments, but only one
 *      transposing-setting per score window. that is, you won't be 
 *      able to display your (C-)strings in the same window as your 
 *      B-trumpet. this will be very easy to implement
 * 
 *      REASON: the above problem still exists, but is circumvented
 *              by simply not having to align them against each other
 *              (because they're in different windows)
 * 
 * 
 *   4. support different transposing instruments in the same score
 *      window. this will be some hassle, because we need to align
 *      the scores properly. for example, when the C-violin has 
 *      C-major (no accidentials), then the B-trumpet need some
 *      accidentials. we now must align the staves so that the
 *      "note-after-keychange"s of both staves are again at the
 *      same x-position
 * 
 *      REASON: some solution for that problem must be written.
 *              this is a large step, which atm isn't very important
 * 
 * 
 *   5. support different keys per track. this wouldn't be that
 *      hard, when 4) is already done; because we then already have
 *      the "align it properly" functionality, and can use it
 * 
 * 			REASON: this is only a nice-to-have, which can however be
 *              easily implemented when 4) is done
 */

