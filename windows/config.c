/*
 * config.c - the Windows-specific parts of the PuTTY configuration
 * box.
 */

#include <assert.h>
#include <stdlib.h>

#include "putty.h"
#include "dialog.h"
#include "storage.h"

static void about_handler(dlgcontrol *ctrl, dlgparam *dlg,
                          void *data, int event)
{
    HWND *hwndp = (HWND *)ctrl->context.p;

    if (event == EVENT_ACTION) {
        modal_about_box(*hwndp);
    }
}

static void help_handler(dlgcontrol *ctrl, dlgparam *dlg,
                         void *data, int event)
{
    HWND *hwndp = (HWND *)ctrl->context.p;

    if (event == EVENT_ACTION) {
        show_help(*hwndp);
    }
}

static void variable_pitch_handler(dlgcontrol *ctrl, dlgparam *dlg,
                                   void *data, int event)
{
    if (event == EVENT_REFRESH) {
        dlg_checkbox_set(ctrl, dlg, !dlg_get_fixed_pitch_flag(dlg));
    } else if (event == EVENT_VALCHANGE) {
        dlg_set_fixed_pitch_flag(dlg, !dlg_checkbox_get(ctrl, dlg));
    }
}

void win_setup_config_box(struct controlbox *b, HWND *hwndp, bool has_help,
                          bool midsession, int protocol)
{
    const struct BackendVtable *backvt;
    bool resize_forbidden = false;
    struct controlset *s;
    dlgcontrol *c;
    char *str;

    if (!midsession) {
        /*
         * Add the About and Help buttons to the standard panel.
         */
        s = ctrl_getset(b, "", "", "");
        c = ctrl_pushbutton(s, "����", 'a', HELPCTX(no_help),
                            about_handler, P(hwndp));
        c->column = 0;
        if (has_help) {
            c = ctrl_pushbutton(s, "����", 'h', HELPCTX(no_help),
                                help_handler, P(hwndp));
            c->column = 1;
        }
    }

    /*
     * Full-screen mode is a Windows peculiarity; hence
     * scrollbar_in_fullscreen is as well.
     */
    s = ctrl_getset(b, "����", "scrollback",
                    "���ڹ������ã�");
    ctrl_checkbox(s, "ȫ��ģʽ����ʾ������", 'i',
                  HELPCTX(window_scrollback),
                  conf_checkbox_handler,
                  I(CONF_scrollbar_in_fullscreen));
    /*
     * Really this wants to go just after `Display scrollbar'. See
     * if we can find that control, and do some shuffling.
     */
    {
        int i;
        for (i = 0; i < s->ncontrols; i++) {
            c = s->ctrls[i];
            if (c->type == CTRL_CHECKBOX &&
                c->context.i == CONF_scrollbar) {
                /*
                 * Control i is the scrollbar checkbox.
                 * Control s->ncontrols-1 is the scrollbar-in-FS one.
                 */
                if (i < s->ncontrols-2) {
                    c = s->ctrls[s->ncontrols-1];
                    memmove(s->ctrls+i+2, s->ctrls+i+1,
                            (s->ncontrols-i-2)*sizeof(dlgcontrol *));
                    s->ctrls[i+1] = c;
                }
                break;
            }
        }
    }

    /*
     * Windows has the AltGr key, which has various Windows-
     * specific options.
     */
    s = ctrl_getset(b, "�ն�/����", "features",
                    "���ö���ļ��̹��ܣ�");
    ctrl_checkbox(s, "AltGr�䵱Compose��", 't',
                  HELPCTX(keyboard_compose),
                  conf_checkbox_handler, I(CONF_compose_key));
    ctrl_checkbox(s, "Control-Alt��AltGr��ͬ", 'd',
                  HELPCTX(keyboard_ctrlalt),
                  conf_checkbox_handler, I(CONF_ctrlaltkeys));

    /*
     * Windows allows an arbitrary .WAV to be played as a bell, and
     * also the use of the PC speaker. For this we must search the
     * existing controlset for the radio-button set controlling the
     * `beep' option, and add extra buttons to it.
     *
     * Note that although this _looks_ like a hideous hack, it's
     * actually all above board. The well-defined interface to the
     * per-platform dialog box code is the _data structures_ `union
     * control', `struct controlset' and so on; so code like this
     * that reaches into those data structures and changes bits of
     * them is perfectly legitimate and crosses no boundaries. All
     * the ctrl_* routines that create most of the controls are
     * convenient shortcuts provided on the cross-platform side of
     * the interface, and template creation code is under no actual
     * obligation to use them.
     */
    s = ctrl_getset(b, "�ն�/��ʾ��", "style", "�Զ�������");
    {
        int i;
        for (i = 0; i < s->ncontrols; i++) {
            c = s->ctrls[i];
            if (c->type == CTRL_RADIO &&
                c->context.i == CONF_beep) {
                assert(c->handler == conf_radiobutton_handler);
                c->radio.nbuttons += 2;
                c->radio.buttons =
                    sresize(c->radio.buttons, c->radio.nbuttons, char *);
                c->radio.buttons[c->radio.nbuttons-1] =
                    dupstr("�����Զ�����ʾ��");
                c->radio.buttons[c->radio.nbuttons-2] =
                    dupstr("PC��������ʾ��");
                c->radio.buttondata =
                    sresize(c->radio.buttondata, c->radio.nbuttons, intorptr);
                c->radio.buttondata[c->radio.nbuttons-1] = I(BELL_WAVEFILE);
                c->radio.buttondata[c->radio.nbuttons-2] = I(BELL_PCSPEAKER);
                if (c->radio.shortcuts) {
                    c->radio.shortcuts =
                        sresize(c->radio.shortcuts, c->radio.nbuttons, char);
                    c->radio.shortcuts[c->radio.nbuttons-1] = NO_SHORTCUT;
                    c->radio.shortcuts[c->radio.nbuttons-2] = NO_SHORTCUT;
                }
                break;
            }
        }
    }
    ctrl_filesel(s, "�Զ�����ʾ���ļ���", NO_SHORTCUT,
                 FILTER_WAVE_FILES, false, "ѡ�������ļ�",
                 HELPCTX(bell_style),
                 conf_filesel_handler, I(CONF_bell_wavefile));

    /*
     * While we've got this box open, taskbar flashing on a bell is
     * also Windows-specific.
     */
    ctrl_radiobuttons(s, "������/��������ʾ����־��", 'i', 3,
                      HELPCTX(bell_taskbar),
                      conf_radiobutton_handler,
                      I(CONF_beep_ind),
                      "�ѽ���", I(B_IND_DISABLED),
                      "��˸", I(B_IND_FLASH),
                      "����", I(B_IND_STEADY));

    /*
     * The sunken-edge border is a Windows GUI feature.
     */
    s = ctrl_getset(b, "����/���", "border",
                    "�������ڱ߿�");
    ctrl_checkbox(s, "�³��߿��Ե(�Ժ�)", 's',
                  HELPCTX(appearance_border),
                  conf_checkbox_handler, I(CONF_sunken_edge));

    /*
     * Configurable font quality settings for Windows.
     */
    s = ctrl_getset(b, "����/���", "font",
                    "�������ã�");
    ctrl_checkbox(s, "����ѡ��ɱ�������", NO_SHORTCUT,
                  HELPCTX(appearance_font), variable_pitch_handler, I(0));
    ctrl_radiobuttons(s, "����Ч����", 'q', 2,
                      HELPCTX(appearance_font),
                      conf_radiobutton_handler,
                      I(CONF_font_quality),
                      "�����", I(FQ_ANTIALIASED),
                      "�޿����", I(FQ_NONANTIALIASED),
                      "ClearType", I(FQ_CLEARTYPE),
                      "Ĭ��", I(FQ_DEFAULT));

    /*
     * Cyrillic Lock is a horrid misfeature even on Windows, and
     * the least we can do is ensure it never makes it to any other
     * platform (at least unless someone fixes it!).
     */
    s = ctrl_getset(b, "����/�ַ�ת��", "tweaks", NULL);
    ctrl_checkbox(s, "��д����������Cyrillic�л�", 's',
                  HELPCTX(translation_cyrillic),
                  conf_checkbox_handler,
                  I(CONF_xlat_capslockcyr));

    /*
     * On Windows we can use but not enumerate translation tables
     * from the operating system. Briefly document this.
     */
    s = ctrl_getset(b, "����/�ַ�ת��", "trans",
                    "�������ݵ��ַ���ת��");
    ctrl_text(s, "(Windows֧�ֵ�δ�г����ַ���,"
              "����ܶ�ϵͳ�϶��е�CP866,�����ֶ�����)",
              HELPCTX(translation_codepage));

    /*
     * Windows has the weird OEM font mode, which gives us some
     * additional options when working with line-drawing
     * characters.
     */
    str = dupprintf("����%s�����ַ��ķ�ʽ��", appname);
    s = ctrl_getset(b, "����/�ַ�ת��", "linedraw", str);
    sfree(str);
    {
        int i;
        for (i = 0; i < s->ncontrols; i++) {
            c = s->ctrls[i];
            if (c->type == CTRL_RADIO &&
                c->context.i == CONF_vtmode) {
                assert(c->handler == conf_radiobutton_handler);
                c->radio.nbuttons += 3;
                c->radio.buttons =
                    sresize(c->radio.buttons, c->radio.nbuttons, char *);
                c->radio.buttons[c->radio.nbuttons-3] =
                    dupstr("X Windows ���߻���");
                c->radio.buttons[c->radio.nbuttons-2] =
                    dupstr("ANSI/OEM ģʽ����");
                c->radio.buttons[c->radio.nbuttons-1] =
                    dupstr("��OEMģʽ�������");
                c->radio.buttondata =
                    sresize(c->radio.buttondata, c->radio.nbuttons, intorptr);
                c->radio.buttondata[c->radio.nbuttons-3] = I(VT_XWINDOWS);
                c->radio.buttondata[c->radio.nbuttons-2] = I(VT_OEMANSI);
                c->radio.buttondata[c->radio.nbuttons-1] = I(VT_OEMONLY);
                if (!c->radio.shortcuts) {
                    int j;
                    c->radio.shortcuts = snewn(c->radio.nbuttons, char);
                    for (j = 0; j < c->radio.nbuttons; j++)
                        c->radio.shortcuts[j] = NO_SHORTCUT;
                } else {
                    c->radio.shortcuts = sresize(c->radio.shortcuts,
                                                 c->radio.nbuttons, char);
                }
                c->radio.shortcuts[c->radio.nbuttons-3] = 'x';
                c->radio.shortcuts[c->radio.nbuttons-2] = 'b';
                c->radio.shortcuts[c->radio.nbuttons-1] = 'e';
                break;
            }
        }
    }

    /*
     * RTF paste is Windows-specific.
     */
    s = ctrl_getset(b, "����/ѡ��/����", "format",
                    "�����ַ��ķ�ʽ��");
    ctrl_checkbox(s, "��RTF�ʹ��ı���ʽ����", 'f',
                  HELPCTX(copy_rtf),
                  conf_checkbox_handler, I(CONF_rtf_paste));

    /*
     * Windows often has no middle button, so we supply a selection
     * mode in which the more critical Paste action is available on
     * the right button instead.
     */
    s = ctrl_getset(b, "����/ѡ��", "mouse",
                    "����ʹ�ã�");
    ctrl_radiobuttons(s, "��갴��������", 'm', 1,
                      HELPCTX(selection_buttons),
                      conf_radiobutton_handler,
                      I(CONF_mouse_is_xterm),
                      "Windows--�м���չ,�Ҽ��˵�", I(2),
                      "Compromise--�м���չ,�Ҽ�ճ��", I(0),
                      "xterm--�Ҽ���չ,�м�ճ��", I(1));
    /*
     * This really ought to go at the _top_ of its box, not the
     * bottom, so we'll just do some shuffling now we've set it
     * up...
     */
    c = s->ctrls[s->ncontrols-1];      /* this should be the new control */
    memmove(s->ctrls+1, s->ctrls, (s->ncontrols-1)*sizeof(dlgcontrol *));
    s->ctrls[0] = c;

    /*
     * Logical palettes don't even make sense anywhere except Windows.
     */
    s = ctrl_getset(b, "����/��ɫ", "general",
                    "��ɫʹ�õĳ���ѡ��");
    ctrl_checkbox(s, "����ʹ���߼���ɫ��", 'l',
                  HELPCTX(colours_logpal),
                  conf_checkbox_handler, I(CONF_try_palette));
    ctrl_checkbox(s, "ʹ��ϵͳ��ɫ", 's',
                  HELPCTX(colours_system),
                  conf_checkbox_handler, I(CONF_system_colour));


    /*
     * Resize-by-changing-font is a Windows insanity.
     */

    backvt = backend_vt_from_proto(protocol);
    if (backvt)
        resize_forbidden = (backvt->flags & BACKEND_RESIZE_FORBIDDEN);
    if (!midsession || !resize_forbidden) {
        s = ctrl_getset(b, "����", "size", "���ô��ڴ�С");
        ctrl_radiobuttons(s, "�������ڴ�Сʱ��", 'z', 1,
                          HELPCTX(window_resize),
                          conf_radiobutton_handler,
                          I(CONF_resize_action),
                          "����������", I(RESIZE_TERM),
                          "���������С", I(RESIZE_FONT),
                          "�������ʱ���������С", I(RESIZE_EITHER),
                          "��ȫ��ֹ������С", I(RESIZE_DISABLED));
    }

    /*
     * Most of the Window/Behaviour stuff is there to mimic Windows
     * conventions which PuTTY can optionally disregard. Hence,
     * most of these options are Windows-specific.
     */
    s = ctrl_getset(b, "����/��Ϊ", "main", NULL);
    ctrl_checkbox(s, "ALT-F4 �رմ���", '4',
                  HELPCTX(behaviour_altf4),
                  conf_checkbox_handler, I(CONF_alt_f4));
    ctrl_checkbox(s, "ALT-Space ��ʾϵͳ�˵�", 'y',
                  HELPCTX(behaviour_altspace),
                  conf_checkbox_handler, I(CONF_alt_space));
    ctrl_checkbox(s, "ALT ��ʾϵͳ�˵�", 'l',
                  HELPCTX(behaviour_altonly),
                  conf_checkbox_handler, I(CONF_alt_only));
    ctrl_checkbox(s, "�������Ǳ����ڶ���", 'e',
                  HELPCTX(behaviour_alwaysontop),
                  conf_checkbox_handler, I(CONF_alwaysontop));
    ctrl_checkbox(s, "Alt-Enter ����ȫ��", 'f',
                  HELPCTX(behaviour_altenter),
                  conf_checkbox_handler,
                  I(CONF_fullscreenonaltenter));

    /*
     * Windows supports a local-command proxy.
     */
    if (!midsession) {
        int i;
        s = ctrl_getset(b, "����/����", "basics", NULL);
        for (i = 0; i < s->ncontrols; i++) {
            c = s->ctrls[i];
            if (c->type == CTRL_LISTBOX &&
                c->handler == proxy_type_handler) {
                c->context.i |= PROXY_UI_FLAG_LOCAL;
                break;
            }
        }
    }

    /*
     * $XAUTHORITY is not reliable on Windows, so we provide a
     * means to override it.
     */
    if (!midsession && backend_vt_from_proto(PROT_SSH)) {
        s = ctrl_getset(b, "����/SSH/X11", "x11", "X11ת��");
        ctrl_filesel(s, "���ڱ�����ʾ��X��Ȩ�ļ���", 't',
                     NULL, false, "ѡ����Ȩ�ļ�",
                     HELPCTX(ssh_tunnels_xauthority),
                     conf_filesel_handler, I(CONF_xauthfile));
    }
}
